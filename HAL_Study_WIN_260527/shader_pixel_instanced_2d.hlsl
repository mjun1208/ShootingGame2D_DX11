// 인스턴싱 픽셀 셰이더: 개체별 색상 모드와 조명을 적용한다.
// color_mask: 0은 원본 색 곱하기, 1은 알파 모양만 이용한 단색, 2~3은 바깥→안쪽 채움이다.
// t0은 텍스처 슬롯, s0은 필터링/주소 처리용 샘플러 슬롯, b0은 공통 조명 버퍼다.

#define SPRITE_LIGHT_BUFFER_REGISTER b0
#include "sprite_lighting.hlsli"

struct PS_IN
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float color_mask : COLOR1;
};

Texture2D sprite_texture : register(t0);
SamplerState sprite_sampler : register(s0);

// color_mask는 모드와 진행률을 함께 저장한다. CPU가 전달하는 값 범위를 유지한다.
static const float COLOR_MASK_THRESHOLD = 0.5f;
static const float RADIAL_FILL_MODE_BASE = 2.0f;
static const float ALPHA_DISCARD_THRESHOLD = 0.001f;
static const float FILL_EDGE_FEATHER = 0.025f;
static const float FILL_GLOW_WIDTH = 0.075f;

// 추가 텍스처 없이 UV 거리로 소환 예고 문양을 바깥에서 안쪽으로 드러낸다.
float4 ApplyRadialFill(float4 color, float2 uv, float color_mask)
{
    // UV 중심에서의 거리를 이용한다. 진행률이 커지면 표시 경계가 중심으로 이동한다.
    const float fill_progress = saturate(color_mask - RADIAL_FILL_MODE_BASE);
    const float radial_distance = saturate(length(uv - 0.5f) * 2.0f);
    const float fill_edge = 1.0f - fill_progress;
    const float reveal = smoothstep(
        fill_edge - FILL_EDGE_FEATHER, fill_edge + FILL_EDGE_FEATHER, radial_distance);
    // 진행 경계 주변에만 밝은 띠를 더해 채워지는 방향을 보여 준다.
    const float leading_glow = 1.0f - smoothstep(
        0.0f, FILL_GLOW_WIDTH, abs(radial_distance - fill_edge));
    color.a *= reveal;
    color.rgb *= 0.82f + leading_glow * 1.15f;
    return color;
}

float4 main(PS_IN input) : SV_TARGET
{
    const float4 sampled_color = sprite_texture.Sample(sprite_sampler, input.uv);
    float4 color = sampled_color * input.color;
    if (input.color_mask >= RADIAL_FILL_MODE_BASE)
    {
        color = ApplyRadialFill(color, input.uv, input.color_mask);
    }
    else if (input.color_mask > COLOR_MASK_THRESHOLD)
    {
        // 텍스처 RGB 대신 지정색을 쓰되, 원본 알파 윤곽과 개체 투명도는 유지한다.
        color = float4(input.color.rgb, sampled_color.a * input.color.a);
    }
    if (color.a <= ALPHA_DISCARD_THRESHOLD)
    {
        discard;
    }

    // 화면 중심형 조명 보정을 먼저 적용하고 월드 광원 밝기를 곱한다. 알파는 유지한다.
    color.rgb = SpriteLighting_ApplyRadial(color.rgb, input.position.xy);
    color.rgb = SpriteLighting_ApplyWorld(color.rgb, input.position.xy);
    return color;
}
