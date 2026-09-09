// 여러 스프라이트 픽셀 셰이더가 공유하는 조명 함수와 상수 버퍼.
// .hlsli는 #include로 가져오는 공통 코드다. 포함하는 쪽에서 조명 버퍼 슬롯을 지정한다.
// CPU의 SpriteLightConstants와 아래 필드의 순서/크기가 일치해야 한다.

#ifndef SPRITE_LIGHTING_HLSLI
#define SPRITE_LIGHTING_HLSLI

#include "Engine/Graphics/sprite_lighting.h"

#ifndef SPRITE_LIGHT_BUFFER_REGISTER
#error SPRITE_LIGHT_BUFFER_REGISTER must be defined before including sprite_lighting.hlsli
#endif

cbuffer LightBuffer : register(SPRITE_LIGHT_BUFFER_REGISTER)
{
    // radial_light: xy=화면 중심(픽셀), z=반경, w=활성 여부.
    // light_levels: x=주변 밝기, y=중심 밝기, z=감마 지수, w=미사용.
    // light_color: rgb=추가 광채색, a=광채 강도. direct_light: rgb=전역 광원색, a=강도.
    // point_light_meta: x=점광원 개수, y=환경 밝기, zw=미사용.
    // point_lights: xy=화면 위치, z=화면 반경, w=강도. point_light_colors: rgb=색, a=미사용.
    float4 radial_light;
    float4 light_levels;
    float4 light_color;
    float4 direct_light;
    float4 point_light_meta;
    float4 point_lights[SPRITE_POINT_LIGHT_CAPACITY];
    float4 point_light_colors[SPRITE_POINT_LIGHT_CAPACITY];
}

// 화면상의 원형 조명: 중앙 20%는 최대 밝기, 바깥 반경까지 부드럽게 감쇠한다.
float3 SpriteLighting_ApplyRadial(float3 color, float2 screen_position)
{
    // 벡터 성분 접근은 여기서 풀고, 아래 수식에는 역할이 드러나는 이름을 쓴다.
    const float2 light_position = radial_light.xy;
    const float light_radius = radial_light.z;
    const float ambient_brightness = light_levels.x;
    const float peak_brightness = light_levels.y;
    const float gamma = light_levels.z;
    const float glow_strength = light_color.a;
    const float distance_from_light = distance(screen_position, light_position);
    const float inner_radius = light_radius * 0.20f;
    const float attenuation = 1.0f - smoothstep(
        inner_radius, light_radius, distance_from_light);
    const float brightness = lerp(ambient_brightness, peak_brightness, attenuation);
    // 감마 지수로 색의 명암을 보정한 뒤 밝기와 색 광채를 적용한다.
    const float3 corrected_color =
        pow(saturate(color), gamma.xxx) * brightness;
    const float3 radial_color =
        light_color.rgb * glow_strength * attenuation;
    // step은 활성 플래그를 0/1로 바꾼다. 꺼져 있으면 입력 색을 그대로 선택한다.
    const float enabled = step(0.5f, radial_light.w);
    return lerp(color, corrected_color + radial_color, enabled);
}

// 월드 조명도 거리 계산은 화면 좌표로 한다. CPU가 광원 위치와 반경을 미리 변환한다.
float3 SpriteLighting_ApplyWorld(float3 color, float2 screen_position)
{
    // 환경 밝기 + 전역 광원 + 거리 감쇠한 점광원을 합산한다. 법선 기반 명암 계산은 없다.
    const float ambient_brightness = point_light_meta.y;
    const float direct_strength = direct_light.a;
    float3 illumination = ambient_brightness.xxx +
        direct_light.rgb * direct_strength;
    const int point_light_count = min(
        (int)point_light_meta.x, SPRITE_POINT_LIGHT_CAPACITY);
    [loop]
    for (int i = 0; i < point_light_count; ++i)
    {
        const float2 light_position = point_lights[i].xy;
        const float light_radius = point_lights[i].z;
        const float light_strength = point_lights[i].w;
        const float distance_from_light = distance(
            screen_position, light_position);
        const float inner_radius = light_radius * 0.12f;
        const float attenuation = 1.0f - smoothstep(
            inner_radius, light_radius, distance_from_light);
        illumination += point_light_colors[i].rgb *
            light_strength * attenuation;
    }
    // 최종 조명은 RGB에 곱한다. 1보다 큰 밝기도 허용해 강한 빛을 표현한다.
    return color * max(illumination, 0.0f);
}

#endif
