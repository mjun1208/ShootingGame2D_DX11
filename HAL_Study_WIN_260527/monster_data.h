#ifndef MONSTER_DATA_H
#define MONSTER_DATA_H

#include "game_enemy.h"

#include <DirectXMath.h>

#include <array>
#include <string>

enum class MonsterDeathEffect
{
	Slime,
	Blood,
	Bones,
};

struct MonsterData
{
	MonsterType Type;
	std::string Id;
	std::wstring TexturePath;
	float MaxHitPoint;
	float MoveSpeed;
	DirectX::XMFLOAT2 DrawSize;
	DirectX::XMFLOAT2 AimOffset;
	float CollisionRadius;
	int FrameWidth;
	int FrameHeight;
	int FrameCount;
	float AnimationSpeed;
	bool SourceFacesLeft;
	MonsterDeathEffect DeathEffect;
	int ExperienceDrop;
	float SpawnWeight;
};

class MonsterGameData
{
public:
	static constexpr std::size_t MONSTER_COUNT = 14;

	bool Load(const char* file_path);
	const MonsterData& Get(MonsterType type) const;

private:
	std::array<MonsterData, MONSTER_COUNT> m_Monsters{};
};

#endif // MONSTER_DATA_H
