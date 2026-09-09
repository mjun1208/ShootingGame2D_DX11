#ifndef PROCEDURAL_MAP_INTERNAL_H
#define PROCEDURAL_MAP_INTERNAL_H

#include "procedural_map.h"
#include "Constants/map_constants.h"

#include "game_data_manager.h"
#include "random_utils.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <queue>
#include <string>
#include <vector>

namespace ProceduralMapInternal
{
	using namespace DirectX;

	// 맵 내부 자료형, 상수, 공유 상태를 선언한다.

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
		// 맵 한 칸에 필요한 지형과 표시 정보를 저장한다.
		CellKind Kind{ CellKind::Solid };
		GroundTile Ground{ GroundTile::VoidDeep };
		int RoomIndex{ -1 };
		float Rotation{ 0.0f };
		float Brightness{ 1.0f };
	};

	struct DungeonRoom
	{
		// 외부에 공개할 방 정보와 내부 연결 및 배치 정보를 함께 저장한다.
		ProceduralMapRoom Info{};
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

	// enum의 Count를 기준으로 타일 배열의 크기를 맞춘다.
	inline constexpr std::size_t GROUND_TILE_TYPE_COUNT = static_cast<std::size_t>(GroundTile::Count);
	inline constexpr std::size_t DECOR_TILE_TYPE_COUNT = static_cast<std::size_t>(DecorTile::Count);
	inline constexpr std::size_t ENCOUNTER_BARRIER_TILE_TYPE_COUNT =
	    static_cast<std::size_t>(EncounterBarrierTile::Count);
	inline constexpr std::size_t MAP_THEME_COUNT = 3;
	inline constexpr std::array<const wchar_t*, MAP_THEME_COUNT> GROUND_TEXTURE_DIRECTORIES = {
		L"asset/texture/map/dungeon/themes/forest/room/",
		L"asset/texture/map/dungeon/themes/crypt/room/",
		L"asset/texture/map/dungeon/room/",
	};
	inline constexpr std::array<const wchar_t*, GROUND_TILE_TYPE_COUNT> GROUND_TEXTURE_FILENAMES = {
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

	inline constexpr std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT> FOREST_DECOR_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/themes/forest/decor/torch.png",
		L"asset/texture/map/dungeon/themes/forest/decor/candle.png",
		L"asset/texture/map/dungeon/themes/forest/decor/coin.png",
		L"asset/texture/map/dungeon/themes/forest/decor/pot_red.png",
		L"asset/texture/map/dungeon/themes/forest/decor/pot_blue.png",
		L"asset/texture/map/dungeon/themes/forest/decor/bones.png",
		L"asset/texture/map/dungeon/themes/forest/decor/skull.png",
		L"asset/texture/map/dungeon/themes/forest/decor/chest.png",
	};
	inline constexpr std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT> CRYPT_DECOR_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/themes/crypt/decor/torch.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/candle.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/coin.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/pot_red.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/pot_blue.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/bones.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/skull.png",
		L"asset/texture/map/dungeon/themes/crypt/decor/chest.png",
	};
	inline constexpr std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT> CURRENT_DECOR_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/torch/torch_1.png",  L"asset/texture/map/dungeon/decor/candle.png",
		L"asset/texture/map/dungeon/decor/coin.png",     L"asset/texture/map/dungeon/decor/pot_red.png",
		L"asset/texture/map/dungeon/decor/pot_blue.png", L"asset/texture/map/dungeon/decor/bones.png",
		L"asset/texture/map/dungeon/decor/skull.png",    L"asset/texture/map/dungeon/decor/chest.png",
	};

	inline constexpr std::array<const wchar_t*, 4> TORCH_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/torch/torch_1.png",
		L"asset/texture/map/dungeon/torch/torch_2.png",
		L"asset/texture/map/dungeon/torch/torch_3.png",
		L"asset/texture/map/dungeon/torch/torch_4.png",
	};

	inline constexpr std::array<const wchar_t*, ENCOUNTER_BARRIER_TILE_TYPE_COUNT> ENCOUNTER_BARRIER_TEXTURE_PATHS = {
		L"asset/texture/map/dungeon/barrier/horizontal_01.png",
		L"asset/texture/map/dungeon/barrier/horizontal_02.png",
		L"asset/texture/map/dungeon/barrier/vertical_01.png",
		L"asset/texture/map/dungeon/barrier/vertical_02.png",
	};

	inline constexpr std::array<std::array<const wchar_t*, DECOR_TILE_TYPE_COUNT>, MAP_THEME_COUNT>
	    DECOR_TEXTURE_PATHS = {
		    FOREST_DECOR_TEXTURE_PATHS,
		    CRYPT_DECOR_TEXTURE_PATHS,
		    CURRENT_DECOR_TEXTURE_PATHS,
	    };

	// 공유 변수의 정의는 procedural_map_state.cpp에 둔다.
	extern std::array<std::array<int, GROUND_TILE_TYPE_COUNT>, MAP_THEME_COUNT> g_GroundTextureIds;
	extern std::array<std::array<int, DECOR_TILE_TYPE_COUNT>, MAP_THEME_COUNT> g_DecorTextureIds;
	extern std::array<int, TORCH_TEXTURE_PATHS.size()> g_TorchTextureIds;
	extern std::array<int, ENCOUNTER_BARRIER_TILE_TYPE_COUNT> g_EncounterBarrierTextureIds;
	extern std::array<std::vector<SpriteInstance>, GROUND_TILE_TYPE_COUNT> g_VisibleGroundBatches;
	extern std::array<std::vector<SpriteInstance>, DECOR_TILE_TYPE_COUNT> g_VisibleDecorBatches;
	extern int g_OverviewTextureId;
	extern std::vector<SpriteInstance> g_OverviewTiles;
	extern std::vector<MapCell> g_Cells;
	extern std::vector<DungeonRoom> g_Rooms;
	extern std::vector<RoomEdge> g_Connections;
	extern std::vector<Decoration> g_Decorations;
	extern std::vector<EncounterBarrier> g_EncounterBarriers;
	extern int g_Round;
	extern int g_StartRoomIndex;
	extern int g_FinalEncounterRoomIndex;
	extern int g_ExitRoomIndex;
	extern int g_LockedEncounterRoom;
	extern float g_TorchAnimationElapsed;
	extern bool g_Initialized;

	inline MapTheme ActiveMapTheme()
	{
		return GetMapData().GetRoundEncounter(g_Round).Theme;
	}

	inline std::size_t ActiveMapThemeIndex()
	{
		switch (ActiveMapTheme())
		{
		case MapTheme::Forest:
			return 0;
		case MapTheme::CryptDungeon:
			return 1;
		default:
			return 2;
		}
	}

	inline int MapColumns()
	{
		return GetMapData().GetMapColumns();
	}

	inline int MapRows()
	{
		return GetMapData().GetMapRows();
	}

	inline float MapTileSize()
	{
		return GetMapData().GetTileSize();
	}

	inline int RoomTileWidth()
	{
		return GetMapData().GetRoomTileWidth();
	}

	inline int RoomTileHeight()
	{
		return GetMapData().GetRoomTileHeight();
	}

	inline int LargeRoomTileWidth()
	{
		return GetMapData().GetLargeRoomTileWidth();
	}

	inline int LargeRoomTileHeight()
	{
		return GetMapData().GetLargeRoomTileHeight();
	}

	inline std::size_t ToIndex(GroundTile tile)
	{
		return static_cast<std::size_t>(tile);
	}

	inline std::size_t ToIndex(DecorTile tile)
	{
		return static_cast<std::size_t>(tile);
	}

	inline GroundTile PickTileVariant(GroundTile first, int count, float detail)
	{
		const int variant = std::clamp(static_cast<int>(detail * static_cast<float>(count)), 0, count - 1);
		return static_cast<GroundTile>(ToIndex(first) + static_cast<std::size_t>(variant));
	}

	inline int ResolveGroundTexture(GroundTile tile)
	{
		return g_GroundTextureIds[ActiveMapThemeIndex()][ToIndex(tile)];
	}

	inline int ResolveDecorTexture(DecorTile tile)
	{
		return g_DecorTextureIds[ActiveMapThemeIndex()][ToIndex(tile)];
	}

	inline int CellIndex(int x, int y)
	{
		return y * MapColumns() + x;
	}

	inline bool IsInsideMap(int x, int y)
	{
		return x >= 0 && x < MapColumns() && y >= 0 && y < MapRows();
	}

	inline bool IsWalkableCell(int x, int y)
	{
		return IsInsideMap(x, y) && g_Cells[CellIndex(x, y)].Kind != CellKind::Solid;
	}

	inline bool IsCorridorCell(int x, int y)
	{
		return IsInsideMap(x, y) && g_Cells[CellIndex(x, y)].Kind == CellKind::Corridor;
	}

	inline constexpr std::uint8_t CARDINAL_NORTH = 0x01;
	inline constexpr std::uint8_t CARDINAL_EAST = 0x02;
	inline constexpr std::uint8_t CARDINAL_SOUTH = 0x04;
	inline constexpr std::uint8_t CARDINAL_WEST = 0x08;
	inline constexpr std::uint8_t DIAGONAL_NORTH_EAST = 0x01;
	inline constexpr std::uint8_t DIAGONAL_SOUTH_EAST = 0x02;
	inline constexpr std::uint8_t DIAGONAL_SOUTH_WEST = 0x04;
	inline constexpr std::uint8_t DIAGONAL_NORTH_WEST = 0x08;

	inline int CountNeighborBits(std::uint8_t mask)
	{
		int count = 0;
		for (std::uint8_t bit = 1; bit <= 8; bit <<= 1)
		{
			count += (mask & bit) != 0 ? 1 : 0;
		}
		return count;
	}

	inline std::uint8_t GetCardinalWalkableMask(int x, int y)
	{
		std::uint8_t mask = 0;
		mask |= IsWalkableCell(x, y - 1) ? CARDINAL_NORTH : 0;
		mask |= IsWalkableCell(x + 1, y) ? CARDINAL_EAST : 0;
		mask |= IsWalkableCell(x, y + 1) ? CARDINAL_SOUTH : 0;
		mask |= IsWalkableCell(x - 1, y) ? CARDINAL_WEST : 0;
		return mask;
	}

	inline std::uint8_t GetDiagonalWalkableMask(int x, int y)
	{
		std::uint8_t mask = 0;
		mask |= IsWalkableCell(x + 1, y - 1) ? DIAGONAL_NORTH_EAST : 0;
		mask |= IsWalkableCell(x + 1, y + 1) ? DIAGONAL_SOUTH_EAST : 0;
		mask |= IsWalkableCell(x - 1, y + 1) ? DIAGONAL_SOUTH_WEST : 0;
		mask |= IsWalkableCell(x - 1, y - 1) ? DIAGONAL_NORTH_WEST : 0;
		return mask;
	}

	inline bool IsVoidGround(GroundTile tile)
	{
		return tile == GroundTile::VoidDeep || tile == GroundTile::VoidMottle;
	}

	inline bool IsTopWallGround(GroundTile tile)
	{
		return tile >= GroundTile::Top01 && tile <= GroundTile::Top04;
	}

	inline void AssignSolidGround(MapCell& cell, int x, int y, float detail)
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
				case DIAGONAL_SOUTH_EAST:
					cell.Ground = GroundTile::TopLeft;
					break;
				case DIAGONAL_SOUTH_WEST:
					cell.Ground = GroundTile::TopRight;
					break;
				case DIAGONAL_NORTH_EAST:
					cell.Ground = GroundTile::BottomLeft;
					break;
				default:
					cell.Ground = GroundTile::BottomRight;
					break;
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
			if (cardinal == (CARDINAL_EAST | CARDINAL_WEST) || cardinal == (CARDINAL_NORTH | CARDINAL_SOUTH))
			{
				cell.Ground = cardinal == (CARDINAL_NORTH | CARDINAL_SOUTH)
				                  ? PickTileVariant(GroundTile::Left01, 3, detail)
				                  : PickTileVariant(GroundTile::Top01, 4, detail);
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

	inline XMFLOAT2 CellCenter(int x, int y)
	{
		return {
			(static_cast<float>(x) + 0.5f) * MapTileSize(),
			(static_cast<float>(y) + 0.5f) * MapTileSize(),
		};
	}

	inline void AddHorizontalEncounterBarrier(int first_x, int last_x, int corridor_y, float collision_world_y)
	{
		const float left = first_x * MapTileSize() - MapConstants::EncounterBarrier::EndpointExtension;
		const float right = (last_x + 1) * MapTileSize() + MapConstants::EncounterBarrier::EndpointExtension;
		g_EncounterBarriers.push_back({
		    { (left + right) * 0.5f, collision_world_y },
		    { (left + right) * 0.5f, (static_cast<float>(corridor_y) + 0.5f) * MapTileSize() },
		    { right - left, MapConstants::EncounterBarrier::Thickness },
		    EncounterBarrierStyle::Horizontal,
		});
	}

	inline void AddVerticalEncounterBarrier(int first_y, int last_y, int corridor_x, float collision_world_x)
	{
		const float top = first_y * MapTileSize() - MapConstants::EncounterBarrier::EndpointExtension;
		const float bottom = (last_y + 1) * MapTileSize() + MapConstants::EncounterBarrier::EndpointExtension;
		g_EncounterBarriers.push_back({
		    { collision_world_x, (top + bottom) * 0.5f },
		    { (static_cast<float>(corridor_x) + 0.5f) * MapTileSize(), (top + bottom) * 0.5f },
		    { MapConstants::EncounterBarrier::Thickness, bottom - top },
		    EncounterBarrierStyle::Vertical,
		});
	}

	inline void BuildEncounterBarriers(int room_index)
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

	inline void ResetTextureIds()
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
	inline bool LoadTextureSet(const std::array<const wchar_t*, Count>& paths, std::array<int, Count>& texture_ids)
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

	template <std::size_t Count> inline void ReleaseTextureSet(std::array<int, Count>& texture_ids)
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

	inline bool IsInsideRoomGrid(int grid_x, int grid_y)
	{
		return grid_x >= MapConstants::Generation::RoomGridMinX && grid_x <= MapConstants::Generation::RoomGridMaxX &&
		       grid_y >= MapConstants::Generation::RoomGridMinY && grid_y <= MapConstants::Generation::RoomGridMaxY;
	}

	inline int FindRoomAtGrid(int grid_x, int grid_y)
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

	inline RoomCandidate MakeCenteredRoomCandidate(int tile_width, int tile_height)
	{
		return {
			MapColumns() / 2 - tile_width / 2,
			MapRows() / 2 - tile_height / 2,
			tile_width,
			tile_height,
		};
	}

	template <std::size_t ThemeCount, std::size_t TextureCount>
	inline bool LoadThemeTextureSets(const std::array<std::array<const wchar_t*, TextureCount>, ThemeCount>& paths,
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
	inline bool LoadThemeTextureSets(const std::array<const wchar_t*, ThemeCount>& directories,
	                                 const std::array<const wchar_t*, TextureCount>& filenames,
	                                 std::array<std::array<int, TextureCount>, ThemeCount>& texture_ids)
	{
		for (std::size_t theme_index = 0; theme_index < ThemeCount; ++theme_index)
		{
			for (std::size_t texture_index = 0; texture_index < TextureCount; ++texture_index)
			{
				const std::wstring path = std::wstring(directories[theme_index]) + filenames[texture_index];
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
	inline void ReleaseThemeTextureSets(std::array<std::array<int, TextureCount>, ThemeCount>& texture_ids)
	{
		for (auto& theme_texture_ids : texture_ids)
		{
			ReleaseTextureSet(theme_texture_ids);
		}
	}

	inline RoomCandidate MakeConnectedRoomCandidate(const ProceduralMapRoom& parent, int direction_x, int direction_y,
	                                                int tile_width, int tile_height)
	{
		const int parent_center_x = parent.TileX + parent.TileWidth / 2;
		const int parent_center_y = parent.TileY + parent.TileHeight / 2;
		if (direction_x > 0)
		{
			return {
				parent.TileX + parent.TileWidth + MapConstants::Generation::RoomConnectionLength,
				parent_center_y - tile_height / 2,
				tile_width,
				tile_height,
			};
		}
		if (direction_x < 0)
		{
			return {
				parent.TileX - MapConstants::Generation::RoomConnectionLength - tile_width,
				parent_center_y - tile_height / 2,
				tile_width,
				tile_height,
			};
		}
		if (direction_y > 0)
		{
			return {
				parent_center_x - tile_width / 2,
				parent.TileY + parent.TileHeight + MapConstants::Generation::RoomConnectionLength,
				tile_width,
				tile_height,
			};
		}
		return {
			parent_center_x - tile_width / 2,
			parent.TileY - MapConstants::Generation::RoomConnectionLength - tile_height,
			tile_width,
			tile_height,
		};
	}

	void GenerateRooms();
	void BuildRoomConnections();
	void ComputeRoomDepths(bool select_final_encounter);
	void AddBossRoom();
	void GenerateMap();
	bool IsOverviewCellVisible(int x, int y, const MapCell& cell, ProceduralMapRoomVisibilityPredicate is_room_visible,
	                           ProceduralMapRoomVisibilityPredicate is_room_cleared);
	bool CircleIntersectsSolidCell(const XMFLOAT2& position, float radius, int cell_x, int cell_y);
	bool IsEncounterBarrierClear(const XMFLOAT2& position, float radius);

} // namespace ProceduralMapInternal

#endif // PROCEDURAL_MAP_INTERNAL_H
