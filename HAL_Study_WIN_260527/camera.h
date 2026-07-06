#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>

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

	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;
	DirectX::XMMATRIX GetViewProjectionMatrix() const;

	DirectX::XMFLOAT2 WorldToScreen(const DirectX::XMFLOAT2& world_position) const;
	DirectX::XMFLOAT2 ScreenToWorld(const DirectX::XMFLOAT2& screen_position) const;

private:
	DirectX::XMFLOAT2 m_Position{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 m_ScreenSize{ 0.0f, 0.0f };
	float m_Zoom{ 1.0f };
	float m_Rotation{ 0.0f };
};

#endif // !CAMERA_H
