#ifndef PROCEDURAL_MAP_H
#define PROCEDURAL_MAP_H

#include "sprite_lighting.h"

#include <DirectXMath.h>

#include <cstdint>

struct ProceduralMapRoom
{
	int Index{ -1 };
	int Depth{ 0 };
	int TileX{ 0 };
	int TileY{ 0 };
	int TileWidth{ 0 };
	int TileHeight{ 0 };
	DirectX::XMFLOAT2 WorldMin{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 WorldMax{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 Center{ 0.0f, 0.0f };
	bool IsLargeRoom{ false };
	bool IsBossRoom{ false };
	bool IsPortalRoom{ false };
};

struct ProceduralMapOverviewLayout
{
	DirectX::XMFLOAT2 Origin{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 Size{ 0.0f, 0.0f };
	float WorldScale{ 0.0f };
	bool IsExpanded{ false };
};

using ProceduralMapRoomVisibilityPredicate = bool (*)(int room_index);

bool ProceduralMap_Initialize(std::uint32_t seed = 0);
void ProceduralMap_Finalize();
void ProceduralMap_Update(float delta_time);
void ProceduralMap_Regenerate(std::uint32_t seed = 0);
void ProceduralMap_GenerateRound(int round, std::uint32_t seed = 0);
void ProceduralMap_Draw(
	const DirectX::XMFLOAT2& camera_position,
	const DirectX::XMFLOAT2& viewport_size);
int ProceduralMap_AppendTorchLights(
	SpritePointLight* lights,
	int light_count,
	int capacity,
	const DirectX::XMFLOAT2& camera_position,
	const DirectX::XMFLOAT2& viewport_size);
void ProceduralMap_DrawEncounterLock();
void ProceduralMap_DrawFadeOverlay(
	const DirectX::XMFLOAT2& viewport_size,
	float alpha);
ProceduralMapOverviewLayout ProceduralMap_DrawOverview(
	const DirectX::XMFLOAT2& viewport_size,
	bool expanded,
	ProceduralMapRoomVisibilityPredicate is_room_visible = nullptr,
	ProceduralMapRoomVisibilityPredicate is_room_cleared = nullptr);

std::uint32_t ProceduralMap_GetSeed();
int ProceduralMap_GetRound();
bool ProceduralMap_IsBossRound();
DirectX::XMFLOAT2 ProceduralMap_GetWorldSize();
DirectX::XMFLOAT2 ProceduralMap_GetPlayerSpawnPosition();
int ProceduralMap_GetBossRoomIndex();
int ProceduralMap_GetFinalEncounterRoomIndex();
int ProceduralMap_GetExitRoomIndex();
DirectX::XMFLOAT2 ProceduralMap_GetRoundExitPosition();
DirectX::XMFLOAT2 ProceduralMap_ClampCameraPosition(
	const DirectX::XMFLOAT2& desired_position,
	const DirectX::XMFLOAT2& viewport_size);

bool ProceduralMap_IsCircleWalkable(
	const DirectX::XMFLOAT2& position,
	float radius);
bool ProceduralMap_IsSegmentWalkable(
	const DirectX::XMFLOAT2& start,
	const DirectX::XMFLOAT2& end,
	float radius);
DirectX::XMFLOAT2 ProceduralMap_MoveCircle(
	const DirectX::XMFLOAT2& position,
	const DirectX::XMFLOAT2& movement,
	float radius);
DirectX::XMFLOAT2 ProceduralMap_MoveActorCircle(
	const DirectX::XMFLOAT2& position,
	const DirectX::XMFLOAT2& movement,
	float radius);

bool ProceduralMap_LockEncounterRoom(int room_index);
void ProceduralMap_ClearEncounterLock(int room_index = -1);
int ProceduralMap_GetLockedEncounterRoom();

int ProceduralMap_GetRoomCount();
int ProceduralMap_GetStartRoomIndex();
int ProceduralMap_GetRoomIndexAt(const DirectX::XMFLOAT2& world_position);
const ProceduralMapRoom* ProceduralMap_GetRoom(int room_index);
bool ProceduralMap_TryGetRoomSpawnPosition(
	int room_index,
	int sequence,
	const DirectX::XMFLOAT2& avoid_position,
	float minimum_distance,
	float clearance_radius,
	DirectX::XMFLOAT2& out_position);

#endif // !PROCEDURAL_MAP_H
