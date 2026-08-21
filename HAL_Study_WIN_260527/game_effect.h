#ifndef GAME_EFFECT_H
#define GAME_EFFECT_H

#include "effect.h"

#include <DirectXMath.h>

#include <array>
#include <cstddef>
#include <cstdint>

enum class GameEffectType : std::uint8_t
{
	ElectricImpact,
	WarmExplosion,
	VoidImplosion,
	EnemyDefeatSmoke,
	SmokePoof,
	Count
};

class cGameEffectManager final
{
public:
	static cGameEffectManager& GetInstance();

	void Initialize();
	void Finalize();
	void Update(float delta_time);
	void Clear();
	void Draw() const;
	bool Play(
		GameEffectType type,
		const DirectX::XMFLOAT2& position,
		float scale = 1.0f,
		const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
	void PlayEnemyDefeat(const DirectX::XMFLOAT2& position);

private:
	struct Definition
	{
		const wchar_t* TexturePath{ nullptr };
		int TextureID{ -1 };
		int FrameWidth{ 1 };
		int FrameHeight{ 1 };
		int FrameCount{ 1 };
		int FrameColumns{ 1 };
		float FrameTime{ 0.05f };
		float DrawWidth{ 1.0f };
		float DrawHeight{ 1.0f };
		DirectX::XMFLOAT2 PivotOffset{};
	};

	static constexpr std::size_t EFFECT_MAX = 256;
	static constexpr std::size_t TYPE_COUNT =
		static_cast<std::size_t>(GameEffectType::Count);

	cGameEffectManager() = default;
	~cGameEffectManager() = default;
	cGameEffectManager(const cGameEffectManager&) = delete;
	cGameEffectManager& operator=(const cGameEffectManager&) = delete;

	const Definition* GetDefinition(GameEffectType type) const;

	std::array<Definition, TYPE_COUNT> m_Definitions{};
	std::array<cEffect, EFFECT_MAX> m_Effects{};
	std::size_t m_ReplaceIndex{ 0 };
	bool m_IsInitialized{ false };
};

#endif // !GAME_EFFECT_H
