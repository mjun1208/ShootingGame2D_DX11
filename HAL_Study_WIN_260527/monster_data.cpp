#include "monster_data.h"

#include "json_reader.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace
{
	std::size_t TypeToIndex(MonsterType type)
	{
		const int index = static_cast<int>(type) - 1;
		return index >= 0 &&
			index < static_cast<int>(MonsterGameData::MONSTER_COUNT) ?
			static_cast<std::size_t>(index) : 0u;
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

	bool TryParseDeathEffect(
		std::string_view value,
		MonsterDeathEffect& death_effect)
	{
		if (value == "slime") death_effect = MonsterDeathEffect::Slime;
		else if (value == "blood") death_effect = MonsterDeathEffect::Blood;
		else if (value == "bones") death_effect = MonsterDeathEffect::Bones;
		else return false;
		return true;
	}

	std::wstring ToWide(std::string_view text)
	{
		return { text.begin(), text.end() };
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
		MonsterJsonParser(
			std::string_view source,
			std::array<MonsterData, MonsterGameData::MONSTER_COUNT>& monsters)
			: m_Reader(source), m_Monsters(monsters)
		{
		}

		bool Parse()
		{
			if (!m_Reader.Consume('{') || m_Reader.Consume('}')) return false;

			bool has_monsters = false;
			while (true)
			{
				std::string key;
				if (!m_Reader.ParseString(key) || !m_Reader.Consume(':')) return false;
				if (key == "monsters")
				{
					if (has_monsters || !ParseMonsterArray()) return false;
					has_monsters = true;
				}
				else if (!m_Reader.SkipValue())
				{
					return false;
				}

				if (m_Reader.Consume('}')) break;
				if (!m_Reader.Consume(',')) return false;
			}

			if (!has_monsters || !m_Reader.IsAtEnd()) return false;
			for (bool was_parsed : m_ParsedMonsters)
			{
				if (!was_parsed) return false;
			}
			return true;
		}

	private:
		bool ParseFloat2(DirectX::XMFLOAT2& output)
		{
			return m_Reader.Consume('[') &&
				m_Reader.ParseFloat(output.x) && m_Reader.Consume(',') &&
				m_Reader.ParseFloat(output.y) && m_Reader.Consume(']');
		}

		bool ParseInt2(int& first, int& second)
		{
			return m_Reader.Consume('[') &&
				m_Reader.ParseInt(first) && m_Reader.Consume(',') &&
				m_Reader.ParseInt(second) && m_Reader.Consume(']');
		}

		bool ParseMonsterArray()
		{
			if (!m_Reader.Consume('[') || m_Reader.Consume(']')) return false;
			while (true)
			{
				if (!ParseMonster()) return false;
				if (m_Reader.Consume(']')) return true;
				if (!m_Reader.Consume(',')) return false;
			}
		}

		bool ParseMonster()
		{
			if (!m_Reader.Consume('{') || m_Reader.Consume('}')) return false;

			MonsterData parsed{};
			std::uint16_t parsed_fields = 0;
			while (true)
			{
				std::string key;
				if (!m_Reader.ParseString(key) || !m_Reader.Consume(':')) return false;
				if (!ParseField(key, parsed, parsed_fields)) return false;

				if (m_Reader.Consume('}')) break;
				if (!m_Reader.Consume(',')) return false;
			}

			if (parsed_fields != ALL_FIELDS || !Validate(parsed)) return false;
			const std::size_t index = TypeToIndex(parsed.Type);
			if (m_ParsedMonsters[index]) return false;

			m_ParsedMonsters[index] = true;
			m_Monsters[index] = std::move(parsed);
			return true;
		}

		bool ParseField(
			std::string_view key,
			MonsterData& data,
			std::uint16_t& parsed_fields)
		{
			if (key == "id")
			{
				if (!MarkField(parsed_fields, FIELD_ID) ||
					!m_Reader.ParseString(data.Id)) return false;
				return TryParseMonsterType(data.Id, data.Type);
			}
			if (key == "texture")
			{
				if (!MarkField(parsed_fields, FIELD_TEXTURE)) return false;
				std::string texture_path;
				if (!m_Reader.ParseString(texture_path)) return false;
				data.TexturePath = ToWide(texture_path);
				return true;
			}
			if (key == "hp")
			{
				return MarkField(parsed_fields, FIELD_HP) &&
					m_Reader.ParseFloat(data.MaxHitPoint);
			}
			if (key == "moveSpeed")
			{
				return MarkField(parsed_fields, FIELD_MOVE_SPEED) &&
					m_Reader.ParseFloat(data.MoveSpeed);
			}
			if (key == "drawSize")
			{
				return MarkField(parsed_fields, FIELD_DRAW_SIZE) && ParseFloat2(data.DrawSize);
			}
			if (key == "aimOffset")
			{
				return MarkField(parsed_fields, FIELD_AIM_OFFSET) && ParseFloat2(data.AimOffset);
			}
			if (key == "collisionRadius")
			{
				return MarkField(parsed_fields, FIELD_COLLISION_RADIUS) &&
					m_Reader.ParseFloat(data.CollisionRadius);
			}
			if (key == "frameSize")
			{
				return MarkField(parsed_fields, FIELD_FRAME_SIZE) &&
					ParseInt2(data.FrameWidth, data.FrameHeight);
			}
			if (key == "frameCount")
			{
				return MarkField(parsed_fields, FIELD_FRAME_COUNT) &&
					m_Reader.ParseInt(data.FrameCount);
			}
			if (key == "animationSpeed")
			{
				return MarkField(parsed_fields, FIELD_ANIMATION_SPEED) &&
					m_Reader.ParseFloat(data.AnimationSpeed);
			}
			if (key == "sourceFacesLeft")
			{
				return MarkField(parsed_fields, FIELD_SOURCE_FACES_LEFT) &&
					m_Reader.ParseBool(data.SourceFacesLeft);
			}
			if (key == "deathEffect")
			{
				if (!MarkField(parsed_fields, FIELD_DEATH_EFFECT)) return false;
				std::string death_effect;
				return m_Reader.ParseString(death_effect) &&
					TryParseDeathEffect(death_effect, data.DeathEffect);
			}
			if (key == "experience")
			{
				return MarkField(parsed_fields, FIELD_EXPERIENCE) &&
					m_Reader.ParseInt(data.ExperienceDrop);
			}
			if (key == "spawnWeight")
			{
				return MarkField(parsed_fields, FIELD_SPAWN_WEIGHT) &&
					m_Reader.ParseFloat(data.SpawnWeight);
			}
			return m_Reader.SkipValue();
		}

		bool MarkField(std::uint16_t& parsed_fields, std::uint16_t field) const
		{
			if ((parsed_fields & field) != 0) return false;
			parsed_fields |= field;
			return true;
		}

		bool Validate(const MonsterData& data) const
		{
			return !data.Id.empty() &&
				!data.TexturePath.empty() &&
				data.MaxHitPoint > 0.0f &&
				data.MoveSpeed >= 0.0f &&
				data.DrawSize.x > 0.0f &&
				data.DrawSize.y > 0.0f &&
				data.CollisionRadius > 0.0f &&
				data.FrameWidth > 0 &&
				data.FrameHeight > 0 &&
				data.FrameCount > 0 &&
				data.AnimationSpeed > 0.0f &&
				data.ExperienceDrop >= 0 &&
				data.SpawnWeight >= 0.0f;
		}

		Json::Reader m_Reader;
		std::array<MonsterData, MonsterGameData::MONSTER_COUNT>& m_Monsters;
		std::array<bool, MonsterGameData::MONSTER_COUNT> m_ParsedMonsters{};
	};
}

bool MonsterGameData::Load(const char* file_path)
{
	std::string source;
	if (!Json::LoadTextFile(file_path, source)) return false;

	std::array<MonsterData, MonsterGameData::MONSTER_COUNT> parsed_monsters{};
	MonsterJsonParser parser(source, parsed_monsters);
	if (!parser.Parse()) return false;

	m_Monsters = std::move(parsed_monsters);
	return true;
}

const MonsterData& MonsterGameData::Get(MonsterType type) const
{
	return m_Monsters[TypeToIndex(type)];
}
