#ifndef MONSTER_DATA_H
#define MONSTER_DATA_H

#include "game_enemy.h"

#include <DirectXMath.h>

#include <string>

enum class MonsterDeathEffect
{
	Slime,
	Blood,
	Bones,
};

struct MonsterData
{
	MonsterType Type{ MonsterType::Slime };
	std::string Id;
	std::wstring TexturePath;
	float MaxHitPoint{ 23.0f };
	float MoveSpeed{ 120.0f };
	DirectX::XMFLOAT2 DrawSize{ 48.0f, 48.0f };
	float CollisionRadius{ 24.0f };
	int FrameWidth{ 16 };
	int FrameHeight{ 16 };
	int FrameCount{ 1 };
	float AnimationSpeed{ 0.16f };
	bool SourceFacesLeft{ false };
	MonsterDeathEffect DeathEffect{ MonsterDeathEffect::Blood };
	int ExperienceDrop{ 1 };
	float SpawnWeight{ 1.0f };
};

namespace MonsterDatabase
{
	bool Load(const char* file_path);
	void ResetDefaults();
	const MonsterData& Get(MonsterType type);
}

#endif // MONSTER_DATA_H
