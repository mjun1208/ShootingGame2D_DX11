#ifndef MONSTER_TYPE_H
#define MONSTER_TYPE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// 맵, 몬스터 데이터, 전투 코드에서 공통으로 사용하는 몬스터 종류 번호.
enum class MonsterType : std::uint8_t
{
	Slime = 1,
	SkeletonBase,
	SkeletonMage,
	SkeletonRogue,
	SkeletonWarrior,
	Bat,
	Orc,
	OrcRogue,
	OrcShaman,
	OrcWarrior,
	BossSlime,
	BossCorruptedKnight,
	BossCorruptedMage,
	BossCthulhu,
};
inline constexpr std::array<MonsterType, 14> MONSTER_TYPES{
	MonsterType::Slime,
	MonsterType::SkeletonBase,
	MonsterType::SkeletonMage,
	MonsterType::SkeletonRogue,
	MonsterType::SkeletonWarrior,
	MonsterType::Bat,
	MonsterType::Orc,
	MonsterType::OrcRogue,
	MonsterType::OrcShaman,
	MonsterType::OrcWarrior,
	MonsterType::BossSlime,
	MonsterType::BossCorruptedKnight,
	MonsterType::BossCorruptedMage,
	MonsterType::BossCthulhu,
};
inline constexpr std::size_t MONSTER_TYPE_COUNT = MONSTER_TYPES.size();

constexpr std::size_t MonsterTypeToIndex(MonsterType type)
{
	const int index = static_cast<int>(type) - 1;
	return index >= 0 && index < static_cast<int>(MONSTER_TYPE_COUNT) ? static_cast<std::size_t>(index)
	                                                                  : MONSTER_TYPE_COUNT;
}

inline bool TryParseMonsterType(std::string_view id, MonsterType& type)
{
	if (id == "slime")
	{
		type = MonsterType::Slime;
	}
	else if (id == "skeleton_base")
	{
		type = MonsterType::SkeletonBase;
	}
	else if (id == "skeleton_mage")
	{
		type = MonsterType::SkeletonMage;
	}
	else if (id == "skeleton_rogue")
	{
		type = MonsterType::SkeletonRogue;
	}
	else if (id == "skeleton_warrior")
	{
		type = MonsterType::SkeletonWarrior;
	}
	else if (id == "bat")
	{
		type = MonsterType::Bat;
	}
	else if (id == "orc")
	{
		type = MonsterType::Orc;
	}
	else if (id == "orc_rogue")
	{
		type = MonsterType::OrcRogue;
	}
	else if (id == "orc_shaman")
	{
		type = MonsterType::OrcShaman;
	}
	else if (id == "orc_warrior")
	{
		type = MonsterType::OrcWarrior;
	}
	else if (id == "boss_slime")
	{
		type = MonsterType::BossSlime;
	}
	else if (id == "boss_corrupted_knight")
	{
		type = MonsterType::BossCorruptedKnight;
	}
	else if (id == "boss_corrupted_mage")
	{
		type = MonsterType::BossCorruptedMage;
	}
	else if (id == "boss_cthulhu")
	{
		type = MonsterType::BossCthulhu;
	}
	else
	{
		return false;
	}
	return true;
}
#endif // MONSTER_TYPE_H
