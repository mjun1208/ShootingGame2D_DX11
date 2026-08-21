struct PS_IN
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float color_mask : COLOR1;
};

Texture2D lightning_texture : register(t0);
SamplerState lightning_sampler : register(s0);

float4 main(PS_IN input) : SV_TARGET
{
    // The authored sprite stores the bolt and its falloff in alpha.  Build
    // three controllable bands from that single mask so the core stays clean
    // even when the sprite is stretched over a long chain jump.
    uint texture_width;
    uint texture_height;
    lightning_texture.GetDimensions(texture_width, texture_height);
    const float2 horizontal_texel = float2(
        1.0f / max(float(texture_width), 1.0f), 0.0f);

    const float mask = lightning_texture.Sample(lightning_sampler, input.uv).a;
    float core_mask = mask;
    core_mask = max(core_mask, lightning_texture.Sample(
        lightning_sampler, input.uv - horizontal_texel * 2.0f).a);
    core_mask = max(core_mask, lightning_texture.Sample(
        lightning_sampler, input.uv + horizontal_texel * 2.0f).a);
    core_mask = max(core_mask, lightning_texture.Sample(
        lightning_sampler, input.uv - horizontal_texel * 4.0f).a);
    core_mask = max(core_mask, lightning_texture.Sample(
        lightning_sampler, input.uv + horizontal_texel * 4.0f).a);

    float body_mask = core_mask;
    body_mask = max(body_mask, lightning_texture.Sample(
        lightning_sampler, input.uv - horizontal_texel * 6.0f).a);
    body_mask = max(body_mask, lightning_texture.Sample(
        lightning_sampler, input.uv + horizontal_texel * 6.0f).a);
    if (body_mask <= 0.002f)
    {
        discard;
    }

    const float glow = pow(saturate(max(mask, body_mask * 0.48f)), 0.72f);
    const float body = smoothstep(0.10f, 0.48f, body_mask);
    const float core = smoothstep(0.58f, 0.92f, core_mask);
    const float fade = saturate(input.color.a);

    const float3 glow_color = input.color.rgb;
    const float3 body_color = lerp(glow_color, float3(0.45f, 0.95f, 1.0f), 0.55f);
    const float3 core_color = float3(1.0f, 1.0f, 1.0f);
    const float3 emission =
        glow_color * glow * 0.42f +
        body_color * body * 0.92f +
        core_color * core * 1.85f;

    // The lightning draw path uses ONE + ONE blending, so brightness and
    // lifetime are applied here instead of being multiplied by SrcAlpha.
    return float4(emission * fade, saturate((glow + body + core) * fade));
}
