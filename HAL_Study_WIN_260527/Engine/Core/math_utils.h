#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <DirectXMath.h>
#include <cstdint>

constexpr bool IsValidIndex(int index, int count)
{
	return index >= 0 && index < count;
}

float Saturate(float value);
float PositiveOr(float value, float fallback);
float SmoothStep(float value);
float EaseOutCubic(float value);
float LengthSquared(const DirectX::XMFLOAT2& vector);
float DistanceSquared(const DirectX::XMFLOAT2& first, const DirectX::XMFLOAT2& second);
float Length(const DirectX::XMFLOAT2& vector);
float Distance(const DirectX::XMFLOAT2& first, const DirectX::XMFLOAT2& second);
DirectX::XMFLOAT2 NormalizeOr(const DirectX::XMFLOAT2& vector, const DirectX::XMFLOAT2& fallback,
                              float minimum_length_squared = 0.0001f);
DirectX::XMFLOAT2 Lerp(const DirectX::XMFLOAT2& from, const DirectX::XMFLOAT2& to, float amount);
DirectX::XMFLOAT2 GetDirection(const DirectX::XMFLOAT2& origin, const DirectX::XMFLOAT2& target);
DirectX::XMFLOAT2 RotateDirection(const DirectX::XMFLOAT2& direction, float angle);
int WorldToGridCell(float value, float cell_size);
int SpatialHashIndex(int cell_x, int cell_y, int bucket_count, std::uint32_t x_multiplier = 0x8da6b343u,
                     std::uint32_t y_multiplier = 0xd8163841u);

#endif // MATH_UTILS_H
