Texture2D SceneTexture : register(t0);
SamplerState SceneSampler : register(s0);

cbuffer TimeStopConstants : register(b0)
{
    float2 CenterUv;
    float Radius;
    float Phase;
    float2 Resolution;
    float EffectTime;
    float MaxRadius;
};

float3 TimeStoppedGrade(float3 color)
{
    float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    float3 graded = lerp(luminance.xxx, color, 0.58f);
    graded *= float3(0.91f, 0.96f, 1.035f);
    return graded + float3(0.004f, 0.010f, 0.022f);
}

float Ring(float signed_distance, float half_width, float feather)
{
    return 1.0f - smoothstep(
        half_width, half_width + feather, abs(signed_distance));
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float aspect = Resolution.x / max(Resolution.y, 1.0f);
    float2 delta = uv - CenterUv;
    float2 aspect_delta = float2(delta.x * aspect, delta.y);
    float distance_from_center = length(aspect_delta);
    float angle = atan2(aspect_delta.y, aspect_delta.x);

    // Three harmonics keep the front fluid instead of looking like a stock circle wipe.
    float wobble = sin(angle * 5.0f + EffectTime * 7.0f) * 0.0090f;
    wobble += sin(angle * 11.0f - EffectTime * 4.5f) * 0.0050f;
    wobble += sin(angle * 23.0f + EffectTime * 2.7f) * 0.0022f;
    float boundary = Radius + wobble;
    float signed_wave_distance = distance_from_center - boundary;
    float pixel_width = 1.0f / max(Resolution.y, 1.0f);

    float inside = 1.0f - smoothstep(
        -pixel_width * 2.0f, pixel_width * 2.0f, signed_wave_distance);
    float core = Ring(signed_wave_distance, pixel_width * 1.6f, pixel_width * 3.0f);
    float halo = Ring(signed_wave_distance, 0.006f, 0.028f);
    float inner_echo = Ring(signed_wave_distance + 0.040f, 0.0025f, 0.010f);
    float outer_echo = Ring(signed_wave_distance - 0.052f, 0.0020f, 0.012f);

    float2 direction = aspect_delta / max(distance_from_center, 0.0001f);
    direction.x /= aspect;
    float ripple = sin(signed_wave_distance * 215.0f - EffectTime * 13.0f);
    float distortion_strength = halo * ripple * 0.012f;
    float2 distorted_uv = uv + direction * distortion_strength;

    float4 original = SceneTexture.Sample(SceneSampler, uv);
    float4 warped = SceneTexture.Sample(SceneSampler, distorted_uv);
    float chroma_offset = halo * 0.0028f;
    float red = SceneTexture.Sample(
        SceneSampler, distorted_uv + direction * chroma_offset).r;
    float blue = SceneTexture.Sample(
        SceneSampler, distorted_uv - direction * chroma_offset).b;
    float3 refracted = float3(red, warped.g, blue);

    float3 inverted = pow(saturate(1.0f - refracted), 0.88f);
    inverted = lerp(inverted, inverted.bgr, 0.08f);
    float3 desaturated = TimeStoppedGrade(refracted);
    float3 result = original.rgb;

    if (Phase > 0.5f && Phase < 1.5f)
    {
        result = lerp(refracted, inverted, inside);
    }
    else if (Phase > 1.5f && Phase < 2.5f)
    {
        result = lerp(desaturated, inverted, inside);
    }
    else if (Phase > 2.5f && Phase < 3.5f)
    {
        result = TimeStoppedGrade(original.rgb);
    }
    else if (Phase > 3.5f && Phase < 4.5f)
    {
        result = lerp(refracted, desaturated, inside);
    }

    const bool has_moving_wave =
        Phase > 0.5f && (Phase < 2.5f || Phase > 3.5f);
    if (has_moving_wave)
    {
        float palette_shift = 0.5f + 0.5f * sin(angle * 3.0f - EffectTime * 2.5f);
        float3 spectral_color = lerp(
            float3(0.12f, 0.52f, 1.00f),
            float3(0.67f, 0.24f, 1.00f),
            palette_shift);
        result = lerp(result, spectral_color, halo * 0.24f);
        result += spectral_color * (inner_echo * 0.11f + outer_echo * 0.07f);
        result += (1.0f - result) * core * 0.78f;
    }

    return float4(saturate(result), original.a);
}
