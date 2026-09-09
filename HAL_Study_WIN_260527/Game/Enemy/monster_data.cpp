#include "monster_data.h"

#include "file_utils.h"
#include "json_reader.h"

#include <array>
#include <stdexcept>
#include <cstdint>
#include <string_view>
#include <utility>

namespace
{
	using Json::MarkField;
	using Json::ToWide;

	bool TryParseDeathEffect(std::string_view value, MonsterDeathEffect& death_effect)
	{
		if (value == "slime")
		{
			death_effect = MonsterDeathEffect::Slime;
		}
		else if (value == "blood")
		{
			death_effect = MonsterDeathEffect::Blood;
		}
		else if (value == "bones")
		{
			death_effect = MonsterDeathEffect::Bones;
		}
		else
		{
			return false;
		}
		return true;
	}

	constexpr std::uint16_t FIELD_ID = 1u << 0;
	constexpr std::uint16_t FIELD_TEXTURE = 1u << 1;
	constexpr std::uint16_t FIELD_HP = 1u << 2;
	constexpr std::uint16_t FIELD_MOVE_SPEED = 1u << 3;
	constexpr std::uint16_t FIELD_DRAW_SIZE = 1u << 4;
	constexpr std::uint16_t FIELD_COLLISION_RADIUS = 1u << 5;
	constexpr std::uint16_t FIELD_FRAME_SIZE = 1u << 6;
	constexpr std::uint16_t FIELD_FRAME_COUNT = 1u << 7;
	constexpr std::uint16_t FIELD_ANIMATION_SPEED = 1u << 8;
	constexpr std::uint16_t FIELD_SOURCE_FACES_LEFT = 1u << 9;
	constexpr std::uint16_t FIELD_DEATH_EFFECT = 1u << 10;
	constexpr std::uint16_t FIELD_EXPERIENCE = 1u << 11;
	constexpr std::uint16_t FIELD_SPAWN_WEIGHT = 1u << 12;
	constexpr std::uint16_t FIELD_AIM_OFFSET = 1u << 13;
	constexpr std::uint16_t ALL_FIELDS = (1u << 14) - 1u;

	class MonsterJsonParser
	{
	  public:
		MonsterJsonParser(std::string_view source, std::array<MonsterData, MonsterGameData::MONSTER_COUNT>& monsters)
		    : m_Reader(source), m_Monsters(monsters)
		{
		}

		bool Parse()
		{
			bool has_monsters = false;
			const bool parsed_root = m_Reader.ParseObject(
			    [this, &has_monsters](std::string_view key)
			    {
				    if (key != "monsters")
				    {
					    return m_Reader.SkipValue();
				    }
				    if (has_monsters || !ParseMonsterArray())
				    {
					    return false;
				    }
				    has_monsters = true;
				    return true;
			    },
			    false);

			if (!parsed_root || !has_monsters || !m_Reader.IsAtEnd())
			{
				return false;
			}
			for (bool was_parsed : m_ParsedMonsters)
			{
				if (!was_parsed)
				{
					return false;
				}
			}
			return true;
		}

	  private:
		bool ParseMonsterArray()
		{
			return m_Reader.ParseArray(
			    [this]
			    {
				    return ParseMonster();
			    },
			    false);
		}

		bool ParseMonster()
		{
			MonsterData parsed{};
			std::uint16_t parsed_fields = 0;
			if (!m_Reader.ParseObject(
			        [this, &parsed, &parsed_fields](std::string_view key)
			        {
				        return ParseField(key, parsed, parsed_fields);
			        },
			        false))
			{
				return false;
			}

			if (parsed_fields != ALL_FIELDS || !Validate(parsed))
			{
				return false;
			}
			const std::size_t index = MonsterTypeToIndex(parsed.Type);
			if (m_ParsedMonsters[index])
			{
				return false;
			}

			m_ParsedMonsters[index] = true;
			m_Monsters[index] = std::move(parsed);
			return true;
		}

		bool ParseField(std::string_view key, MonsterData& data, std::uint16_t& parsed_fields)
		{
			if (key == "id")
			{
				if (!MarkField(parsed_fields, FIELD_ID) || !m_Reader.ParseString(data.Id))
				{
					return false;
				}
				return TryParseMonsterType(data.Id, data.Type);
			}
			if (key == "texture")
			{
				if (!MarkField(parsed_fields, FIELD_TEXTURE))
				{
					return false;
				}
				std::string texture_path;
				if (!m_Reader.ParseString(texture_path))
				{
					return false;
				}
				data.TexturePath = ToWide(texture_path);
				return true;
			}
			if (key == "hp")
			{
				return MarkField(parsed_fields, FIELD_HP) && m_Reader.ParseFloat(data.MaxHitPoint);
			}
			if (key == "moveSpeed")
			{
				return MarkField(parsed_fields, FIELD_MOVE_SPEED) && m_Reader.ParseFloat(data.MoveSpeed);
			}
			if (key == "drawSize")
			{
				return MarkField(parsed_fields, FIELD_DRAW_SIZE) &&
				       m_Reader.ParseFloat2(data.DrawSize.x, data.DrawSize.y);
			}
			if (key == "aimOffset")
			{
				return MarkField(parsed_fields, FIELD_AIM_OFFSET) &&
				       m_Reader.ParseFloat2(data.AimOffset.x, data.AimOffset.y);
			}
			if (key == "collisionRadius")
			{
				return MarkField(parsed_fields, FIELD_COLLISION_RADIUS) && m_Reader.ParseFloat(data.CollisionRadius);
			}
			if (key == "frameSize")
			{
				return MarkField(parsed_fields, FIELD_FRAME_SIZE) &&
				       m_Reader.ParseInt2(data.FrameWidth, data.FrameHeight);
			}
			if (key == "frameCount")
			{
				return MarkField(parsed_fields, FIELD_FRAME_COUNT) && m_Reader.ParseInt(data.FrameCount);
			}
			if (key == "animationSpeed")
			{
				return MarkField(parsed_fields, FIELD_ANIMATION_SPEED) && m_Reader.ParseFloat(data.AnimationSpeed);
			}
			if (key == "sourceFacesLeft")
			{
				return MarkField(parsed_fields, FIELD_SOURCE_FACES_LEFT) && m_Reader.ParseBool(data.SourceFacesLeft);
			}
			if (key == "deathEffect")
			{
				if (!MarkField(parsed_fields, FIELD_DEATH_EFFECT))
				{
					return false;
				}
				std::string death_effect;
				return m_Reader.ParseString(death_effect) && TryParseDeathEffect(death_effect, data.DeathEffect);
			}
			if (key == "experience")
			{
				return MarkField(parsed_fields, FIELD_EXPERIENCE) && m_Reader.ParseInt(data.ExperienceDrop);
			}
			if (key == "spawnWeight")
			{
				return MarkField(parsed_fields, FIELD_SPAWN_WEIGHT) && m_Reader.ParseFloat(data.SpawnWeight);
			}
			return m_Reader.SkipValue();
		}

		bool Validate(const MonsterData& data) const
		{
			return !data.Id.empty() && !data.TexturePath.empty() && data.MaxHitPoint > 0.0f && data.MoveSpeed >= 0.0f &&
			       data.DrawSize.x > 0.0f && data.DrawSize.y > 0.0f && data.CollisionRadius > 0.0f &&
			       data.FrameWidth > 0 && data.FrameHeight > 0 && data.FrameCount > 0 && data.AnimationSpeed > 0.0f &&
			       data.ExperienceDrop >= 0 && data.SpawnWeight >= 0.0f;
		}

		Json::Reader m_Reader;
		std::array<MonsterData, MonsterGameData::MONSTER_COUNT>& m_Monsters;
		std::array<bool, MonsterGameData::MONSTER_COUNT> m_ParsedMonsters{};
	};
} // namespace

bool MonsterGameData::Load(const char* file_path)
{
	std::string source;
	if (!ReadTextFile(file_path, source))
	{
		return false;
	}

	std::array<MonsterData, MonsterGameData::MONSTER_COUNT> parsed_monsters{};
	MonsterJsonParser parser(source, parsed_monsters);
	if (!parser.Parse())
	{
		return false;
	}

	m_Monsters = std::move(parsed_monsters);
	m_IsLoaded = true;
	return true;
}

const MonsterData& MonsterGameData::Get(MonsterType type) const
{
	const MonsterData* monster = Find(type);
	if (!monster)
	{
		throw std::out_of_range("MonsterGameData::Get: missing monster");
	}
	return *monster;
}

const MonsterData* MonsterGameData::Find(MonsterType type) const
{
	const std::size_t index = MonsterTypeToIndex(type);
	return m_IsLoaded && index < MONSTER_COUNT ? &m_Monsters[index] : nullptr;
}
