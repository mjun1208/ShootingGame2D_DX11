#include "math_utils.h"

#include <algorithm>
#include <cmath>

float Saturate(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

float PositiveOr(float value, float fallback)
{
	return value > 0.0f ? value : fallback;
}

float SmoothStep(float value)
{
	value = Saturate(value);
	return value * value * (3.0f - 2.0f * value);
}

float EaseOutCubic(float value)
{
	const float remaining = 1.0f - Saturate(value);
	return 1.0f - remaining * remaining * remaining;
}

float LengthSquared(const DirectX::XMFLOAT2& vector)
{
	return vector.x * vector.x + vector.y * vector.y;
}

float DistanceSquared(const DirectX::XMFLOAT2& first, const DirectX::XMFLOAT2& second)
{
	return LengthSquared({ second.x - first.x, second.y - first.y });
}

float Length(const DirectX::XMFLOAT2& vector)
{
	return std::sqrt(LengthSquared(vector));
}

float Distance(const DirectX::XMFLOAT2& first, const DirectX::XMFLOAT2& second)
{
	return std::sqrt(DistanceSquared(first, second));
}

DirectX::XMFLOAT2 NormalizeOr(const DirectX::XMFLOAT2& vector, const DirectX::XMFLOAT2& fallback,
                              float minimum_length_squared)
{
	const float length_squared = LengthSquared(vector);
	if (length_squared <= minimum_length_squared)
	{
		return fallback;
	}

	const float inverse_length = 1.0f / std::sqrt(length_squared);
	return { vector.x * inverse_length, vector.y * inverse_length };
}

DirectX::XMFLOAT2 Lerp(const DirectX::XMFLOAT2& from, const DirectX::XMFLOAT2& to, float amount)
{
	return { std::lerp(from.x, to.x, amount), std::lerp(from.y, to.y, amount) };
}

DirectX::XMFLOAT2 GetDirection(const DirectX::XMFLOAT2& origin, const DirectX::XMFLOAT2& target)
{
	return NormalizeOr({ target.x - origin.x, target.y - origin.y }, { 1.0f, 0.0f });
}

DirectX::XMFLOAT2 RotateDirection(const DirectX::XMFLOAT2& direction, float angle)
{
	const float sine = std::sin(angle);
	const float cosine = std::cos(angle);
	return {
		direction.x * cosine - direction.y * sine,
		direction.x * sine + direction.y * cosine,
	};
}

int WorldToGridCell(float value, float cell_size)
{
	return static_cast<int>(std::floor(value / cell_size));
}

int SpatialHashIndex(int cell_x, int cell_y, int bucket_count, std::uint32_t x_multiplier, std::uint32_t y_multiplier)
{
	const std::uint32_t hash =
	    static_cast<std::uint32_t>(cell_x) * x_multiplier ^ static_cast<std::uint32_t>(cell_y) * y_multiplier;
	return static_cast<int>(hash % static_cast<std::uint32_t>(bucket_count));
}
