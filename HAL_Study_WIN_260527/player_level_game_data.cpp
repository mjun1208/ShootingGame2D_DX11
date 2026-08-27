#include "player_level_game_data.h"

#include "json_reader.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace
{
	bool ParseExperienceRequirements(
		Json::Reader& reader,
		std::vector<int>& requirements)
	{
		if (!reader.Consume('[') || reader.Consume(']')) return false;
		while (true)
		{
			int requirement = 0;
			if (!reader.ParseInt(requirement) || requirement <= 0) return false;
			requirements.push_back(requirement);

			if (reader.Consume(']')) return true;
			if (!reader.Consume(',')) return false;
		}
	}
}

bool PlayerLevelGameData::Load(const char* file_path)
{
	std::string source;
	if (!Json::LoadTextFile(file_path, source)) return false;

	Json::Reader reader(source);
	if (!reader.Consume('{') || reader.Consume('}')) return false;

	std::vector<int> parsed_requirements;
	bool has_requirements = false;
	while (true)
	{
		std::string key;
		if (!reader.ParseString(key) || !reader.Consume(':')) return false;
		if (key == "experienceToNextLevel")
		{
			if (has_requirements ||
				!ParseExperienceRequirements(reader, parsed_requirements))
			{
				return false;
			}
			has_requirements = true;
		}
		else if (!reader.SkipValue())
		{
			return false;
		}

		if (reader.Consume('}')) break;
		if (!reader.Consume(',')) return false;
	}

	if (!has_requirements || !reader.IsAtEnd()) return false;
	m_ExperienceToNextLevel = std::move(parsed_requirements);
	return true;
}

int PlayerLevelGameData::GetExperienceToNextLevel(int level) const
{
	if (m_ExperienceToNextLevel.empty()) return 0;

	level = std::max(level, 1);
	const std::size_t index = static_cast<std::size_t>(level - 1);
	if (index < m_ExperienceToNextLevel.size())
	{
		return m_ExperienceToNextLevel[index];
	}

	const int last_requirement = m_ExperienceToNextLevel.back();
	const int growth = m_ExperienceToNextLevel.size() >= 2 ?
		std::max(
			last_requirement -
			m_ExperienceToNextLevel[m_ExperienceToNextLevel.size() - 2],
			0) :
		0;
	const long long extra_levels = static_cast<long long>(
		index - m_ExperienceToNextLevel.size() + 1);
	const long long extrapolated =
		static_cast<long long>(last_requirement) + extra_levels * growth;
	return static_cast<int>(std::min(
		extrapolated,
		static_cast<long long>(std::numeric_limits<int>::max())));
}
