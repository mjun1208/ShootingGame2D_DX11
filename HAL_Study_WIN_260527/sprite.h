#ifndef SPRITE_H
#define SPRITE_H

#include <d3d11.h>
#include <DirectXMath.h>

bool Sprite_Initialize();
void Sprite_Finalize();

enum SpriteFilter
{
	kPOINT,
	kLINEAR,
};
void Sprite_SetFilter(SpriteFilter filter);
void Sprite_SetViewMatrix(const DirectX::XMMATRIX& view_matrix);
void Sprite_ResetViewMatrix();

void Sprite_Draw(int texture_Id, float pos_X, float pos_Y);
void Sprite_Draw(int texture_Id, const DirectX::XMFLOAT2& pos);
void Sprite_Draw(int texture_Id, float pos_X, float pos_Y, float width, float height);
void Sprite_Draw(
	int texture_Id,
	float pos_X,
	float pos_Y,
	float width,
	float height,
	const DirectX::XMFLOAT4& color);
void Sprite_Draw(
	int texture_Id,
	float pos_X,
	float pos_Y,
	float width,
	float height,
	float rotation_radian,
	const DirectX::XMFLOAT4& color);
void Sprite_DrawDissolve(
	int texture_Id,
	int noise_texture_Id,
	float pos_X,
	float pos_Y,
	float width,
	float height,
	float dissolve_amount,
	float edge_width,
	const DirectX::XMFLOAT4& edge_color);
void Sprite_Draw(
	int texture_Id,
	float pos_X,
	float pos_Y,
	float width,
	float height,
	int texture_X,
	int texture_Y,
	int texture_Width,
	int texture_Height,
	const DirectX::XMFLOAT4& color);
void Sprite_Draw(int texture_Id, float pos_X, float pos_Y, int texture_X, int texture_Y, int texture_Width, int texture_Height);

#endif // !SPRITE_H
