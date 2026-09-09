#ifndef PANEL_DRAW_H
#define PANEL_DRAW_H

#include "sprite.h"
#include "texture.h"

struct NineSlicePanelStyle
{
	int TextureSize;
	int SourceBorder;
	float DrawBorder;
};

inline void DrawNineSlicePanel(int texture_id, const DirectX::XMFLOAT2& center, const DirectX::XMFLOAT2& size,
                               const NineSlicePanelStyle& style, const DirectX::XMFLOAT4& color)
{
	if (texture_id == TEXTURE_INVALID_ID)
	{
		return;
	}
	const float widths[] = { style.DrawBorder, size.x - style.DrawBorder * 2.0f, style.DrawBorder };
	const float heights[] = { style.DrawBorder, size.y - style.DrawBorder * 2.0f, style.DrawBorder };
	const int positions[] = { 0, style.SourceBorder, style.TextureSize - style.SourceBorder };
	const int sizes[] = { style.SourceBorder, style.TextureSize - style.SourceBorder * 2, style.SourceBorder };
	float y = center.y - size.y * 0.5f;
	for (int row = 0; row < 3; ++row)
	{
		float x = center.x - size.x * 0.5f;
		for (int column = 0; column < 3; ++column)
		{
			Sprite_DrawRegion(texture_id, { x + widths[column] * 0.5f, y + heights[row] * 0.5f },
			                  { widths[column], heights[row] },
			                  { positions[column], positions[row], sizes[column], sizes[row] }, color);
			x += widths[column];
		}
		y += heights[row];
	}
}

#endif // PANEL_DRAW_H
