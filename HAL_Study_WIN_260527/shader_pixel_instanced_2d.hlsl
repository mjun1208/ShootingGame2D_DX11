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

float4 main(PS_IN input) : SV_TARGET
{
    const float4 sampled_color = sprite_texture.Sample(sprite_sampler, input.uv);
    float4 color = sampled_color * input.color;
    if (input.color_mask >= 2.0f)
    {
        // Values in [2, 3] select an outside-in radial fill. This lets spawn
        // telegraphs reveal the rune art itself without a second texture or
        // one draw call per clipping band.
        const float fill_progress = saturate(input.color_mask - 2.0f);
        const float radial_distance = saturate(length(input.uv - 0.5f) * 2.0f);
        const float fill_edge = 1.0f - fill_progress;
        const float reveal = smoothstep(
            fill_edge - 0.025f, fill_edge + 0.025f, radial_distance);
        const float leading_glow = 1.0f - smoothstep(
            0.0f, 0.075f, abs(radial_distance - fill_edge));
        color.a *= reveal;
        color.rgb *= 0.82f + leading_glow * 1.15f;
    }
    else if (input.color_mask > 0.5f)
    {
        color = float4(input.color.rgb, sampled_color.a * input.color.a);
    }
    if (color.a <= 0.001f)
    {
        discard;
    }

    color.rgb = SpriteLighting_ApplyRadial(color.rgb, input.position.xy);
    color.rgb = SpriteLighting_ApplyWorld(color.rgb, input.position.xy);
    return color;
}
