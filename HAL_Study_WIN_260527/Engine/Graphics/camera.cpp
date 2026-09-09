#include "camera.h"

#include "config.h"
#include "math_utils.h"
#include "random_utils.h"

#include <algorithm>

using namespace DirectX;

cCamera::cCamera() : cCamera(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT))
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
	static constexpr float CAMERA_MIN_ZOOM = 0.0001f;

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

void cCamera::Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	static constexpr float CAMERA_SHAKE_SAMPLE_INTERVAL = 1.0f / 30.0f;

	if (m_ZoomPunchRemaining > 0.0f)
	{
		m_ZoomPunchRemaining = std::max(m_ZoomPunchRemaining - delta_time, 0.0f);
		if (m_ZoomPunchRemaining <= 0.0f)
		{
			StopZoomPunch();
		}
	}

	if (m_ShakeRemaining <= 0.0f)
	{
		return;
	}

	m_ShakeRemaining = std::max(m_ShakeRemaining - delta_time, 0.0f);
	if (m_ShakeRemaining <= 0.0f)
	{
		StopShake();
		return;
	}

	m_ShakeSampleElapsed += delta_time;
	while (m_ShakeSampleElapsed >= CAMERA_SHAKE_SAMPLE_INTERVAL)
	{
		m_ShakeSampleElapsed -= CAMERA_SHAKE_SAMPLE_INTERVAL;
		m_ShakeNoiseFrom = m_ShakeNoiseTo;
		m_ShakeNoiseTo = NextShakeNoise2D();
	}

	const float sample_ratio = Saturate(m_ShakeSampleElapsed / CAMERA_SHAKE_SAMPLE_INTERVAL);
	const float smooth_ratio = SmoothStep(sample_ratio);
	const float strength = m_ShakeNoiseSize * (m_ShakeRemaining / m_ShakeDuration);
	m_ShakeOffset = {
		(m_ShakeNoiseFrom.x + (m_ShakeNoiseTo.x - m_ShakeNoiseFrom.x) * smooth_ratio) * strength,
		(m_ShakeNoiseFrom.y + (m_ShakeNoiseTo.y - m_ShakeNoiseFrom.y) * smooth_ratio) * strength,
	};
}

void cCamera::Shake(float noise_size, float duration)
{
	noise_size = std::max(noise_size, 0.0f);
	duration = std::max(duration, 0.0f);
	if (noise_size <= 0.0f || duration <= 0.0f)
	{
		return;
	}

	const bool was_shaking = m_ShakeRemaining > 0.0f;
	// 이미 진행 중인 강한 흔들림을 유지하면서 연속 충격으로 지속 시간을 늘린다.
	// 이때 흔들림 강도가 갑자기 낮아지지 않도록 한다.
	const float current_strength =
	    m_ShakeDuration > 0.0f ? m_ShakeNoiseSize * (m_ShakeRemaining / m_ShakeDuration) : 0.0f;
	m_ShakeNoiseSize = std::max(current_strength, noise_size);
	m_ShakeDuration = std::max(m_ShakeRemaining, duration);
	m_ShakeRemaining = m_ShakeDuration;

	if (!was_shaking)
	{
		m_ShakeSampleElapsed = 0.0f;
		m_ShakeNoiseFrom = NextShakeNoise2D();
		m_ShakeNoiseTo = NextShakeNoise2D();
		m_ShakeOffset = {
			m_ShakeNoiseFrom.x * m_ShakeNoiseSize,
			m_ShakeNoiseFrom.y * m_ShakeNoiseSize,
		};
	}
	else if (current_strength > 0.0f)
	{
		const float strength_ratio = m_ShakeNoiseSize / current_strength;
		m_ShakeOffset.x *= strength_ratio;
		m_ShakeOffset.y *= strength_ratio;
	}
}

void cCamera::StopShake()
{
	m_ShakeOffset = { 0.0f, 0.0f };
	m_ShakeNoiseFrom = { 0.0f, 0.0f };
	m_ShakeNoiseTo = { 0.0f, 0.0f };
	m_ShakeNoiseSize = 0.0f;
	m_ShakeDuration = 0.0f;
	m_ShakeRemaining = 0.0f;
	m_ShakeSampleElapsed = 0.0f;
}

void cCamera::PunchZoom(float amount, float duration)
{
	amount = std::max(amount, 0.0f);
	duration = std::max(duration, 0.0f);
	if (amount <= 0.0f || duration <= 0.0f)
	{
		return;
	}

	const float current_amount =
	    m_ZoomPunchDuration > 0.0f ? m_ZoomPunchAmount * (m_ZoomPunchRemaining / m_ZoomPunchDuration) : 0.0f;
	m_ZoomPunchAmount = std::max(current_amount, amount);
	m_ZoomPunchDuration = std::max(m_ZoomPunchRemaining, duration);
	m_ZoomPunchRemaining = m_ZoomPunchDuration;
}

void cCamera::StopZoomPunch()
{
	m_ZoomPunchAmount = 0.0f;
	m_ZoomPunchDuration = 0.0f;
	m_ZoomPunchRemaining = 0.0f;
}

XMFLOAT2 cCamera::NextShakeNoise2D()
{
	return { RandomSigned(), RandomSigned() };
}

XMMATRIX cCamera::GetViewMatrix() const
{
	const float zoom_punch =
	    m_ZoomPunchDuration > 0.0f ? m_ZoomPunchAmount * (m_ZoomPunchRemaining / m_ZoomPunchDuration) : 0.0f;
	const XMMATRIX translation =
	    XMMatrixTranslation(-(m_Position.x + m_ShakeOffset.x), -(m_Position.y + m_ShakeOffset.y), 0.0f);
	const XMMATRIX rotation = XMMatrixRotationZ(-m_Rotation);
	const float presented_zoom = m_Zoom * (1.0f + zoom_punch);
	const XMMATRIX scaling = XMMatrixScaling(presented_zoom, presented_zoom, 1.0f);
	const XMMATRIX screen_center = XMMatrixTranslation(m_ScreenSize.x / 2.0f, m_ScreenSize.y / 2.0f, 0.0f);

	return translation * rotation * scaling * screen_center;
}

XMMATRIX cCamera::GetProjectionMatrix() const
{
	return XMMatrixOrthographicOffCenterLH(0.0f, m_ScreenSize.x, m_ScreenSize.y, 0.0f, 0.0f, 1.0f);
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
