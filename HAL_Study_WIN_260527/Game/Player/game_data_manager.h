#ifndef GAME_DATA_MANAGER_H
#define GAME_DATA_MANAGER_H

#include "map_data.h"
#include "monster_data.h"
#include "player_level_game_data.h"
#include "singleton.h"
#include "weapon_data.h"

class GameDataManager final : public cSingleton<GameDataManager>
{
	friend class cSingleton<GameDataManager>;

  public:
	bool LoadAll();
	bool IsLoaded() const;

	const MonsterGameData& GetMonsterGameData() const;
	const PlayerLevelGameData& GetPlayerLevelGameData() const;
	const WeaponGameData& GetWeaponGameData() const;
	const MapGameData& GetMapGameData() const;

  private:
	GameDataManager() = default;

	MonsterGameData m_MonsterGameData;
	PlayerLevelGameData m_PlayerLevelGameData;
	WeaponGameData m_WeaponGameData;
	MapGameData m_MapGameData;
	bool m_IsLoaded{ false };
};

inline const MapGameData& GetMapData()
{
	return GameDataManager::GetInstance().GetMapGameData();
}

inline const MonsterData& GetMonsterData(MonsterType type)
{
	return GameDataManager::GetInstance().GetMonsterGameData().Get(type);
}

inline const WeaponData& GetWeaponData(std::size_t index)
{
	return GameDataManager::GetInstance().GetWeaponGameData().Get(index);
}

#endif // GAME_DATA_MANAGER_H
