#include "map_data.h"

#include "json_reader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace
{
	constexpr std::size_t MONSTER_TYPE_COUNT = 14;

	std::size_t MonsterTypeToIndex(MonsterType type)
	{
		const int index = static_cast<int>(type) - 1;
		return index >= 0 && index < static_cast<int>(MONSTER_TYPE_COUNT) ?
			static_cast<std::size_t>(index) : MONSTER_TYPE_COUNT;
	}

	bool TryParseMonsterType(std::string_view id, MonsterType& type)
	{
		if (id == "slime") type = MonsterType::Slime;
		else if (id == "skeleton_base") type = MonsterType::SkeletonBase;
		else if (id == "skeleton_mage") type = MonsterType::SkeletonMage;
		else if (id == "skeleton_rogue") type = MonsterType::SkeletonRogue;
		else if (id == "skeleton_warrior") type = MonsterType::SkeletonWarrior;
		else if (id == "bat") type = MonsterType::Bat;
		else if (id == "orc") type = MonsterType::Orc;
		else if (id == "orc_rogue") type = MonsterType::OrcRogue;
		else if (id == "orc_shaman") type = MonsterType::OrcShaman;
		else if (id == "orc_warrior") type = MonsterType::OrcWarrior;
		else if (id == "boss_slime") type = MonsterType::BossSlime;
		else if (id == "boss_corrupted_knight") type = MonsterType::BossCorruptedKnight;
		else if (id == "boss_corrupted_mage") type = MonsterType::BossCorruptedMage;
		else if (id == "boss_cthulhu") type = MonsterType::BossCthulhu;
		else return false;
		return true;
	}

	bool ParseInt2(Json::Reader& reader, int& first, int& second)
	{
		return reader.Consume('[') &&
			reader.ParseInt(first) && reader.Consume(',') &&
			reader.ParseInt(second) && reader.Consume(']');
	}

	bool ParseAllowedMonsters(
		Json::Reader& reader,
		std::array<bool, MONSTER_TYPE_COUNT>& allowed_monsters)
	{
		if (!reader.Consume('[') || reader.Consume(']')) return false;
		while (true)
		{
			std::string id;
			MonsterType type{};
			if (!reader.ParseString(id) || !TryParseMonsterType(id, type)) return false;
			const std::size_t type_index = MonsterTypeToIndex(type);
			if (type_index >= allowed_monsters.size() || allowed_monsters[type_index])
			{
				return false;
			}
			allowed_monsters[type_index] = true;

			if (reader.Consume(']')) return true;
			if (!reader.Consume(',')) return false;
		}
	}

	bool TryParseMapTheme(std::string_view id, MapTheme& theme)
	{
		if (id == "forest") theme = MapTheme::Forest;
		else if (id == "crypt_dungeon") theme = MapTheme::CryptDungeon;
		else if (id == "current_dungeon") theme = MapTheme::CurrentDungeon;
		else return false;
		return true;
	}

	constexpr std::uint8_t ROUND_FIELD_ROUND = 1u << 0;
	constexpr std::uint8_t ROUND_FIELD_MAP_THEME = 1u << 1;
	constexpr std::uint8_t ROUND_FIELD_ENEMY_COUNT = 1u << 2;
	constexpr std::uint8_t ROUND_FIELD_WAVE_COUNT = 1u << 3;
	constexpr std::uint8_t ROUND_FIELD_LARGE_ROOM_WAVE_COUNT = 1u << 4;
	constexpr std::uint8_t ROUND_FIELD_LARGE_ROOM_ENEMY_BONUS = 1u << 5;
	constexpr std::uint8_t ROUND_FIELD_ALLOWED_MONSTERS = 1u << 6;
	constexpr std::uint8_t ALL_ROUND_FIELDS = (1u << 7) - 1u;

	bool MarkField(std::uint8_t& fields, std::uint8_t field)
	{
		if ((fields & field) != 0) return false;
		fields |= field;
		return true;
	}

	bool ParseRound(Json::Reader& reader, RoundEncounterData& round_data)
	{
		if (!reader.Consume('{') || reader.Consume('}')) return false;

		std::uint8_t parsed_fields = 0;
		while (true)
		{
			std::string key;
			if (!reader.ParseString(key) || !reader.Consume(':')) return false;
			if (key == "round")
			{
				if (!MarkField(parsed_fields, ROUND_FIELD_ROUND) ||
					!reader.ParseInt(round_data.Round)) return false;
			}
			else if (key == "mapTheme")
			{
				std::string theme_id;
				if (!MarkField(parsed_fields, ROUND_FIELD_MAP_THEME) ||
					!reader.ParseString(theme_id) ||
					!TryParseMapTheme(theme_id, round_data.Theme)) return false;
			}
			else if (key == "enemyCount")
			{
				if (!MarkField(parsed_fields, ROUND_FIELD_ENEMY_COUNT) ||
					!ParseInt2(reader, round_data.MinEnemiesPerRoom,
						round_data.MaxEnemiesPerRoom)) return false;
			}
			else if (key == "waveCount")
			{
				if (!MarkField(parsed_fields, ROUND_FIELD_WAVE_COUNT) ||
					!ParseInt2(reader, round_data.MinWaveCount,
						round_data.MaxWaveCount)) return false;
			}
			else if (key == "largeRoomWaveCount")
			{
				if (!MarkField(parsed_fields, ROUND_FIELD_LARGE_ROOM_WAVE_COUNT) ||
					!reader.ParseInt(round_data.LargeRoomWaveCount)) return false;
			}
			else if (key == "largeRoomEnemyBonus")
			{
				if (!MarkField(parsed_fields, ROUND_FIELD_LARGE_ROOM_ENEMY_BONUS) ||
					!reader.ParseInt(round_data.LargeRoomEnemyBonus)) return false;
			}
			else if (key == "allowedMonsters")
			{
				if (!MarkField(parsed_fields, ROUND_FIELD_ALLOWED_MONSTERS) ||
					!ParseAllowedMonsters(reader, round_data.AllowedMonsters)) return false;
			}
			else if (!reader.SkipValue())
			{
				return false;
			}

			if (reader.Consume('}')) break;
			if (!reader.Consume(',')) return false;
		}

		return parsed_fields == ALL_ROUND_FIELDS &&
			round_data.Round > 0 &&
			round_data.MinEnemiesPerRoom > 0 &&
			round_data.MaxEnemiesPerRoom >= round_data.MinEnemiesPerRoom &&
			round_data.MinWaveCount > 0 &&
			round_data.MaxWaveCount >= round_data.MinWaveCount &&
			round_data.LargeRoomWaveCount > 0 &&
			round_data.LargeRoomEnemyBonus >= 0 &&
			std::any_of(
				round_data.AllowedMonsters.begin(),
				round_data.AllowedMonsters.end(),
				[](bool allowed) { return allowed; });
	}

	bool ParseRounds(Json::Reader& reader, std::vector<RoundEncounterData>& rounds)
	{
		if (!reader.Consume('[') || reader.Consume(']')) return false;
		while (true)
		{
			RoundEncounterData round_data{};
			if (!ParseRound(reader, round_data)) return false;
			rounds.push_back(std::move(round_data));

			if (reader.Consume(']')) return true;
			if (!reader.Consume(',')) return false;
		}
	}

	constexpr std::uint16_t MAP_FIELD_TOTAL_ROUNDS = 1u << 0;
	constexpr std::uint16_t MAP_FIELD_BOSS_ROUND = 1u << 1;
	constexpr std::uint16_t MAP_FIELD_REGULAR_ROOM_COUNT = 1u << 2;
	constexpr std::uint16_t MAP_FIELD_BOSS_ROOM_COUNT = 1u << 3;
	constexpr std::uint16_t MAP_FIELD_SIZE_IN_TILES = 1u << 4;
	constexpr std::uint16_t MAP_FIELD_TILE_SIZE = 1u << 5;
	constexpr std::uint16_t MAP_FIELD_ROOM_SIZE_IN_TILES = 1u << 6;
	constexpr std::uint16_t MAP_FIELD_ROOM_GRID_SPACING = 1u << 7;
	constexpr std::uint16_t MAP_FIELD_LARGE_ROOM_SIZE_IN_TILES = 1u << 8;
	constexpr std::uint16_t ALL_MAP_FIELDS = (1u << 9) - 1u;

	bool MarkMapField(std::uint16_t& fields, std::uint16_t field)
	{
		if ((fields & field) != 0) return false;
		fields |= field;
		return true;
	}

	bool ParseMap(
		Json::Reader& reader,
		int& total_rounds,
		int& boss_round,
		int& regular_room_min_count,
		int& regular_room_max_count,
		int& boss_room_count,
		int& map_columns,
		int& map_rows,
		float& tile_size,
		int& room_tile_width,
		int& room_tile_height,
		int& large_room_tile_width,
		int& large_room_tile_height,
		int& room_grid_spacing_x,
		int& room_grid_spacing_y)
	{
		if (!reader.Consume('{') || reader.Consume('}')) return false;

		std::uint16_t parsed_fields = 0;
		while (true)
		{
			std::string key;
			if (!reader.ParseString(key) || !reader.Consume(':')) return false;
			if (key == "totalRounds")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_TOTAL_ROUNDS) ||
					!reader.ParseInt(total_rounds)) return false;
			}
			else if (key == "bossRound")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_BOSS_ROUND) ||
					!reader.ParseInt(boss_round)) return false;
			}
			else if (key == "regularRoomCount")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_REGULAR_ROOM_COUNT) ||
					!ParseInt2(reader, regular_room_min_count,
						regular_room_max_count)) return false;
			}
			else if (key == "bossRoomCount")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_BOSS_ROOM_COUNT) ||
					!reader.ParseInt(boss_room_count)) return false;
			}
			else if (key == "sizeInTiles")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_SIZE_IN_TILES) ||
					!ParseInt2(reader, map_columns, map_rows)) return false;
			}
			else if (key == "tileSize")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_TILE_SIZE) ||
					!reader.ParseFloat(tile_size)) return false;
			}
			else if (key == "roomSizeInTiles")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_ROOM_SIZE_IN_TILES) ||
					!ParseInt2(reader, room_tile_width, room_tile_height)) return false;
			}
			else if (key == "roomGridSpacing")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_ROOM_GRID_SPACING) ||
					!ParseInt2(reader, room_grid_spacing_x,
						room_grid_spacing_y)) return false;
			}
			else if (key == "largeRoomSizeInTiles")
			{
				if (!MarkMapField(parsed_fields, MAP_FIELD_LARGE_ROOM_SIZE_IN_TILES) ||
					!ParseInt2(reader, large_room_tile_width,
						large_room_tile_height)) return false;
			}
			else if (!reader.SkipValue())
			{
				return false;
			}

			if (reader.Consume('}')) break;
			if (!reader.Consume(',')) return false;
		}

		return parsed_fields == ALL_MAP_FIELDS &&
			total_rounds > 0 && boss_round > 0 && boss_round <= total_rounds &&
			regular_room_min_count >= 2 &&
			regular_room_max_count >= regular_room_min_count &&
			boss_room_count >= 1 &&
			map_columns > 0 && map_rows > 0 && tile_size > 0.0f &&
			room_tile_width >= 8 && room_tile_height >= 8 &&
			large_room_tile_width > room_tile_width &&
			large_room_tile_height > room_tile_height &&
			room_grid_spacing_x >= large_room_tile_width &&
			room_grid_spacing_y >= large_room_tile_height &&
			map_columns >= room_grid_spacing_x * 4 + large_room_tile_width &&
			map_rows >= room_grid_spacing_y * 4 + large_room_tile_height;
	}
}

bool MapGameData::Load(const char* file_path)
{
	std::string source;
	if (!Json::LoadTextFile(file_path, source)) return false;

	Json::Reader reader(source);
	if (!reader.Consume('{') || reader.Consume('}')) return false;

	int total_rounds = 0;
	int boss_round = 0;
	int regular_room_min_count = 0;
	int regular_room_max_count = 0;
	int boss_room_count = 0;
	int map_columns = 0;
	int map_rows = 0;
	float tile_size = 0.0f;
	int room_tile_width = 0;
	int room_tile_height = 0;
	int large_room_tile_width = 0;
	int large_room_tile_height = 0;
	int room_grid_spacing_x = 0;
	int room_grid_spacing_y = 0;
	std::vector<RoundEncounterData> rounds;
	bool has_map = false;
	bool has_rounds = false;
	while (true)
	{
		std::string key;
		if (!reader.ParseString(key) || !reader.Consume(':')) return false;
		if (key == "map")
		{
			if (has_map || !ParseMap(
				reader,
				total_rounds,
				boss_round,
				regular_room_min_count,
				regular_room_max_count,
				boss_room_count,
				map_columns,
				map_rows,
				tile_size,
				room_tile_width,
				room_tile_height,
				large_room_tile_width,
				large_room_tile_height,
				room_grid_spacing_x,
				room_grid_spacing_y)) return false;
			has_map = true;
		}
		else if (key == "rounds")
		{
			if (has_rounds || !ParseRounds(reader, rounds)) return false;
			has_rounds = true;
		}
		else if (!reader.SkipValue())
		{
			return false;
		}

		if (reader.Consume('}')) break;
		if (!reader.Consume(',')) return false;
	}

	if (!has_map || !has_rounds || !reader.IsAtEnd() ||
		static_cast<int>(rounds.size()) != total_rounds)
	{
		return false;
	}

	std::vector<RoundEncounterData> ordered_rounds(
		static_cast<std::size_t>(total_rounds));
	std::vector<bool> parsed_rounds(static_cast<std::size_t>(total_rounds), false);
	for (RoundEncounterData& round_data : rounds)
	{
		const int index = round_data.Round - 1;
		if (index < 0 || index >= total_rounds || parsed_rounds[index]) return false;
		parsed_rounds[index] = true;
		ordered_rounds[index] = std::move(round_data);
	}

	m_TotalRoundCount = total_rounds;
	m_BossRound = boss_round;
	m_RegularRoomMinCount = regular_room_min_count;
	m_RegularRoomMaxCount = regular_room_max_count;
	m_BossRoomCount = boss_room_count;
	m_MapColumns = map_columns;
	m_MapRows = map_rows;
	m_TileSize = tile_size;
	m_RoomTileWidth = room_tile_width;
	m_RoomTileHeight = room_tile_height;
	m_LargeRoomTileWidth = large_room_tile_width;
	m_LargeRoomTileHeight = large_room_tile_height;
	m_RoomGridSpacingX = room_grid_spacing_x;
	m_RoomGridSpacingY = room_grid_spacing_y;
	m_RoundEncounters = std::move(ordered_rounds);
	return true;
}

int MapGameData::GetTotalRoundCount() const
{
	return m_TotalRoundCount;
}

int MapGameData::GetBossRound() const
{
	return m_BossRound;
}

int MapGameData::GetRegularRoomMinCount() const
{
	return m_RegularRoomMinCount;
}

int MapGameData::GetRegularRoomMaxCount() const
{
	return m_RegularRoomMaxCount;
}

int MapGameData::GetBossRoomCount() const
{
	return m_BossRoomCount;
}

int MapGameData::GetMapColumns() const
{
	return m_MapColumns;
}

int MapGameData::GetMapRows() const
{
	return m_MapRows;
}

float MapGameData::GetTileSize() const
{
	return m_TileSize;
}

int MapGameData::GetRoomTileWidth() const
{
	return m_RoomTileWidth;
}

int MapGameData::GetRoomTileHeight() const
{
	return m_RoomTileHeight;
}

int MapGameData::GetLargeRoomTileWidth() const
{
	return m_LargeRoomTileWidth;
}

int MapGameData::GetLargeRoomTileHeight() const
{
	return m_LargeRoomTileHeight;
}

int MapGameData::GetRoomGridSpacingX() const
{
	return m_RoomGridSpacingX;
}

int MapGameData::GetRoomGridSpacingY() const
{
	return m_RoomGridSpacingY;
}

const RoundEncounterData& MapGameData::GetRoundEncounter(int round) const
{
	const int safe_round = std::clamp(round, 1, m_TotalRoundCount);
	return m_RoundEncounters[static_cast<std::size_t>(safe_round - 1)];
}

bool MapGameData::IsMonsterAllowed(int round, MonsterType type) const
{
	const std::size_t type_index = MonsterTypeToIndex(type);
	return type_index < MONSTER_TYPE_COUNT &&
		GetRoundEncounter(round).AllowedMonsters[type_index];
}
