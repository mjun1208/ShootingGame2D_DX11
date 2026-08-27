#ifndef CHAIN_LIGHTNING_H
#define CHAIN_LIGHTNING_H

#include "sprite_lighting.h"

#include <DirectXMath.h>

#include <array>
#include <cstdint>

class cChainLightning
{
public:
	void Initialize();
	void Finalize();
	void Clear();

	bool TryTrigger(
		int first_enemy_id,
		const DirectX::XMFLOAT2& first_hit_position,
		float attack_damage);
	void Update(float delta_time);
	void Draw() const;
	int AppendPointLights(
		SpritePointLight* lights,
		int light_count,
		int capacity) const;

private:
	struct Arc
	{
		DirectX::XMFLOAT2 Start{};
		DirectX::XMFLOAT2 End{};
		float Age{ 0.0f };
		float FrameTimer{ 0.0f };
		float FrameInterval{ 0.04f };
		float FrameBrightness{ 1.0f };
		int FrameIndex{ 0 };
		bool IsActive{ false };
	};

	static constexpr int ARC_MAX = 48;

	float NextRandom01();
	void SpawnArc(
		const DirectX::XMFLOAT2& start,
		const DirectX::XMFLOAT2& end);

	std::array<Arc, ARC_MAX> m_Arcs{};
	std::array<int, 4> m_LightningTextureIDs{ -1, -1, -1, -1 };
	int m_DischargeAudioID{ -1 };
	std::uint32_t m_RandomState{ 0x4C11DB7u };
	int m_ReplaceIndex{ 0 };
};

#endif // !CHAIN_LIGHTNING_H
