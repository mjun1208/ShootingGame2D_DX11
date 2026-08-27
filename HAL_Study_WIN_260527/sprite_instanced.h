#ifndef SPRITE_INSTANCED_H
#define SPRITE_INSTANCED_H

#include "sprite_lighting.h"

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

bool SpriteInstanced_Initialize();
void SpriteInstanced_Finalize();
void SpriteInstanced_SetViewMatrix(const DirectX::XMMATRIX& view_matrix);
void SpriteInstanced_Draw(int texture_id, const SpriteInstance* instances, int instance_count);
void SpriteInstanced_DrawUnlit(
	int texture_id,
	const SpriteInstance* instances,
	int instance_count);
void SpriteInstanced_DrawOutlinedUnlit(
	int texture_id,
	const SpriteInstance* instances,
	int instance_count,
	const DirectX::XMFLOAT4& outline_color,
	float outline_thickness);
void SpriteInstanced_DrawWrapUUnlit(
	int texture_id,
	const SpriteInstance* instances,
	int instance_count);
void SpriteInstanced_DrawAdditive(
	int texture_id,
	const SpriteInstance* instances,
	int instance_count);
void SpriteInstanced_DrawAdditiveUnlit(
	int texture_id,
	const SpriteInstance* instances,
	int instance_count);
void SpriteInstanced_DrawLightning(
	int texture_id,
	const SpriteInstance* instances,
	int instance_count);

#endif // SPRITE_INSTANCED_H
