#include "procedural_map.h"

#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <queue>
#include <vector>

using namespace DirectX;

namespace
{
	constexpr int MAP_COLUMNS = 240;
	constexpr int MAP_ROWS = 180;
	constexpr int MAP_TILE_COUNT = MAP_COLUMNS * MAP_ROWS;
	constexpr float MAP_TILE_SIZE = 60.0f;
	constexpr int REGULAR_ROOM_MIN_COUNT = 3;
	constexpr int REGULAR_ROOM_COUNT_VARIATION = 3;
	constexpr int BOSS_ROUND_ROOM_COUNT = 2;
	constexpr int BOSS_ROUND = 4;
	constexpr int ROOM_TILE_WIDTH = 28;
	constexpr int ROOM_TILE_HEIGHT = 22;
	constexpr int ROOM_GRID_SPACING_X = 34;
	constexpr int ROOM_GRID_SPACING_Y = 28;
	constexpr int ROOM_GRID_MIN_X = -3;
	constexpr int ROOM_GRID_MAX_X = 3;
	constexpr int ROOM_GRID_MIN_Y = -2;
	constexpr int ROOM_GRID_MAX_Y = 2;
	constexpr int CORRIDOR_RADIUS = 2;
	constexpr float MINIMAP_VIEWPORT_FRACTION = 0.40f;
	constexpr float WORLD_MAP_VIEWPORT_FRACTION = 0.80f;
	constexpr float MINIMAP_MAX_WORLD_SCALE = 1.0f / 20.0f;
	constexpr float WORLD_MAP_MAX_WORLD_SCALE = 1.0f / 10.0f;
	constexpr float MINIMAP_SCREEN_MARGIN = 20.0f;
	constexpr float ENCOUNTER_BARRIER_THICKNESS = 14.0f;
	constexpr float ENCOUNTER_BARRIER_ENDPOINT_EXTENSION = 4.0f;
	constexpr float WALL_TORCH_CHANCE = 0.07f;
	constexpr float PI = 3.14159265358979323846f;

	enum class GroundTile : std::size_t
	{
		TopLeft,
		Top01,
		Top02,
		Top03,
		Top04,
		TopRight,
		Left01,
		Left02,
		Left03,
		Floor01,
		Floor02,
		Floor03,
		Floor04,
		Floor05,
		Floor06,
		Floor07,
		Floor08,
		Floor09,
		Floor10,
		Floor11,
		Floor12,
		Right01,
		Right02,
		Right03,
		BottomLeft,
		Bottom01,
		Bottom02,
		Bottom03,
		Bottom04,
		BottomRight,
		ConcaveDownColumn0,
		ConcaveDownColumn3,
		VoidDeep,
		VoidMottle,
		Count,
	};

	enum class DecorTile : std::size_t
	{
		Torch,
		Candle,
		Coin,
		PotRed,
		PotBlue,
		Bones,
		Skull,
		Chest,
		Count,
	};

	enum class EncounterBarrierStyle : std::size_t
	{
		Horizontal,
		Vertical,
	};

	enum class EncounterBarrierTile : std::size_t
	{
		Horizontal01,
		Horizontal02,
		Vertical01,
		Vertical02,
		Count,
	};

	enum class CellKind : std::uint8_t
	{
		Solid,
		Room,
		Corridor,
	};

	struct MapCell
	{
		CellKind Kind{ CellKind::Solid };
		GroundTile Ground{ GroundTile::VoidDeep };
		int RoomIndex{ -1 };
		float Rotation{ 0.0f };
		float Brightness{ 1.0f };
	};

	struct DungeonRoom
	{
		ProceduralMapRoom Info{};
		int FloorVariant{ 0 };
		int GridX{ 0 };
		int GridY{ 0 };
		int ParentRoomIndex{ -1 };
		std::vector<int> Neighbors;
	};

	struct RoomCandidate
	{
		int X{ 0 };
		int Y{ 0 };
		int Width{ 0 };
		int Height{ 0 };
	};

	struct RoomEdge
	{
		int A{ -1 };
		int B{ -1 };
	};

	struct RoomExpansionCandidate
	{
		int ParentRoomIndex{ -1 };
		int GridX{ 0 };
		int GridY{ 0 };
	};

	struct Decoration
	{
		DecorTile Tile{ DecorTile::Candle };
		int CellX{ 0 };
		int CellY{ 0 };
		SpriteInstance Instance{};
	};

	struct EncounterBarrier
	{
		DirectX::XMFLOAT2 Center{ 0.0f, 0.0f };
		DirectX::XMFLOAT2 VisualCenter{ 0.0f, 0.0f };
		DirectX::XMFLOAT2 Size{ 0.0f, 0.0f };
		EncounterBarrierStyle Style{ EncounterBarrierStyle::Horizontal };
	};

	struct SeededRandom
	{
		explicit SeededRandom(std::uint32_t seed)
			: State(seed == 0 ? 1u : seed)
		{
		}

		std::uint32_t Next();

		int Range(int minimum, int maximum_exclusive)
		{
			if (maximum_exclusive <= minimum)
			{
				return minimum;
			}
			return minimum + static_cast<int>(Next() %
				static_cast<std::uint32_t>(maximum_exclusive - minimum));
		}

		std::uint32_t State{ 1u };
	};

	constexpr std::size_t GROUND_TILE_TYPE_COUNT = static_cast<std::size_t>(GroundTile::Count);
	constexpr std::size_t DECOR_TILE_TYPE_COUNT = static_cast<std::size_t>(DecorTile::Count);
	constexpr std::size_t ENCOUNTER_BARRIER_TILE_TYPE_COUNT =
		static_cast<std::size_t>(EncounterBarrierTile::Count);
	constexpr std::array<const wchar_t*, GROUND_TILE_TYPE_COUNT> GROUND_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/room/top_left.png",
		L"asset/texture/map/dungeon/room/top_01.png",
		L"asset/texture/map/dungeon/room/top_02.png",
		L"asset/texture/map/dungeon/room/top_03.png",
		L"asset/texture/map/dungeon/room/top_04.png",
		L"asset/texture/map/dungeon/room/top_right.png",
		L"asset/texture/map/dungeon/room/left_01.png",
		L"asset/texture/map/dungeon/room/left_02.png",
		L"asset/texture/map/dungeon/room/left_03.png",
		L"asset/texture/map/dungeon/room/floor_01.png",
		L"asset/texture/map/dungeon/room/floor_02.png",
		L"asset/texture/map/dungeon/room/floor_03.png",
		L"asset/texture/map/dungeon/room/floor_04.png",
		L"asset/texture/map/dungeon/room/floor_05.png",
		L"asset/texture/map/dungeon/room/floor_06.png",
		L"asset/texture/map/dungeon/room/floor_07.png",
		L"asset/texture/map/dungeon/room/floor_08.png",
		L"asset/texture/map/dungeon/room/floor_09.png",
		L"asset/texture/map/dungeon/room/floor_10.png",
		L"asset/texture/map/dungeon/room/floor_11.png",
		L"asset/texture/map/dungeon/room/floor_12.png",
		L"asset/texture/map/dungeon/room/right_01.png",
		L"asset/texture/map/dungeon/room/right_02.png",
		L"asset/texture/map/dungeon/room/right_03.png",
		L"asset/texture/map/dungeon/room/bottom_left.png",
		L"asset/texture/map/dungeon/room/bottom_01.png",
		L"asset/texture/map/dungeon/room/bottom_02.png",
		L"asset/texture/map/dungeon/room/bottom_03.png",
		L"asset/texture/map/dungeon/room/bottom_04.png",
		L"asset/texture/map/dungeon/room/bottom_right.png",
		L"asset/texture/map/dungeon/room/concave_down_column_0.png",
		L"asset/texture/map/dungeon/room/concave_down_column_3.png",
		L"asset/texture/map/dungeon/room/void_deep.png",
		L"asset/texture/map/dungeon/room/void_mottle.png",
	};

	constexpr std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT> DECOR_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/decor/torch.png",
		L"asset/texture/map/dungeon/decor/candle.png",
		L"asset/texture/map/dungeon/decor/coin.png",
		L"asset/texture/map/dungeon/decor/pot_red.png",
		L"asset/texture/map/dungeon/decor/pot_blue.png",
		L"asset/texture/map/dungeon/decor/bones.png",
		L"asset/texture/map/dungeon/decor/skull.png",
		L"asset/texture/map/dungeon/decor/chest.png",
	};

	constexpr std::array<const wchar_t*, ENCOUNTER_BARRIER_TILE_TYPE_COUNT>
		ENCOUNTER_BARRIER_TEXTURE_PATHS = {
			L"asset/texture/map/dungeon/barrier/horizontal_01.png",
			L"asset/texture/map/dungeon/barrier/horizontal_02.png",
			L"asset/texture/map/dungeon/barrier/vertical_01.png",
			L"asset/texture/map/dungeon/barrier/vertical_02.png",
	};

	std::array<int, GROUND_TILE_TYPE_COUNT> g_GroundTextureIds{};
	std::array<int, DECOR_TILE_TYPE_COUNT> g_DecorTextureIds{};
	std::array<int, ENCOUNTER_BARRIER_TILE_TYPE_COUNT> g_EncounterBarrierTextureIds{};
	std::array<std::vector<SpriteInstance>, GROUND_TILE_TYPE_COUNT> g_VisibleGroundBatches{};
	std::array<std::vector<SpriteInstance>, DECOR_TILE_TYPE_COUNT> g_VisibleDecorBatches{};
	std::array<std::vector<SpriteInstance>, GROUND_TILE_TYPE_COUNT> g_OverviewGroundBatches{};
	std::vector<MapCell> g_Cells;
	std::vector<DungeonRoom> g_Rooms;
	std::vector<RoomEdge> g_Connections;
	std::vector<Decoration> g_Decorations;
	std::vector<EncounterBarrier> g_EncounterBarriers;
	std::uint32_t g_MapSeed = 0;
	int g_Round = 1;
	int g_StartRoomIndex = 0;
	int g_FinalEncounterRoomIndex = 0;
	int g_ExitRoomIndex = 0;
	int g_LockedEncounterRoom = -1;
	bool g_Initialized = false;

	std::uint32_t Hash32(std::uint32_t value)
	{
		value ^= value >> 16;
		value *= 0x7feb352du;
		value ^= value >> 15;
		value *= 0x846ca68bu;
		value ^= value >> 16;
		return value;
	}

	std::uint32_t SeededRandom::Next()
	{
		State += 0x9e3779b9u;
		return Hash32(State);
	}

	float HashToUnitFloat(std::uint32_t value)
	{
		return static_cast<float>(Hash32(value) & 0x00ffffffu) /
			static_cast<float>(0x01000000u);
	}

	std::uint32_t CellHash(int x, int y, std::uint32_t salt)
	{
		const std::uint32_t x_hash = Hash32(static_cast<std::uint32_t>(x) + 0x68bc21ebu);
		const std::uint32_t y_hash = Hash32(static_cast<std::uint32_t>(y) + 0x02e5be93u);
		return Hash32(g_MapSeed ^ x_hash ^ (y_hash << 1) ^ salt);
	}

	std::uint32_t MakeInitialSeed()
	{
		const auto ticks = static_cast<std::uint64_t>(
			std::chrono::high_resolution_clock::now().time_since_epoch().count());
		const std::uint32_t seed = Hash32(
			static_cast<std::uint32_t>(ticks) ^ static_cast<std::uint32_t>(ticks >> 32));
		return seed == 0 ? 1u : seed;
	}

	std::size_t ToIndex(GroundTile tile)
	{
		return static_cast<std::size_t>(tile);
	}

	std::size_t ToIndex(DecorTile tile)
	{
		return static_cast<std::size_t>(tile);
	}

	GroundTile PickTileVariant(GroundTile first, int count, float detail)
	{
		const int variant = std::clamp(
			static_cast<int>(detail * static_cast<float>(count)), 0, count - 1);
		return static_cast<GroundTile>(ToIndex(first) + static_cast<std::size_t>(variant));
	}

	int ResolveGroundTexture(GroundTile tile)
	{
		return g_GroundTextureIds[ToIndex(tile)];
	}

	bool IsDirectionalPath(GroundTile tile)
	{
		(void)tile;
		return false;
	}

	int CellIndex(int x, int y)
	{
		return y * MAP_COLUMNS + x;
	}

	bool IsInsideMap(int x, int y)
	{
		return x >= 0 && x < MAP_COLUMNS && y >= 0 && y < MAP_ROWS;
	}

	bool IsWalkableCell(int x, int y)
	{
		return IsInsideMap(x, y) && g_Cells[CellIndex(x, y)].Kind != CellKind::Solid;
	}

	bool IsCorridorCell(int x, int y)
	{
		return IsInsideMap(x, y) && g_Cells[CellIndex(x, y)].Kind == CellKind::Corridor;
	}

	constexpr std::uint8_t CARDINAL_NORTH = 0x01;
	constexpr std::uint8_t CARDINAL_EAST = 0x02;
	constexpr std::uint8_t CARDINAL_SOUTH = 0x04;
	constexpr std::uint8_t CARDINAL_WEST = 0x08;
	constexpr std::uint8_t DIAGONAL_NORTH_EAST = 0x01;
	constexpr std::uint8_t DIAGONAL_SOUTH_EAST = 0x02;
	constexpr std::uint8_t DIAGONAL_SOUTH_WEST = 0x04;
	constexpr std::uint8_t DIAGONAL_NORTH_WEST = 0x08;

	int CountNeighborBits(std::uint8_t mask)
	{
		int count = 0;
		for (std::uint8_t bit = 1; bit <= 8; bit <<= 1)
		{
			count += (mask & bit) != 0 ? 1 : 0;
		}
		return count;
	}

	std::uint8_t GetCardinalWalkableMask(int x, int y)
	{
		std::uint8_t mask = 0;
		mask |= IsWalkableCell(x, y - 1) ? CARDINAL_NORTH : 0;
		mask |= IsWalkableCell(x + 1, y) ? CARDINAL_EAST : 0;
		mask |= IsWalkableCell(x, y + 1) ? CARDINAL_SOUTH : 0;
		mask |= IsWalkableCell(x - 1, y) ? CARDINAL_WEST : 0;
		return mask;
	}

	std::uint8_t GetDiagonalWalkableMask(int x, int y)
	{
		std::uint8_t mask = 0;
		mask |= IsWalkableCell(x + 1, y - 1) ? DIAGONAL_NORTH_EAST : 0;
		mask |= IsWalkableCell(x + 1, y + 1) ? DIAGONAL_SOUTH_EAST : 0;
		mask |= IsWalkableCell(x - 1, y + 1) ? DIAGONAL_SOUTH_WEST : 0;
		mask |= IsWalkableCell(x - 1, y - 1) ? DIAGONAL_NORTH_WEST : 0;
		return mask;
	}

	bool IsVoidGround(GroundTile tile)
	{
		return tile == GroundTile::VoidDeep || tile == GroundTile::VoidMottle;
	}

	bool IsTopWallGround(GroundTile tile)
	{
		return tile >= GroundTile::Top01 && tile <= GroundTile::Top04;
	}

	void AssignSolidGround(MapCell& cell, int x, int y, float detail)
	{
		const std::uint8_t cardinal = GetCardinalWalkableMask(x, y);
		const int cardinal_count = CountNeighborBits(cardinal);
		cell.Rotation = 0.0f;

		if (cardinal_count == 0)
		{
			const std::uint8_t diagonal = GetDiagonalWalkableMask(x, y);
			if (CountNeighborBits(diagonal) == 1)
			{
				switch (diagonal)
				{
				case DIAGONAL_SOUTH_EAST: cell.Ground = GroundTile::TopLeft; break;
				case DIAGONAL_SOUTH_WEST: cell.Ground = GroundTile::TopRight; break;
				case DIAGONAL_NORTH_EAST: cell.Ground = GroundTile::BottomLeft; break;
				default: cell.Ground = GroundTile::BottomRight; break;
				}
				return;
			}
			cell.Ground = detail < 0.06f ? GroundTile::VoidMottle : GroundTile::VoidDeep;
			return;
		}

		if (cardinal_count == 1)
		{
			switch (cardinal)
			{
			case CARDINAL_SOUTH:
				cell.Ground = PickTileVariant(GroundTile::Top01, 4, detail);
				break;
			case CARDINAL_EAST:
				cell.Ground = PickTileVariant(GroundTile::Left01, 3, detail);
				break;
			case CARDINAL_WEST:
				cell.Ground = PickTileVariant(GroundTile::Right01, 3, detail);
				break;
			default:
				cell.Ground = PickTileVariant(GroundTile::Bottom01, 4, detail);
				break;
			}
			return;
		}

		if (cardinal_count == 2)
		{
			if (cardinal == (CARDINAL_EAST | CARDINAL_WEST) ||
				cardinal == (CARDINAL_NORTH | CARDINAL_SOUTH))
			{
				cell.Ground = cardinal == (CARDINAL_NORTH | CARDINAL_SOUTH) ?
					PickTileVariant(GroundTile::Left01, 3, detail) :
					PickTileVariant(GroundTile::Top01, 4, detail);
				return;
			}
			switch (cardinal)
			{
			case CARDINAL_EAST | CARDINAL_SOUTH:
			case CARDINAL_SOUTH | CARDINAL_WEST:
				cell.Ground = PickTileVariant(GroundTile::Top01, 4, detail);
				break;
			case CARDINAL_NORTH | CARDINAL_EAST:
				cell.Ground = GroundTile::ConcaveDownColumn3;
				break;
			default:
				cell.Ground = GroundTile::ConcaveDownColumn0;
				break;
			}
			return;
		}

		if (cardinal_count == 3)
		{
			switch (cardinal)
			{
			case CARDINAL_NORTH | CARDINAL_EAST | CARDINAL_SOUTH:
				cell.Ground = PickTileVariant(GroundTile::Left01, 3, detail);
				break;
			case CARDINAL_NORTH | CARDINAL_SOUTH | CARDINAL_WEST:
				cell.Ground = PickTileVariant(GroundTile::Right01, 3, detail);
				break;
			case CARDINAL_EAST | CARDINAL_SOUTH | CARDINAL_WEST:
				cell.Ground = PickTileVariant(GroundTile::Top01, 4, detail);
				break;
			default:
				cell.Ground = PickTileVariant(GroundTile::Bottom01, 4, detail);
				break;
			}
			return;
		}

		cell.Ground = PickTileVariant(GroundTile::Floor06, 2, detail);
	}

	XMFLOAT2 CellCenter(int x, int y)
	{
		return {
			(static_cast<float>(x) + 0.5f) * MAP_TILE_SIZE,
			(static_cast<float>(y) + 0.5f) * MAP_TILE_SIZE,
		};
	}

	void AddHorizontalEncounterBarrier(
		int first_x,
		int last_x,
		int corridor_y,
		float collision_world_y)
	{
		const float left = first_x * MAP_TILE_SIZE - ENCOUNTER_BARRIER_ENDPOINT_EXTENSION;
		const float right = (last_x + 1) * MAP_TILE_SIZE + ENCOUNTER_BARRIER_ENDPOINT_EXTENSION;
		g_EncounterBarriers.push_back({
			{ (left + right) * 0.5f, collision_world_y },
			{ (left + right) * 0.5f, (static_cast<float>(corridor_y) + 0.5f) * MAP_TILE_SIZE },
			{ right - left, ENCOUNTER_BARRIER_THICKNESS },
			EncounterBarrierStyle::Horizontal,
		});
	}

	void AddVerticalEncounterBarrier(
		int first_y,
		int last_y,
		int corridor_x,
		float collision_world_x)
	{
		const float top = first_y * MAP_TILE_SIZE - ENCOUNTER_BARRIER_ENDPOINT_EXTENSION;
		const float bottom = (last_y + 1) * MAP_TILE_SIZE + ENCOUNTER_BARRIER_ENDPOINT_EXTENSION;
		g_EncounterBarriers.push_back({
			{ collision_world_x, (top + bottom) * 0.5f },
			{ (static_cast<float>(corridor_x) + 0.5f) * MAP_TILE_SIZE, (top + bottom) * 0.5f },
			{ ENCOUNTER_BARRIER_THICKNESS, bottom - top },
			EncounterBarrierStyle::Vertical,
		});
	}

	void BuildEncounterBarriers(int room_index)
	{
		g_EncounterBarriers.clear();
		if (room_index < 0 || room_index >= static_cast<int>(g_Rooms.size()))
		{
			return;
		}

		const ProceduralMapRoom& room = g_Rooms[room_index].Info;
		auto scan_horizontal = [&](int corridor_y, float barrier_world_y)
		{
			int run_start = -1;
			const int first_x = room.TileX;
			const int last_x = room.TileX + room.TileWidth - 1;
			for (int x = first_x; x <= last_x + 1; ++x)
			{
				const bool is_doorway = x <= last_x && IsCorridorCell(x, corridor_y);
				if (is_doorway && run_start < 0)
				{
					run_start = x;
				}
				else if (!is_doorway && run_start >= 0)
				{
					AddHorizontalEncounterBarrier(run_start, x - 1, corridor_y, barrier_world_y);
					run_start = -1;
				}
			}
		};
		auto scan_vertical = [&](int corridor_x, float barrier_world_x)
		{
			int run_start = -1;
			const int first_y = room.TileY;
			const int last_y = room.TileY + room.TileHeight - 1;
			for (int y = first_y; y <= last_y + 1; ++y)
			{
				const bool is_doorway = y <= last_y && IsCorridorCell(corridor_x, y);
				if (is_doorway && run_start < 0)
				{
					run_start = y;
				}
				else if (!is_doorway && run_start >= 0)
				{
					AddVerticalEncounterBarrier(run_start, y - 1, corridor_x, barrier_world_x);
					run_start = -1;
				}
			}
		};

		scan_horizontal(room.TileY - 1, room.WorldMin.y);
		scan_horizontal(room.TileY + room.TileHeight, room.WorldMax.y);
		scan_vertical(room.TileX - 1, room.WorldMin.x);
		scan_vertical(room.TileX + room.TileWidth, room.WorldMax.x);
	}

	void ResetTextureIds()
	{
		g_GroundTextureIds.fill(TEXTURE_INVALID_ID);
		g_DecorTextureIds.fill(TEXTURE_INVALID_ID);
		g_EncounterBarrierTextureIds.fill(TEXTURE_INVALID_ID);
	}

	template <std::size_t Count>
	bool LoadTextureSet(
		const std::array<const wchar_t*, Count>& paths,
		std::array<int, Count>& texture_ids)
	{
		for (std::size_t i = 0; i < Count; ++i)
		{
			texture_ids[i] = Texture_Load(paths[i], false);
			if (texture_ids[i] == TEXTURE_INVALID_ID)
			{
				return false;
			}
		}
		return true;
	}

	template <std::size_t Count>
	void ReleaseTextureSet(std::array<int, Count>& texture_ids)
	{
		for (int& texture_id : texture_ids)
		{
			if (texture_id != TEXTURE_INVALID_ID)
			{
				Texture_Release(texture_id);
				texture_id = TEXTURE_INVALID_ID;
			}
		}
	}

	bool IsInsideRoomGrid(int grid_x, int grid_y)
	{
		return grid_x >= ROOM_GRID_MIN_X && grid_x <= ROOM_GRID_MAX_X &&
			grid_y >= ROOM_GRID_MIN_Y && grid_y <= ROOM_GRID_MAX_Y;
	}

	int FindRoomAtGrid(int grid_x, int grid_y)
	{
		for (int room_index = 0; room_index < static_cast<int>(g_Rooms.size()); ++room_index)
		{
			const DungeonRoom& room = g_Rooms[room_index];
			if (room.GridX == grid_x && room.GridY == grid_y)
			{
				return room_index;
			}
		}
		return -1;
	}

	RoomCandidate MakeRoomCandidate(int grid_x, int grid_y)
	{
		const int center_x = MAP_COLUMNS / 2 + grid_x * ROOM_GRID_SPACING_X;
		const int center_y = MAP_ROWS / 2 + grid_y * ROOM_GRID_SPACING_Y;
		return {
			center_x - ROOM_TILE_WIDTH / 2,
			center_y - ROOM_TILE_HEIGHT / 2,
			ROOM_TILE_WIDTH,
			ROOM_TILE_HEIGHT,
		};
	}

	void AddRoom(
		const RoomCandidate& candidate,
		int grid_x,
		int grid_y,
		int parent_room_index)
	{
		DungeonRoom room{};
		room.Info.Index = static_cast<int>(g_Rooms.size());
		room.Info.TileX = candidate.X;
		room.Info.TileY = candidate.Y;
		room.Info.TileWidth = candidate.Width;
		room.Info.TileHeight = candidate.Height;
		room.Info.WorldMin = {
			candidate.X * MAP_TILE_SIZE,
			candidate.Y * MAP_TILE_SIZE,
		};
		room.Info.WorldMax = {
			(candidate.X + candidate.Width) * MAP_TILE_SIZE,
			(candidate.Y + candidate.Height) * MAP_TILE_SIZE,
		};
		room.Info.Center = {
			(room.Info.WorldMin.x + room.Info.WorldMax.x) * 0.5f,
			(room.Info.WorldMin.y + room.Info.WorldMax.y) * 0.5f,
		};
		room.GridX = grid_x;
		room.GridY = grid_y;
		room.ParentRoomIndex = parent_room_index;
		room.FloorVariant = static_cast<int>(Hash32(
			g_MapSeed ^ static_cast<std::uint32_t>(room.Info.Index) * 0x9e3779b9u) % 3u);
		g_Rooms.push_back(room);
	}

	void GenerateRooms()
	{
		g_Rooms.clear();
		SeededRandom random(Hash32(g_MapSeed ^ 0x51ed270bu));
		const int target_count = g_Round == BOSS_ROUND ? BOSS_ROUND_ROOM_COUNT :
			REGULAR_ROOM_MIN_COUNT + random.Range(0, REGULAR_ROOM_COUNT_VARIATION);

		AddRoom(MakeRoomCandidate(0, 0), 0, 0, -1);
		g_Rooms.front().FloorVariant = 0;

		constexpr std::array<std::array<int, 2>, 4> DIRECTIONS = { {
			{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
		} };
		while (static_cast<int>(g_Rooms.size()) < target_count)
		{
			std::vector<RoomExpansionCandidate> expansion_candidates;
			expansion_candidates.reserve(g_Rooms.size() * DIRECTIONS.size());
			for (int parent_index = 0;
				parent_index < static_cast<int>(g_Rooms.size());
				++parent_index)
			{
				const DungeonRoom& parent = g_Rooms[parent_index];
				for (const auto& direction : DIRECTIONS)
				{
					const int grid_x = parent.GridX + direction[0];
					const int grid_y = parent.GridY + direction[1];
					if (IsInsideRoomGrid(grid_x, grid_y) &&
						FindRoomAtGrid(grid_x, grid_y) < 0)
					{
						expansion_candidates.push_back({ parent_index, grid_x, grid_y });
					}
				}
			}

			if (expansion_candidates.empty())
			{
				break;
			}

			const RoomExpansionCandidate& expansion = expansion_candidates[
				random.Range(0, static_cast<int>(expansion_candidates.size()))];
			AddRoom(
				MakeRoomCandidate(expansion.GridX, expansion.GridY),
				expansion.GridX,
				expansion.GridY,
				expansion.ParentRoomIndex);
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
			const DungeonRoom& parent = g_Rooms[parent_index];
			const int grid_distance =
				std::abs(room.GridX - parent.GridX) + std::abs(room.GridY - parent.GridY);
			if (grid_distance == 1)
			{
				AddConnection(parent_index, room_index);
			}
		}
	}

	bool HasAvailableRoomNeighbor(const DungeonRoom& room)
	{
		constexpr std::array<std::array<int, 2>, 4> DIRECTIONS = { {
			{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
		} };
		for (const auto& direction : DIRECTIONS)
		{
			const int grid_x = room.GridX + direction[0];
			const int grid_y = room.GridY + direction[1];
			if (IsInsideRoomGrid(grid_x, grid_y) && FindRoomAtGrid(grid_x, grid_y) < 0)
			{
				return true;
			}
		}
		return false;
	}

	void ComputeRoomDepths(bool select_final_encounter)
	{
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
		std::uint32_t exit_tie_breaker = 0;
		for (int room_index = 0; room_index < static_cast<int>(g_Rooms.size()); ++room_index)
		{
			const DungeonRoom& room = g_Rooms[room_index];
			if (room_index == g_StartRoomIndex || room.Info.Depth < 0 ||
				room.Neighbors.size() != 1 || !HasAvailableRoomNeighbor(room))
			{
				continue;
			}
			const std::uint32_t tie_breaker = Hash32(
				g_MapSeed ^ static_cast<std::uint32_t>(room_index + 1) * 0x632be59bu);
			if (room.Info.Depth > exit_depth ||
				(room.Info.Depth == exit_depth && tie_breaker > exit_tie_breaker))
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
					HasAvailableRoomNeighbor(g_Rooms[room_index]))
				{
					exit_room = room_index;
					exit_depth = room.Depth;
				}
			}
		}
		g_FinalEncounterRoomIndex = exit_room;
		if (g_Round == BOSS_ROUND && g_FinalEncounterRoomIndex >= 0)
		{
			g_Rooms[g_FinalEncounterRoomIndex].Info.IsBossRoom = true;
			g_Rooms[g_FinalEncounterRoomIndex].FloorVariant = 1;
		}
	}

	void AddPortalRoom()
	{
		g_ExitRoomIndex = g_FinalEncounterRoomIndex;
		if (g_FinalEncounterRoomIndex < 0 ||
			g_FinalEncounterRoomIndex >= static_cast<int>(g_Rooms.size()))
		{
			return;
		}

		constexpr std::array<std::array<int, 2>, 4> DIRECTIONS = { {
			{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
		} };
		const DungeonRoom& parent = g_Rooms[g_FinalEncounterRoomIndex];
		const int first_direction = static_cast<int>(Hash32(
			g_MapSeed ^ static_cast<std::uint32_t>(g_FinalEncounterRoomIndex + 1) *
				0x27d4eb2du) % DIRECTIONS.size());
		for (int offset = 0; offset < static_cast<int>(DIRECTIONS.size()); ++offset)
		{
			const auto& direction = DIRECTIONS[
				(first_direction + offset) % static_cast<int>(DIRECTIONS.size())];
			const int grid_x = parent.GridX + direction[0];
			const int grid_y = parent.GridY + direction[1];
			if (!IsInsideRoomGrid(grid_x, grid_y) || FindRoomAtGrid(grid_x, grid_y) >= 0)
			{
				continue;
			}

			AddRoom(
				MakeRoomCandidate(grid_x, grid_y),
				grid_x,
				grid_y,
				g_FinalEncounterRoomIndex);
			DungeonRoom& portal_room = g_Rooms.back();
			portal_room.Info.IsPortalRoom = true;
			portal_room.FloorVariant = 1;
			g_ExitRoomIndex = portal_room.Info.Index;
			return;
		}
	}

	void CarveRoom(const DungeonRoom& room)
	{
		for (int y = room.Info.TileY; y < room.Info.TileY + room.Info.TileHeight; ++y)
		{
			for (int x = room.Info.TileX; x < room.Info.TileX + room.Info.TileWidth; ++x)
			{
				MapCell& cell = g_Cells[CellIndex(x, y)];
				cell.Kind = CellKind::Room;
				cell.RoomIndex = room.Info.Index;
			}
		}
	}

	void CarveCorridorCell(int x, int y)
	{
		if (!IsInsideMap(x, y))
		{
			return;
		}
		MapCell& cell = g_Cells[CellIndex(x, y)];
		if (cell.Kind == CellKind::Solid)
		{
			cell.Kind = CellKind::Corridor;
			cell.RoomIndex = -1;
		}
	}

	void CarveHorizontal(int x0, int x1, int y)
	{
		if (x0 > x1)
		{
			std::swap(x0, x1);
		}
		for (int x = x0; x <= x1; ++x)
		{
			for (int offset = -CORRIDOR_RADIUS; offset <= CORRIDOR_RADIUS; ++offset)
			{
				CarveCorridorCell(x, y + offset);
			}
		}
	}

	void CarveVertical(int x, int y0, int y1)
	{
		if (y0 > y1)
		{
			std::swap(y0, y1);
		}
		for (int y = y0; y <= y1; ++y)
		{
			for (int offset = -CORRIDOR_RADIUS; offset <= CORRIDOR_RADIUS; ++offset)
			{
				CarveCorridorCell(x + offset, y);
			}
		}
	}

	void CarveDungeon()
	{
		g_Cells.assign(MAP_TILE_COUNT, MapCell{});
		for (const DungeonRoom& room : g_Rooms)
		{
			CarveRoom(room);
		}

		for (const RoomEdge& edge : g_Connections)
		{
			const ProceduralMapRoom& room_a = g_Rooms[edge.A].Info;
			const ProceduralMapRoom& room_b = g_Rooms[edge.B].Info;
			const int ax = room_a.TileX + room_a.TileWidth / 2;
			const int ay = room_a.TileY + room_a.TileHeight / 2;
			const int bx = room_b.TileX + room_b.TileWidth / 2;
			const int by = room_b.TileY + room_b.TileHeight / 2;
			if (ay == by)
			{
				CarveHorizontal(ax, bx, ay);
			}
			else if (ax == bx)
			{
				CarveVertical(ax, ay, by);
			}
		}
	}

	GroundTile ChooseRoomGround(const DungeonRoom& room, int x, int y, float detail)
	{
		const ProceduralMapRoom& info = room.Info;
		const bool is_left = x == info.TileX;
		const bool is_right = x == info.TileX + info.TileWidth - 1;
		const bool is_top = y == info.TileY;
		const bool is_bottom = y == info.TileY + info.TileHeight - 1;

		if (is_top)
		{
			if (is_left)
			{
				return GroundTile::Floor01;
			}
			if (is_right)
			{
				return GroundTile::Floor04;
			}
			return PickTileVariant(GroundTile::Floor02, 2, detail);
		}
		if (is_bottom)
		{
			if (is_left)
			{
				return GroundTile::Floor09;
			}
			if (is_right)
			{
				return GroundTile::Floor12;
			}
			return PickTileVariant(GroundTile::Floor10, 2, detail);
		}
		if (is_left)
		{
			return GroundTile::Floor05;
		}
		if (is_right)
		{
			return GroundTile::Floor08;
		}
		return PickTileVariant(GroundTile::Floor06, 2, detail);
	}

	void AssignGroundMaterials()
	{
		for (int y = 0; y < MAP_ROWS; ++y)
		{
			for (int x = 0; x < MAP_COLUMNS; ++x)
			{
				MapCell& cell = g_Cells[CellIndex(x, y)];
				const float detail = HashToUnitFloat(CellHash(x, y, 0x7f4a7c15u));
				cell.Brightness = 0.98f +
					HashToUnitFloat(CellHash(x, y, 0xed5ad4bbu)) * 0.02f;

				if (cell.Kind == CellKind::Solid)
				{
					AssignSolidGround(cell, x, y, detail);
					continue;
				}
				if (cell.Kind == CellKind::Room)
				{
					cell.Ground = ChooseRoomGround(g_Rooms[cell.RoomIndex], x, y, detail);
				}
				else if (cell.Kind == CellKind::Corridor)
				{
					cell.Ground = PickTileVariant(GroundTile::Floor06, 2, detail);
				}
				if (IsDirectionalPath(cell.Ground))
				{
					const int horizontal =
						(IsWalkableCell(x - 1, y) ? 1 : 0) +
						(IsWalkableCell(x + 1, y) ? 1 : 0);
					const int vertical =
						(IsWalkableCell(x, y - 1) ? 1 : 0) +
						(IsWalkableCell(x, y + 1) ? 1 : 0);
					cell.Rotation = horizontal > vertical ? PI * 0.5f : 0.0f;
				}
				else
				{
					cell.Rotation = 0.0f;
				}
			}
		}
	}

	void AddDecoration(
		DecorTile tile,
		int cell_x,
		int cell_y,
		float scale,
		bool allow_rotation,
		bool allow_jitter = true)
	{
		const int texture_id = g_DecorTextureIds[ToIndex(tile)];
		const float texture_width = static_cast<float>(Texture_GetWidth(texture_id));
		const float texture_height = static_cast<float>(Texture_GetHeight(texture_id));
		if (texture_width <= 0.0f || texture_height <= 0.0f)
		{
			return;
		}

		const float jitter_x = allow_jitter ?
			(HashToUnitFloat(CellHash(cell_x, cell_y, 0x6d2b79f5u)) - 0.5f) * MAP_TILE_SIZE * 0.28f : 0.0f;
		const float jitter_y = allow_jitter ?
			(HashToUnitFloat(CellHash(cell_x, cell_y, 0x9e3779b9u)) - 0.5f) * MAP_TILE_SIZE * 0.24f : 0.0f;
		const float brightness = 0.92f +
			HashToUnitFloat(CellHash(cell_x, cell_y, 0x27d4eb2fu)) * 0.08f;
		const float rotation = allow_rotation &&
			HashToUnitFloat(CellHash(cell_x, cell_y, 0x165667b1u)) < 0.5f ? PI : 0.0f;

		g_Decorations.push_back({
			tile,
			cell_x,
			cell_y,
			{
				{
					(static_cast<float>(cell_x) + 0.5f) * MAP_TILE_SIZE + jitter_x,
					(static_cast<float>(cell_y) + 0.5f) * MAP_TILE_SIZE + jitter_y,
				},
				{ MAP_TILE_SIZE * scale, MAP_TILE_SIZE * scale },
				rotation,
				{ brightness, brightness, brightness, 1.0f },
			},
		});
	}

	void GenerateDecorations()
	{
		g_Decorations.clear();
		g_Decorations.reserve(1200);

		int boss_room_index = -1;
		for (const DungeonRoom& room : g_Rooms)
		{
			if (room.Info.IsBossRoom)
			{
				boss_room_index = room.Info.Index;
				break;
			}
		}

		for (int y = 0; y < MAP_ROWS; ++y)
		{
			for (int x = 0; x < MAP_COLUMNS; ++x)
			{
				const MapCell& cell = g_Cells[CellIndex(x, y)];
				const float roll = HashToUnitFloat(CellHash(x, y, 0x94d049bbu));
				const float scale = 0.86f +
					HashToUnitFloat(CellHash(x, y, 0xed5ad4bbu)) * 0.20f;

				if (cell.Kind == CellKind::Solid && IsTopWallGround(cell.Ground))
				{
					if (roll < WALL_TORCH_CHANCE)
					{
						AddDecoration(DecorTile::Torch, x, y, 1.0f, false, false);
						g_Decorations.back().Instance.Color = { 1.08f, 1.0f, 0.90f, 1.0f };
					}
					continue;
				}

				if (cell.Kind == CellKind::Corridor)
				{
					if (roll < 0.006f)
					{
						AddDecoration(DecorTile::Bones, x, y, scale * 0.62f, true);
					}
					else if (roll < 0.010f)
					{
						AddDecoration(DecorTile::Candle, x, y, scale * 0.68f, false);
					}
					continue;
				}

				if (cell.Kind != CellKind::Room)
				{
					continue;
				}

				const DungeonRoom& room = g_Rooms[cell.RoomIndex];
				const int center_x = room.Info.TileX + room.Info.TileWidth / 2;
				const int center_y = room.Info.TileY + room.Info.TileHeight / 2;
				const int distance_from_center = std::abs(x - center_x) + std::abs(y - center_y);
				if (room.Info.Index == boss_room_index && x == center_x && y == center_y)
				{
					AddDecoration(DecorTile::Chest, x, y, 0.95f, false, false);
					g_Decorations.back().Instance.Position = room.Info.Center;
					continue;
				}
				if (distance_from_center <= 5)
				{
					continue;
				}

				if (room.FloorVariant == 0)
				{
					if (roll < 0.004f)
					{
						AddDecoration(DecorTile::PotRed, x, y, scale * 0.74f, false);
					}
					else if (roll < 0.008f)
					{
						AddDecoration(DecorTile::Coin, x, y, scale * 0.64f, false);
					}
				}
				else if (room.FloorVariant == 2 && roll < 0.010f)
				{
					AddDecoration(roll < 0.005f ? DecorTile::Skull : DecorTile::Bones,
						x, y, scale * 0.78f, false);
				}
				else if (room.FloorVariant == 1 && roll < 0.008f)
				{
					AddDecoration(DecorTile::PotBlue, x, y, scale * 0.70f, true);
				}
			}
		}
	}

	void GenerateMap(std::uint32_t seed)
	{
		g_LockedEncounterRoom = -1;
		g_EncounterBarriers.clear();
		g_MapSeed = seed == 0 ? 1u : seed;
		g_StartRoomIndex = 0;
		g_FinalEncounterRoomIndex = 0;
		g_ExitRoomIndex = 0;
		GenerateRooms();
		BuildRoomConnections();
		ComputeRoomDepths(true);
		if (g_Round == BOSS_ROUND)
		{
			// Boss rounds use the boss arena itself as the exit room. The portal
			// appears at its center only after the boss encounter is cleared.
			g_ExitRoomIndex = g_FinalEncounterRoomIndex;
		}
		else
		{
			AddPortalRoom();
		}
		BuildRoomConnections();
		ComputeRoomDepths(false);
		CarveDungeon();
		AssignGroundMaterials();
		GenerateDecorations();
	}

	bool CircleIntersectsSolidCell(const XMFLOAT2& position, float radius, int cell_x, int cell_y)
	{
		const float left = cell_x * MAP_TILE_SIZE;
		const float top = cell_y * MAP_TILE_SIZE;
		const float right = left + MAP_TILE_SIZE;
		const float bottom = top + MAP_TILE_SIZE;
		const float closest_x = std::clamp(position.x, left, right);
		const float closest_y = std::clamp(position.y, top, bottom);
		const float dx = position.x - closest_x;
		const float dy = position.y - closest_y;
		return dx * dx + dy * dy < radius * radius;
	}

	bool CircleIntersectsEncounterBarrier(
		const XMFLOAT2& position,
		float radius,
		const EncounterBarrier& barrier)
	{
		const float half_width = barrier.Size.x * 0.5f;
		const float half_height = barrier.Size.y * 0.5f;
		const float closest_x = std::clamp(
			position.x, barrier.Center.x - half_width, barrier.Center.x + half_width);
		const float closest_y = std::clamp(
			position.y, barrier.Center.y - half_height, barrier.Center.y + half_height);
		const float dx = position.x - closest_x;
		const float dy = position.y - closest_y;
		return dx * dx + dy * dy < radius * radius;
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
}

bool ProceduralMap_Initialize(std::uint32_t seed)
{
	if (g_Initialized)
	{
		ProceduralMap_Regenerate(seed);
		return true;
	}

	ResetTextureIds();
	if (!LoadTextureSet(GROUND_TEXTURE_PATHS, g_GroundTextureIds) ||
		!LoadTextureSet(DECOR_TEXTURE_PATHS, g_DecorTextureIds) ||
		!LoadTextureSet(ENCOUNTER_BARRIER_TEXTURE_PATHS, g_EncounterBarrierTextureIds))
	{
		ReleaseTextureSet(g_EncounterBarrierTextureIds);
		ReleaseTextureSet(g_DecorTextureIds);
		ReleaseTextureSet(g_GroundTextureIds);
		return false;
	}

	for (auto& batch : g_VisibleGroundBatches)
	{
		batch.reserve(768);
	}
	for (auto& batch : g_VisibleDecorBatches)
	{
		batch.reserve(128);
	}
	for (auto& batch : g_OverviewGroundBatches)
	{
		batch.reserve(4096);
	}
	g_EncounterBarriers.reserve(16);

	g_Round = 1;
	g_Initialized = true;
	GenerateMap(seed == 0 ? MakeInitialSeed() : seed);
	return true;
}

void ProceduralMap_Finalize()
{
	if (!g_Initialized)
	{
		ResetTextureIds();
		g_EncounterBarriers.clear();
		g_LockedEncounterRoom = -1;
		g_MapSeed = 0;
		g_Round = 1;
		g_FinalEncounterRoomIndex = 0;
		g_ExitRoomIndex = 0;
		return;
	}

	for (auto& batch : g_VisibleGroundBatches)
	{
		batch.clear();
	}
	for (auto& batch : g_VisibleDecorBatches)
	{
		batch.clear();
	}
	for (auto& batch : g_OverviewGroundBatches)
	{
		batch.clear();
	}
	g_Decorations.clear();
	g_EncounterBarriers.clear();
	g_Connections.clear();
	g_Rooms.clear();
	g_Cells.clear();
	ReleaseTextureSet(g_EncounterBarrierTextureIds);
	ReleaseTextureSet(g_DecorTextureIds);
	ReleaseTextureSet(g_GroundTextureIds);
	g_MapSeed = 0;
	g_StartRoomIndex = 0;
	g_FinalEncounterRoomIndex = 0;
	g_ExitRoomIndex = 0;
	g_LockedEncounterRoom = -1;
	g_Round = 1;
	g_Initialized = false;
}

void ProceduralMap_Regenerate(std::uint32_t seed)
{
	if (!g_Initialized)
	{
		return;
	}
	const std::uint32_t next_seed = seed == 0 ?
		Hash32(g_MapSeed + 0x9e3779b9u) : seed;
	GenerateMap(next_seed == 0 ? 1u : next_seed);
}

void ProceduralMap_GenerateRound(int round, std::uint32_t seed)
{
	if (!g_Initialized)
	{
		return;
	}
	round = std::clamp(round, 1, BOSS_ROUND);

	const std::uint32_t round_salt =
		static_cast<std::uint32_t>(round) * 0xc2b2ae35u;
	const std::uint32_t next_seed = seed == 0 ?
		Hash32(g_MapSeed + 0x9e3779b9u + round_salt) : seed;
	g_Round = round;
	GenerateMap(next_seed == 0 ? 1u : next_seed);
}

void ProceduralMap_Draw(const XMFLOAT2& camera_position, const XMFLOAT2& viewport_size)
{
	if (!g_Initialized)
	{
		return;
	}

	for (auto& batch : g_VisibleGroundBatches)
	{
		batch.clear();
	}
	for (auto& batch : g_VisibleDecorBatches)
	{
		batch.clear();
	}

	const float half_width = std::max(viewport_size.x, 1.0f) * 0.5f;
	const float half_height = std::max(viewport_size.y, 1.0f) * 0.5f;
	const int minimum_x = std::clamp(
		static_cast<int>(std::floor((camera_position.x - half_width) / MAP_TILE_SIZE)) - 1,
		0, MAP_COLUMNS - 1);
	const int maximum_x = std::clamp(
		static_cast<int>(std::floor((camera_position.x + half_width) / MAP_TILE_SIZE)) + 1,
		0, MAP_COLUMNS - 1);
	const int minimum_y = std::clamp(
		static_cast<int>(std::floor((camera_position.y - half_height) / MAP_TILE_SIZE)) - 1,
		0, MAP_ROWS - 1);
	const int maximum_y = std::clamp(
		static_cast<int>(std::floor((camera_position.y + half_height) / MAP_TILE_SIZE)) + 1,
		0, MAP_ROWS - 1);

	for (int y = minimum_y; y <= maximum_y; ++y)
	{
		for (int x = minimum_x; x <= maximum_x; ++x)
		{
			const MapCell& cell = g_Cells[CellIndex(x, y)];
			g_VisibleGroundBatches[ToIndex(cell.Ground)].push_back({
				CellCenter(x, y),
				{ MAP_TILE_SIZE, MAP_TILE_SIZE },
				cell.Rotation,
				{ cell.Brightness, cell.Brightness, cell.Brightness, 1.0f },
			});
		}
	}

	const float decor_margin = MAP_TILE_SIZE * 2.0f;
	const float visible_left = camera_position.x - half_width - decor_margin;
	const float visible_right = camera_position.x + half_width + decor_margin;
	const float visible_top = camera_position.y - half_height - decor_margin;
	const float visible_bottom = camera_position.y + half_height + decor_margin;
	for (const Decoration& decoration : g_Decorations)
	{
		const XMFLOAT2& position = decoration.Instance.Position;
		if (position.x < visible_left || position.x > visible_right ||
			position.y < visible_top || position.y > visible_bottom)
		{
			continue;
		}
		g_VisibleDecorBatches[ToIndex(decoration.Tile)].push_back(decoration.Instance);
	}

	for (std::size_t i = 0; i < GROUND_TILE_TYPE_COUNT; ++i)
	{
		const auto& batch = g_VisibleGroundBatches[i];
		if (!batch.empty())
		{
			SpriteInstanced_Draw(
				ResolveGroundTexture(static_cast<GroundTile>(i)),
				batch.data(),
				static_cast<int>(batch.size()));
		}
	}
	for (std::size_t i = 0; i < DECOR_TILE_TYPE_COUNT; ++i)
	{
		const auto& batch = g_VisibleDecorBatches[i];
		if (!batch.empty())
		{
			SpriteInstanced_Draw(g_DecorTextureIds[i], batch.data(), static_cast<int>(batch.size()));
		}
	}
}

void ProceduralMap_DrawFadeOverlay(const XMFLOAT2& viewport_size, float alpha)
{
	if (!g_Initialized || alpha <= 0.0f)
	{
		return;
	}
	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(XMMatrixIdentity());
	Sprite_DrawSized(
		ResolveGroundTexture(GroundTile::VoidDeep),
		viewport_size.x * 0.5f,
		viewport_size.y * 0.5f,
		viewport_size.x,
		viewport_size.y,
		{ 0.0f, 0.0f, 0.0f, std::clamp(alpha, 0.0f, 1.0f) });
}

void ProceduralMap_DrawEncounterLock()
{
	if (!g_Initialized || g_EncounterBarriers.empty())
	{
		return;
	}

	static std::array<std::vector<SpriteInstance>, ENCOUNTER_BARRIER_TILE_TYPE_COUNT>
		barrier_batches;
	for (auto& batch : barrier_batches)
	{
		batch.clear();
		batch.reserve(g_EncounterBarriers.size() * 5);
	}
	for (const EncounterBarrier& barrier : g_EncounterBarriers)
	{
		const bool horizontal = barrier.Style == EncounterBarrierStyle::Horizontal;
		const float visual_length = horizontal ? barrier.Size.x : barrier.Size.y;
		const int segment_count = std::max(1, static_cast<int>(std::lround(
			(visual_length - ENCOUNTER_BARRIER_ENDPOINT_EXTENSION * 2.0f) / MAP_TILE_SIZE)));
		const float first_offset = -0.5f * static_cast<float>(segment_count - 1) * MAP_TILE_SIZE;
		const std::size_t style_offset = static_cast<std::size_t>(barrier.Style) * 2;
		for (int segment = 0; segment < segment_count; ++segment)
		{
			const float offset = first_offset + static_cast<float>(segment) * MAP_TILE_SIZE;
			const std::size_t tile_index = style_offset + static_cast<std::size_t>(segment & 1);
			barrier_batches[tile_index].push_back({
				{
					barrier.VisualCenter.x + (horizontal ? offset : 0.0f),
					barrier.VisualCenter.y + (horizontal ? 0.0f : offset),
				},
				{ MAP_TILE_SIZE, MAP_TILE_SIZE },
				0.0f,
				{ 1.0f, 1.0f, 1.0f, 1.0f },
			});
		}
	}

	for (std::size_t i = 0; i < ENCOUNTER_BARRIER_TILE_TYPE_COUNT; ++i)
	{
		const auto& batch = barrier_batches[i];
		if (!batch.empty())
		{
			SpriteInstanced_Draw(
				g_EncounterBarrierTextureIds[i],
				batch.data(),
				static_cast<int>(batch.size()));
		}
	}
}

ProceduralMapOverviewLayout ProceduralMap_DrawOverview(
	const XMFLOAT2& camera_position,
	const XMFLOAT2& viewport_size,
	bool expanded)
{
	ProceduralMapOverviewLayout layout{};
	if (!g_Initialized)
	{
		return layout;
	}

	const XMFLOAT2 world_size = ProceduralMap_GetWorldSize();
	const float viewport_fraction = expanded ?
		WORLD_MAP_VIEWPORT_FRACTION : MINIMAP_VIEWPORT_FRACTION;
	const float maximum_scale = expanded ?
		WORLD_MAP_MAX_WORLD_SCALE : MINIMAP_MAX_WORLD_SCALE;
	layout.WorldScale = std::min({
		maximum_scale,
		std::max(viewport_size.x, 1.0f) * viewport_fraction / std::max(world_size.x, 1.0f),
		std::max(viewport_size.y, 1.0f) * viewport_fraction / std::max(world_size.y, 1.0f),
	});
	layout.Size = {
		world_size.x * layout.WorldScale,
		world_size.y * layout.WorldScale,
	};
	layout.IsExpanded = expanded;
	const float frame_padding = expanded ? 14.0f : 8.0f;
	if (expanded)
	{
		layout.Origin = {
			(viewport_size.x - layout.Size.x) * 0.5f,
			(viewport_size.y - layout.Size.y) * 0.5f,
		};
	}
	else
	{
		layout.Origin = {
			viewport_size.x - layout.Size.x - MINIMAP_SCREEN_MARGIN - frame_padding,
			MINIMAP_SCREEN_MARGIN + frame_padding,
		};
	}

	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(XMMatrixIdentity());
	const XMFLOAT2 panel_center = {
		layout.Origin.x + layout.Size.x * 0.5f,
		layout.Origin.y + layout.Size.y * 0.5f,
	};
	if (expanded)
	{
		Sprite_DrawSized(
			ResolveGroundTexture(GroundTile::VoidDeep),
			viewport_size.x * 0.5f,
			viewport_size.y * 0.5f,
			viewport_size.x,
			viewport_size.y,
			{ 0.58f, 0.64f, 0.70f, 0.84f });
	}
	Sprite_DrawSized(
		ResolveGroundTexture(GroundTile::Floor01),
		panel_center.x,
		panel_center.y,
		layout.Size.x + frame_padding * 2.0f,
		layout.Size.y + frame_padding * 2.0f,
		{ 0.22f, 0.25f, 0.20f, 0.96f });
	Sprite_DrawSized(
		ResolveGroundTexture(GroundTile::VoidDeep),
		panel_center.x,
		panel_center.y,
		layout.Size.x,
		layout.Size.y,
		{ 0.70f, 0.76f, 0.82f, 0.97f });

	for (auto& batch : g_OverviewGroundBatches)
	{
		batch.clear();
	}
	const float overview_tile_size = MAP_TILE_SIZE * layout.WorldScale;
	for (int y = 0; y < MAP_ROWS; ++y)
	{
		for (int x = 0; x < MAP_COLUMNS; ++x)
		{
			const MapCell& cell = g_Cells[CellIndex(x, y)];
			if (cell.Kind == CellKind::Solid && IsVoidGround(cell.Ground))
			{
				continue;
			}
			const float brightness = cell.Kind == CellKind::Solid ?
				0.82f : std::min(cell.Brightness + 0.08f, 1.0f);
			g_OverviewGroundBatches[ToIndex(cell.Ground)].push_back({
				{
					layout.Origin.x + (static_cast<float>(x) + 0.5f) * overview_tile_size,
					layout.Origin.y + (static_cast<float>(y) + 0.5f) * overview_tile_size,
				},
				{ overview_tile_size, overview_tile_size },
				cell.Rotation,
				{ brightness, brightness, brightness, 0.98f },
			});
		}
	}
	for (std::size_t i = 0; i < GROUND_TILE_TYPE_COUNT; ++i)
	{
		const auto& batch = g_OverviewGroundBatches[i];
		if (!batch.empty())
		{
			SpriteInstanced_Draw(
				ResolveGroundTexture(static_cast<GroundTile>(i)),
				batch.data(),
				static_cast<int>(batch.size()));
		}
	}

	if (!g_EncounterBarriers.empty())
	{
		static std::vector<SpriteInstance> overview_barriers;
		overview_barriers.clear();
		overview_barriers.reserve(g_EncounterBarriers.size());
		const float minimum_thickness = expanded ? 3.0f : 2.0f;
		for (const EncounterBarrier& barrier : g_EncounterBarriers)
		{
			overview_barriers.push_back({
				{
					layout.Origin.x + barrier.Center.x * layout.WorldScale,
					layout.Origin.y + barrier.Center.y * layout.WorldScale,
				},
				{
					std::max(barrier.Size.x * layout.WorldScale, minimum_thickness),
					std::max(barrier.Size.y * layout.WorldScale, minimum_thickness),
				},
				0.0f,
				{ 1.0f, 0.34f, 0.10f, 1.0f },
			});
		}
		SpriteInstanced_Draw(
			ResolveGroundTexture(GroundTile::Floor06),
			overview_barriers.data(),
			static_cast<int>(overview_barriers.size()));
	}

	const float camera_border_thickness = expanded ? 3.0f : 2.0f;
	const XMFLOAT2 camera_center = {
		layout.Origin.x + camera_position.x * layout.WorldScale,
		layout.Origin.y + camera_position.y * layout.WorldScale,
	};
	const XMFLOAT2 camera_size = {
		viewport_size.x * layout.WorldScale,
		viewport_size.y * layout.WorldScale,
	};
	const std::array<SpriteInstance, 4> camera_border = { {
		{
			{ camera_center.x, camera_center.y - camera_size.y * 0.5f },
			{ camera_size.x, camera_border_thickness },
			0.0f,
			{ 0.55f, 0.92f, 1.0f, 0.95f },
		},
		{
			{ camera_center.x, camera_center.y + camera_size.y * 0.5f },
			{ camera_size.x, camera_border_thickness },
			0.0f,
			{ 0.55f, 0.92f, 1.0f, 0.95f },
		},
		{
			{ camera_center.x - camera_size.x * 0.5f, camera_center.y },
			{ camera_border_thickness, camera_size.y },
			0.0f,
			{ 0.55f, 0.92f, 1.0f, 0.95f },
		},
		{
			{ camera_center.x + camera_size.x * 0.5f, camera_center.y },
			{ camera_border_thickness, camera_size.y },
			0.0f,
			{ 0.55f, 0.92f, 1.0f, 0.95f },
		},
	} };
	SpriteInstanced_Draw(
		ResolveGroundTexture(GroundTile::Floor12),
		camera_border.data(),
		static_cast<int>(camera_border.size()));

	if (expanded)
	{
		const ProceduralMapRoom* start_room = ProceduralMap_GetRoom(g_StartRoomIndex);
		const ProceduralMapRoom* exit_room = ProceduralMap_GetRoom(g_ExitRoomIndex);
		if (start_room)
		{
			const SpriteInstance marker = {
				{
					layout.Origin.x + start_room->Center.x * layout.WorldScale,
					layout.Origin.y + start_room->Center.y * layout.WorldScale,
				},
				{ 18.0f, 18.0f },
				0.0f,
				{ 0.62f, 0.92f, 1.0f, 1.0f },
			};
			SpriteInstanced_Draw(
				g_DecorTextureIds[ToIndex(DecorTile::Coin)],
				&marker,
				1);
		}
		if (exit_room)
		{
			const SpriteInstance marker = {
				{
					layout.Origin.x + exit_room->Center.x * layout.WorldScale,
					layout.Origin.y + exit_room->Center.y * layout.WorldScale,
				},
				{ 24.0f, 24.0f },
				0.0f,
				{ 1.0f, 0.48f, 0.32f, 1.0f },
			};
			SpriteInstanced_Draw(
				g_DecorTextureIds[ToIndex(DecorTile::Chest)],
				&marker,
				1);
		}
	}

	return layout;
}

std::uint32_t ProceduralMap_GetSeed()
{
	return g_MapSeed;
}

int ProceduralMap_GetRound()
{
	return g_Round;
}

bool ProceduralMap_IsBossRound()
{
	return g_Round == BOSS_ROUND;
}

XMFLOAT2 ProceduralMap_GetWorldSize()
{
	return {
		MAP_COLUMNS * MAP_TILE_SIZE,
		MAP_ROWS * MAP_TILE_SIZE,
	};
}

XMFLOAT2 ProceduralMap_GetPlayerSpawnPosition()
{
	if (g_Rooms.empty() || g_StartRoomIndex < 0 ||
		g_StartRoomIndex >= static_cast<int>(g_Rooms.size()))
	{
		return { MAP_TILE_SIZE, MAP_TILE_SIZE };
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

XMFLOAT2 ProceduralMap_ClampCameraPosition(
	const XMFLOAT2& desired_position,
	const XMFLOAT2& viewport_size)
{
	const XMFLOAT2 world_size = ProceduralMap_GetWorldSize();
	const float half_width = std::max(viewport_size.x, 1.0f) * 0.5f;
	const float half_height = std::max(viewport_size.y, 1.0f) * 0.5f;
	XMFLOAT2 result = desired_position;
	result.x = world_size.x <= viewport_size.x ? world_size.x * 0.5f :
		std::clamp(result.x, half_width, world_size.x - half_width);
	result.y = world_size.y <= viewport_size.y ? world_size.y * 0.5f :
		std::clamp(result.y, half_height, world_size.y - half_height);
	return result;
}

bool ProceduralMap_IsCircleWalkable(const XMFLOAT2& position, float radius)
{
	if (!g_Initialized || g_Cells.empty())
	{
		return false;
	}
	radius = std::max(radius, 0.0f);
	const XMFLOAT2 world_size = ProceduralMap_GetWorldSize();
	if (position.x - radius < 0.0f || position.y - radius < 0.0f ||
		position.x + radius > world_size.x || position.y + radius > world_size.y)
	{
		return false;
	}

	const int minimum_x = std::clamp(
		static_cast<int>(std::floor((position.x - radius) / MAP_TILE_SIZE)), 0, MAP_COLUMNS - 1);
	const int maximum_x = std::clamp(
		static_cast<int>(std::floor((position.x + radius) / MAP_TILE_SIZE)), 0, MAP_COLUMNS - 1);
	const int minimum_y = std::clamp(
		static_cast<int>(std::floor((position.y - radius) / MAP_TILE_SIZE)), 0, MAP_ROWS - 1);
	const int maximum_y = std::clamp(
		static_cast<int>(std::floor((position.y + radius) / MAP_TILE_SIZE)), 0, MAP_ROWS - 1);

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

bool ProceduralMap_IsSegmentWalkable(
	const XMFLOAT2& start,
	const XMFLOAT2& end,
	float radius)
{
	const float dx = end.x - start.x;
	const float dy = end.y - start.y;
	const float distance = std::sqrt(dx * dx + dy * dy);
	const int steps = std::max(1, static_cast<int>(std::ceil(distance / 12.0f)));
	for (int step = 0; step <= steps; ++step)
	{
		const float amount = static_cast<float>(step) / static_cast<float>(steps);
		const XMFLOAT2 position = {
			start.x + dx * amount,
			start.y + dy * amount,
		};
		if (!ProceduralMap_IsCircleWalkable(position, radius) ||
			!IsEncounterBarrierClear(position, radius))
		{
			return false;
		}
	}
	return true;
}

XMFLOAT2 ProceduralMap_MoveCircle(
	const XMFLOAT2& position,
	const XMFLOAT2& movement,
	float radius)
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
		if (ProceduralMap_IsCircleWalkable(candidate, radius))
		{
			result.x = candidate.x;
		}
		candidate = { result.x, result.y + step.y };
		if (ProceduralMap_IsCircleWalkable(candidate, radius))
		{
			result.y = candidate.y;
		}
	}
	return result;
}

XMFLOAT2 ProceduralMap_MoveActorCircle(
	const XMFLOAT2& position,
	const XMFLOAT2& movement,
	float radius)
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
		if (ProceduralMap_IsCircleWalkable(candidate, radius) &&
			IsEncounterBarrierClear(candidate, radius))
		{
			result.x = candidate.x;
		}
		candidate = { result.x, result.y + step.y };
		if (ProceduralMap_IsCircleWalkable(candidate, radius) &&
			IsEncounterBarrierClear(candidate, radius))
		{
			result.y = candidate.y;
		}
	}
	return result;
}

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
	const int cell_x = static_cast<int>(std::floor(world_position.x / MAP_TILE_SIZE));
	const int cell_y = static_cast<int>(std::floor(world_position.y / MAP_TILE_SIZE));
	if (!IsInsideMap(cell_x, cell_y))
	{
		return -1;
	}
	const MapCell& cell = g_Cells[CellIndex(cell_x, cell_y)];
	return cell.Kind == CellKind::Room ? cell.RoomIndex : -1;
}

const ProceduralMapRoom* ProceduralMap_GetRoom(int room_index)
{
	if (room_index < 0 || room_index >= static_cast<int>(g_Rooms.size()))
	{
		return nullptr;
	}
	return &g_Rooms[room_index].Info;
}

bool ProceduralMap_TryGetRoomSpawnPosition(
	int room_index,
	int sequence,
	const XMFLOAT2& avoid_position,
	float minimum_distance,
	float clearance_radius,
	XMFLOAT2& out_position)
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
	const std::uint32_t sequence_hash = Hash32(
		g_MapSeed ^ static_cast<std::uint32_t>(room_index + 1) * 0x9e3779b9u ^
		static_cast<std::uint32_t>(sequence + 1) * 0x85ebca6bu);
	const int start = static_cast<int>(sequence_hash % static_cast<std::uint32_t>(candidate_count));
	int stride = static_cast<int>((Hash32(sequence_hash ^ 0xc2b2ae35u) | 1u) %
		static_cast<std::uint32_t>(candidate_count));
	stride = std::max(stride, 1);
	while (std::gcd(stride, candidate_count) != 1)
	{
		stride += 2;
		if (stride >= candidate_count)
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
		const float dx = position.x - avoid_position.x;
		const float dy = position.y - avoid_position.y;
		const float distance_squared = dx * dx + dy * dy;
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
