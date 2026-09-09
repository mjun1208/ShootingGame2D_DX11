#include "game_data_manager.h"

#include <utility>

namespace
{
	constexpr char MONSTER_DATA_PATH[] = "asset/data/monsters.json";
	constexpr char PLAYER_LEVEL_DATA_PATH[] = "asset/data/player_levels.json";
	constexpr char WEAPON_DATA_PATH[] = "asset/data/weapons.json";
	constexpr char MAP_DATA_PATH[] = "asset/data/maps.json";
} // namespace

bool GameDataManager::LoadAll()
{
	if (m_IsLoaded)
	{
		return true;
	}

	// 모든 JSON 파일을 읽는 데 성공한 뒤에만 데이터를 교체한다.
	// 기존 데이터와 새 데이터가 섞여 사용되는 것을 방지한다.
	MonsterGameData monster_game_data;
	PlayerLevelGameData player_level_game_data;
	WeaponGameData weapon_game_data;
	MapGameData map_game_data;
	if (!monster_game_data.Load(MONSTER_DATA_PATH) || !player_level_game_data.Load(PLAYER_LEVEL_DATA_PATH) ||
	    !weapon_game_data.Load(WEAPON_DATA_PATH) || !map_game_data.Load(MAP_DATA_PATH))
	{
		return false;
	}

	m_MonsterGameData = std::move(monster_game_data);
	m_PlayerLevelGameData = std::move(player_level_game_data);
	m_WeaponGameData = std::move(weapon_game_data);
	m_MapGameData = std::move(map_game_data);
	m_IsLoaded = true;
	return true;
}

bool GameDataManager::IsLoaded() const
{
	return m_IsLoaded;
}

const MonsterGameData& GameDataManager::GetMonsterGameData() const
{
	return m_MonsterGameData;
}

const PlayerLevelGameData& GameDataManager::GetPlayerLevelGameData() const
{
	return m_PlayerLevelGameData;
}

const WeaponGameData& GameDataManager::GetWeaponGameData() const
{
	return m_WeaponGameData;
}

const MapGameData& GameDataManager::GetMapGameData() const
{
	return m_MapGameData;
}
