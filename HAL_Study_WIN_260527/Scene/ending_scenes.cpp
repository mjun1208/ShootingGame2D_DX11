#include "ending_scene.h"
#include "config.h"
#include "bitmap_text.h"
#include "direct3d.h"
#include "scene_manager.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	namespace EndingTuning
	{
		constexpr int RestartButtonIndex = 0;
		constexpr char InputHint[] = "UP/DOWN  SELECT    ENTER  CONFIRM";
		constexpr float FireworkLifetime = 2.4f;
		constexpr size_t BurstsPerVolley = 3;
		constexpr float VolleyInterval = 0.4f;
		constexpr float BurstInterval = 0.035f;
		constexpr int SparkCount = 48;
	} // namespace EndingTuning

	std::string FormatClearTime(float clear_time_seconds)
	{
		const int total_centiseconds = static_cast<int>(std::round(std::max(clear_time_seconds, 0.0f) * 100.0f));
		const int centiseconds = total_centiseconds % 100;
		const int total_seconds = total_centiseconds / 100;
		const int seconds = total_seconds % 60;
		const int total_minutes = total_seconds / 60;
		const int minutes = total_minutes % 60;
		const int hours = total_minutes / 60;

		std::ostringstream stream;
		stream << "CLEAR TIME  ";
		if (hours > 0)
		{
			stream << std::setfill('0') << std::setw(2) << hours << ':';
		}
		stream << std::setfill('0') << std::setw(2) << minutes << ':' << std::setw(2) << seconds << '.' << std::setw(2)
		       << centiseconds;
		return stream.str();
	}

} // namespace

EndingScene::EndingScene(EndingResult result, float clear_time_seconds)
    : m_Result(result), m_ClearTimeSeconds(std::max(clear_time_seconds, 0.0f))
{
}

EndingScene::~EndingScene() = default;

bool EndingScene::Initialize()
{
	const char* header_label = m_Result == EndingResult::Clear ? "RUN CLEAR" : "GAME OVER";

	m_HeaderText = hal::CreateCenteredText(header_label, SCREEN_WIDTH * 0.5f, 190.0f, 26.0f, 32.0f);
	if (m_Result == EndingResult::Clear)
	{
		// 엔딩 씬 초기화 전에 IngameScene이 이 렌더러를 해제한다.
		// 클리어 씬에서도 렌더러를 소유하며, 이펙트 초기화에 실패해도 메뉴는 사용할 수 있게 한다.
		m_FireworkRendererInitialized = SpriteInstanced_Initialize();
		if (m_FireworkRendererInitialized)
		{
			m_FireworkTextureID = Texture_Load(L"asset/texture/white_square.png", false);
		}
		for (size_t i = 0; i < m_Fireworks.size(); ++i)
		{
			// 한 묶음마다 짧은 간격으로 세 번 터뜨리고 여러 묶음을 겹친다.
			const float delay = static_cast<float>(i / EndingTuning::BurstsPerVolley) * EndingTuning::VolleyInterval +
			                    static_cast<float>(i % EndingTuning::BurstsPerVolley) * EndingTuning::BurstInterval;
			m_Fireworks[i] = { 0.06f - delay, static_cast<unsigned int>(i) };
		}
		const std::string clear_time = FormatClearTime(m_ClearTimeSeconds);
		m_TimeText = hal::CreateCenteredText(clear_time.c_str(), SCREEN_WIDTH * 0.5f, 320.0f, 22.0f, 32.0f);
	}
	m_HintText = hal::CreateCenteredText(EndingTuning::InputHint, SCREEN_WIDTH * 0.5f, 690.0f, 20.0f, 32.0f);

	m_RestartButton.Initialize(m_Result == EndingResult::Clear ? "NEW RUN" : "RETRY", { SCREEN_WIDTH * 0.5f, 500.0f },
	                           { 345.0f, 90.0f });
	m_TitleButton.Initialize("TITLE", { SCREEN_WIDTH * 0.5f, 620.0f }, { 345.0f, 90.0f });

	m_Menu.Initialize({ &m_RestartButton, &m_TitleButton }, EndingTuning::RestartButtonIndex,
	                  ButtonMenuNavigation::Vertical, ButtonMenuBoundary::Clamp);
	m_IsTransitioning = false;
	return true;
}

void EndingScene::Finalize()
{
	Texture_Release(m_FireworkTextureID);
	m_FireworkTextureID = TEXTURE_INVALID_ID;
	if (m_FireworkRendererInitialized)
	{
		SpriteInstanced_Finalize();
		m_FireworkRendererInitialized = false;
	}
	m_Menu.Clear();
	m_TitleButton.Finalize();
	m_RestartButton.Finalize();
	m_HintText.reset();
	m_TimeText.reset();
	m_HeaderText.reset();
}

void EndingScene::Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	if (m_Result == EndingResult::Clear)
	{
		for (auto& burst : m_Fireworks)
		{
			burst.Age += std::clamp(delta_time, 0.0f, 0.1f);
			if (burst.Age >= EndingTuning::FireworkLifetime)
			{
				burst.Age -= EndingTuning::FireworkLifetime;
				burst.Sequence += static_cast<unsigned int>(m_Fireworks.size());
			}
		}
	}
	if (m_IsTransitioning)
	{
		return;
	}

	if (m_Menu.Update() != ButtonMenuController::NO_ACTIVATION)
	{
		ActivateSelectedButton();
	}
}

void EndingScene::Draw()
{
	auto* context = Direct3D_GetDeviceContext();
	ID3D11RenderTargetView* render_target = nullptr;
	context->OMGetRenderTargets(1, &render_target, nullptr);
	if (render_target)
	{
		constexpr float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		context->ClearRenderTargetView(render_target, black);
		render_target->Release();
	}

	const char* header_label = m_Result == EndingResult::Clear ? "RUN CLEAR" : "GAME OVER";
	Sprite_ResetViewMatrix();
	Sprite_SetFilter(kPOINT);
	// 배경을 지운 뒤 축하 이펙트를 먼저 그려 글자와 버튼 뒤에 배치한다.
	DrawFireworks();

	if (m_HeaderText)
	{
		m_HeaderText->Clear();
		m_HeaderText->SetText(header_label, { 1.0f, 0.50f, 0.18f, 1.0f });
		m_HeaderText->Draw();
	}
	if (m_TimeText)
	{
		const std::string clear_time = FormatClearTime(m_ClearTimeSeconds);
		m_TimeText->Clear();
		m_TimeText->SetText(clear_time.c_str(), { 0.95f, 0.90f, 0.76f, 1.0f });
		m_TimeText->Draw();
	}

	m_RestartButton.Draw();
	m_TitleButton.Draw();

	if (m_HintText)
	{
		m_HintText->Clear();
		m_HintText->SetText(EndingTuning::InputHint, { 0.55f, 0.53f, 0.58f, 1.0f });
		m_HintText->Draw();
	}
}

void EndingScene::DrawFireworks()
{
	if (m_Result != EndingResult::Clear || !m_FireworkRendererInitialized || m_FireworkTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	constexpr DirectX::XMFLOAT4 colors[] = {
		{ 1.0f, 0.62f, 0.16f, 1.0f }, { 0.25f, 0.78f, 1.0f, 1.0f }, { 1.0f, 0.30f, 0.52f, 1.0f },
		{ 0.50f, 1.0f, 0.48f, 1.0f }, { 0.76f, 0.42f, 1.0f, 1.0f }, { 1.0f, 0.88f, 0.48f, 1.0f },
	};
	// 최대 개수를 제한해 축하 이펙트 전체를 한 번의 인스턴스 드로우로 그린다.
	std::vector<SpriteInstance> sparks;
	sparks.reserve(m_Fireworks.size() * EndingTuning::SparkCount * 4);
	for (const auto& burst : m_Fireworks)
	{
		if (burst.Age < 0.0f)
		{
			continue;
		}
		const unsigned int sequence = burst.Sequence;
		const float origin_x = SCREEN_WIDTH * (0.14f + 0.072f * static_cast<float>((sequence * 7u) % 11u));
		const float origin_y = SCREEN_HEIGHT * (0.14f + 0.055f * static_cast<float>((sequence * 3u) % 7u));
		const auto& color = colors[sequence % 6u];
		for (int i = 0; i < EndingTuning::SparkCount; ++i)
		{
			const float angle = DirectX::XM_2PI * static_cast<float>(i) / EndingTuning::SparkCount +
			                    static_cast<float>(sequence % 17u) * 0.13f;
			const float speed = 95.0f + static_cast<float>((i * 37 + sequence % 31u) % 110u);
			const float lifetime = 1.45f + static_cast<float>(i % 7) * 0.12f;
			for (int trail = 3; trail >= 0; --trail)
			{
				const float age = burst.Age - static_cast<float>(trail) * 0.045f;
				if (age < 0.0f || burst.Age >= lifetime)
				{
					continue;
				}
				const float fade = 1.0f - burst.Age / lifetime;
				const float travel = speed * (1.0f - std::exp(-1.35f * age)) / 1.35f;
				SpriteInstance spark{};
				spark.Position = { origin_x + std::cos(angle) * travel,
					               origin_y + std::sin(angle) * travel + 44.0f * age * age };
				const float size = (trail == 0 ? 3.5f : 2.5f) * (0.45f + 0.55f * fade);
				spark.Size = { size, size };
				spark.Rotation = angle;
				spark.Color = color;
				spark.Color.w = fade * fade * (trail == 0 ? 1.0f : 0.32f);
				sparks.push_back(spark);
			}
		}
	}
	SpriteInstanced_SetViewMatrix(DirectX::XMMatrixIdentity());
	SpriteInstanced_DrawAdditiveUnlit(m_FireworkTextureID, sparks.data(), static_cast<int>(sparks.size()));
}

void EndingScene::ActivateSelectedButton()
{
	m_IsTransitioning = true;
	SceneManager_ChangeScene(m_Menu.GetSelectedIndex() == EndingTuning::RestartButtonIndex ? SceneID::Ingame
	                                                                                       : SceneID::Title);
}
