#include "map_data.h"

#include "file_utils.h"
#include "json_reader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
	using Json::MarkField;

	bool ParseAllowedMonsters(Json::Reader& reader, std::array<bool, MONSTER_TYPE_COUNT>& allowed_monsters)
	{
		return reader.ParseArray(
		    [&reader, &allowed_monsters]
		    {
			    std::string id;
			    MonsterType type{};
			    if (!reader.ParseString(id) || !TryParseMonsterType(id, type))
			    {
				    return false;
			    }
			    const std::size_t type_index = MonsterTypeToIndex(type);
			    if (type_index >= allowed_monsters.size() || allowed_monsters[type_index])
			    {
				    return false;
			    }
			    allowed_monsters[type_index] = true;
			    return true;
		    },
		    false);
	}

	bool TryParseMapTheme(std::string_view id, MapTheme& theme)
	{
		if (id == "forest")
		{
			theme = MapTheme::Forest;
		}
		else if (id == "crypt_dungeon")
		{
			theme = MapTheme::CryptDungeon;
		}
		else if (id == "current_dungeon")
		{
			theme = MapTheme::CurrentDungeon;
		}
		else
		{
			return false;
		}
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

	bool ParseRound(Json::Reader& reader, RoundEncounterData& round_data)
	{
		std::uint8_t parsed_fields = 0;
		if (!reader.ParseObject(
		        [&reader, &round_data, &parsed_fields](std::string_view key)
		        {
			        if (key == "round")
			        {
				        if (!MarkField(parsed_fields, ROUND_FIELD_ROUND) || !reader.ParseInt(round_data.Round))
				        {
					        return false;
				        }
			        }
			        else if (key == "mapTheme")
			        {
				        std::string theme_id;
				        if (!MarkField(parsed_fields, ROUND_FIELD_MAP_THEME) || !reader.ParseString(theme_id) ||
				            !TryParseMapTheme(theme_id, round_data.Theme))
				        {
					        return false;
				        }
			        }
			        else if (key == "enemyCount")
			        {
				        if (!MarkField(parsed_fields, ROUND_FIELD_ENEMY_COUNT) ||
				            !reader.ParseInt2(round_data.MinEnemiesPerRoom, round_data.MaxEnemiesPerRoom))
				        {
					        return false;
				        }
			        }
			        else if (key == "waveCount")
			        {
				        if (!MarkField(parsed_fields, ROUND_FIELD_WAVE_COUNT) ||
				            !reader.ParseInt2(round_data.MinWaveCount, round_data.MaxWaveCount))
				        {
					        return false;
				        }
			        }
			        else if (key == "largeRoomWaveCount")
			        {
				        if (!MarkField(parsed_fields, ROUND_FIELD_LARGE_ROOM_WAVE_COUNT) ||
				            !reader.ParseInt(round_data.LargeRoomWaveCount))
				        {
					        return false;
				        }
			        }
			        else if (key == "largeRoomEnemyBonus")
			        {
				        if (!MarkField(parsed_fields, ROUND_FIELD_LARGE_ROOM_ENEMY_BONUS) ||
				            !reader.ParseInt(round_data.LargeRoomEnemyBonus))
				        {
					        return false;
				        }
			        }
			        else if (key == "allowedMonsters")
			        {
				        if (!MarkField(parsed_fields, ROUND_FIELD_ALLOWED_MONSTERS) ||
				            !ParseAllowedMonsters(reader, round_data.AllowedMonsters))
				        {
					        return false;
				        }
			        }
			        else
			        {
				        return reader.SkipValue();
			        }
			        return true;
		        },
		        false))
		{
			return false;
		}

		return parsed_fields == ALL_ROUND_FIELDS && round_data.Round > 0 && round_data.MinEnemiesPerRoom > 0 &&
		       round_data.MaxEnemiesPerRoom >= round_data.MinEnemiesPerRoom && round_data.MinWaveCount > 0 &&
		       round_data.MaxWaveCount >= round_data.MinWaveCount && round_data.LargeRoomWaveCount > 0 &&
		       round_data.LargeRoomEnemyBonus >= 0 &&
		       std::any_of(round_data.AllowedMonsters.begin(), round_data.AllowedMonsters.end(),
		                   [](bool allowed)
		                   {
			                   return allowed;
		                   });
	}

	bool ParseRounds(Json::Reader& reader, std::vector<RoundEncounterData>& rounds)
	{
		return reader.ParseArray(
		    [&reader, &rounds]
		    {
			    RoundEncounterData round_data{};
			    if (!ParseRound(reader, round_data))
			    {
				    return false;
			    }
			    rounds.push_back(std::move(round_data));
			    return true;
		    },
		    false);
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

	bool ValidateMapSettings(const MapSettings& settings)
	{
		const bool valid_rounds =
		    settings.TotalRoundCount > 0 && settings.BossRound > 0 && settings.BossRound <= settings.TotalRoundCount;
		const bool valid_room_counts = settings.RegularRoomMinCount >= 2 &&
		                               settings.RegularRoomMaxCount >= settings.RegularRoomMinCount &&
		                               settings.BossRoomCount >= 1;
		const bool valid_dimensions = settings.MapColumns > 0 && settings.MapRows > 0 && settings.TileSize > 0.0f &&
		                              settings.RoomTileWidth >= 8 && settings.RoomTileHeight >= 8 &&
		                              settings.LargeRoomTileWidth > settings.RoomTileWidth &&
		                              settings.LargeRoomTileHeight > settings.RoomTileHeight;
		if (!valid_rounds || !valid_room_counts || !valid_dimensions)
		{
			return false;
		}

		return settings.RoomGridSpacingX >= settings.LargeRoomTileWidth &&
		       settings.RoomGridSpacingY >= settings.LargeRoomTileHeight &&
		       settings.MapColumns >= settings.RoomGridSpacingX * 4LL + settings.LargeRoomTileWidth &&
		       settings.MapRows >= settings.RoomGridSpacingY * 4LL + settings.LargeRoomTileHeight;
	}

	bool ParseMap(Json::Reader& reader, MapSettings& settings)
	{
		std::uint16_t parsed_fields = 0;
		if (!reader.ParseObject(
		        [&](std::string_view key)
		        {
			        if (key == "totalRounds")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_TOTAL_ROUNDS) &&
				               reader.ParseInt(settings.TotalRoundCount);
			        }
			        else if (key == "bossRound")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_BOSS_ROUND) && reader.ParseInt(settings.BossRound);
			        }
			        else if (key == "regularRoomCount")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_REGULAR_ROOM_COUNT) &&
				               reader.ParseInt2(settings.RegularRoomMinCount, settings.RegularRoomMaxCount);
			        }
			        else if (key == "bossRoomCount")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_BOSS_ROOM_COUNT) &&
				               reader.ParseInt(settings.BossRoomCount);
			        }
			        else if (key == "sizeInTiles")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_SIZE_IN_TILES) &&
				               reader.ParseInt2(settings.MapColumns, settings.MapRows);
			        }
			        else if (key == "tileSize")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_TILE_SIZE) && reader.ParseFloat(settings.TileSize);
			        }
			        else if (key == "roomSizeInTiles")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_ROOM_SIZE_IN_TILES) &&
				               reader.ParseInt2(settings.RoomTileWidth, settings.RoomTileHeight);
			        }
			        else if (key == "roomGridSpacing")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_ROOM_GRID_SPACING) &&
				               reader.ParseInt2(settings.RoomGridSpacingX, settings.RoomGridSpacingY);
			        }
			        else if (key == "largeRoomSizeInTiles")
			        {
				        return MarkField(parsed_fields, MAP_FIELD_LARGE_ROOM_SIZE_IN_TILES) &&
				               reader.ParseInt2(settings.LargeRoomTileWidth, settings.LargeRoomTileHeight);
			        }
			        else
			        {
				        return reader.SkipValue();
			        }
		        },
		        false))
		{
			return false;
		}

		return parsed_fields == ALL_MAP_FIELDS;
	}
} // namespace

bool MapGameData::Load(const char* file_path)
{
	std::string source;
	if (!ReadTextFile(file_path, source))
	{
		return false;
	}

	Json::Reader reader(source);
	MapSettings settings{};
	std::vector<RoundEncounterData> rounds;
	bool has_map = false;
	bool has_rounds = false;
	const bool parsed_root = reader.ParseObject(
	    [&](std::string_view key)
	    {
		    if (key == "map")
		    {
			    if (has_map || !ParseMap(reader, settings))
			    {
				    return false;
			    }
			    has_map = true;
		    }
		    else if (key == "rounds")
		    {
			    if (has_rounds || !ParseRounds(reader, rounds))
			    {
				    return false;
			    }
			    has_rounds = true;
		    }
		    else
		    {
			    return reader.SkipValue();
		    }
		    return true;
	    },
	    false);

	if (!parsed_root || !has_map || !has_rounds || !reader.IsAtEnd() || !ValidateMapSettings(settings) ||
	    static_cast<int>(rounds.size()) != settings.TotalRoundCount)
	{
		return false;
	}

	std::vector<RoundEncounterData> ordered_rounds(static_cast<std::size_t>(settings.TotalRoundCount));
	std::vector<bool> parsed_rounds(static_cast<std::size_t>(settings.TotalRoundCount), false);
	for (RoundEncounterData& round_data : rounds)
	{
		const int index = round_data.Round - 1;
		if (index < 0 || index >= settings.TotalRoundCount || parsed_rounds[index])
		{
			return false;
		}
		parsed_rounds[index] = true;
		ordered_rounds[index] = std::move(round_data);
	}

	m_Settings = settings;
	m_RoundEncounters = std::move(ordered_rounds);
	return true;
}

int MapGameData::GetTotalRoundCount() const
{
	return m_Settings.TotalRoundCount;
}

int MapGameData::GetBossRound() const
{
	return m_Settings.BossRound;
}

int MapGameData::GetRegularRoomMinCount() const
{
	return m_Settings.RegularRoomMinCount;
}

int MapGameData::GetRegularRoomMaxCount() const
{
	return m_Settings.RegularRoomMaxCount;
}

int MapGameData::GetBossRoomCount() const
{
	return m_Settings.BossRoomCount;
}

int MapGameData::GetMapColumns() const
{
	return m_Settings.MapColumns;
}

int MapGameData::GetMapRows() const
{
	return m_Settings.MapRows;
}

float MapGameData::GetTileSize() const
{
	return m_Settings.TileSize;
}

int MapGameData::GetRoomTileWidth() const
{
	return m_Settings.RoomTileWidth;
}

int MapGameData::GetRoomTileHeight() const
{
	return m_Settings.RoomTileHeight;
}

int MapGameData::GetLargeRoomTileWidth() const
{
	return m_Settings.LargeRoomTileWidth;
}

int MapGameData::GetLargeRoomTileHeight() const
{
	return m_Settings.LargeRoomTileHeight;
}

int MapGameData::GetRoomGridSpacingX() const
{
	return m_Settings.RoomGridSpacingX;
}

int MapGameData::GetRoomGridSpacingY() const
{
	return m_Settings.RoomGridSpacingY;
}

const RoundEncounterData& MapGameData::GetRoundEncounter(int round) const
{
	const RoundEncounterData* encounter = FindRoundEncounter(round);
	if (!encounter)
	{
		throw std::out_of_range("MapGameData::GetRoundEncounter: missing round");
	}
	return *encounter;
}

const RoundEncounterData* MapGameData::FindRoundEncounter(int round) const
{
	if (round <= 0 || static_cast<std::size_t>(round) > m_RoundEncounters.size())
	{
		return nullptr;
	}
	return &m_RoundEncounters[static_cast<std::size_t>(round - 1)];
}

bool MapGameData::IsMonsterAllowed(int round, MonsterType type) const
{
	const std::size_t type_index = MonsterTypeToIndex(type);
	const RoundEncounterData* encounter = FindRoundEncounter(round);
	return encounter && type_index < MONSTER_TYPE_COUNT && encounter->AllowedMonsters[type_index];
}
