struct PS_IN
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float color_mask : COLOR1;
};

Texture2D sprite_texture : register(t0);
SamplerState sprite_sampler : register(s0);

static const int POINT_LIGHT_CAPACITY = 16;

cbuffer LightBuffer : register(b0)
{
    float4 radial_light;
    float4 light_levels;
    float4 light_color;
    float4 direct_light;
    float4 point_light_meta;
    float4 point_lights[POINT_LIGHT_CAPACITY];
    float4 point_light_colors[POINT_LIGHT_CAPACITY];
}

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

    if (radial_light.w > 0.5f)
    {
        const float distance_from_light = distance(input.position.xy, radial_light.xy);
        const float inner_radius = radial_light.z * 0.20f;
        const float attenuation = 1.0f - smoothstep(
            inner_radius, radial_light.z, distance_from_light);
        const float brightness = lerp(light_levels.x, light_levels.y, attenuation);
        color.rgb = pow(saturate(color.rgb), light_levels.zzz) * brightness;
        color.rgb += light_color.rgb * light_color.a * attenuation;
    }

    color.rgb *= 1.0f + direct_light.rgb * direct_light.a;

    const int point_light_count = min((int)point_light_meta.x, POINT_LIGHT_CAPACITY);
    [loop]
    for (int i = 0; i < point_light_count; ++i)
    {
        const float distance_from_light = distance(input.position.xy, point_lights[i].xy);
        const float inner_radius = point_lights[i].z * 0.12f;
        const float attenuation = 1.0f - smoothstep(
            inner_radius, point_lights[i].z, distance_from_light);
        color.rgb += point_light_colors[i].rgb * point_lights[i].w * attenuation;
    }
    return color;
}
