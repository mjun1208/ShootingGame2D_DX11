#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>

#include <cstdint>

class cCamera
{
public:
	cCamera();
	cCamera(float screen_width, float screen_height);

	void SetScreenSize(float screen_width, float screen_height);
	DirectX::XMFLOAT2 GetScreenSize() const;

	void SetPosition(const DirectX::XMFLOAT2& position);
	void SetPosition(float x, float y);
	DirectX::XMFLOAT2 GetPosition() const;

	void Move(const DirectX::XMFLOAT2& offset);
	void Move(float x, float y);

	void SetZoom(float zoom);
	float GetZoom() const;

	void SetRotation(float radians);
	float GetRotation() const;

	void Update(float delta_time);
	void Shake(float noise_size, float duration);
	void StopShake();

	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;
	DirectX::XMMATRIX GetViewProjectionMatrix() const;

	DirectX::XMFLOAT2 WorldToScreen(const DirectX::XMFLOAT2& world_position) const;
	DirectX::XMFLOAT2 ScreenToWorld(const DirectX::XMFLOAT2& screen_position) const;

private:
	float NextShakeNoise();
	DirectX::XMFLOAT2 NextShakeNoise2D();

	DirectX::XMFLOAT2 m_Position{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 m_ScreenSize{ 0.0f, 0.0f };
	float m_Zoom{ 1.0f };
	float m_Rotation{ 0.0f };
	DirectX::XMFLOAT2 m_ShakeOffset{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 m_ShakeNoiseFrom{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 m_ShakeNoiseTo{ 0.0f, 0.0f };
	float m_ShakeNoiseSize{ 0.0f };
	float m_ShakeDuration{ 0.0f };
	float m_ShakeRemaining{ 0.0f };
	float m_ShakeSampleElapsed{ 0.0f };
	std::uint32_t m_ShakeRandomState{ 0xA341316Cu };
};

#endif // !CAMERA_H
