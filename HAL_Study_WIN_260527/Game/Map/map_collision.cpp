#include "procedural_map_internal.h"

#include "math_utils.h"

using namespace DirectX;
using namespace ProceduralMapInternal;

// 캐릭터 이동과 총알 경로가 지나갈 수 있는지 검사한다.
bool ProceduralMap_IsCircleWalkable(const XMFLOAT2& position, float radius)
{
	if (!g_Initialized || g_Cells.empty())
	{
		return false;
	}
	radius = std::max(radius, 0.0f);
	const XMFLOAT2 world_size = ProceduralMap_GetWorldSize();
	if (position.x - radius < 0.0f || position.y - radius < 0.0f || position.x + radius > world_size.x ||
	    position.y + radius > world_size.y)
	{
		return false;
	}

	const int minimum_x =
	    std::clamp(static_cast<int>(std::floor((position.x - radius) / MapTileSize())), 0, MapColumns() - 1);
	const int maximum_x =
	    std::clamp(static_cast<int>(std::floor((position.x + radius) / MapTileSize())), 0, MapColumns() - 1);
	const int minimum_y =
	    std::clamp(static_cast<int>(std::floor((position.y - radius) / MapTileSize())), 0, MapRows() - 1);
	const int maximum_y =
	    std::clamp(static_cast<int>(std::floor((position.y + radius) / MapTileSize())), 0, MapRows() - 1);

	for (int y = minimum_y; y <= maximum_y; ++y)
	{
		for (int x = minimum_x; x <= maximum_x; ++x)
		{
			if (!IsWalkableCell(x, y) && CircleIntersectsSolidCell(position, radius, x, y))
			{
				return false;
			}
		}
	}
	return true;
}

bool ProceduralMap_IsSegmentWalkable(const XMFLOAT2& start, const XMFLOAT2& end, float radius)
{
	const float distance = Distance(start, end);
	const int steps = std::max(1, static_cast<int>(std::ceil(distance / 12.0f)));
	for (int step = 0; step <= steps; ++step)
	{
		const float amount = static_cast<float>(step) / static_cast<float>(steps);
		const XMFLOAT2 position = Lerp(start, end, amount);
		if (!ProceduralMap_IsCircleWalkable(position, radius) || !IsEncounterBarrierClear(position, radius))
		{
			return false;
		}
	}
	return true;
}

XMFLOAT2 ProceduralMap_TraceWalkableSegment(const XMFLOAT2& start, const XMFLOAT2& direction, float maximum_distance,
                                            float radius, float trace_step)
{
	XMFLOAT2 traced_position = start;
	float travelled = 0.0f;
	maximum_distance = std::max(maximum_distance, 0.0f);
	trace_step = std::max(trace_step, 0.0001f);
	while (travelled < maximum_distance)
	{
		const float step = std::min(trace_step, maximum_distance - travelled);
		const XMFLOAT2 next_position{
			traced_position.x + direction.x * step,
			traced_position.y + direction.y * step,
		};
		if (!ProceduralMap_IsSegmentWalkable(traced_position, next_position, radius))
		{
			break;
		}
		traced_position = next_position;
		travelled += step;
	}
	return traced_position;
}

XMFLOAT2 ProceduralMap_MoveActorCircle(const XMFLOAT2& position, const XMFLOAT2& movement, float radius)
{
	XMFLOAT2 result = position;
	const float longest_axis = std::max(std::abs(movement.x), std::abs(movement.y));
	const int steps = std::max(1, static_cast<int>(std::ceil(longest_axis / 15.0f)));
	const XMFLOAT2 step = {
		movement.x / static_cast<float>(steps),
		movement.y / static_cast<float>(steps),
	};

	for (int i = 0; i < steps; ++i)
	{
		XMFLOAT2 candidate = { result.x + step.x, result.y };
		if (ProceduralMap_IsCircleWalkable(candidate, radius) && IsEncounterBarrierClear(candidate, radius))
		{
			result.x = candidate.x;
		}
		candidate = { result.x, result.y + step.y };
		if (ProceduralMap_IsCircleWalkable(candidate, radius) && IsEncounterBarrierClear(candidate, radius))
		{
			result.y = candidate.y;
		}
	}
	return result;
}

namespace ProceduralMapInternal
{
	// 맵 충돌 검사에서만 쓰는 세부 함수들.
	bool CircleIntersectsSolidCell(const XMFLOAT2& position, float radius, int cell_x, int cell_y)
	{
		const float left = cell_x * MapTileSize();
		const float top = cell_y * MapTileSize();
		const float right = left + MapTileSize();
		const float bottom = top + MapTileSize();
		const float closest_x = std::clamp(position.x, left, right);
		const float closest_y = std::clamp(position.y, top, bottom);
		return DistanceSquared(position, { closest_x, closest_y }) < radius * radius;
	}

	bool CircleIntersectsEncounterBarrier(const XMFLOAT2& position, float radius, const EncounterBarrier& barrier)
	{
		const float half_width = barrier.Size.x * 0.5f;
		const float half_height = barrier.Size.y * 0.5f;
		const float closest_x = std::clamp(position.x, barrier.Center.x - half_width, barrier.Center.x + half_width);
		const float closest_y = std::clamp(position.y, barrier.Center.y - half_height, barrier.Center.y + half_height);
		return DistanceSquared(position, { closest_x, closest_y }) < radius * radius;
	}

	bool IsEncounterBarrierClear(const XMFLOAT2& position, float radius)
	{
		for (const EncounterBarrier& barrier : g_EncounterBarriers)
		{
			if (CircleIntersectsEncounterBarrier(position, radius, barrier))
			{
				return false;
			}
		}
		return true;
	}
} // namespace ProceduralMapInternal

// 전투가 끝날 때까지 현재 방의 출입구를 잠근다.
bool ProceduralMap_LockEncounterRoom(int room_index)
{
	if (!g_Initialized || room_index < 0 || room_index >= static_cast<int>(g_Rooms.size()))
	{
		return false;
	}
	if (g_LockedEncounterRoom == room_index && !g_EncounterBarriers.empty())
	{
		return true;
	}

	g_LockedEncounterRoom = room_index;
	BuildEncounterBarriers(room_index);
	if (g_EncounterBarriers.empty())
	{
		g_LockedEncounterRoom = -1;
		return false;
	}
	return true;
}

void ProceduralMap_ClearEncounterLock(int room_index)
{
	if (room_index >= 0 && g_LockedEncounterRoom != room_index)
	{
		return;
	}
	g_LockedEncounterRoom = -1;
	g_EncounterBarriers.clear();
}

int ProceduralMap_GetLockedEncounterRoom()
{
	return g_LockedEncounterRoom;
}
