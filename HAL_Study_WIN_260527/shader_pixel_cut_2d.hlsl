// 절단 조각 픽셀 셰이더: 반쪽 선택 → 노이즈 소멸 → 절단면 색상 → 조명 순서로 처리한다.
// clip(x)는 x가 음수인 픽셀을 버린다. saturate(x)는 값을 0~1로 제한한다.

#define SPRITE_LIGHT_BUFFER_REGISTER b0
#include "sprite_lighting.hlsli"

struct PS_IN
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float2 local_position : TEXCOORD1;
    float4 color : COLOR0;
    // 보간하지 않는 조각별 설정: 절단 법선, 진행 정보, 경계색.
    nointerpolation float2 cut_normal : TEXCOORD2;
    nointerpolation float4 cut_parameters : TEXCOORD3;
    nointerpolation float4 edge_color : COLOR1;
};

Texture2D sprite_texture : register(t0);
SamplerState sprite_sampler : register(s0);

// 소멸 무늬와 경계 두께는 회전 전 로컬 좌표 단위다.
static const float DISSOLVE_CELL_SIZE = 4.0f;
static const float DISSOLVE_NOISE_WIDTH = 24.0f;
static const float CUT_EDGE_WIDTH = 2.25f;
static const float DISSOLVE_EDGE_WIDTH = 5.5f;
static const float ALPHA_DISCARD_THRESHOLD = 0.001f;
static const float DISSOLVE_EDGE_START = 0.001f;

// 셀 위치와 시드가 같으면 같은 0~1 노이즈를 만든다. 별도 노이즈 텍스처가 필요 없다.
float HashCell(float2 cell, float seed)
{
    float3 value = frac(float3(cell.xyx) * 0.1031f + seed * 0.017f);
    value += dot(value, value.yzx + 33.33f);
    return frac((value.x + value.y) * value.z);
}

float4 main(PS_IN input) : SV_TARGET
{
    float4 color = sprite_texture.Sample(sprite_sampler, input.uv) * input.color;
    if (color.a <= ALPHA_DISCARD_THRESHOLD)
    {
        discard;
    }

    // 법선 길이로 나누어 거리 단위를 맞추고, 0으로 나누는 상황을 피한다.
    // cut_parameters: x=남길 방향(+1/-1), y=소멸 진행률, z=최대 절단 거리, w=노이즈 시드.
    const float normal_length = max(length(input.cut_normal), 0.0001f);
    const float2 cut_normal = input.cut_normal / normal_length;
    const float cut_side = input.cut_parameters.x;
    const float dissolve_progress = saturate(input.cut_parameters.y);
    const float max_cut_distance = max(input.cut_parameters.z, 1.0f);
    const float noise_seed = input.cut_parameters.w;

    // 로컬 원점을 지나는 절단선까지의 부호 있는 거리. 방향을 곱해 남길 반쪽을 고른다.
    const float signed_distance = dot(input.local_position, cut_normal);
    const float distance_into_piece = signed_distance * cut_side;
    clip(distance_into_piece);

    // 4단위 셀마다 같은 노이즈를 사용해 픽셀 아트의 각진 소멸 무늬를 유지한다.
    const float cell_noise = HashCell(
        floor(input.local_position / DISSOLVE_CELL_SIZE), noise_seed);
    const float dissolve_front =
        dissolve_progress * (max_cut_distance + DISSOLVE_NOISE_WIDTH);
    const float distance_to_dissolve_front =
        distance_into_piece + cell_noise * DISSOLVE_NOISE_WIDTH - dissolve_front;
    clip(distance_to_dissolve_front);

    // 절단선과 소멸 경계 중 더 강한 띠를 선택해 edge_color로 물들인다.
    const float cut_edge = 1.0f - smoothstep(0.0f, CUT_EDGE_WIDTH, distance_into_piece);
    const float dissolve_edge = dissolve_progress > DISSOLVE_EDGE_START ?
        1.0f - smoothstep(0.0f, DISSOLVE_EDGE_WIDTH, distance_to_dissolve_front) : 0.0f;
    const float edge = saturate(max(cut_edge, dissolve_edge));
    color.rgb = lerp(
        color.rgb,
        input.edge_color.rgb,
        edge * saturate(input.edge_color.a));

    color.rgb = SpriteLighting_ApplyRadial(color.rgb, input.position.xy);
    color.rgb = SpriteLighting_ApplyWorld(color.rgb, input.position.xy);
    return color;
}
