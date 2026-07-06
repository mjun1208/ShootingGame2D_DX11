#include "camera.h"

#include "config.h"

#include <algorithm>

using namespace DirectX;

static constexpr float CAMERA_MIN_ZOOM = 0.0001f;

cCamera::cCamera()
	: cCamera(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT))
{
}

cCamera::cCamera(float screen_width, float screen_height)
{
	SetScreenSize(screen_width, screen_height);
}

void cCamera::SetScreenSize(float screen_width, float screen_height)
{
	m_ScreenSize.x = std::max(screen_width, 1.0f);
	m_ScreenSize.y = std::max(screen_height, 1.0f);
}

XMFLOAT2 cCamera::GetScreenSize() const
{
	return m_ScreenSize;
}

void cCamera::SetPosition(const XMFLOAT2& position)
{
	m_Position = position;
}

void cCamera::SetPosition(float x, float y)
{
	SetPosition({ x, y });
}

XMFLOAT2 cCamera::GetPosition() const
{
	return m_Position;
}

void cCamera::Move(const XMFLOAT2& offset)
{
	m_Position.x += offset.x;
	m_Position.y += offset.y;
}

void cCamera::Move(float x, float y)
{
	Move({ x, y });
}

void cCamera::SetZoom(float zoom)
{
	m_Zoom = std::max(zoom, CAMERA_MIN_ZOOM);
}

float cCamera::GetZoom() const
{
	return m_Zoom;
}

void cCamera::SetRotation(float radians)
{
	m_Rotation = radians;
}

float cCamera::GetRotation() const
{
	return m_Rotation;
}

XMMATRIX cCamera::GetViewMatrix() const
{
	const XMMATRIX translation = XMMatrixTranslation(-m_Position.x, -m_Position.y, 0.0f);
	const XMMATRIX rotation = XMMatrixRotationZ(-m_Rotation);
	const XMMATRIX scaling = XMMatrixScaling(m_Zoom, m_Zoom, 1.0f);
	const XMMATRIX screen_center = XMMatrixTranslation(m_ScreenSize.x / 2.0f, m_ScreenSize.y / 2.0f, 0.0f);

	return translation * rotation * scaling * screen_center;
}

XMMATRIX cCamera::GetProjectionMatrix() const
{
	return XMMatrixOrthographicOffCenterLH(
		0.0f,
		m_ScreenSize.x,
		m_ScreenSize.y,
		0.0f,
		0.0f,
		1.0f);
}

XMMATRIX cCamera::GetViewProjectionMatrix() const
{
	return GetViewMatrix() * GetProjectionMatrix();
}

XMFLOAT2 cCamera::WorldToScreen(const XMFLOAT2& world_position) const
{
	const XMVECTOR position = XMVectorSet(world_position.x, world_position.y, 0.0f, 1.0f);
	const XMVECTOR screen_position = XMVector3TransformCoord(position, GetViewMatrix());

	XMFLOAT2 result{};
	XMStoreFloat2(&result, screen_position);
	return result;
}

XMFLOAT2 cCamera::ScreenToWorld(const XMFLOAT2& screen_position) const
{
	const XMVECTOR position = XMVectorSet(screen_position.x, screen_position.y, 0.0f, 1.0f);
	const XMMATRIX inverse_view = XMMatrixInverse(nullptr, GetViewMatrix());
	const XMVECTOR world_position = XMVector3TransformCoord(position, inverse_view);

	XMFLOAT2 result{};
	XMStoreFloat2(&result, world_position);
	return result;
}
