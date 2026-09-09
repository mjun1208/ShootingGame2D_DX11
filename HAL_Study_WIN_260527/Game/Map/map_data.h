#ifndef MAP_DATA_H
#define MAP_DATA_H

#include "monster_type.h"

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
	std::array<bool, MONSTER_TYPE_COUNT> AllowedMonsters{};
};

struct MapSettings
{
	int TotalRoundCount{ 1 };
	int BossRound{ 1 };
	int RegularRoomMinCount{ 1 };
	int RegularRoomMaxCount{ 1 };
	int BossRoomCount{ 1 };
	int MapColumns{ 1 };
	int MapRows{ 1 };
	float TileSize{ 1.0f };
	int RoomTileWidth{ 1 };
	int RoomTileHeight{ 1 };
	int LargeRoomTileWidth{ 1 };
	int LargeRoomTileHeight{ 1 };
	int RoomGridSpacingX{ 1 };
	int RoomGridSpacingY{ 1 };
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
	// Missing or unloaded data returns nullptr; GetRoundEncounter throws std::out_of_range.
	const RoundEncounterData* FindRoundEncounter(int round) const;
	bool IsMonsterAllowed(int round, MonsterType type) const;

  private:
	MapSettings m_Settings{};
	std::vector<RoundEncounterData> m_RoundEncounters;
};

#endif // MAP_DATA_H
