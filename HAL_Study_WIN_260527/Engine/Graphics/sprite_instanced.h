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

struct CutSpriteInstance
{
	DirectX::XMFLOAT2 Position{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 Size{ 1.0f, 1.0f };
	float Rotation{ 0.0f };
	DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT2 TexcoordOffset{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 TexcoordScale{ 1.0f, 1.0f };
	DirectX::XMFLOAT2 CutNormal{ 0.0f, 1.0f };
	// x: 절단면 방향(-1 또는 +1), y: 소멸 진행률,
	// z: 절단선에서 가장 먼 픽셀까지의 거리, w: 고정 노이즈 시드.
	DirectX::XMFLOAT4 CutParameters{ 1.0f, 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT4 EdgeColor{ 0.58f, 0.92f, 1.0f, 0.95f };
};

bool SpriteInstanced_Initialize();
void SpriteInstanced_Finalize();
void SpriteInstanced_SetViewMatrix(const DirectX::XMMATRIX& view_matrix);
void SpriteInstanced_Draw(int texture_id, const SpriteInstance* instances, int instance_count);
void SpriteInstanced_DrawUnlit(int texture_id, const SpriteInstance* instances, int instance_count);
void SpriteInstanced_DrawOutlinedUnlit(int texture_id, const SpriteInstance* instances, int instance_count,
                                       const DirectX::XMFLOAT4& outline_color, float outline_thickness);
void SpriteInstanced_DrawWrapUUnlit(int texture_id, const SpriteInstance* instances, int instance_count);
void SpriteInstanced_DrawAdditive(int texture_id, const SpriteInstance* instances, int instance_count);
void SpriteInstanced_DrawAdditiveUnlit(int texture_id, const SpriteInstance* instances, int instance_count);
void SpriteInstanced_DrawLightning(int texture_id, const SpriteInstance* instances, int instance_count);
void SpriteInstanced_DrawCut(int texture_id, const CutSpriteInstance* instances, int instance_count);

#endif // SPRITE_INSTANCED_H
