#include "monster_data.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string_view>

namespace
{
	constexpr std::size_t MONSTER_COUNT = 6;
	std::array<MonsterData, MONSTER_COUNT> g_Monsters{};

	std::size_t TypeToIndex(MonsterType type)
	{
		const int index = static_cast<int>(type) - 1;
		return index >= 0 && index < static_cast<int>(MONSTER_COUNT) ?
			static_cast<std::size_t>(index) : 0u;
	}

	MonsterType IdToType(std::string_view id)
	{
		if (id == "skeleton_base") return MonsterType::SkeletonBase;
		if (id == "skeleton_mage") return MonsterType::SkeletonMage;
		if (id == "skeleton_rogue") return MonsterType::SkeletonRogue;
		if (id == "skeleton_warrior") return MonsterType::SkeletonWarrior;
		if (id == "bat") return MonsterType::Bat;
		return MonsterType::Slime;
	}

	std::wstring ToWide(std::string_view text)
	{
		return { text.begin(), text.end() };
	}

	MonsterDeathEffect ParseDeathEffect(std::string_view value)
	{
		if (value == "slime") return MonsterDeathEffect::Slime;
		if (value == "bones") return MonsterDeathEffect::Bones;
		return MonsterDeathEffect::Blood;
	}

	class JsonReader
	{
	public:
		explicit JsonReader(std::string_view source) : m_Source(source) {}

		bool ParseMonsters()
		{
			if (!Consume('{')) return false;
			while (!Consume('}'))
			{
				std::string key;
				if (!ParseString(key) || !Consume(':')) return false;
				if (key == "monsters")
				{
					if (!ParseMonsterArray()) return false;
				}
				else if (!SkipValue())
				{
					return false;
				}
				if (Consume('}')) return true;
				if (!Consume(',')) return false;
			}
			return true;
		}

	private:
		void SkipWhitespace()
		{
			while (m_Position < m_Source.size() &&
				std::isspace(static_cast<unsigned char>(m_Source[m_Position])))
			{
				++m_Position;
			}
		}

		bool Consume(char expected)
		{
			SkipWhitespace();
			if (m_Position >= m_Source.size() || m_Source[m_Position] != expected)
			{
				return false;
			}
			++m_Position;
			return true;
		}

		bool ParseString(std::string& output)
		{
			SkipWhitespace();
			if (m_Position >= m_Source.size() || m_Source[m_Position++] != '"')
			{
				return false;
			}
			output.clear();
			while (m_Position < m_Source.size())
			{
				const char character = m_Source[m_Position++];
				if (character == '"') return true;
				if (character == '\\')
				{
					if (m_Position >= m_Source.size()) return false;
					const char escaped = m_Source[m_Position++];
					switch (escaped)
					{
					case 'n': output.push_back('\n'); break;
					case 'r': output.push_back('\r'); break;
					case 't': output.push_back('\t'); break;
					case '\\': output.push_back('\\'); break;
					case '"': output.push_back('"'); break;
					default: return false;
					}
				}
				else
				{
					output.push_back(character);
				}
			}
			return false;
		}

		bool ParseNumber(float& output)
		{
			SkipWhitespace();
			const std::size_t start = m_Position;
			while (m_Position < m_Source.size())
			{
				const char value = m_Source[m_Position];
				if (!(std::isdigit(static_cast<unsigned char>(value)) ||
					value == '-' || value == '+' || value == '.' || value == 'e' || value == 'E'))
				{
					break;
				}
				++m_Position;
			}
			if (start == m_Position) return false;
			const char* begin = m_Source.data() + start;
			const char* end = m_Source.data() + m_Position;
			const auto result = std::from_chars(begin, end, output);
			return result.ec == std::errc{} && result.ptr == end;
		}

		bool ParseBool(bool& output)
		{
			SkipWhitespace();
			if (m_Source.substr(m_Position, 4) == "true")
			{
				m_Position += 4;
				output = true;
				return true;
			}
			if (m_Source.substr(m_Position, 5) == "false")
			{
				m_Position += 5;
				output = false;
				return true;
			}
			return false;
		}

		bool ParseFloat2(DirectX::XMFLOAT2& output)
		{
			if (!Consume('[')) return false;
			if (!ParseNumber(output.x) || !Consume(',') || !ParseNumber(output.y)) return false;
			return Consume(']');
		}

		bool ParseInt2(int& first, int& second)
		{
			DirectX::XMFLOAT2 values{};
			if (!ParseFloat2(values)) return false;
			first = static_cast<int>(values.x);
			second = static_cast<int>(values.y);
			return true;
		}

		bool ParseMonsterArray()
		{
			if (!Consume('[')) return false;
			if (Consume(']')) return true;
			while (true)
			{
				if (!ParseMonster()) return false;
				if (Consume(']')) return true;
				if (!Consume(',')) return false;
			}
		}

		bool ParseMonster()
		{
			if (!Consume('{')) return false;
			MonsterData parsed{};
			bool has_id = false;
			while (!Consume('}'))
			{
				std::string key;
				if (!ParseString(key) || !Consume(':')) return false;
				if (key == "id")
				{
					std::string id;
					if (!ParseString(id)) return false;
					parsed.Type = IdToType(id);
					parsed = g_Monsters[TypeToIndex(parsed.Type)];
					parsed.Id = id;
					has_id = true;
				}
				else if (!has_id)
				{
					// Keep object parsing deterministic: the id selects the defaults
					// that subsequent fields override.
					return false;
				}
				else if (key == "texture")
				{
					std::string value;
					if (!ParseString(value)) return false;
					parsed.TexturePath = ToWide(value);
				}
				else if (key == "hp") { if (!ParseNumber(parsed.MaxHitPoint)) return false; }
				else if (key == "moveSpeed") { if (!ParseNumber(parsed.MoveSpeed)) return false; }
				else if (key == "drawSize") { if (!ParseFloat2(parsed.DrawSize)) return false; }
				else if (key == "collisionRadius") { if (!ParseNumber(parsed.CollisionRadius)) return false; }
				else if (key == "frameSize")
				{
					if (!ParseInt2(parsed.FrameWidth, parsed.FrameHeight)) return false;
				}
				else if (key == "frameCount")
				{
					float value = 0.0f;
					if (!ParseNumber(value)) return false;
					parsed.FrameCount = static_cast<int>(value);
				}
				else if (key == "animationSpeed") { if (!ParseNumber(parsed.AnimationSpeed)) return false; }
				else if (key == "sourceFacesLeft") { if (!ParseBool(parsed.SourceFacesLeft)) return false; }
				else if (key == "deathEffect")
				{
					std::string value;
					if (!ParseString(value)) return false;
					parsed.DeathEffect = ParseDeathEffect(value);
				}
				else if (key == "experience")
				{
					float value = 0.0f;
					if (!ParseNumber(value)) return false;
					parsed.ExperienceDrop = static_cast<int>(value);
				}
				else if (key == "spawnWeight") { if (!ParseNumber(parsed.SpawnWeight)) return false; }
				else if (!SkipValue()) return false;

				if (Consume('}')) break;
				if (!Consume(',')) return false;
			}
			if (!has_id) return false;
			Validate(parsed);
			g_Monsters[TypeToIndex(parsed.Type)] = parsed;
			return true;
		}

		void Validate(MonsterData& data)
		{
			data.MaxHitPoint = std::max(data.MaxHitPoint, 1.0f);
			data.MoveSpeed = std::max(data.MoveSpeed, 0.0f);
			data.DrawSize.x = std::max(data.DrawSize.x, 1.0f);
			data.DrawSize.y = std::max(data.DrawSize.y, 1.0f);
			data.CollisionRadius = std::max(data.CollisionRadius, 1.0f);
			data.FrameWidth = std::max(data.FrameWidth, 1);
			data.FrameHeight = std::max(data.FrameHeight, 1);
			data.FrameCount = std::max(data.FrameCount, 1);
			data.AnimationSpeed = std::max(data.AnimationSpeed, 0.01f);
			data.ExperienceDrop = std::max(data.ExperienceDrop, 0);
			data.SpawnWeight = std::max(data.SpawnWeight, 0.0f);
		}

		bool SkipValue()
		{
			SkipWhitespace();
			if (m_Position >= m_Source.size()) return false;
			if (m_Source[m_Position] == '"')
			{
				std::string ignored;
				return ParseString(ignored);
			}
			if (m_Source[m_Position] == '{')
			{
				if (!Consume('{')) return false;
				if (Consume('}')) return true;
				while (true)
				{
					std::string key;
					if (!ParseString(key) || !Consume(':') || !SkipValue()) return false;
					if (Consume('}')) return true;
					if (!Consume(',')) return false;
				}
			}
			if (m_Source[m_Position] == '[')
			{
				if (!Consume('[')) return false;
				if (Consume(']')) return true;
				while (true)
				{
					if (!SkipValue()) return false;
					if (Consume(']')) return true;
					if (!Consume(',')) return false;
				}
			}
			bool ignored_bool = false;
			if (ParseBool(ignored_bool)) return true;
			SkipWhitespace();
			if (m_Source.substr(m_Position, 4) == "null")
			{
				m_Position += 4;
				return true;
			}
			float ignored_number = 0.0f;
			return ParseNumber(ignored_number);
		}

		std::string_view m_Source;
		std::size_t m_Position{ 0 };
	};
}

namespace MonsterDatabase
{
	void ResetDefaults()
	{
		g_Monsters[TypeToIndex(MonsterType::Slime)] = {
			MonsterType::Slime,
			"slime",
			L"asset/texture/slime_pack/green_blob.png",
			23.0f, 120.0f, { 96.0f, 96.0f }, 38.0f,
			16, 16, 4, 0.16f, false,
			MonsterDeathEffect::Slime, 1, 1.0f,
		};
		g_Monsters[TypeToIndex(MonsterType::SkeletonBase)] = {
			MonsterType::SkeletonBase,
			"skeleton_base",
			L"asset/Mobs/Skeleton Crew/Skeleton - Base/Run/Run-Sheet.png",
			24.0f, 125.0f, { 192.0f, 192.0f }, 24.0f,
			64, 64, 6, 0.11f, false,
			MonsterDeathEffect::Bones, 1, 0.5f,
		};
		g_Monsters[TypeToIndex(MonsterType::SkeletonMage)] = {
			MonsterType::SkeletonMage,
			"skeleton_mage",
			L"asset/Mobs/Skeleton Crew/Skeleton - Mage/Run/Run-Sheet.png",
			24.0f, 125.0f, { 192.0f, 192.0f }, 24.0f,
			64, 64, 6, 0.11f, false,
			MonsterDeathEffect::Bones, 1, 0.5f,
		};
		g_Monsters[TypeToIndex(MonsterType::SkeletonRogue)] = {
			MonsterType::SkeletonRogue,
			"skeleton_rogue",
			L"asset/Mobs/Skeleton Crew/Skeleton - Rogue/Run/Run-Sheet.png",
			24.0f, 125.0f, { 192.0f, 192.0f }, 24.0f,
			64, 64, 6, 0.11f, false,
			MonsterDeathEffect::Bones, 1, 0.5f,
		};
		g_Monsters[TypeToIndex(MonsterType::SkeletonWarrior)] = {
			MonsterType::SkeletonWarrior,
			"skeleton_warrior",
			L"asset/Mobs/Skeleton Crew/Skeleton - Warrior/Run/Run-Sheet.png",
			24.0f, 125.0f, { 192.0f, 192.0f }, 24.0f,
			64, 64, 6, 0.11f, false,
			MonsterDeathEffect::Bones, 1, 0.5f,
		};
		g_Monsters[TypeToIndex(MonsterType::Bat)] = {
			MonsterType::Bat,
			"bat",
			L"asset/monster/02_bat/02_bat_purple_fly.png",
			16.0f, 155.0f, { 48.0f, 48.0f }, 20.0f,
			16, 16, 2, 0.13f, true,
			MonsterDeathEffect::Blood, 1, 1.0f,
		};
	}

	bool Load(const char* file_path)
	{
		ResetDefaults();
		std::ifstream input(file_path, std::ios::binary);
		if (!input) return false;
		const std::string source{
			std::istreambuf_iterator<char>(input),
			std::istreambuf_iterator<char>{}
		};
		JsonReader reader(source);
		if (!reader.ParseMonsters())
		{
			ResetDefaults();
			return false;
		}
		return true;
	}

	const MonsterData& Get(MonsterType type)
	{
		return g_Monsters[TypeToIndex(type)];
	}
}
