#ifndef PLAYER_LEVEL_GAME_DATA_H
#define PLAYER_LEVEL_GAME_DATA_H

#include <vector>

class PlayerLevelGameData
{
public:
	bool Load(const char* file_path);
	int GetExperienceToNextLevel(int level) const;

private:
	std::vector<int> m_ExperienceToNextLevel;
};

#endif // PLAYER_LEVEL_GAME_DATA_H
