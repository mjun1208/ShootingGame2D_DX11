// 기본 스프라이트 픽셀 셰이더: 텍스처 → 색상/마스크 → 디졸브 → 월드 조명 순서로 처리한다.
// 픽셀 단계의 SV_POSITION.xy는 화면 픽셀 좌표이고 SV_TARGET은 렌더 타깃에 쓸 색이다.
// b0은 스프라이트 설정이므로 공통 조명 버퍼는 b1에 연결한다.

#define SPRITE_LIGHT_BUFFER_REGISTER b1
#include "sprite_lighting.hlsli"

struct PS_IN
{
    float4 screen_position : SV_POSITION0;
    float2 uv : TEXCOORD0;
};

cbuffer SpriteBuffer : register(b0)
{
    // color: RGB 틴트와 알파. dissolve: x=활성, y=소멸량, z=경계 폭, w=단색 마스크 모드.
    // edge_color: RGB 경계색, a=경계 발광 강도. CPU의 상수 버퍼 배치를 그대로 따른다.
    float4 color;
    float4 dissolve;
    float4 edge_color;
}

Texture2D sprite_texture : register(t0);
Texture2D dissolve_texture : register(t1);
SamplerState sprite_sampler : register(s0);

static const float FLAG_ENABLED_THRESHOLD = 0.5f;
static const float ALPHA_DISCARD_THRESHOLD = 0.001f;
static const float MIN_DISSOLVE_EDGE_WIDTH = 0.001f;

float4 main(PS_IN input) : SV_TARGET
{
    const float4 sampled_color = sprite_texture.Sample(sprite_sampler, input.uv);
    float4 texture_color = sampled_color * color;
    const bool use_color_mask = dissolve.w > FLAG_ENABLED_THRESHOLD;
    if (use_color_mask)
    {
        // 원본 RGB 대신 지정색을 사용하고 텍스처의 알파 윤곽만 유지한다.
        texture_color = float4(color.rgb, sampled_color.a * color.a);
    }
    // 거의 투명한 픽셀은 이후 효과/조명 계산 전에 버린다.
    if (texture_color.a <= ALPHA_DISCARD_THRESHOLD)
    {
        discard;
    }

    // 노이즈가 소멸량보다 작은 영역부터 지운다. 소멸량을 높이면 남는 면적이 줄어든다.
    const bool dissolve_enabled = dissolve.x > FLAG_ENABLED_THRESHOLD;
    if (dissolve_enabled)
    {
        const float noise = dissolve_texture.Sample(sprite_sampler, input.uv).r;
        // CPU의 압축된 float4 설정에서 진행량과 경계 폭을 읽는다.
        const float dissolve_amount = saturate(dissolve.y);
        const float dissolve_edge_width = max(dissolve.z, MIN_DISSOLVE_EDGE_WIDTH);
        const float edge_distance = noise - dissolve_amount;

        if (edge_distance < 0.0f)
        {
            discard;
        }

        // smoothstep은 두 경계 사이를 부드럽게 잇는다. 소멸 경계 근처에만 빛을 더한다.
        const float edge = 1.0f - smoothstep(0.0f, dissolve_edge_width, edge_distance);
        texture_color.rgb += edge_color.rgb * edge_color.a * edge;
    }

    texture_color.rgb = SpriteLighting_ApplyWorld(
        texture_color.rgb, input.screen_position.xy);

    return texture_color;
}
