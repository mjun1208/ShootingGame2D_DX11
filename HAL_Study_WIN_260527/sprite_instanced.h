#ifndef SPRITE_INSTANCED_H
#define SPRITE_INSTANCED_H

#include <DirectXMath.h>

struct SpriteInstance
{
	DirectX::XMFLOAT2 Position{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 Size{ 1.0f, 1.0f };
	float Rotation{ 0.0f };
	DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT2 TexcoordOffset{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 TexcoordScale{ 1.0f, 1.0f };
	float ColorMask{ 0.0f };
};

inline constexpr int SPRITE_INSTANCED_POINT_LIGHT_CAPACITY = 16;

bool SpriteInstanced_Initialize();
void SpriteInstanced_Finalize();
void SpriteInstanced_SetViewMatrix(const DirectX::XMMATRIX& view_matrix);
void SpriteInstanced_SetRadialLight(
	const DirectX::XMFLOAT2& screen_position,
	float radius,
	float ambient_brightness,
	float peak_brightness,
	float gamma,
	const DirectX::XMFLOAT3& color,
	float color_strength);
void SpriteInstanced_DisableRadialLight();
void SpriteInstanced_SetDirectLight(
	const DirectX::XMFLOAT3& color,
	float strength);
void SpriteInstanced_DisableDirectLight();
void SpriteInstanced_SetPointLightsWorld(
	const DirectX::XMFLOAT2* world_positions,
	int light_count,
	float radius,
	const DirectX::XMFLOAT3& color,
	float strength);
void SpriteInstanced_DisablePointLights();
void SpriteInstanced_Draw(int texture_id, const SpriteInstance* instances, int instance_count);
void SpriteInstanced_DrawAdditive(
	int texture_id,
	const SpriteInstance* instances,
	int instance_count);
void SpriteInstanced_DrawLightning(
	int texture_id,
	const SpriteInstance* instances,
	int instance_count);

#endif // SPRITE_INSTANCED_H
