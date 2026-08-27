#ifndef SPRITE_LIGHTING_HLSLI
#define SPRITE_LIGHTING_HLSLI

#include "sprite_lighting.h"

#ifndef SPRITE_LIGHT_BUFFER_REGISTER
#error SPRITE_LIGHT_BUFFER_REGISTER must be defined before including sprite_lighting.hlsli
#endif

cbuffer LightBuffer : register(SPRITE_LIGHT_BUFFER_REGISTER)
{
    float4 radial_light;
    float4 light_levels;
    float4 light_color;
    float4 direct_light;
    float4 point_light_meta;
    float4 point_lights[SPRITE_POINT_LIGHT_CAPACITY];
    float4 point_light_colors[SPRITE_POINT_LIGHT_CAPACITY];
}

float3 SpriteLighting_ApplyRadial(float3 color, float2 screen_position)
{
    const float distance_from_light = distance(screen_position, radial_light.xy);
    const float inner_radius = radial_light.z * 0.20f;
    const float attenuation = 1.0f - smoothstep(
        inner_radius, radial_light.z, distance_from_light);
    const float brightness = lerp(light_levels.x, light_levels.y, attenuation);
    const float3 corrected_color =
        pow(saturate(color), light_levels.zzz) * brightness;
    const float3 radial_color =
        light_color.rgb * light_color.a * attenuation;
    const float enabled = step(0.5f, radial_light.w);
    return lerp(color, corrected_color + radial_color, enabled);
}

float3 SpriteLighting_ApplyWorld(float3 color, float2 screen_position)
{
    float3 illumination = point_light_meta.yyy +
        direct_light.rgb * direct_light.a;
    const int point_light_count = min(
        (int)point_light_meta.x, SPRITE_POINT_LIGHT_CAPACITY);
    [loop]
    for (int i = 0; i < point_light_count; ++i)
    {
        const float distance_from_light = distance(
            screen_position, point_lights[i].xy);
        const float inner_radius = point_lights[i].z * 0.12f;
        const float attenuation = 1.0f - smoothstep(
            inner_radius, point_lights[i].z, distance_from_light);
        illumination += point_light_colors[i].rgb *
            point_lights[i].w * attenuation;
    }
    return color * max(illumination, 0.0f);
}

#endif
