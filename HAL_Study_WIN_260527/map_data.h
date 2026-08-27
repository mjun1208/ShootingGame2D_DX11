#ifndef MAP_DATA_H
#define MAP_DATA_H

#include "game_enemy.h"

#include <array>
#include <vector>

enum class MapTheme
{
	Forest,
	CryptDungeon,
	CurrentDungeon,
};

struct RoundEncounterData
{
	int Round{ 1 };
	MapTheme Theme{ MapTheme::CurrentDungeon };
	int MinEnemiesPerRoom{ 1 };
	int MaxEnemiesPerRoom{ 1 };
	int MinWaveCount{ 1 };
	int MaxWaveCount{ 1 };
	int LargeRoomWaveCount{ 1 };
	int LargeRoomEnemyBonus{ 0 };
	std::array<bool, 14> AllowedMonsters{};
};

class MapGameData
{
public:
	bool Load(const char* file_path);

	int GetTotalRoundCount() const;
	int GetBossRound() const;
	int GetRegularRoomMinCount() const;
	int GetRegularRoomMaxCount() const;
	int GetBossRoomCount() const;
	int GetMapColumns() const;
	int GetMapRows() const;
	float GetTileSize() const;
	int GetRoomTileWidth() const;
	int GetRoomTileHeight() const;
	int GetLargeRoomTileWidth() const;
	int GetLargeRoomTileHeight() const;
	int GetRoomGridSpacingX() const;
	int GetRoomGridSpacingY() const;
	const RoundEncounterData& GetRoundEncounter(int round) const;
	bool IsMonsterAllowed(int round, MonsterType type) const;

private:
	int m_TotalRoundCount{ 1 };
	int m_BossRound{ 1 };
	int m_RegularRoomMinCount{ 1 };
	int m_RegularRoomMaxCount{ 1 };
	int m_BossRoomCount{ 1 };
	int m_MapColumns{ 1 };
	int m_MapRows{ 1 };
	float m_TileSize{ 1.0f };
	int m_RoomTileWidth{ 1 };
	int m_RoomTileHeight{ 1 };
	int m_LargeRoomTileWidth{ 1 };
	int m_LargeRoomTileHeight{ 1 };
	int m_RoomGridSpacingX{ 1 };
	int m_RoomGridSpacingY{ 1 };
	std::vector<RoundEncounterData> m_RoundEncounters;
};

#endif // MAP_DATA_H
