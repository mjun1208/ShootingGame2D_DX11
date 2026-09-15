#include "procedural_map_internal.h"

#include <algorithm>
#include "Constants/map_constants.h"

#include "math_utils.h"

namespace ProceduralMapInternal
{
	// 생성된 맵 한 장이 사용하는 상태는 맵의 생명주기 구현과 함께 둔다.
	std::array<std::array<int, GROUND_TILE_TYPE_COUNT>, MAP_THEME_COUNT> g_GroundTextureIds{};
	std::array<std::array<int, DECOR_TILE_TYPE_COUNT>, MAP_THEME_COUNT> g_DecorTextureIds{};
	std::array<int, TORCH_TEXTURE_PATHS.size()> g_TorchTextureIds{};
	std::array<int, ENCOUNTER_BARRIER_TILE_TYPE_COUNT> g_EncounterBarrierTextureIds{};
	std::array<std::vector<SpriteInstance>, GROUND_TILE_TYPE_COUNT> g_VisibleGroundBatches{};
	std::array<std::vector<SpriteInstance>, DECOR_TILE_TYPE_COUNT> g_VisibleDecorBatches{};
	int g_OverviewTextureId = TEXTURE_INVALID_ID;
	std::vector<SpriteInstance> g_OverviewTiles;
	std::vector<MapCell> g_Cells;
	std::vector<DungeonRoom> g_Rooms;
	std::vector<RoomEdge> g_Connections;
	std::vector<Decoration> g_Decorations;
	std::vector<EncounterBarrier> g_EncounterBarriers;
	int g_Round = 1;
	int g_StartRoomIndex = 0;
	int g_FinalEncounterRoomIndex = 0;
	int g_ExitRoomIndex = 0;
	int g_LockedEncounterRoom = -1;
	float g_TorchAnimationElapsed = 0.0f;
	bool g_Initialized = false;
} // namespace ProceduralMapInternal

using namespace DirectX;
using namespace ProceduralMapInternal;

// 맵의 생성과 해제는 여기서 시작한다.
bool ProceduralMap_Initialize()
{
	if (g_Initialized)
	{
		ProceduralMap_Regenerate();
		return true;
	}

	// 세 테마의 바닥/장식 이미지와 공용 미니맵 이미지를 한 번에 준비한다.
	ResetTextureIds();
	g_OverviewTextureId = Texture_Load(L"asset/texture/white_square.png", false);
	if (g_OverviewTextureId == TEXTURE_INVALID_ID ||
	    !LoadThemeTextureSets(GROUND_TEXTURE_DIRECTORIES, GROUND_TEXTURE_FILENAMES, g_GroundTextureIds) ||
	    !LoadThemeTextureSets(DECOR_TEXTURE_PATHS, g_DecorTextureIds) ||
	    !LoadTextureSet(TORCH_TEXTURE_PATHS, g_TorchTextureIds) ||
	    !LoadTextureSet(ENCOUNTER_BARRIER_TEXTURE_PATHS, g_EncounterBarrierTextureIds))
	{
		ReleaseTextureSet(g_EncounterBarrierTextureIds);
		ReleaseTextureSet(g_TorchTextureIds);
		ReleaseThemeTextureSets(g_DecorTextureIds);
		ReleaseThemeTextureSets(g_GroundTextureIds);
		Texture_Release(g_OverviewTextureId);
		g_OverviewTextureId = TEXTURE_INVALID_ID;
		return false;
	}

	// 매 프레임 벡터가 커지지 않도록 예상 타일 수만큼 미리 잡아 둔다.
	for (auto& batch : g_VisibleGroundBatches)
	{
		batch.reserve(768);
	}
	for (auto& batch : g_VisibleDecorBatches)
	{
		batch.reserve(128);
	}
	g_OverviewTiles.reserve(4096);
	g_EncounterBarriers.reserve(16);

	g_Round = 1;
	g_TorchAnimationElapsed = 0.0f;
	g_Initialized = true;
	GenerateMap();
	return true;
}

void ProceduralMap_Finalize()
{
	if (!g_Initialized)
	{
		ResetTextureIds();
		g_EncounterBarriers.clear();
		g_LockedEncounterRoom = -1;
		g_Round = 1;
		g_TorchAnimationElapsed = 0.0f;
		g_FinalEncounterRoomIndex = 0;
		g_ExitRoomIndex = 0;
		return;
	}

	// 생성된 맵 데이터와 GPU 리소스를 역순으로 정리한다.
	for (auto& batch : g_VisibleGroundBatches)
	{
		batch.clear();
	}
	for (auto& batch : g_VisibleDecorBatches)
	{
		batch.clear();
	}
	g_OverviewTiles.clear();
	g_Decorations.clear();
	g_EncounterBarriers.clear();
	g_Connections.clear();
	g_Rooms.clear();
	g_Cells.clear();
	ReleaseTextureSet(g_EncounterBarrierTextureIds);
	ReleaseTextureSet(g_TorchTextureIds);
	ReleaseThemeTextureSets(g_DecorTextureIds);
	ReleaseThemeTextureSets(g_GroundTextureIds);
	Texture_Release(g_OverviewTextureId);
	g_OverviewTextureId = TEXTURE_INVALID_ID;
	g_StartRoomIndex = 0;
	g_FinalEncounterRoomIndex = 0;
	g_ExitRoomIndex = 0;
	g_LockedEncounterRoom = -1;
	g_Round = 1;
	g_TorchAnimationElapsed = 0.0f;
	g_Initialized = false;
}

void ProceduralMap_Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	if (!g_Initialized)
	{
		return;
	}

	const float animation_duration =
	    MapConstants::Decoration::TorchFrameTime * static_cast<float>(TORCH_TEXTURE_PATHS.size());
	g_TorchAnimationElapsed = std::fmod(g_TorchAnimationElapsed + delta_time, animation_duration);
}

void ProceduralMap_Regenerate()
{
	if (!g_Initialized)
	{
		return;
	}
	GenerateMap();
}

void ProceduralMap_GenerateRound(int round)
{
	if (!g_Initialized)
	{
		return;
	}
	round = std::clamp(round, 1, GetMapData().GetTotalRoundCount());

	g_Round = round;
	GenerateMap();
}

// 맵 상태를 바꾸지 않는 조회 함수 모음.
int ProceduralMap_GetRound()
{
	return g_Round;
}

bool ProceduralMap_IsBossRound()
{
	return g_Round == GetMapData().GetBossRound();
}

XMFLOAT2 ProceduralMap_GetWorldSize()
{
	return {
		MapColumns() * MapTileSize(),
		MapRows() * MapTileSize(),
	};
}

XMFLOAT2 ProceduralMap_GetPlayerSpawnPosition()
{
	if (g_Rooms.empty() || g_StartRoomIndex < 0 || g_StartRoomIndex >= static_cast<int>(g_Rooms.size()))
	{
		return { MapTileSize(), MapTileSize() };
	}
	return g_Rooms[g_StartRoomIndex].Info.Center;
}

int ProceduralMap_GetBossRoomIndex()
{
	for (const DungeonRoom& room : g_Rooms)
	{
		if (room.Info.IsBossRoom)
		{
			return room.Info.Index;
		}
	}
	return -1;
}

int ProceduralMap_GetFinalEncounterRoomIndex()
{
	return g_FinalEncounterRoomIndex;
}

int ProceduralMap_GetExitRoomIndex()
{
	return g_ExitRoomIndex;
}

XMFLOAT2 ProceduralMap_GetRoundExitPosition()
{
	const ProceduralMapRoom* exit_room = ProceduralMap_GetRoom(g_ExitRoomIndex);
	return exit_room ? exit_room->Center : XMFLOAT2{ -1.0f, -1.0f };
}

XMFLOAT2 ProceduralMap_ClampCameraPosition(const XMFLOAT2& desired_position, const XMFLOAT2& viewport_size)
{
	const XMFLOAT2 world_size = ProceduralMap_GetWorldSize();
	const float half_width = std::max(viewport_size.x, 1.0f) * 0.5f;
	const float half_height = std::max(viewport_size.y, 1.0f) * 0.5f;
	XMFLOAT2 result = desired_position;
	result.x = world_size.x <= viewport_size.x ? world_size.x * 0.5f
	                                           : std::clamp(result.x, half_width, world_size.x - half_width);
	result.y = world_size.y <= viewport_size.y ? world_size.y * 0.5f
	                                           : std::clamp(result.y, half_height, world_size.y - half_height);
	return result;
}

int ProceduralMap_GetRoomCount()
{
	return static_cast<int>(g_Rooms.size());
}

int ProceduralMap_GetStartRoomIndex()
{
	return g_StartRoomIndex;
}

int ProceduralMap_GetRoomIndexAt(const XMFLOAT2& world_position)
{
	if (!g_Initialized || world_position.x < 0.0f || world_position.y < 0.0f)
	{
		return -1;
	}
	const int cell_x = static_cast<int>(std::floor(world_position.x / MapTileSize()));
	const int cell_y = static_cast<int>(std::floor(world_position.y / MapTileSize()));
	if (!IsInsideMap(cell_x, cell_y))
	{
		return -1;
	}
	const MapCell& cell = g_Cells[CellIndex(cell_x, cell_y)];
	return cell.Kind == CellKind::Room ? cell.RoomIndex : -1;
}

// 룸 정보 구하기
const ProceduralMapRoom* ProceduralMap_GetRoom(int room_index)
{
	if (room_index < 0 || room_index >= static_cast<int>(g_Rooms.size()))
	{
		return nullptr;
	}
	return &g_Rooms[room_index].Info;
}

// 몬스터 스폰 위치 구하기
bool ProceduralMap_TryGetRoomSpawnPosition(int room_index, const XMFLOAT2& avoid_position, float minimum_distance,
                                           float clearance_radius, XMFLOAT2& out_position)
{
	const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
	if (!room)
	{
		return false;
	}

	const int inset = 2;
	const int usable_width = room->TileWidth - inset * 2;
	const int usable_height = room->TileHeight - inset * 2;
	if (usable_width <= 0 || usable_height <= 0)
	{
		return false;
	}

	const int candidate_count = usable_width * usable_height;
	const int start = RandomInt(0, candidate_count - 1);
	int stride = RandomInt(1, candidate_count);
	while (std::gcd(stride, candidate_count) != 1)
	{
		++stride;
		if (stride > candidate_count)
		{
			stride = 1;
		}
	}

	const float minimum_distance_squared = minimum_distance * minimum_distance;
	float best_distance_squared = -1.0f;
	XMFLOAT2 best_position{};
	for (int attempt = 0; attempt < candidate_count; ++attempt)
	{
		const int candidate_index = (start + attempt * stride) % candidate_count;
		const int cell_x = room->TileX + inset + candidate_index % usable_width;
		const int cell_y = room->TileY + inset + candidate_index / usable_width;
		const XMFLOAT2 position = CellCenter(cell_x, cell_y);
		if (!ProceduralMap_IsCircleWalkable(position, clearance_radius))
		{
			continue;
		}
		const float distance_squared = DistanceSquared(position, avoid_position);
		if (distance_squared > best_distance_squared)
		{
			best_distance_squared = distance_squared;
			best_position = position;
		}
		if (distance_squared >= minimum_distance_squared)
		{
			out_position = position;
			return true;
		}
	}

	if (best_distance_squared >= 0.0f)
	{
		out_position = best_position;
		return true;
	}
	return false;
}

namespace ProceduralMapInternal
{
	// 방을 배치하고 길을 연결한 뒤 마지막 전투 방을 고른다.
	bool IsRoomCandidateInsideMap(const RoomCandidate& candidate)
	{
		static constexpr int RoomMapPadding = 1;
		return candidate.X >= RoomMapPadding && candidate.Y >= RoomMapPadding &&
		       candidate.X + candidate.Width <= MapColumns() - RoomMapPadding &&
		       candidate.Y + candidate.Height <= MapRows() - RoomMapPadding;
	}

	bool RoomCandidateOverlapsExistingRoom(const RoomCandidate& candidate, int ignored_room_index = -1)
	{
		static constexpr int RoomCollisionPadding = MapConstants::Generation::CorridorRadius + 1;
		for (const DungeonRoom& room : g_Rooms)
		{
			if (room.Info.Index == ignored_room_index)
			{
				continue;
			}
			const bool separated = candidate.X + candidate.Width + RoomCollisionPadding <= room.Info.TileX ||
			                       room.Info.TileX + room.Info.TileWidth + RoomCollisionPadding <= candidate.X ||
			                       candidate.Y + candidate.Height + RoomCollisionPadding <= room.Info.TileY ||
			                       room.Info.TileY + room.Info.TileHeight + RoomCollisionPadding <= candidate.Y;
			if (!separated)
			{
				return true;
			}
		}
		return false;
	}

	bool IsValidRoomCandidate(const RoomCandidate& candidate, int parent_room_index)
	{
		return IsRoomCandidateInsideMap(candidate) && !RoomCandidateOverlapsExistingRoom(candidate, parent_room_index);
	}

	void ApplyRoomBounds(DungeonRoom& room, const RoomCandidate& candidate)
	{
		room.Info.TileX = candidate.X;
		room.Info.TileY = candidate.Y;
		room.Info.TileWidth = candidate.Width;
		room.Info.TileHeight = candidate.Height;
		room.Info.WorldMin = {
			candidate.X * MapTileSize(),
			candidate.Y * MapTileSize(),
		};
		room.Info.WorldMax = {
			(candidate.X + candidate.Width) * MapTileSize(),
			(candidate.Y + candidate.Height) * MapTileSize(),
		};
		room.Info.Center = {
			(room.Info.WorldMin.x + room.Info.WorldMax.x) * 0.5f,
			(room.Info.WorldMin.y + room.Info.WorldMax.y) * 0.5f,
		};
	}

	void AddRoom(const RoomCandidate& candidate, int grid_x, int grid_y, int parent_room_index)
	{
		DungeonRoom room{};
		room.Info.Index = static_cast<int>(g_Rooms.size());
		ApplyRoomBounds(room, candidate);
		room.GridX = grid_x;
		room.GridY = grid_y;
		room.ParentRoomIndex = parent_room_index;
		g_Rooms.push_back(room);
	}

	void GenerateRooms()
	{
		static constexpr int UpGridY = -1;
		static constexpr int UpGridX = 0;
		static constexpr int LargeEncounterCount = 2;
		static constexpr int NorthArenaCount = LargeEncounterCount + 1;
		// 마지막 라운드는 고정 구조, 그 외 라운드는 전역 난수 엔진으로 구조를 고른다.
		g_Rooms.clear();
		const MapGameData& map_data = GetMapData();
		const bool is_boss_round = g_Round == map_data.GetBossRound();
		if (is_boss_round)
		{
			// 시작 방 아래에 큰 방 두 개와 보스 방을 차례로 둔다.
			const int layout_height =
			    RoomTileHeight() +
			    NorthArenaCount * (MapConstants::Generation::RoomConnectionLength + LargeRoomTileHeight());
			const int layout_top = (MapRows() - layout_height) / 2;
			const RoomCandidate start_bounds = {
				MapColumns() / 2 - RoomTileWidth() / 2,
				layout_top + layout_height - RoomTileHeight(),
				RoomTileWidth(),
				RoomTileHeight(),
			};
			AddRoom(start_bounds, 0, 1, -1);
			for (int arena_index = 0; arena_index < NorthArenaCount; ++arena_index)
			{
				const int parent_index = static_cast<int>(g_Rooms.size()) - 1;
				const RoomCandidate arena_bounds = MakeConnectedRoomCandidate(
				    g_Rooms[parent_index].Info, UpGridX, UpGridY, LargeRoomTileWidth(), LargeRoomTileHeight());
				AddRoom(arena_bounds, UpGridX, -arena_index, parent_index);
				DungeonRoom& arena = g_Rooms.back();
				arena.Info.IsLargeRoom = true;
				arena.Info.IsBossRoom = arena_index == NorthArenaCount - 1;
			}
			return;
		}
		const int target_count = RandomInt(map_data.GetRegularRoomMinCount(), map_data.GetRegularRoomMaxCount());
		AddRoom(MakeCenteredRoomCandidate(RoomTileWidth(), RoomTileHeight()), 0, 0, -1);
		constexpr std::array<std::array<int, 2>, 4> DIRECTIONS = { {
			{ 0, -1 },
			{ 1, 0 },
			{ 0, 1 },
			{ -1, 0 },
		} };
		while (static_cast<int>(g_Rooms.size()) < target_count)
		{
			const bool next_room_is_large = static_cast<int>(g_Rooms.size()) == target_count - 1;
			const int child_width = next_room_is_large ? LargeRoomTileWidth() : RoomTileWidth();
			const int child_height = next_room_is_large ? LargeRoomTileHeight() : RoomTileHeight();
			std::vector<RoomExpansionCandidate> expansion_candidates;
			expansion_candidates.reserve(g_Rooms.size() * DIRECTIONS.size());
			for (int parent_index = 0; parent_index < static_cast<int>(g_Rooms.size()); ++parent_index)
			{
				const DungeonRoom& parent = g_Rooms[parent_index];
				for (const auto& direction : DIRECTIONS)
				{
					const int grid_x = parent.GridX + direction[0];
					const int grid_y = parent.GridY + direction[1];
					const RoomCandidate bounds =
					    MakeConnectedRoomCandidate(parent.Info, direction[0], direction[1], child_width, child_height);
					if (IsInsideRoomGrid(grid_x, grid_y) && FindRoomAtGrid(grid_x, grid_y) < 0 &&
					    IsValidRoomCandidate(bounds, parent_index))
					{
						expansion_candidates.push_back({
						    parent_index,
						    grid_x,
						    grid_y,
						    bounds,
						});
					}
				}
			}
			if (expansion_candidates.empty())
			{
				break;
			}
			const RoomExpansionCandidate& expansion =
			    expansion_candidates[RandomInt(0, static_cast<int>(expansion_candidates.size()) - 1)];
			AddRoom(expansion.Bounds, expansion.GridX, expansion.GridY, expansion.ParentRoomIndex);
			if (next_room_is_large)
			{
				g_Rooms.back().Info.IsLargeRoom = true;
			}
		}
	}

	bool RoomsAreConnected(int room_a, int room_b)
	{
		const auto& neighbors = g_Rooms[room_a].Neighbors;
		return std::find(neighbors.begin(), neighbors.end(), room_b) != neighbors.end();
	}

	void AddConnection(int room_a, int room_b)
	{
		if (room_a == room_b || RoomsAreConnected(room_a, room_b))
		{
			return;
		}
		g_Rooms[room_a].Neighbors.push_back(room_b);
		g_Rooms[room_b].Neighbors.push_back(room_a);
		g_Connections.push_back({ room_a, room_b });
	}

	void BuildRoomConnections()
	{
		// 배치할 때 기록한 부모 방을 기준으로 양방향 연결을 만든다.
		g_Connections.clear();
		for (DungeonRoom& room : g_Rooms)
		{
			room.Neighbors.clear();
		}
		for (int room_index = 0; room_index < static_cast<int>(g_Rooms.size()); ++room_index)
		{
			const DungeonRoom& room = g_Rooms[room_index];
			const int parent_index = room.ParentRoomIndex;
			if (parent_index < 0 || parent_index >= room_index)
			{
				continue;
			}
			AddConnection(parent_index, room_index);
		}
	}

	bool HasAvailableBossRoomNeighbor(const DungeonRoom& room)
	{
		constexpr std::array<std::array<int, 2>, 4> DIRECTIONS = { {
			{ 0, -1 },
			{ 1, 0 },
			{ 0, 1 },
			{ -1, 0 },
		} };
		for (const auto& direction : DIRECTIONS)
		{
			const int grid_x = room.GridX + direction[0];
			const int grid_y = room.GridY + direction[1];
			const RoomCandidate bounds = MakeConnectedRoomCandidate(room.Info, direction[0], direction[1],
			                                                        LargeRoomTileWidth(), LargeRoomTileHeight());
			if (IsInsideRoomGrid(grid_x, grid_y) && FindRoomAtGrid(grid_x, grid_y) < 0 &&
			    IsValidRoomCandidate(bounds, room.Info.Index))
			{
				return true;
			}
		}
		return false;
	}

	void ComputeRoomDepths(bool select_final_encounter)
	{
		// 시작 방부터 BFS로 거리를 매겨 가장 깊은 방을 찾는다.
		for (DungeonRoom& room : g_Rooms)
		{
			room.Info.Depth = -1;
			if (select_final_encounter)
			{
				room.Info.IsBossRoom = false;
			}
		}
		if (g_Rooms.empty())
		{
			return;
		}
		std::queue<int> open;
		g_Rooms[g_StartRoomIndex].Info.Depth = 0;
		open.push(g_StartRoomIndex);
		while (!open.empty())
		{
			const int current_room_index = open.front();
			open.pop();
			const DungeonRoom& current_room = g_Rooms[current_room_index];
			for (int neighbor_index : current_room.Neighbors)
			{
				DungeonRoom& neighbor = g_Rooms[neighbor_index];
				if (neighbor.Info.Depth >= 0)
				{
					continue;
				}
				neighbor.Info.Depth = current_room.Info.Depth + 1;
				open.push(neighbor_index);
			}
		}
		if (!select_final_encounter)
		{
			return;
		}
		int exit_room = g_StartRoomIndex;
		int exit_depth = 0;
		float exit_tie_breaker = -1.0f;
		const bool needs_appended_boss_room = g_Round != GetMapData().GetBossRound();
		for (int room_index = 0; room_index < static_cast<int>(g_Rooms.size()); ++room_index)
		{
			const DungeonRoom& room = g_Rooms[room_index];
			if (room_index == g_StartRoomIndex || room.Info.Depth < 0 || room.Neighbors.size() != 1 ||
			    (needs_appended_boss_room && !HasAvailableBossRoomNeighbor(room)))
			{
				continue;
			}
			const float tie_breaker = Random01();
			if (room.Info.Depth > exit_depth || (room.Info.Depth == exit_depth && tie_breaker > exit_tie_breaker))
			{
				exit_room = room_index;
				exit_depth = room.Info.Depth;
				exit_tie_breaker = tie_breaker;
			}
		}
		if (exit_room == g_StartRoomIndex && g_Rooms.size() > 1)
		{
			for (int room_index = 0; room_index < static_cast<int>(g_Rooms.size()); ++room_index)
			{
				const ProceduralMapRoom& room = g_Rooms[room_index].Info;
				if (room_index != g_StartRoomIndex && room.Depth > exit_depth &&
				    (!needs_appended_boss_room || HasAvailableBossRoomNeighbor(g_Rooms[room_index])))
				{
					exit_room = room_index;
					exit_depth = room.Depth;
				}
			}
		}
		g_FinalEncounterRoomIndex = exit_room;
		if (g_FinalEncounterRoomIndex >= 0)
		{
			DungeonRoom& final_room = g_Rooms[g_FinalEncounterRoomIndex];
			const bool is_immediate_boss_room = g_Round == GetMapData().GetBossRound();
			if (is_immediate_boss_room)
			{
				final_room.Info.IsLargeRoom = true;
			}
			final_room.Info.IsBossRoom = is_immediate_boss_room;
		}
	}

	void AddBossRoom()
	{
		// 마지막 일반 전투 방의 빈 방향에 보스 방을 하나 붙인다.
		g_ExitRoomIndex = g_FinalEncounterRoomIndex;
		if (g_FinalEncounterRoomIndex < 0 || g_FinalEncounterRoomIndex >= static_cast<int>(g_Rooms.size()))
		{
			return;
		}
		constexpr std::array<std::array<int, 2>, 4> DIRECTIONS = { {
			{ 0, -1 },
			{ 1, 0 },
			{ 0, 1 },
			{ -1, 0 },
		} };
		const DungeonRoom& parent = g_Rooms[g_FinalEncounterRoomIndex];
		const int first_direction = RandomInt(0, static_cast<int>(DIRECTIONS.size()) - 1);
		for (int offset = 0; offset < static_cast<int>(DIRECTIONS.size()); ++offset)
		{
			const auto& direction = DIRECTIONS[(first_direction + offset) % static_cast<int>(DIRECTIONS.size())];
			const int grid_x = parent.GridX + direction[0];
			const int grid_y = parent.GridY + direction[1];
			const RoomCandidate bounds = MakeConnectedRoomCandidate(parent.Info, direction[0], direction[1],
			                                                        LargeRoomTileWidth(), LargeRoomTileHeight());
			if (!IsInsideRoomGrid(grid_x, grid_y) || FindRoomAtGrid(grid_x, grid_y) >= 0 ||
			    !IsValidRoomCandidate(bounds, g_FinalEncounterRoomIndex))
			{
				continue;
			}
			AddRoom(bounds, grid_x, grid_y, g_FinalEncounterRoomIndex);
			DungeonRoom& boss_room = g_Rooms.back();
			boss_room.Info.IsLargeRoom = true;
			boss_room.Info.IsBossRoom = true;
			g_FinalEncounterRoomIndex = boss_room.Info.Index;
			g_ExitRoomIndex = boss_room.Info.Index;
			return;
		}
		// 옆자리가 없으면 마지막 일반 방을 보스 방으로 써서 진행이 막히지 않게 한다.
		DungeonRoom& fallback_boss_room = g_Rooms[g_FinalEncounterRoomIndex];
		fallback_boss_room.Info.IsBossRoom = true;
	}
} // namespace ProceduralMapInternal
