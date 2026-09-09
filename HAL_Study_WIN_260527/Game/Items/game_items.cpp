#include "Audio.h"
#include "Constants/item_constants.h"
#include "chest.h"
#include "game_bullet.h"
#include "game_effect.h"
#include "game_experience_gem.h"
#include "game_healing_item.h"
#include "game_player.h"
#include "math_utils.h"
#include "random_utils.h"
#include "round_portal.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>


void Chest::Initialize(const DirectX::XMFLOAT2& position)
{
	m_TextureID = Texture_Load(L"asset/texture/chest/chest-sheet.png", false);
	m_OpenAudioID = Audio_Load("asset/sound/chest-open-1.wav");
	Reset(position);
}

void Chest::Finalize()
{
	if (m_OpenAudioID >= 0)
	{
		Audio_Unload(m_OpenAudioID);
		m_OpenAudioID = -1;
	}
	Texture_Release(m_TextureID);
	m_TextureID = TEXTURE_INVALID_ID;
	m_State = State::Gone;
}

void Chest::Reset(const DirectX::XMFLOAT2& position)
{
	m_Position = position;
	m_AnimationElapsedTime = 0.0f;
	m_State = State::Closed;
}

void Chest::Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	if (m_State != State::Opening)
	{
		return;
	}

	m_AnimationElapsedTime += delta_time;
	if (m_AnimationElapsedTime >= ItemConstants::Chest::ChestFrameTime * ItemConstants::Chest::ChestFrameCount)
	{
		FinishOpening();
	}
}

bool Chest::CanInteract(const DirectX::XMFLOAT2& interactor_position) const
{
	if (m_State != State::Closed)
	{
		return false;
	}

	return DistanceSquared(interactor_position, m_Position) <=
	       ItemConstants::Chest::ChestInteractionRadius * ItemConstants::Chest::ChestInteractionRadius;
}

DirectX::XMFLOAT2 Chest::GetInteractionPromptPosition() const
{
	return { m_Position.x,
		     m_Position.y - ItemConstants::Chest::ChestDrawHeight * 0.5f - ItemConstants::Chest::ChestPromptMargin };
}

void Chest::Interact()
{
	if (m_State != State::Closed)
	{
		return;
	}

	m_AnimationElapsedTime = 0.0f;
	m_State = State::Opening;
	if (m_OpenAudioID >= 0)
	{
		Audio_Play(m_OpenAudioID);
	}
}

void Chest::Draw() const
{
	if (m_State == State::Gone || m_TextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	int frame = 0;
	if (m_State == State::Opening)
	{
		frame = std::clamp(static_cast<int>(m_AnimationElapsedTime / ItemConstants::Chest::ChestFrameTime), 0,
		                   ItemConstants::Chest::ChestFrameCount - 1);
	}

	Sprite_DrawRegion(m_TextureID, m_Position,
	                  { ItemConstants::Chest::ChestDrawWidth, ItemConstants::Chest::ChestDrawHeight },
	                  { (ItemConstants::Chest::ChestFirstOpenFrame + frame) * ItemConstants::Chest::ChestFrameWidth,
	                    ItemConstants::Chest::ChestTextureY, ItemConstants::Chest::ChestFrameWidth,
	                    ItemConstants::Chest::ChestFrameHeight },
	                  { 1.0f, 1.0f, 1.0f, 1.0f });
}

bool Chest::IsGone() const
{
	return m_State == State::Gone;
}

bool Chest::BuildPointLight(SpritePointLight& out_light) const
{
	if (m_State == State::Gone)
	{
		return false;
	}

	float opening_progress = 0.0f;
	if (m_State == State::Opening)
	{
		opening_progress = Saturate(m_AnimationElapsedTime /
		                            (ItemConstants::Chest::ChestFrameTime * ItemConstants::Chest::ChestFrameCount));
	}
	const float opening_pulse = m_State == State::Opening ? 0.14f * std::sin(m_AnimationElapsedTime * 28.0f) : 0.0f;
	out_light = {
		{ m_Position.x, m_Position.y - 12.0f },
		235.0f + opening_progress * 85.0f,
		0.52f + opening_progress * 0.42f + opening_pulse,
		{ 1.0f, 0.56f, 0.14f },
	};
	return true;
}

void Chest::FinishOpening()
{
	m_State = State::Gone;
	if (!GameBullet::UnlockRandomWeapon())
	{
		GamePlayer::Heal(ItemConstants::Chest::ChestFallbackHealAmount);
		const int bonus_experience = std::max(1, static_cast<int>(GamePlayer::GetExperienceToNextLevel() *
		                                                          ItemConstants::Chest::ChestFallbackExperienceRatio));
		GamePlayer::AddExperience(bonus_experience);
	}
	cGameEffectManager::GetInstance().Play(GameEffectType::SmokePoof, { m_Position.x, m_Position.y - 4.0f }, 1.5f);
}

namespace
{

	struct HealingItem
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float MagnetSpeed{ 0.0f };
		float Age{ 0.0f };
		int RoomIndex{ -1 };
		bool Active{ false };
		bool Magnetized{ false };
	};

	std::array<HealingItem, ItemConstants::Healing::HealingItemMax> g_HealingItems{};
	int g_HealingItemTextureID = TEXTURE_INVALID_ID;

	bool RollDropChance()
	{
		return Random01() < ItemConstants::Healing::HealingItemDropChance;
	}
} // namespace

namespace GameHealingItem
{
	void Initialize()
	{
		g_HealingItemTextureID = Texture_Load(L"asset/texture/healing_potion.png", false);
		Clear();
	}

	void Finalize()
	{
		Clear();
		Texture_Release(g_HealingItemTextureID);
		g_HealingItemTextureID = TEXTURE_INVALID_ID;
	}

	void Clear()
	{
		g_HealingItems.fill(HealingItem{});
	}

	void TrySpawn(const DirectX::XMFLOAT2& world_position, int room_index)
	{
		if (!RollDropChance())
		{
			return;
		}

		auto free_item = std::find_if(g_HealingItems.begin(), g_HealingItems.end(),
		                              [](const HealingItem& item)
		                              {
			                              return !item.Active;
		                              });
		if (free_item == g_HealingItems.end())
		{
			return;
		}

		const float angle = RandomFloat(0.0f, DirectX::XM_2PI);
		const float speed_scale = RandomFloat(0.75f, 1.25f);
		free_item->Position = world_position;
		free_item->Velocity = {
			std::cos(angle) * ItemConstants::Healing::HealingItemStartSpeed * speed_scale,
			std::sin(angle) * ItemConstants::Healing::HealingItemStartSpeed * speed_scale,
		};
		free_item->RoomIndex = room_index;
		free_item->Active = true;
	}

	void Update(float delta_time, const DirectX::XMFLOAT2& player_position)
	{
		delta_time = std::max(delta_time, 0.0f);
		const float safe_delta_time = delta_time;
		const bool player_can_heal =
		    GamePlayer::GetHitPoint() > 0.0f && GamePlayer::GetHitPoint() < GamePlayer::GetMaxHitPoint();

		for (HealingItem& item : g_HealingItems)
		{
			if (!item.Active)
			{
				continue;
			}

			item.Age += safe_delta_time;
			if (!item.Magnetized)
			{
				item.Position.x += item.Velocity.x * safe_delta_time;
				item.Position.y += item.Velocity.y * safe_delta_time;
				const float speed_sq = LengthSquared(item.Velocity);
				if (speed_sq > 0.0001f)
				{
					const float speed = std::sqrt(speed_sq);
					const float next_speed = std::max(
					    0.0f, speed - ItemConstants::Healing::HealingItemScatterDeceleration * safe_delta_time);
					const float speed_ratio = next_speed / speed;
					item.Velocity.x *= speed_ratio;
					item.Velocity.y *= speed_ratio;
				}

				if (player_can_heal && DistanceSquared(player_position, item.Position) <=
				                           ItemConstants::Healing::HealingItemAttractionRadiusSq)
				{
					item.Magnetized = true;
					item.MagnetSpeed = ItemConstants::Healing::HealingItemMagnetStartSpeed;
					item.Velocity = { 0.0f, 0.0f };
				}
				continue;
			}

			if (!player_can_heal)
			{
				item.Magnetized = false;
				item.MagnetSpeed = 0.0f;
				continue;
			}

			const float dx = player_position.x - item.Position.x;
			const float dy = player_position.y - item.Position.y;
			const float distance_sq = DistanceSquared(player_position, item.Position);
			if (distance_sq <= ItemConstants::Healing::HealingItemAbsorbRadiusSq)
			{
				if (GamePlayer::Heal(ItemConstants::Healing::HealingItemHealAmount) > 0.0f)
				{
					item = HealingItem{};
				}
				continue;
			}

			const float distance = std::sqrt(distance_sq);
			item.MagnetSpeed =
			    std::min(ItemConstants::Healing::HealingItemMagnetMaxSpeed,
			             item.MagnetSpeed + ItemConstants::Healing::HealingItemMagnetAcceleration * safe_delta_time);
			const float move_distance = std::min(item.MagnetSpeed * safe_delta_time, distance);
			const DirectX::XMFLOAT2 direction = NormalizeOr({ dx, dy }, { 0.0f, 0.0f });
			item.Position.x += direction.x * move_distance;
			item.Position.y += direction.y * move_distance;
		}
	}

	void Draw()
	{
		if (g_HealingItemTextureID == TEXTURE_INVALID_ID)
		{
			return;
		}

		static std::array<SpriteInstance, ItemConstants::Healing::HealingItemMax> item_instances;
		static std::array<SpriteInstance, ItemConstants::Healing::HealingItemMax> glow_instances;
		int instance_count = 0;
		for (const HealingItem& item : g_HealingItems)
		{
			if (!item.Active)
			{
				continue;
			}

			const float pulse_phase = std::sin(item.Age * 6.5f);
			const float pulse = 1.0f + pulse_phase * 0.08f;
			const DirectX::XMFLOAT2 draw_position = {
				item.Position.x,
				item.Position.y + std::sin(item.Age * 4.2f) * ItemConstants::Healing::HealingItemBobAmount,
			};
			glow_instances[instance_count] = {
				draw_position,
				{ ItemConstants::Healing::HealingItemGlowSize * pulse,
				  ItemConstants::Healing::HealingItemGlowSize * pulse },
				0.0f,
				{ 0.18f, 1.0f, 0.28f, 0.34f + pulse_phase * 0.06f },
				{ 0.0f, 0.0f },
				{ 1.0f, 1.0f },
				1.0f,
			};
			item_instances[instance_count] = {
				draw_position,
				{ ItemConstants::Healing::HealingItemDrawSize * pulse,
				  ItemConstants::Healing::HealingItemDrawSize * pulse },
				0.0f,
				{ 1.0f, 1.0f, 1.0f, 1.0f },
			};
			++instance_count;
		}

		if (instance_count <= 0)
		{
			return;
		}
		SpriteInstanced_DrawAdditiveUnlit(g_HealingItemTextureID, glow_instances.data(), instance_count);
		SpriteInstanced_Draw(g_HealingItemTextureID, item_instances.data(), instance_count);
	}
} // namespace GameHealingItem

namespace
{

	struct ExperienceGem
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float MagnetSpeed{ 0.0f };
		float Age{ 0.0f };
		int Experience{ 0 };
		int RoomIndex{ -1 };
		bool Active{ false };
		bool Magnetized{ false };
		bool AttractAfterScatter{ false };
	};

	struct ExperienceGemGrid
	{
		int BucketHeads[ItemConstants::Experience::ExperienceGemGridBucketCount]{};
		int NextGem[ItemConstants::Experience::ExperienceGemMax]{};
		int CellX[ItemConstants::Experience::ExperienceGemMax]{};
		int CellY[ItemConstants::Experience::ExperienceGemMax]{};
	};

	std::array<ExperienceGem, ItemConstants::Experience::ExperienceGemMax> g_ExperienceGems{};
	ExperienceGemGrid g_ExperienceGemGrid;
	int g_ExperienceGemTextureID = TEXTURE_INVALID_ID;
	int g_ExperiencePickupAudioID = AudioInvalidID;
	float g_ExperiencePickupCooldown = 0.0f;
	constexpr float ExperiencePickupInterval = 0.12f;
	std::uint32_t g_ExperienceSpawnSerial = 0;

	bool IsScattering(const ExperienceGem& gem)
	{
		return LengthSquared(gem.Velocity) > 0.0001f;
	}

	void BeginAttraction(ExperienceGem& gem)
	{
		gem.Magnetized = true;
		gem.AttractAfterScatter = false;
		gem.MagnetSpeed = ItemConstants::Experience::ExperienceGemMagnetStartSpeed;
		gem.Velocity = { 0.0f, 0.0f };
	}

	void BuildGrid()
	{
		std::fill_n(g_ExperienceGemGrid.BucketHeads, ItemConstants::Experience::ExperienceGemGridBucketCount,
		            ItemConstants::Experience::ExperienceGemInvalidIndex);
		std::fill_n(g_ExperienceGemGrid.NextGem, ItemConstants::Experience::ExperienceGemMax,
		            ItemConstants::Experience::ExperienceGemInvalidIndex);

		for (int gem_id = 0; gem_id < ItemConstants::Experience::ExperienceGemMax; ++gem_id)
		{
			const ExperienceGem& gem = g_ExperienceGems[gem_id];
			if (!gem.Active || gem.Magnetized || IsScattering(gem))
			{
				continue;
			}

			const int cell_x = WorldToGridCell(gem.Position.x, ItemConstants::Experience::ExperienceGemGridCellSize);
			const int cell_y = WorldToGridCell(gem.Position.y, ItemConstants::Experience::ExperienceGemGridCellSize);
			const int bucket =
			    SpatialHashIndex(cell_x, cell_y, ItemConstants::Experience::ExperienceGemGridBucketCount);
			g_ExperienceGemGrid.CellX[gem_id] = cell_x;
			g_ExperienceGemGrid.CellY[gem_id] = cell_y;
			g_ExperienceGemGrid.NextGem[gem_id] = g_ExperienceGemGrid.BucketHeads[bucket];
			g_ExperienceGemGrid.BucketHeads[bucket] = gem_id;
		}
	}

	void AttractNearbyGems(const DirectX::XMFLOAT2& player_position)
	{
		const int min_cell_x =
		    WorldToGridCell(player_position.x - ItemConstants::Experience::ExperienceGemAttractionRadius,
		                    ItemConstants::Experience::ExperienceGemGridCellSize);
		const int max_cell_x =
		    WorldToGridCell(player_position.x + ItemConstants::Experience::ExperienceGemAttractionRadius,
		                    ItemConstants::Experience::ExperienceGemGridCellSize);
		const int min_cell_y =
		    WorldToGridCell(player_position.y - ItemConstants::Experience::ExperienceGemAttractionRadius,
		                    ItemConstants::Experience::ExperienceGemGridCellSize);
		const int max_cell_y =
		    WorldToGridCell(player_position.y + ItemConstants::Experience::ExperienceGemAttractionRadius,
		                    ItemConstants::Experience::ExperienceGemGridCellSize);

		for (int cell_y = min_cell_y; cell_y <= max_cell_y; ++cell_y)
		{
			for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x)
			{
				const int bucket =
				    SpatialHashIndex(cell_x, cell_y, ItemConstants::Experience::ExperienceGemGridBucketCount);
				for (int gem_id = g_ExperienceGemGrid.BucketHeads[bucket];
				     gem_id != ItemConstants::Experience::ExperienceGemInvalidIndex;
				     gem_id = g_ExperienceGemGrid.NextGem[gem_id])
				{
					if (g_ExperienceGemGrid.CellX[gem_id] != cell_x || g_ExperienceGemGrid.CellY[gem_id] != cell_y)
					{
						continue;
					}

					ExperienceGem& gem = g_ExperienceGems[gem_id];
					if (DistanceSquared(player_position, gem.Position) <=
					    ItemConstants::Experience::ExperienceGemAttractionRadiusSq)
					{
						BeginAttraction(gem);
					}
				}
			}
		}
	}
} // namespace

namespace GameExperienceGem
{
	void Initialize()
	{
		g_ExperienceGemTextureID = Texture_Load(L"asset/texture/experience_gem.png", false);
		g_ExperiencePickupAudioID = Audio_Load("asset/sound/farfadet46-experience-pickup.wav");
		Audio_SetGain(g_ExperiencePickupAudioID, 0.35f);
		Clear();
	}

	void Finalize()
	{
		Clear();
		Texture_Release(g_ExperienceGemTextureID);
		g_ExperienceGemTextureID = TEXTURE_INVALID_ID;
		Audio_Unload(g_ExperiencePickupAudioID);
		g_ExperiencePickupAudioID = AudioInvalidID;
	}

	void Clear()
	{
		for (ExperienceGem& gem : g_ExperienceGems)
		{
			gem = ExperienceGem{};
		}
		g_ExperienceSpawnSerial = 0;
		g_ExperiencePickupCooldown = 0.0f;
		Audio_Stop(g_ExperiencePickupAudioID);
		BuildGrid();
	}

	void Spawn(const DirectX::XMFLOAT2& world_position, int experience, int room_index)
	{
		if (experience <= 0)
		{
			return;
		}

		int free_id = ItemConstants::Experience::ExperienceGemInvalidIndex;
		for (int gem_id = 0; gem_id < ItemConstants::Experience::ExperienceGemMax; ++gem_id)
		{
			if (!g_ExperienceGems[gem_id].Active)
			{
				free_id = gem_id;
				break;
			}
		}
		if (free_id == ItemConstants::Experience::ExperienceGemInvalidIndex)
		{
			return;
		}

		const std::uint32_t spawn_serial = ++g_ExperienceSpawnSerial;
		const float angle_jitter = RandomFloat(-0.14f, 0.14f);
		const float angle =
		    static_cast<float>(spawn_serial % 4096u) * ItemConstants::Experience::ExperienceGemGoldenAngle +
		    angle_jitter;
		const float speed_scale = RandomFloat(0.78f, 1.32f);
		ExperienceGem& gem = g_ExperienceGems[free_id];
		gem.Position = world_position;
		gem.Velocity = {
			std::cos(angle) * ItemConstants::Experience::ExperienceGemStartSpeed * speed_scale,
			std::sin(angle) * ItemConstants::Experience::ExperienceGemStartSpeed * speed_scale,
		};
		gem.Experience = experience;
		gem.RoomIndex = room_index;
		gem.Active = true;
	}

	void AttractAllInRoom(int room_index)
	{
		if (room_index < 0)
		{
			return;
		}

		for (ExperienceGem& gem : g_ExperienceGems)
		{
			if (!gem.Active || gem.RoomIndex != room_index)
			{
				continue;
			}

			if (IsScattering(gem))
			{
				gem.AttractAfterScatter = true;
				continue;
			}

			BeginAttraction(gem);
		}
	}

	void Update(float delta_time, const DirectX::XMFLOAT2& player_position)
	{
		delta_time = std::max(delta_time, 0.0f);
		const float safe_delta_time = delta_time;
		g_ExperiencePickupCooldown = std::max(0.0f, g_ExperiencePickupCooldown - safe_delta_time);
		for (ExperienceGem& gem : g_ExperienceGems)
		{
			if (!gem.Active || gem.Magnetized)
			{
				continue;
			}

			gem.Age += safe_delta_time;
			gem.Position.x += gem.Velocity.x * safe_delta_time;
			gem.Position.y += gem.Velocity.y * safe_delta_time;
			const float speed_sq = LengthSquared(gem.Velocity);
			if (speed_sq > 0.0001f)
			{
				const float speed = std::sqrt(speed_sq);
				const float next_speed = std::max(
				    0.0f, speed - ItemConstants::Experience::ExperienceGemScatterDeceleration * safe_delta_time);
				const float speed_ratio = next_speed / speed;
				gem.Velocity.x *= speed_ratio;
				gem.Velocity.y *= speed_ratio;
			}

			if (!IsScattering(gem) && gem.AttractAfterScatter)
			{
				BeginAttraction(gem);
			}
		}

		BuildGrid();
		AttractNearbyGems(player_position);

		for (ExperienceGem& gem : g_ExperienceGems)
		{
			if (!gem.Active || !gem.Magnetized)
			{
				continue;
			}

			gem.Age += safe_delta_time;
			const float dx = player_position.x - gem.Position.x;
			const float dy = player_position.y - gem.Position.y;
			const float distance_sq = DistanceSquared(player_position, gem.Position);
			if (distance_sq <= ItemConstants::Experience::ExperienceGemAbsorbRadiusSq)
			{
				GamePlayer::AddExperience(gem.Experience);
				if (g_ExperiencePickupCooldown <= 0.0f)
				{
					Audio_Play(g_ExperiencePickupAudioID);
					g_ExperiencePickupCooldown = ExperiencePickupInterval;
				}
				gem = ExperienceGem{};
				continue;
			}

			const float distance = std::sqrt(distance_sq);
			gem.MagnetSpeed = std::min(ItemConstants::Experience::ExperienceGemMagnetMaxSpeed,
			                           gem.MagnetSpeed + ItemConstants::Experience::ExperienceGemMagnetAcceleration *
			                                                 safe_delta_time);
			const float move_distance = std::min(gem.MagnetSpeed * safe_delta_time, distance);
			const DirectX::XMFLOAT2 direction = NormalizeOr({ dx, dy }, { 0.0f, 0.0f });
			gem.Position.x += direction.x * move_distance;
			gem.Position.y += direction.y * move_distance;
		}
	}

	void Draw()
	{
		if (g_ExperienceGemTextureID == TEXTURE_INVALID_ID)
		{
			return;
		}

		static std::vector<SpriteInstance> gem_instances;
		static std::vector<SpriteInstance> glow_instances;
		if (gem_instances.capacity() < ItemConstants::Experience::ExperienceGemMax)
		{
			gem_instances.reserve(ItemConstants::Experience::ExperienceGemMax);
			glow_instances.reserve(ItemConstants::Experience::ExperienceGemMax);
		}
		gem_instances.clear();
		glow_instances.clear();

		for (const ExperienceGem& gem : g_ExperienceGems)
		{
			if (!gem.Active)
			{
				continue;
			}

			const float pulse_phase = std::sin(gem.Age * 7.0f);
			const float pulse = 1.0f + pulse_phase * 0.10f;
			const DirectX::XMFLOAT2 draw_position = {
				gem.Position.x,
				gem.Position.y + std::sin(gem.Age * 4.0f) * ItemConstants::Experience::ExperienceGemBobAmount,
			};
			glow_instances.push_back({
			    draw_position,
			    { ItemConstants::Experience::ExperienceGemGlowSize * pulse,
			      ItemConstants::Experience::ExperienceGemGlowSize * pulse },
			    0.0f,
			    { 0.16f, 0.82f, 1.0f, 0.46f + pulse_phase * 0.08f },
			    { 0.0f, 0.0f },
			    { 1.0f, 1.0f },
			    1.0f,
			});
			gem_instances.push_back({
			    draw_position,
			    { ItemConstants::Experience::ExperienceGemDrawSize * pulse,
			      ItemConstants::Experience::ExperienceGemDrawSize * pulse },
			    0.0f,
			    { 1.0f, 1.0f, 1.0f, 1.0f },
			});
		}

		if (!glow_instances.empty())
		{
			SpriteInstanced_DrawAdditiveUnlit(g_ExperienceGemTextureID, glow_instances.data(),
			                                  static_cast<int>(glow_instances.size()));
			// 가산 합성으로 한 번 더 그려 보석이 빛나도록 한다.
			// 아이템마다 제한된 점광원 슬롯을 사용하지 않아도 된다.
			SpriteInstanced_DrawAdditiveUnlit(g_ExperienceGemTextureID, gem_instances.data(),
			                                  static_cast<int>(gem_instances.size()));
			SpriteInstanced_Draw(g_ExperienceGemTextureID, gem_instances.data(),
			                     static_cast<int>(gem_instances.size()));
		}
	}
} // namespace GameExperienceGem


void RoundPortal::Initialize(const DirectX::XMFLOAT2& position)
{
	m_OpenTextureID = Texture_Load(L"asset/texture/portal/portal_open.png", false);
	m_IdleTextureID = Texture_Load(L"asset/texture/portal/portal_idle.png", false);
	Reset(position);
}

void RoundPortal::Finalize()
{
	Texture_Release(m_IdleTextureID);
	Texture_Release(m_OpenTextureID);
	m_IdleTextureID = TEXTURE_INVALID_ID;
	m_OpenTextureID = TEXTURE_INVALID_ID;
	m_IsOpen = false;
	m_IsActivated = false;
}

void RoundPortal::Reset(const DirectX::XMFLOAT2& position)
{
	m_Position = position;
	m_AnimationElapsedTime = 0.0f;
	m_IsOpen = false;
	m_IsActivated = false;
}

void RoundPortal::Update(float delta_time, bool open)
{
	delta_time = std::max(delta_time, 0.0f);
	if (open && !m_IsOpen)
	{
		m_AnimationElapsedTime = 0.0f;
		m_IsActivated = false;
	}

	m_IsOpen = open;
	if (m_IsOpen)
	{
		m_AnimationElapsedTime += delta_time;
	}
}

bool RoundPortal::CanInteract(const DirectX::XMFLOAT2& interactor_position) const
{
	if (!m_IsOpen || m_IsActivated)
	{
		return false;
	}

	return DistanceSquared(interactor_position, m_Position) <=
	       ItemConstants::Portal::PortalInteractionRadius * ItemConstants::Portal::PortalInteractionRadius;
}

DirectX::XMFLOAT2 RoundPortal::GetInteractionPromptPosition() const
{
	return { m_Position.x, m_Position.y - ItemConstants::Portal::PortalPromptOffsetY };
}

void RoundPortal::Interact()
{
	if (m_IsOpen)
	{
		m_IsActivated = true;
	}
}

bool RoundPortal::ConsumeActivation()
{
	const bool activated = m_IsActivated;
	m_IsActivated = false;
	return activated;
}

void RoundPortal::Draw() const
{
	if (!m_IsOpen || m_OpenTextureID == TEXTURE_INVALID_ID || m_IdleTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	const float open_duration =
	    ItemConstants::Portal::PortalOpenFrameCount * ItemConstants::Portal::PortalOpenFrameTime;
	int texture_id = m_IdleTextureID;
	int texture_x = 0;
	int texture_y = 0;
	if (m_AnimationElapsedTime < open_duration)
	{
		const int frame =
		    std::clamp(static_cast<int>(m_AnimationElapsedTime / ItemConstants::Portal::PortalOpenFrameTime), 0,
		               ItemConstants::Portal::PortalOpenFrameCount - 1);
		texture_id = m_OpenTextureID;
		texture_x = frame % ItemConstants::Portal::PortalOpenColumns * ItemConstants::Portal::PortalFrameWidth;
		texture_y = frame / ItemConstants::Portal::PortalOpenColumns * ItemConstants::Portal::PortalFrameHeight;
	}
	else
	{
		const float idle_time = m_AnimationElapsedTime - open_duration;
		const int frame = static_cast<int>(idle_time / ItemConstants::Portal::PortalIdleFrameTime) %
		                  ItemConstants::Portal::PortalIdleFrameCount;
		texture_x = frame * ItemConstants::Portal::PortalFrameWidth;
	}

	const bool lighting_was_enabled = Sprite_SetLightingEnabled(false);
	Sprite_DrawRegion(
	    texture_id, m_Position, { ItemConstants::Portal::PortalDrawSize, ItemConstants::Portal::PortalDrawSize },
	    { texture_x, texture_y, ItemConstants::Portal::PortalFrameWidth, ItemConstants::Portal::PortalFrameHeight },
	    { 1.0f, 1.0f, 1.0f, 1.0f });
	Sprite_SetLightingEnabled(lighting_was_enabled);
}
