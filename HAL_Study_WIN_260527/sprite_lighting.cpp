#include "sprite_lighting.h"

#include <algorithm>

using namespace DirectX;

namespace
{
	struct SpriteLightingState
	{
		XMFLOAT4 RadialLight{ 0.0f, 0.0f, 1.0f, 0.0f };
		XMFLOAT4 LightLevels{ 1.0f, 1.0f, 1.0f, 0.0f };
		XMFLOAT4 LightColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		XMFLOAT4 DirectLight{ 1.0f, 1.0f, 1.0f, 0.0f };
		float AmbientBrightness{ 1.0f };
		std::array<SpritePointLight, SPRITE_POINT_LIGHT_CAPACITY> PointLights{};
		int PointLightCount{ 0 };
	};

	SpriteLightingState g_State{};
}

void SpriteLighting_SetWorldLighting(
	float ambient_brightness,
	const XMFLOAT3& direct_color,
	float direct_strength,
	const SpritePointLight* point_lights,
	int point_light_count)
{
	g_State.AmbientBrightness = std::max(ambient_brightness, 0.0f);
	g_State.DirectLight = {
		std::max(direct_color.x, 0.0f),
		std::max(direct_color.y, 0.0f),
		std::max(direct_color.z, 0.0f),
		std::max(direct_strength, 0.0f),
	};
	g_State.PointLightCount = point_lights ?
		std::clamp(point_light_count, 0, SPRITE_POINT_LIGHT_CAPACITY) : 0;
	if (g_State.PointLightCount > 0)
	{
		std::copy_n(
			point_lights,
			g_State.PointLightCount,
			g_State.PointLights.begin());
	}
}

void SpriteLighting_DisableWorldLighting()
{
	g_State.AmbientBrightness = 1.0f;
	g_State.DirectLight.w = 0.0f;
	g_State.PointLightCount = 0;
}

void SpriteLighting_SetRadialLight(
	const XMFLOAT2& screen_position,
	float radius,
	float ambient_brightness,
	float peak_brightness,
	float gamma,
	const XMFLOAT3& color,
	float color_strength)
{
	g_State.RadialLight = {
		screen_position.x,
		screen_position.y,
		std::max(radius, 1.0f),
		1.0f,
	};
	g_State.LightLevels = {
		std::max(ambient_brightness, 0.0f),
		std::max(peak_brightness, 0.0f),
		std::max(gamma, 0.01f),
		0.0f,
	};
	g_State.LightColor = {
		std::max(color.x, 0.0f),
		std::max(color.y, 0.0f),
		std::max(color.z, 0.0f),
		std::max(color_strength, 0.0f),
	};
}

void SpriteLighting_DisableRadialLight()
{
	g_State.RadialLight.w = 0.0f;
}

SpriteLightConstants SpriteLighting_BuildConstants(
	const XMFLOAT4X4& view_matrix,
	bool lighting_enabled,
	bool radial_light_enabled)
{
	SpriteLightConstants constants{};
	constants.RadialLight = g_State.RadialLight;
	constants.LightLevels = g_State.LightLevels;
	constants.LightColor = g_State.LightColor;
	constants.DirectLight = g_State.DirectLight;
	constants.PointLightMeta.y = g_State.AmbientBrightness;

	if (!radial_light_enabled)
	{
		constants.RadialLight.w = 0.0f;
	}
	if (!lighting_enabled)
	{
		constants.RadialLight.w = 0.0f;
		constants.DirectLight.w = 0.0f;
		constants.PointLightMeta.x = 0.0f;
		constants.PointLightMeta.y = 1.0f;
		return constants;
	}

	const XMMATRIX view = XMLoadFloat4x4(&view_matrix);
	const float view_scale = XMVectorGetX(XMVector3Length(
		XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), view)));
	for (int i = 0; i < g_State.PointLightCount; ++i)
	{
		const SpritePointLight& light = g_State.PointLights[i];
		const XMVECTOR world_position = XMVectorSet(
			light.Position.x, light.Position.y, 0.0f, 1.0f);
		const XMVECTOR screen_position = XMVector3TransformCoord(
			world_position, view);
		XMFLOAT2 screen{};
		XMStoreFloat2(&screen, screen_position);
		constants.PointLights[i] = {
			screen.x,
			screen.y,
			std::max(light.Radius * view_scale, 1.0f),
			std::max(light.Strength, 0.0f),
		};
		constants.PointLightColors[i] = {
			std::max(light.Color.x, 0.0f),
			std::max(light.Color.y, 0.0f),
			std::max(light.Color.z, 0.0f),
			0.0f,
		};
	}
	constants.PointLightMeta.x = static_cast<float>(g_State.PointLightCount);
	return constants;
}
