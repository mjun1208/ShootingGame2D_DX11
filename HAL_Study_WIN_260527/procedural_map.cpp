#include "procedural_map.h"

#include "game_data_manager.h"
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
#include <string>
#include <vector>

using namespace DirectX;

namespace
{
	constexpr int ROOM_GRID_MIN_X = -2;
	constexpr int ROOM_GRID_MAX_X = 2;
	constexpr int ROOM_GRID_MIN_Y = -2;
	constexpr int ROOM_GRID_MAX_Y = 2;
	constexpr int CORRIDOR_RADIUS = 2;
	constexpr int ROOM_CONNECTION_LENGTH = 6;
	constexpr int ROOM_COLLISION_PADDING = CORRIDOR_RADIUS + 1;
	constexpr int ROOM_MAP_PADDING = 1;
	constexpr float MINIMAP_VIEWPORT_FRACTION = 0.40f;
	constexpr float WORLD_MAP_VIEWPORT_FRACTION = 0.80f;
	constexpr float MINIMAP_MAX_WORLD_SCALE = 1.0f / 20.0f;
	constexpr float WORLD_MAP_MAX_WORLD_SCALE = 1.0f / 10.0f;
	constexpr float MINIMAP_SCREEN_MARGIN = 20.0f;
	constexpr float ENCOUNTER_BARRIER_THICKNESS = 14.0f;
	constexpr float ENCOUNTER_BARRIER_ENDPOINT_EXTENSION = 4.0f;
	constexpr float WALL_DECOR_CHANCE = 0.07f;
	constexpr float FLOOR_DECOR_CHANCE = 0.01f;
	constexpr float FOREST_FLOOR_DECOR_CHANCE = 0.035f;
	constexpr float TORCH_FRAME_TIME = 0.12f;
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
		RoomCandidate Bounds{};
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
	constexpr std::size_t MAP_THEME_COUNT = 3;
	constexpr std::array<const wchar_t*, MAP_THEME_COUNT> GROUND_TEXTURE_DIRECTORIES = {
		L"asset/texture/map/dungeon/themes/forest/room/",
		L"asset/texture/map/dungeon/themes/crypt/room/",
		L"asset/texture/map/dungeon/room/",
	};
	constexpr std::array<const wchar_t*, GROUND_TILE_TYPE_COUNT> GROUND_TEXTURE_FILENAMES = {
		L"top_left.png",
		L"top_01.png",
		L"top_02.png",
		L"top_03.png",
		L"top_04.png",
		L"top_right.png",
		L"left_01.png",
		L"left_02.png",
		L"left_03.png",
		L"floor_01.png",
		L"floor_02.png",
		L"floor_03.png",
		L"floor_04.png",
		L"floor_05.png",
		L"floor_06.png",
		L"floor_07.png",
		L"floor_08.png",
		L"floor_09.png",
		L"floor_10.png",
		L"floor_11.png",
		L"floor_12.png",
		L"right_01.png",
		L"right_02.png",
		L"right_03.png",
		L"bottom_left.png",
		L"bottom_01.png",
		L"bottom_02.png",
		L"bottom_03.png",
		L"bottom_04.png",
		L"bottom_right.png",
		L"concave_down_column_0.png",
		L"concave_down_column_3.png",
		L"void_deep.png",
		L"void_mottle.png",
	};

	constexpr std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT> FOREST_DECOR_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/themes/forest/decor/torch.png",
		L"asset/texture/map/dungeon/themes/forest/decor/candle.png",
		L"asset/texture/map/dungeon/themes/forest/decor/coin.png",
		L"asset/texture/map/dungeon/themes/forest/decor/pot_red.png",
		L"asset/texture/map/dungeon/themes/forest/decor/pot_blue.png",
		L"asset/texture/map/dungeon/themes/forest/decor/bones.png",
		L"asset/texture/map/dungeon/themes/forest/decor/skull.png",
		L"asset/texture/map/dungeon/themes/forest/decor/chest.png",
	};
	constexpr std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT> CRYPT_DECOR_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/themes/crypt/decor/torch.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/candle.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/coin.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/pot_red.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/pot_blue.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/bones.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/skull.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/chest.png",
	};
	constexpr std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT> CURRENT_DECOR_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/torch/torch_1.png",
		L"asset/texture/map/dungeon/decor/candle.png",
		L"asset/texture/map/dungeon/decor/coin.png",
		L"asset/texture/map/dungeon/decor/pot_red.png",
		L"asset/texture/map/dungeon/decor/pot_blue.png",
		L"asset/texture/map/dungeon/decor/bones.png",
		L"asset/texture/map/dungeon/decor/skull.png",
		L"asset/texture/map/dungeon/decor/chest.png",
	};

	constexpr std::array<const wchar_t*, 4> TORCH_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/torch/torch_1.png",
		L"asset/texture/map/dungeon/torch/torch_2.png",
		L"asset/texture/map/dungeon/torch/torch_3.png",
		L"asset/texture/map/dungeon/torch/torch_4.png",
	};

	constexpr std::array<const wchar_t*, ENCOUNTER_BARRIER_TILE_TYPE_COUNT>
		ENCOUNTER_BARRIER_TEXTURE_PATHS = {
			L"asset/texture/map/dungeon/barrier/horizontal_01.png",
			L"asset/texture/map/dungeon/barrier/horizontal_02.png",
			L"asset/texture/map/dungeon/barrier/vertical_01.png",
			L"asset/texture/map/dungeon/barrier/vertical_02.png",
	};

	constexpr std::array<std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT>, MAP_THEME_COUNT>
		DECOR_TEXTURE_PATHS = {
		FOREST_DECOR_TEXTURE_PATHS,
		CRYPT_DECOR_TEXTURE_PATHS,
		CURRENT_DECOR_TEXTURE_PATHS,
	};

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
	std::uint32_t g_MapSeed = 0;
	int g_Round = 1;
	int g_StartRoomIndex = 0;
	int g_FinalEncounterRoomIndex = 0;
	int g_ExitRoomIndex = 0;
	int g_LockedEncounterRoom = -1;
	float g_TorchAnimationElapsed = 0.0f;
	bool g_Initialized = false;

	const MapGameData& GetMapData()
	{
		return GameDataManager::GetInstance().GetMapGameData();
	}

	MapTheme ActiveMapTheme()
	{
		return GetMapData().GetRoundEncounter(g_Round).Theme;
	}

	std::size_t ActiveMapThemeIndex()
	{
		switch (ActiveMapTheme())
		{
		case MapTheme::Forest: return 0;
		case MapTheme::CryptDungeon: return 1;
		default: return 2;
		}
	}

	int MapColumns() { return GetMapData().GetMapColumns(); }
	int MapRows() { return GetMapData().GetMapRows(); }
	float MapTileSize() { return GetMapData().GetTileSize(); }
	int RoomTileWidth() { return GetMapData().GetRoomTileWidth(); }
	int RoomTileHeight() { return GetMapData().GetRoomTileHeight(); }
	int LargeRoomTileWidth() { return GetMapData().GetLargeRoomTileWidth(); }
	int LargeRoomTileHeight() { return GetMapData().GetLargeRoomTileHeight(); }

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
		return g_GroundTextureIds[ActiveMapThemeIndex()][ToIndex(tile)];
	}

	int ResolveDecorTexture(DecorTile tile)
	{
		return g_DecorTextureIds[ActiveMapThemeIndex()][ToIndex(tile)];
	}

	bool IsDirectionalPath(GroundTile tile)
	{
		(void)tile;
		return false;
	}

	int CellIndex(int x, int y)
	{
		return y * MapColumns() + x;
	}

	bool IsInsideMap(int x, int y)
	{
		return x >= 0 && x < MapColumns() && y >= 0 && y < MapRows();
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
			(static_cast<float>(x) + 0.5f) * MapTileSize(),
			(static_cast<float>(y) + 0.5f) * MapTileSize(),
		};
	}

	void AddHorizontalEncounterBarrier(
		int first_x,
		int last_x,
		int corridor_y,
		float collision_world_y)
	{
		const float left = first_x * MapTileSize() - ENCOUNTER_BARRIER_ENDPOINT_EXTENSION;
		const float right = (last_x + 1) * MapTileSize() + ENCOUNTER_BARRIER_ENDPOINT_EXTENSION;
		g_EncounterBarriers.push_back({
			{ (left + right) * 0.5f, collision_world_y },
			{ (left + right) * 0.5f, (static_cast<float>(corridor_y) + 0.5f) * MapTileSize() },
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
		const float top = first_y * MapTileSize() - ENCOUNTER_BARRIER_ENDPOINT_EXTENSION;
		const float bottom = (last_y + 1) * MapTileSize() + ENCOUNTER_BARRIER_ENDPOINT_EXTENSION;
		g_EncounterBarriers.push_back({
			{ collision_world_x, (top + bottom) * 0.5f },
			{ (static_cast<float>(corridor_x) + 0.5f) * MapTileSize(), (top + bottom) * 0.5f },
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
		for (auto& texture_ids : g_GroundTextureIds)
		{
			texture_ids.fill(TEXTURE_INVALID_ID);
		}
		for (auto& texture_ids : g_DecorTextureIds)
		{
			texture_ids.fill(TEXTURE_INVALID_ID);
		}
		g_TorchTextureIds.fill(TEXTURE_INVALID_ID);
		g_EncounterBarrierTextureIds.fill(TEXTURE_INVALID_ID);
		g_OverviewTextureId = TEXTURE_INVALID_ID;
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

	RoomCandidate MakeCenteredRoomCandidate(int tile_width, int tile_height)
	{
		return {
			MapColumns() / 2 - tile_width / 2,
			MapRows() / 2 - tile_height / 2,
			tile_width,
			tile_height,
		};
	}

	template <std::size_t ThemeCount, std::size_t TextureCount>
	bool LoadThemeTextureSets(
		const std::array<std::array<const wchar_t*, TextureCount>, ThemeCount>& paths,
		std::array<std::array<int, TextureCount>, ThemeCount>& texture_ids)
	{
		for (std::size_t theme_index = 0; theme_index < ThemeCount; ++theme_index)
		{
			if (!LoadTextureSet(paths[theme_index], texture_ids[theme_index]))
			{
				return false;
			}
		}
		return true;
	}

	template <std::size_t ThemeCount, std::size_t TextureCount>
	bool LoadThemeTextureSets(
		const std::array<const wchar_t*, ThemeCount>& directories,
		const std::array<const wchar_t*, TextureCount>& filenames,
		std::array<std::array<int, TextureCount>, ThemeCount>& texture_ids)
	{
		for (std::size_t theme_index = 0; theme_index < ThemeCount; ++theme_index)
		{
			for (std::size_t texture_index = 0;
				texture_index < TextureCount; ++texture_index)
			{
				const std::wstring path =
					std::wstring(directories[theme_index]) + filenames[texture_index];
				texture_ids[theme_index][texture_index] = Texture_Load(path.c_str(), false);
				if (texture_ids[theme_index][texture_index] == TEXTURE_INVALID_ID)
				{
					return false;
				}
			}
		}
		return true;
	}

	template <std::size_t ThemeCount, std::size_t TextureCount>
	void ReleaseThemeTextureSets(
		std::array<std::array<int, TextureCount>, ThemeCount>& texture_ids)
	{
		for (auto& theme_texture_ids : texture_ids)
		{
			ReleaseTextureSet(theme_texture_ids);
		}
	}

	RoomCandidate MakeConnectedRoomCandidate(
		const ProceduralMapRoom& parent,
		int direction_x,
		int direction_y,
		int tile_width,
		int tile_height)
	{
		const int parent_center_x = parent.TileX + parent.TileWidth / 2;
		const int parent_center_y = parent.TileY + parent.TileHeight / 2;
		if (direction_x > 0)
		{
			return {
				parent.TileX + parent.TileWidth + ROOM_CONNECTION_LENGTH,
				parent_center_y - tile_height / 2,
				tile_width,
				tile_height,
			};
		}
		if (direction_x < 0)
		{
			return {
				parent.TileX - ROOM_CONNECTION_LENGTH - tile_width,
				parent_center_y - tile_height / 2,
				tile_width,
				tile_height,
			};
		}
		if (direction_y > 0)
		{
			return {
				parent_center_x - tile_width / 2,
				parent.TileY + parent.TileHeight + ROOM_CONNECTION_LENGTH,
				tile_width,
				tile_height,
			};
		}
		return {
			parent_center_x - tile_width / 2,
			parent.TileY - ROOM_CONNECTION_LENGTH - tile_height,
			tile_width,
			tile_height,
		};
	}

	bool IsRoomCandidateInsideMap(const RoomCandidate& candidate)
	{
		return candidate.X >= ROOM_MAP_PADDING &&
			candidate.Y >= ROOM_MAP_PADDING &&
			candidate.X + candidate.Width <= MapColumns() - ROOM_MAP_PADDING &&
			candidate.Y + candidate.Height <= MapRows() - ROOM_MAP_PADDING;
	}

	bool RoomCandidateOverlapsExistingRoom(
		const RoomCandidate& candidate,
		int ignored_room_index = -1)
	{
		for (const DungeonRoom& room : g_Rooms)
		{
			if (room.Info.Index == ignored_room_index)
			{
				continue;
			}
			const bool separated =
				candidate.X + candidate.Width + ROOM_COLLISION_PADDING <= room.Info.TileX ||
				room.Info.TileX + room.Info.TileWidth + ROOM_COLLISION_PADDING <= candidate.X ||
				candidate.Y + candidate.Height + ROOM_COLLISION_PADDING <= room.Info.TileY ||
				room.Info.TileY + room.Info.TileHeight + ROOM_COLLISION_PADDING <= candidate.Y;
			if (!separated)
			{
				return true;
			}
		}
		return false;
	}

	bool IsValidRoomCandidate(
		const RoomCandidate& candidate,
		int parent_room_index)
	{
		return IsRoomCandidateInsideMap(candidate) &&
			!RoomCandidateOverlapsExistingRoom(candidate, parent_room_index);
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

	void AddRoom(
		const RoomCandidate& candidate,
		int grid_x,
		int grid_y,
		int parent_room_index)
	{
		DungeonRoom room{};
		room.Info.Index = static_cast<int>(g_Rooms.size());
		ApplyRoomBounds(room, candidate);
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
		const MapGameData& map_data = GetMapData();
		const bool is_boss_round = g_Round == map_data.GetBossRound();
		if (is_boss_round)
		{
			// Start below two large encounters, then finish at the boss arena.
			constexpr int LARGE_ENCOUNTER_COUNT = 2;
			constexpr int NORTH_ARENA_COUNT = LARGE_ENCOUNTER_COUNT + 1;
			constexpr int UP_GRID_X = 0;
			constexpr int UP_GRID_Y = -1;
			const int layout_height = RoomTileHeight() +
				NORTH_ARENA_COUNT * (ROOM_CONNECTION_LENGTH + LargeRoomTileHeight());
			const int layout_top = (MapRows() - layout_height) / 2;
			const RoomCandidate start_bounds = {
				MapColumns() / 2 - RoomTileWidth() / 2,
				layout_top + layout_height - RoomTileHeight(),
				RoomTileWidth(),
				RoomTileHeight(),
			};
			AddRoom(start_bounds, 0, 1, -1);
			g_Rooms.front().FloorVariant = 0;

			for (int arena_index = 0; arena_index < NORTH_ARENA_COUNT; ++arena_index)
			{
				const int parent_index = static_cast<int>(g_Rooms.size()) - 1;
				const RoomCandidate arena_bounds = MakeConnectedRoomCandidate(
					g_Rooms[parent_index].Info,
					UP_GRID_X,
					UP_GRID_Y,
					LargeRoomTileWidth(),
					LargeRoomTileHeight());
				AddRoom(
					arena_bounds,
					UP_GRID_X,
					-arena_index,
					parent_index);
				DungeonRoom& arena = g_Rooms.back();
				arena.Info.IsLargeRoom = true;
				arena.Info.IsBossRoom = arena_index == NORTH_ARENA_COUNT - 1;
				arena.FloorVariant = 1;
			}
			return;
		}

		const int target_count = random.Range(
			map_data.GetRegularRoomMinCount(),
			map_data.GetRegularRoomMaxCount() + 1);

		AddRoom(
			MakeCenteredRoomCandidate(RoomTileWidth(), RoomTileHeight()),
			0,
			0,
			-1);
		g_Rooms.front().FloorVariant = 0;

		constexpr std::array<std::array<int, 2>, 4> DIRECTIONS = { {
			{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
		} };
		while (static_cast<int>(g_Rooms.size()) < target_count)
		{
			const bool next_room_is_large =
				static_cast<int>(g_Rooms.size()) == target_count - 1;
			const int child_width = next_room_is_large ?
				LargeRoomTileWidth() : RoomTileWidth();
			const int child_height = next_room_is_large ?
				LargeRoomTileHeight() : RoomTileHeight();
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
					const RoomCandidate bounds = MakeConnectedRoomCandidate(
						parent.Info,
						direction[0],
						direction[1],
						child_width,
						child_height);
					if (IsInsideRoomGrid(grid_x, grid_y) &&
						FindRoomAtGrid(grid_x, grid_y) < 0 &&
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

			const RoomExpansionCandidate& expansion = expansion_candidates[
				random.Range(0, static_cast<int>(expansion_candidates.size()))];
			AddRoom(
				expansion.Bounds,
				expansion.GridX,
				expansion.GridY,
				expansion.ParentRoomIndex);
			if (next_room_is_large)
			{
				g_Rooms.back().Info.IsLargeRoom = true;
				g_Rooms.back().FloorVariant = 1;
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
			{ 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 },
		} };
		for (const auto& direction : DIRECTIONS)
		{
			const int grid_x = room.GridX + direction[0];
			const int grid_y = room.GridY + direction[1];
			const RoomCandidate bounds = MakeConnectedRoomCandidate(
				room.Info,
				direction[0],
				direction[1],
				LargeRoomTileWidth(),
				LargeRoomTileHeight());
			if (IsInsideRoomGrid(grid_x, grid_y) &&
				FindRoomAtGrid(grid_x, grid_y) < 0 &&
				IsValidRoomCandidate(bounds, room.Info.Index))
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
		const bool needs_appended_boss_room =
			g_Round != GetMapData().GetBossRound();
		for (int room_index = 0; room_index < static_cast<int>(g_Rooms.size()); ++room_index)
		{
			const DungeonRoom& room = g_Rooms[room_index];
			if (room_index == g_StartRoomIndex || room.Info.Depth < 0 ||
				room.Neighbors.size() != 1 ||
				(needs_appended_boss_room && !HasAvailableBossRoomNeighbor(room)))
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
					(!needs_appended_boss_room ||
						HasAvailableBossRoomNeighbor(g_Rooms[room_index])))
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
			const bool is_immediate_boss_room =
				g_Round == GetMapData().GetBossRound();
			if (is_immediate_boss_room)
			{
				final_room.Info.IsLargeRoom = true;
			}
			final_room.Info.IsBossRoom = is_immediate_boss_room;
			final_room.FloorVariant = 1;
		}
	}

	void AddBossRoom()
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
			const RoomCandidate bounds = MakeConnectedRoomCandidate(
				parent.Info,
				direction[0],
				direction[1],
				LargeRoomTileWidth(),
				LargeRoomTileHeight());
			if (!IsInsideRoomGrid(grid_x, grid_y) ||
				FindRoomAtGrid(grid_x, grid_y) >= 0 ||
				!IsValidRoomCandidate(bounds, g_FinalEncounterRoomIndex))
			{
				continue;
			}

			AddRoom(
				bounds,
				grid_x,
				grid_y,
				g_FinalEncounterRoomIndex);
			DungeonRoom& boss_room = g_Rooms.back();
			boss_room.Info.IsLargeRoom = true;
			boss_room.Info.IsBossRoom = true;
			boss_room.FloorVariant = 1;
			g_FinalEncounterRoomIndex = boss_room.Info.Index;
			g_ExitRoomIndex = boss_room.Info.Index;
			return;
		}

		// The selected final regular room normally reserves a free neighbor. If a
		// future map layout leaves no slot, keep the round completable by turning
		// that room into the boss arena.
		DungeonRoom& fallback_boss_room = g_Rooms[g_FinalEncounterRoomIndex];
		fallback_boss_room.Info.IsBossRoom = true;
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
		g_Cells.assign(MapColumns() * MapRows(), MapCell{});
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
				const int corridor_start_x = ax < bx ?
					room_a.TileX + room_a.TileWidth :
					room_b.TileX + room_b.TileWidth;
				const int corridor_end_x = ax < bx ?
					room_b.TileX - 1 : room_a.TileX - 1;
				CarveHorizontal(corridor_start_x, corridor_end_x, ay);
			}
			else if (ax == bx)
			{
				const int corridor_start_y = ay < by ?
					room_a.TileY + room_a.TileHeight :
					room_b.TileY + room_b.TileHeight;
				const int corridor_end_y = ay < by ?
					room_b.TileY - 1 : room_a.TileY - 1;
				CarveVertical(ax, corridor_start_y, corridor_end_y);
			}
		}
	}

	bool IsCellPartOfConnection(int x, int y, const RoomEdge& edge)
	{
		if (edge.A < 0 || edge.B < 0 ||
			edge.A >= static_cast<int>(g_Rooms.size()) ||
			edge.B >= static_cast<int>(g_Rooms.size()))
		{
			return false;
		}

		const ProceduralMapRoom& room_a = g_Rooms[edge.A].Info;
		const ProceduralMapRoom& room_b = g_Rooms[edge.B].Info;
		const int ax = room_a.TileX + room_a.TileWidth / 2;
		const int ay = room_a.TileY + room_a.TileHeight / 2;
		const int bx = room_b.TileX + room_b.TileWidth / 2;
		const int by = room_b.TileY + room_b.TileHeight / 2;

		if (ay == by)
		{
			const int corridor_start_x = ax < bx ?
				room_a.TileX + room_a.TileWidth :
				room_b.TileX + room_b.TileWidth;
			const int corridor_end_x = ax < bx ?
				room_b.TileX - 1 : room_a.TileX - 1;
			return x >= corridor_start_x && x <= corridor_end_x &&
				std::abs(y - ay) <= CORRIDOR_RADIUS;
		}
		if (ax == bx)
		{
			const int corridor_start_y = ay < by ?
				room_a.TileY + room_a.TileHeight :
				room_b.TileY + room_b.TileHeight;
			const int corridor_end_y = ay < by ?
				room_b.TileY - 1 : room_a.TileY - 1;
			return y >= corridor_start_y && y <= corridor_end_y &&
				std::abs(x - ax) <= CORRIDOR_RADIUS;
		}
		return false;
	}

	bool IsOverviewCellVisible(
		int x,
		int y,
		const MapCell& cell,
		ProceduralMapRoomVisibilityPredicate is_room_visible,
		ProceduralMapRoomVisibilityPredicate is_room_cleared)
	{
		if (!is_room_visible)
		{
			return true;
		}
		if (cell.Kind == CellKind::Room)
		{
			return cell.RoomIndex >= 0 && is_room_visible(cell.RoomIndex);
		}
		if (cell.Kind != CellKind::Corridor)
		{
			return false;
		}

		const ProceduralMapRoomVisibilityPredicate reveals_corridor =
			is_room_cleared ? is_room_cleared : is_room_visible;
		for (const RoomEdge& edge : g_Connections)
		{
			if ((reveals_corridor(edge.A) || reveals_corridor(edge.B)) &&
				IsCellPartOfConnection(x, y, edge))
			{
				return true;
			}
		}
		return false;
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
		for (int y = 0; y < MapRows(); ++y)
		{
			for (int x = 0; x < MapColumns(); ++x)
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
		const int texture_id = ResolveDecorTexture(tile);
		const float texture_width = static_cast<float>(Texture_GetWidth(texture_id));
		const float texture_height = static_cast<float>(Texture_GetHeight(texture_id));
		if (texture_width <= 0.0f || texture_height <= 0.0f)
		{
			return;
		}

		const float jitter_x = allow_jitter ?
			(HashToUnitFloat(CellHash(cell_x, cell_y, 0x6d2b79f5u)) - 0.5f) * MapTileSize() * 0.28f : 0.0f;
		const float jitter_y = allow_jitter ?
			(HashToUnitFloat(CellHash(cell_x, cell_y, 0x9e3779b9u)) - 0.5f) * MapTileSize() * 0.24f : 0.0f;
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
					(static_cast<float>(cell_x) + 0.5f) * MapTileSize() + jitter_x,
					(static_cast<float>(cell_y) + 0.5f) * MapTileSize() + jitter_y,
				},
				{ MapTileSize() * scale, MapTileSize() * scale },
				rotation,
				{ brightness, brightness, brightness, 1.0f },
			},
		});
	}

	void GenerateDecorations()
	{
		g_Decorations.clear();
		g_Decorations.reserve(1200);

		for (int y = 0; y < MapRows(); ++y)
		{
			for (int x = 0; x < MapColumns(); ++x)
			{
				const bool is_forest = ActiveMapTheme() == MapTheme::Forest;
				const MapCell& cell = g_Cells[CellIndex(x, y)];
				const float roll = HashToUnitFloat(CellHash(x, y, 0x94d049bbu));
				const float scale = 0.86f +
					HashToUnitFloat(CellHash(x, y, 0xed5ad4bbu)) * 0.20f;
				const std::uint32_t decor_hash = CellHash(x, y, 0xa511e9b3u);
				const DecorTile floor_decor = is_forest ?
					static_cast<DecorTile>(1 + decor_hash % (DECOR_TILE_TYPE_COUNT - 1)) :
					(HashToUnitFloat(decor_hash) < 0.5f ?
						DecorTile::Bones : DecorTile::Skull);
				const float floor_decor_chance = is_forest ?
					FOREST_FLOOR_DECOR_CHANCE : FLOOR_DECOR_CHANCE;

				if (cell.Kind == CellKind::Solid && IsTopWallGround(cell.Ground))
				{
					if (roll < WALL_DECOR_CHANCE)
					{
						AddDecoration(DecorTile::Torch, x, y, 1.0f, false, false);
						g_Decorations.back().Instance.Color = is_forest ?
							XMFLOAT4{ 1.0f, 1.04f, 0.98f, 1.0f } :
							XMFLOAT4{ 1.08f, 1.0f, 0.90f, 1.0f };
					}
					continue;
				}

				if (cell.Kind == CellKind::Corridor)
				{
					if (roll < floor_decor_chance)
					{
						AddDecoration(floor_decor, x, y, scale * 0.72f, false);
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
				if (distance_from_center <= 5)
				{
					continue;
				}

				if (roll < floor_decor_chance)
				{
					AddDecoration(floor_decor, x, y, scale * 0.78f, false);
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
		if (g_Round == GetMapData().GetBossRound())
		{
			// The final round starts with only the spawn room and its boss arena.
			g_ExitRoomIndex = g_FinalEncounterRoomIndex;
		}
		else
		{
			// Earlier rounds append a distinct boss arena after all regular rooms.
			AddBossRoom();
		}
		BuildRoomConnections();
		ComputeRoomDepths(false);
		CarveDungeon();
		AssignGroundMaterials();
		GenerateDecorations();
	}

	bool CircleIntersectsSolidCell(const XMFLOAT2& position, float radius, int cell_x, int cell_y)
	{
		const float left = cell_x * MapTileSize();
		const float top = cell_y * MapTileSize();
		const float right = left + MapTileSize();
		const float bottom = top + MapTileSize();
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
	g_OverviewTextureId = Texture_Load(L"asset/texture/white_square.png", false);
	if (g_OverviewTextureId == TEXTURE_INVALID_ID ||
		!LoadThemeTextureSets(
			GROUND_TEXTURE_DIRECTORIES,
			GROUND_TEXTURE_FILENAMES,
			g_GroundTextureIds) ||
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
		g_TorchAnimationElapsed = 0.0f;
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
	g_MapSeed = 0;
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
	if (!g_Initialized)
	{
		return;
	}

	const float animation_duration =
		TORCH_FRAME_TIME * static_cast<float>(TORCH_TEXTURE_PATHS.size());
	g_TorchAnimationElapsed = std::fmod(
		g_TorchAnimationElapsed + std::max(delta_time, 0.0f),
		animation_duration);
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
	round = std::clamp(round, 1, GetMapData().GetTotalRoundCount());

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
		static_cast<int>(std::floor((camera_position.x - half_width) / MapTileSize())) - 1,
		0, MapColumns() - 1);
	const int maximum_x = std::clamp(
		static_cast<int>(std::floor((camera_position.x + half_width) / MapTileSize())) + 1,
		0, MapColumns() - 1);
	const int minimum_y = std::clamp(
		static_cast<int>(std::floor((camera_position.y - half_height) / MapTileSize())) - 1,
		0, MapRows() - 1);
	const int maximum_y = std::clamp(
		static_cast<int>(std::floor((camera_position.y + half_height) / MapTileSize())) + 1,
		0, MapRows() - 1);

	for (int y = minimum_y; y <= maximum_y; ++y)
	{
		for (int x = minimum_x; x <= maximum_x; ++x)
		{
			const MapCell& cell = g_Cells[CellIndex(x, y)];
			const SpriteInstance instance = {
				CellCenter(x, y),
				{ MapTileSize(), MapTileSize() },
				cell.Rotation,
				{ cell.Brightness, cell.Brightness, cell.Brightness, 1.0f },
			};
			g_VisibleGroundBatches[ToIndex(cell.Ground)].push_back(instance);
		}
	}

	const float decor_margin = MapTileSize() * 2.0f;
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
			int texture_id = g_DecorTextureIds[ActiveMapThemeIndex()][i];
			if (i == ToIndex(DecorTile::Torch) &&
				ActiveMapTheme() == MapTheme::CurrentDungeon)
			{
				const std::size_t frame = std::min(
					static_cast<std::size_t>(g_TorchAnimationElapsed / TORCH_FRAME_TIME),
					g_TorchTextureIds.size() - 1);
				texture_id = g_TorchTextureIds[frame];
			}
			SpriteInstanced_Draw(texture_id, batch.data(), static_cast<int>(batch.size()));
		}
	}
}

int ProceduralMap_AppendTorchLights(
	SpritePointLight* lights,
	int light_count,
	int capacity,
	const XMFLOAT2& camera_position,
	const XMFLOAT2& viewport_size)
{
	if (!g_Initialized || !lights || capacity <= 0)
	{
		return 0;
	}

	light_count = std::clamp(light_count, 0, capacity);
	if (ActiveMapTheme() == MapTheme::Forest)
	{
		return light_count;
	}
	const int available_slots = capacity - light_count;
	if (available_slots <= 0)
	{
		return light_count;
	}

	struct TorchCandidate
	{
		float DistanceSquared{ 0.0f };
		XMFLOAT2 Position{};
	};
	static std::vector<TorchCandidate> candidates;
	candidates.clear();
	if (candidates.capacity() < 64)
	{
		candidates.reserve(64);
	}
	const float visibility_radius = std::sqrt(
		viewport_size.x * viewport_size.x +
		viewport_size.y * viewport_size.y) * 0.62f + 280.0f;
	const float visibility_radius_squared =
		visibility_radius * visibility_radius;
	for (const Decoration& decoration : g_Decorations)
	{
		if (decoration.Tile != DecorTile::Torch)
		{
			continue;
		}

		const XMFLOAT2& position = decoration.Instance.Position;
		const float dx = position.x - camera_position.x;
		const float dy = position.y - camera_position.y;
		const float distance_squared = dx * dx + dy * dy;
		if (distance_squared <= visibility_radius_squared)
		{
			candidates.push_back({ distance_squared, position });
		}
	}
	std::sort(candidates.begin(), candidates.end(),
		[](const TorchCandidate& left, const TorchCandidate& right)
		{
			return left.DistanceSquared < right.DistanceSquared;
		});

	const int torch_count = std::min(
		available_slots, static_cast<int>(candidates.size()));
	for (int i = 0; i < torch_count; ++i)
	{
		const XMFLOAT2& position = candidates[i].Position;
		const float flicker = 0.55f + 0.06f * std::sin(
			g_TorchAnimationElapsed * 19.0f +
			position.x * 0.011f + position.y * 0.017f);
		lights[light_count++] = {
			position,
			255.0f,
			flicker,
			{ 1.0f, 0.46f, 0.14f },
		};
	}
	return light_count;
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
			(visual_length - ENCOUNTER_BARRIER_ENDPOINT_EXTENSION * 2.0f) / MapTileSize())));
		const float first_offset = -0.5f * static_cast<float>(segment_count - 1) * MapTileSize();
		const std::size_t style_offset = static_cast<std::size_t>(barrier.Style) * 2;
		for (int segment = 0; segment < segment_count; ++segment)
		{
			const float offset = first_offset + static_cast<float>(segment) * MapTileSize();
			const std::size_t tile_index = style_offset + static_cast<std::size_t>(segment & 1);
			barrier_batches[tile_index].push_back({
				{
					barrier.VisualCenter.x + (horizontal ? offset : 0.0f),
					barrier.VisualCenter.y + (horizontal ? 0.0f : offset),
				},
				{ MapTileSize(), MapTileSize() },
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
	const XMFLOAT2& viewport_size,
	bool expanded,
	ProceduralMapRoomVisibilityPredicate is_room_visible,
	ProceduralMapRoomVisibilityPredicate is_room_cleared)
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
	XMFLOAT2 panel_origin{};
	if (expanded)
	{
		panel_origin = {
			(viewport_size.x - layout.Size.x) * 0.5f,
			(viewport_size.y - layout.Size.y) * 0.5f,
		};
	}
	else
	{
		panel_origin = {
			viewport_size.x - layout.Size.x - MINIMAP_SCREEN_MARGIN - frame_padding,
			MINIMAP_SCREEN_MARGIN + frame_padding,
		};
	}

	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(XMMatrixIdentity());
	const XMFLOAT2 panel_center = {
		panel_origin.x + layout.Size.x * 0.5f,
		panel_origin.y + layout.Size.y * 0.5f,
	};
	XMFLOAT2 overview_center_world = {
		world_size.x * 0.5f,
		world_size.y * 0.5f,
	};
	if (g_StartRoomIndex >= 0 &&
		g_StartRoomIndex < static_cast<int>(g_Rooms.size()))
	{
		overview_center_world = g_Rooms[g_StartRoomIndex].Info.Center;
	}
	layout.Origin = {
		panel_center.x - overview_center_world.x * layout.WorldScale,
		panel_center.y - overview_center_world.y * layout.WorldScale,
	};
	if (expanded)
	{
		Sprite_DrawSized(
			g_OverviewTextureId,
			viewport_size.x * 0.5f,
			viewport_size.y * 0.5f,
			viewport_size.x,
			viewport_size.y,
			{ 0.0f, 0.0f, 0.0f, 0.72f });
	}
	Sprite_DrawSized(
		g_OverviewTextureId,
		panel_center.x,
		panel_center.y,
		layout.Size.x + frame_padding * 2.0f,
		layout.Size.y + frame_padding * 2.0f,
		{ 0.02f, 0.03f, 0.04f, 0.82f });

	g_OverviewTiles.clear();
	const float overview_tile_size = MapTileSize() * layout.WorldScale;
	for (int y = 0; y < MapRows(); ++y)
	{
		for (int x = 0; x < MapColumns(); ++x)
		{
			const MapCell& cell = g_Cells[CellIndex(x, y)];
			if (cell.Kind == CellKind::Solid ||
				!IsOverviewCellVisible(
					x, y, cell, is_room_visible, is_room_cleared))
			{
				continue;
			}
			g_OverviewTiles.push_back({
				{
					layout.Origin.x + (static_cast<float>(x) + 0.5f) * overview_tile_size,
					layout.Origin.y + (static_cast<float>(y) + 0.5f) * overview_tile_size,
				},
				{ overview_tile_size, overview_tile_size },
				0.0f,
				{ 0.62f, 0.66f, 0.70f, 0.95f },
			});
		}
	}
	if (!g_OverviewTiles.empty())
	{
		SpriteInstanced_Draw(
			g_OverviewTextureId,
			g_OverviewTiles.data(),
			static_cast<int>(g_OverviewTiles.size()));
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
	if (g_Rooms.empty() || g_StartRoomIndex < 0 ||
		g_StartRoomIndex >= static_cast<int>(g_Rooms.size()))
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
		static_cast<int>(std::floor((position.x - radius) / MapTileSize())), 0, MapColumns() - 1);
	const int maximum_x = std::clamp(
		static_cast<int>(std::floor((position.x + radius) / MapTileSize())), 0, MapColumns() - 1);
	const int minimum_y = std::clamp(
		static_cast<int>(std::floor((position.y - radius) / MapTileSize())), 0, MapRows() - 1);
	const int maximum_y = std::clamp(
		static_cast<int>(std::floor((position.y + radius) / MapTileSize())), 0, MapRows() - 1);

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
	const int cell_x = static_cast<int>(std::floor(world_position.x / MapTileSize()));
	const int cell_y = static_cast<int>(std::floor(world_position.y / MapTileSize()));
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
