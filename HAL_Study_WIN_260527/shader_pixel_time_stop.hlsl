// 시간 정지 후처리: 이미 그린 장면에 원형 파동·굴절·색 반전·차가운 색조를 적용한다.
// SceneTexture(t0)는 후처리 전 장면, SceneSampler(s0)는 장면을 읽는 샘플러다.

Texture2D SceneTexture : register(t0);
SamplerState SceneSampler : register(s0);

cbuffer TimeStopConstants : register(b0)
{
    // CPU의 time_stop_effect.cpp에 있는 PixelConstants와 동일한 배치. CenterUv는 화면의 0~1 중심 좌표.
    // Radius는 화면 높이 기준 반경, Phase는 0=비활성/1=확산/2=회귀/3=유지/4=복원.
    float2 CenterUv;
    float Radius;
    float Phase;
    // Resolution은 화면 픽셀 크기, EffectTime은 효과 경과 시간(초).
    float2 Resolution;
    float EffectTime;
    // CPU와 버퍼 배치를 맞추는 최대 반경 필드. 현재 셰이더 계산에서는 사용하지 않는다.
    float MaxRadius;
};

// 밝기를 가중합으로 구해 채도를 낮추고 청색을 더해 시간 정지 상태를 표현한다.
float3 TimeStoppedGrade(float3 color)
{
    float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    float3 graded = lerp(luminance.xxx, color, 0.58f);
    graded *= float3(0.91f, 0.96f, 1.035f);
    return graded + float3(0.004f, 0.010f, 0.022f);
}

// 경계까지의 거리 절댓값으로 양쪽에 띠를 만들고 feather 폭만큼 가장자리를 흐린다.
float Ring(float signed_distance, float half_width, float feather)
{
    return 1.0f - smoothstep(
        half_width, half_width + feather, abs(signed_distance));
}

// CPU EffectPhase 값과 대응한다. 기존의 열린 구간 판정을 그대로 유지한다.
static const float PHASE_WAVE_OUT = 1.0f;
static const float PHASE_WAVE_RETURN = 2.0f;
static const float PHASE_HOLD = 3.0f;
static const float PHASE_RESTORE = 4.0f;
static const float PHASE_HALF_RANGE = 0.5f;

bool IsPhase(float phase_value)
{
    return Phase > phase_value - PHASE_HALF_RANGE &&
        Phase < phase_value + PHASE_HALF_RANGE;
}

// 한 픽셀에서 계산한 파동 정보를 굴절과 색상 합성 단계가 공유한다.
struct WaveData
{
    float angle;
    float signed_wave_distance;
    float inside;
    float core;
    float halo;
    float inner_echo;
    float outer_echo;
    float2 direction;
};

// 화면 비율을 보정한 거리로 경계 마스크와 UV 이동 방향을 구한다.
WaveData CalculateWave(float2 uv)
{
    // 가로 거리에 화면 종횡비를 반영해 와이드 화면에서도 파동이 원형으로 보이게 한다.
    float aspect = Resolution.x / max(Resolution.y, 1.0f);
    float2 delta = uv - CenterUv;
    float2 aspect_delta = float2(delta.x * aspect, delta.y);
    float distance_from_center = length(aspect_delta);
    float angle = atan2(aspect_delta.y, aspect_delta.x);

    // 주기가 다른 세 물결을 겹쳐 원형 경계가 유기적으로 흔들리게 한다.
    float wobble = sin(angle * 5.0f + EffectTime * 7.0f) * 0.0090f;
    wobble += sin(angle * 11.0f - EffectTime * 4.5f) * 0.0050f;
    wobble += sin(angle * 23.0f + EffectTime * 2.7f) * 0.0022f;
    float boundary = Radius + wobble;
    float signed_wave_distance = distance_from_center - boundary;
    float pixel_width = 1.0f / max(Resolution.y, 1.0f);

    // 경계 안쪽 마스크와 중심선/광채/안팎 잔상을 분리한다. pixel_width는 화면 1픽셀의 높이다.
    float inside = 1.0f - smoothstep(
        -pixel_width * 2.0f, pixel_width * 2.0f, signed_wave_distance);
    float core = Ring(signed_wave_distance, pixel_width * 1.6f, pixel_width * 3.0f);
    float halo = Ring(signed_wave_distance, 0.006f, 0.028f);
    float inner_echo = Ring(signed_wave_distance + 0.040f, 0.0025f, 0.010f);
    float outer_echo = Ring(signed_wave_distance - 0.052f, 0.0020f, 0.012f);

    // 중심→현재 픽셀 방향으로 UV를 흔든다. 가로축은 UV 단위로 되돌린다.
    float2 direction = aspect_delta / max(distance_from_center, 0.0001f);
    direction.x /= aspect;

    WaveData wave;
    wave.angle = angle;
    wave.signed_wave_distance = signed_wave_distance;
    wave.inside = inside;
    wave.core = core;
    wave.halo = halo;
    wave.inner_echo = inner_echo;
    wave.outer_echo = outer_echo;
    wave.direction = direction;
    return wave;
}

// 파동 주변의 장면을 흔들고 RGB 샘플 위치를 분리해 굴절을 표현한다.
float3 SampleRefractedScene(float2 uv, WaveData wave)
{
    float ripple = sin(wave.signed_wave_distance * 215.0f - EffectTime * 13.0f);
    float distortion_strength = wave.halo * ripple * 0.012f;
    float2 distorted_uv = uv + wave.direction * distortion_strength;

    float4 warped = SceneTexture.Sample(SceneSampler, distorted_uv);
    // 빨강/파랑을 반대쪽 UV에서 읽어 파동 주변에 색수차를 만든다.
    float chroma_offset = wave.halo * 0.0028f;
    float red = SceneTexture.Sample(
        SceneSampler, distorted_uv + wave.direction * chroma_offset).r;
    float blue = SceneTexture.Sample(
        SceneSampler, distorted_uv - wave.direction * chroma_offset).b;
    return float3(red, warped.g, blue);
}

// 파동 안팎에 적용할 색을 현재 시간 정지 단계에 맞춰 선택한다.
float3 ApplyPhaseColor(float3 original_color, float3 refracted, float inside_mask)
{
    float3 inverted = pow(saturate(1.0f - refracted), 0.88f);
    inverted = lerp(inverted, inverted.bgr, 0.08f);
    float3 desaturated = TimeStoppedGrade(refracted);
    float3 result = original_color;

    // 1: 확산 파동 안쪽을 반전. 2: 줄어드는 반전 영역 바깥을 정지 색조로 전환.
    // 3: 전체 정지 색조 유지. 4: 줄어드는 정지 영역 바깥을 원래 색으로 복원.
    if (IsPhase(PHASE_WAVE_OUT))
    {
        result = lerp(refracted, inverted, inside_mask);
    }
    else if (IsPhase(PHASE_WAVE_RETURN))
    {
        result = lerp(desaturated, inverted, inside_mask);
    }
    else if (IsPhase(PHASE_HOLD))
    {
        result = TimeStoppedGrade(original_color);
    }
    else if (IsPhase(PHASE_RESTORE))
    {
        result = lerp(refracted, desaturated, inside_mask);
    }

    return result;
}

// 움직이는 파동 위에 색 광채, 잔상, 흰 중심선을 차례로 더한다.
float3 ApplyWaveGlow(float3 result, WaveData wave)
{
    float palette_shift = 0.5f + 0.5f * sin(wave.angle * 3.0f - EffectTime * 2.5f);
    float3 spectral_color = lerp(
        float3(0.12f, 0.52f, 1.00f),
        float3(0.67f, 0.24f, 1.00f),
        palette_shift);
    result = lerp(result, spectral_color, wave.halo * 0.24f);
    result += spectral_color * (wave.inner_echo * 0.11f + wave.outer_echo * 0.07f);
    result += (1.0f - result) * wave.core * 0.78f;
    return result;
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    const WaveData wave = CalculateWave(uv);
    const float4 original = SceneTexture.Sample(SceneSampler, uv);
    const float3 refracted = SampleRefractedScene(uv, wave);
    float3 result = ApplyPhaseColor(original.rgb, refracted, wave.inside);

    // 기존 범위 판정을 유지한다. 정상 단계에서는 확산/회귀/복원 중에만 파동을 그린다.
    const bool has_moving_wave =
        Phase > PHASE_WAVE_OUT - PHASE_HALF_RANGE &&
        (Phase < PHASE_HOLD - PHASE_HALF_RANGE || Phase > PHASE_HOLD + PHASE_HALF_RANGE);
    if (has_moving_wave)
    {
        result = ApplyWaveGlow(result, wave);
    }

    return float4(saturate(result), original.a);
}
