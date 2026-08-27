#ifndef SPRITE_LIGHTING_H
#define SPRITE_LIGHTING_H

// Covers every enemy and boss projectile at their gameplay maxima, with room
// for the player, effects, portals, chests, and map torches.
#define SPRITE_POINT_LIGHT_CAPACITY_VALUE 1024

#ifdef __cplusplus

#include <DirectXMath.h>

#include <array>

inline constexpr int SPRITE_POINT_LIGHT_CAPACITY =
	SPRITE_POINT_LIGHT_CAPACITY_VALUE;

struct SpritePointLight
{
	DirectX::XMFLOAT2 Position{ 0.0f, 0.0f };
	float Radius{ 1.0f };
	float Strength{ 0.0f };
	DirectX::XMFLOAT3 Color{ 1.0f, 1.0f, 1.0f };
};

struct SpriteLightConstants
{
	DirectX::XMFLOAT4 RadialLight{ 0.0f, 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT4 LightLevels{ 1.0f, 1.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT4 LightColor{ 1.0f, 1.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT4 DirectLight{ 1.0f, 1.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT4 PointLightMeta{ 0.0f, 1.0f, 0.0f, 0.0f };
	std::array<DirectX::XMFLOAT4, SPRITE_POINT_LIGHT_CAPACITY> PointLights{};
	std::array<DirectX::XMFLOAT4, SPRITE_POINT_LIGHT_CAPACITY> PointLightColors{};
};

void SpriteLighting_SetWorldLighting(
	float ambient_brightness,
	const DirectX::XMFLOAT3& direct_color,
	float direct_strength,
	const SpritePointLight* point_lights,
	int point_light_count);
void SpriteLighting_DisableWorldLighting();
void SpriteLighting_SetRadialLight(
	const DirectX::XMFLOAT2& screen_position,
	float radius,
	float ambient_brightness,
	float peak_brightness,
	float gamma,
	const DirectX::XMFLOAT3& color,
	float color_strength);
void SpriteLighting_DisableRadialLight();
SpriteLightConstants SpriteLighting_BuildConstants(
	const DirectX::XMFLOAT4X4& view_matrix,
	bool lighting_enabled,
	bool radial_light_enabled);

#else

static const int SPRITE_POINT_LIGHT_CAPACITY =
	SPRITE_POINT_LIGHT_CAPACITY_VALUE;

#endif

#endif // !SPRITE_LIGHTING_H
