#include "procedural_map_internal.h"
#include "Constants/map_constants.h"

#include "math_utils.h"

namespace
{

	namespace MapTuning::Minimap
	{
		constexpr float ViewportFraction = 0.40f;
		constexpr float MaxWorldScale = 1.0f / 20.0f;
	} // namespace MapTuning::Minimap

	namespace MapTuning::WorldMap
	{
		constexpr float ViewportFraction = 0.80f;
		constexpr float MaxWorldScale = 1.0f / 10.0f;
	} // namespace MapTuning::WorldMap

} // namespace

using namespace DirectX;
using namespace ProceduralMapInternal;

// 바닥부터 장식, 조명, 전투 방 문 순서로 맵을 그린다.
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
	const int minimum_x = std::clamp(static_cast<int>(std::floor((camera_position.x - half_width) / MapTileSize())) - 1,
	                                 0, MapColumns() - 1);
	const int maximum_x = std::clamp(static_cast<int>(std::floor((camera_position.x + half_width) / MapTileSize())) + 1,
	                                 0, MapColumns() - 1);
	const int minimum_y = std::clamp(
	    static_cast<int>(std::floor((camera_position.y - half_height) / MapTileSize())) - 1, 0, MapRows() - 1);
	const int maximum_y = std::clamp(
	    static_cast<int>(std::floor((camera_position.y + half_height) / MapTileSize())) + 1, 0, MapRows() - 1);

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
		if (position.x < visible_left || position.x > visible_right || position.y < visible_top ||
		    position.y > visible_bottom)
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
			SpriteInstanced_Draw(ResolveGroundTexture(static_cast<GroundTile>(i)), batch.data(),
			                     static_cast<int>(batch.size()));
		}
	}
	for (std::size_t i = 0; i < DECOR_TILE_TYPE_COUNT; ++i)
	{
		const auto& batch = g_VisibleDecorBatches[i];
		if (!batch.empty())
		{
			int texture_id = g_DecorTextureIds[ActiveMapThemeIndex()][i];
			if (i == ToIndex(DecorTile::Torch) && ActiveMapTheme() == MapTheme::CurrentDungeon)
			{
				const std::size_t frame = std::min(
				    static_cast<std::size_t>(g_TorchAnimationElapsed / MapConstants::Decoration::TorchFrameTime),
				    g_TorchTextureIds.size() - 1);
				texture_id = g_TorchTextureIds[frame];
			}
			SpriteInstanced_Draw(texture_id, batch.data(), static_cast<int>(batch.size()));
		}
	}
}

int ProceduralMap_AppendTorchLights(SpritePointLight* lights, int light_count, int capacity,
                                    const XMFLOAT2& camera_position, const XMFLOAT2& viewport_size)
{
	light_count = std::clamp(light_count, 0, std::max(capacity, 0));
	if (!g_Initialized || !lights || capacity <= 0)
	{
		return light_count;
	}

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
	const float visibility_radius = Length(viewport_size) * 0.62f + 280.0f;
	const float visibility_radius_squared = visibility_radius * visibility_radius;
	for (const Decoration& decoration : g_Decorations)
	{
		if (decoration.Tile != DecorTile::Torch)
		{
			continue;
		}

		const XMFLOAT2& position = decoration.Instance.Position;
		const float distance_squared = DistanceSquared(position, camera_position);
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

	const int torch_count = std::min(available_slots, static_cast<int>(candidates.size()));
	for (int i = 0; i < torch_count; ++i)
	{
		const XMFLOAT2& position = candidates[i].Position;
		const float flicker =
		    0.55f + 0.06f * std::sin(g_TorchAnimationElapsed * 19.0f + position.x * 0.011f + position.y * 0.017f);
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
	Sprite_DrawSized(ResolveGroundTexture(GroundTile::VoidDeep), viewport_size.x * 0.5f, viewport_size.y * 0.5f,
	                 viewport_size.x, viewport_size.y, { 0.0f, 0.0f, 0.0f, Saturate(alpha) });
}

void ProceduralMap_DrawEncounterLock()
{
	if (!g_Initialized || g_EncounterBarriers.empty())
	{
		return;
	}

	static std::array<std::vector<SpriteInstance>, ENCOUNTER_BARRIER_TILE_TYPE_COUNT> barrier_batches;
	for (auto& batch : barrier_batches)
	{
		batch.clear();
		batch.reserve(g_EncounterBarriers.size() * 5);
	}
	for (const EncounterBarrier& barrier : g_EncounterBarriers)
	{
		const bool horizontal = barrier.Style == EncounterBarrierStyle::Horizontal;
		const float visual_length = horizontal ? barrier.Size.x : barrier.Size.y;
		const int segment_count = std::max(
		    1, static_cast<int>(std::lround((visual_length - MapConstants::EncounterBarrier::EndpointExtension * 2.0f) /
		                                    MapTileSize())));
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
			SpriteInstanced_Draw(g_EncounterBarrierTextureIds[i], batch.data(), static_cast<int>(batch.size()));
		}
	}
}

using namespace DirectX;
using namespace ProceduralMapInternal;

// 지도와 HUD가 같은 화면 배치를 사용한다.
ProceduralMapOverviewLayout ProceduralMap_GetOverviewLayout(const XMFLOAT2& viewport_size, bool expanded)
{
	static constexpr float ScreenMargin = 20.0f;

	ProceduralMapOverviewLayout layout{};
	const XMFLOAT2 world_size = ProceduralMap_GetWorldSize();
	const float viewport_fraction =
	    expanded ? MapTuning::WorldMap::ViewportFraction : MapTuning::Minimap::ViewportFraction;
	const float maximum_scale = expanded ? MapTuning::WorldMap::MaxWorldScale : MapTuning::Minimap::MaxWorldScale;
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
	layout.FramePadding = expanded ? 14.0f : 8.0f;
	if (expanded)
	{
		layout.PanelOrigin = {
			(viewport_size.x - layout.Size.x) * 0.5f,
			(viewport_size.y - layout.Size.y) * 0.5f,
		};
	}
	else
	{
		layout.PanelOrigin = {
			viewport_size.x - layout.Size.x - ScreenMargin - layout.FramePadding,
			ScreenMargin + layout.FramePadding,
		};
	}

	const XMFLOAT2 panel_center = {
		layout.PanelOrigin.x + layout.Size.x * 0.5f,
		layout.PanelOrigin.y + layout.Size.y * 0.5f,
	};
	XMFLOAT2 overview_center_world = {
		world_size.x * 0.5f,
		world_size.y * 0.5f,
	};
	if (g_StartRoomIndex >= 0 && g_StartRoomIndex < static_cast<int>(g_Rooms.size()))
	{
		overview_center_world = g_Rooms[g_StartRoomIndex].Info.Center;
	}
	layout.Origin = {
		panel_center.x - overview_center_world.x * layout.WorldScale,
		panel_center.y - overview_center_world.y * layout.WorldScale,
	};
	return layout;
}

ProceduralMapOverviewLayout ProceduralMap_DrawOverview(const XMFLOAT2& viewport_size, bool expanded,
                                                       ProceduralMapRoomVisibilityPredicate is_room_visible,
                                                       ProceduralMapRoomVisibilityPredicate is_room_cleared)
{
	if (!g_Initialized)
	{
		return {};
	}

	const auto layout = ProceduralMap_GetOverviewLayout(viewport_size, expanded);
	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(XMMatrixIdentity());
	const XMFLOAT2 panel_center = { layout.PanelOrigin.x + layout.Size.x * 0.5f,
		                            layout.PanelOrigin.y + layout.Size.y * 0.5f };
	if (expanded)
	{
		Sprite_DrawSized(g_OverviewTextureId, viewport_size.x * 0.5f, viewport_size.y * 0.5f, viewport_size.x,
		                 viewport_size.y, { 0.0f, 0.0f, 0.0f, 0.72f });
	}
	Sprite_DrawSized(g_OverviewTextureId, panel_center.x, panel_center.y, layout.Size.x + layout.FramePadding * 2.0f,
	                 layout.Size.y + layout.FramePadding * 2.0f, { 0.02f, 0.03f, 0.04f, 0.82f });

	g_OverviewTiles.clear();
	const float overview_tile_size = MapTileSize() * layout.WorldScale;
	for (int y = 0; y < MapRows(); ++y)
	{
		for (int x = 0; x < MapColumns(); ++x)
		{
			const MapCell& cell = g_Cells[CellIndex(x, y)];
			if (cell.Kind == CellKind::Solid || !IsOverviewCellVisible(x, y, cell, is_room_visible, is_room_cleared))
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
		SpriteInstanced_Draw(g_OverviewTextureId, g_OverviewTiles.data(), static_cast<int>(g_OverviewTiles.size()));
	}

	return layout;
}

namespace ProceduralMapInternal
{
	// 방과 복도를 타일에 새기고 벽, 바닥, 장식물을 채운다.
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
			for (int offset = -MapConstants::Generation::CorridorRadius;
			     offset <= MapConstants::Generation::CorridorRadius; ++offset)
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
			for (int offset = -MapConstants::Generation::CorridorRadius;
			     offset <= MapConstants::Generation::CorridorRadius; ++offset)
			{
				CarveCorridorCell(x + offset, y);
			}
		}
	}

	void CarveDungeon()
	{
		// 막힌 셀로 시작해서 방과 연결된 복도만 통로로 바꾼다.
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
				const int corridor_start_x =
				    ax < bx ? room_a.TileX + room_a.TileWidth : room_b.TileX + room_b.TileWidth;
				const int corridor_end_x = ax < bx ? room_b.TileX - 1 : room_a.TileX - 1;
				CarveHorizontal(corridor_start_x, corridor_end_x, ay);
			}
			else if (ax == bx)
			{
				const int corridor_start_y =
				    ay < by ? room_a.TileY + room_a.TileHeight : room_b.TileY + room_b.TileHeight;
				const int corridor_end_y = ay < by ? room_b.TileY - 1 : room_a.TileY - 1;
				CarveVertical(ax, corridor_start_y, corridor_end_y);
			}
		}
	}

	bool IsCellPartOfConnection(int x, int y, const RoomEdge& edge)
	{
		if (edge.A < 0 || edge.B < 0 || edge.A >= static_cast<int>(g_Rooms.size()) ||
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
			const int corridor_start_x = ax < bx ? room_a.TileX + room_a.TileWidth : room_b.TileX + room_b.TileWidth;
			const int corridor_end_x = ax < bx ? room_b.TileX - 1 : room_a.TileX - 1;
			return x >= corridor_start_x && x <= corridor_end_x &&
			       std::abs(y - ay) <= MapConstants::Generation::CorridorRadius;
		}
		if (ax == bx)
		{
			const int corridor_start_y = ay < by ? room_a.TileY + room_a.TileHeight : room_b.TileY + room_b.TileHeight;
			const int corridor_end_y = ay < by ? room_b.TileY - 1 : room_a.TileY - 1;
			return y >= corridor_start_y && y <= corridor_end_y &&
			       std::abs(x - ax) <= MapConstants::Generation::CorridorRadius;
		}
		return false;
	}

	bool IsOverviewCellVisible(int x, int y, const MapCell& cell, ProceduralMapRoomVisibilityPredicate is_room_visible,
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
			if ((reveals_corridor(edge.A) || reveals_corridor(edge.B)) && IsCellPartOfConnection(x, y, edge))
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
		// 셀 주변 모양을 보고 벽/모서리/바닥 타일 종류와 회전을 정한다.
		for (int y = 0; y < MapRows(); ++y)
		{
			for (int x = 0; x < MapColumns(); ++x)
			{
				MapCell& cell = g_Cells[CellIndex(x, y)];
				const float detail = Random01();
				cell.Brightness = RandomFloat(0.98f, 1.0f);
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
				cell.Rotation = 0.0f;
			}
		}
	}

	void AddDecoration(DecorTile tile, int cell_x, int cell_y, float scale, bool allow_rotation,
	                   bool allow_jitter = true)
	{
		static constexpr float Pi = 3.14159265358979323846f;
		const int texture_id = ResolveDecorTexture(tile);
		const float texture_width = static_cast<float>(Texture_GetWidth(texture_id));
		const float texture_height = static_cast<float>(Texture_GetHeight(texture_id));
		if (texture_width <= 0.0f || texture_height <= 0.0f)
		{
			return;
		}
		const float jitter_x = allow_jitter ? RandomSigned() * MapTileSize() * 0.14f : 0.0f;
		const float jitter_y = allow_jitter ? RandomSigned() * MapTileSize() * 0.12f : 0.0f;
		const float brightness = RandomFloat(0.92f, 1.0f);
		const float rotation = allow_rotation && Random01() < 0.5f ? Pi : 0.0f;
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
		static constexpr float ForestFloorDecorChance = 0.035f;
		static constexpr float FloorDecorChance = 0.01f;
		static constexpr float WallDecorChance = 0.07f;
		// 맵을 만들 때 장식 위치와 모양을 한 번 무작위로 정한다.
		g_Decorations.clear();
		g_Decorations.reserve(1200);
		for (int y = 0; y < MapRows(); ++y)
		{
			for (int x = 0; x < MapColumns(); ++x)
			{
				const bool is_forest = ActiveMapTheme() == MapTheme::Forest;
				const MapCell& cell = g_Cells[CellIndex(x, y)];
				const float roll = Random01();
				const float scale = RandomFloat(0.86f, 1.06f);
				const DecorTile floor_decor =
				    is_forest ? static_cast<DecorTile>(RandomInt(1, static_cast<int>(DECOR_TILE_TYPE_COUNT) - 1))
				              : (Random01() < 0.5f ? DecorTile::Bones : DecorTile::Skull);
				const float floor_decor_chance = is_forest ? ForestFloorDecorChance : FloorDecorChance;
				if (cell.Kind == CellKind::Solid && IsTopWallGround(cell.Ground))
				{
					if (roll < WallDecorChance)
					{
						AddDecoration(DecorTile::Torch, x, y, 1.0f, false, false);
						g_Decorations.back().Instance.Color =
						    is_forest ? XMFLOAT4{ 1.0f, 1.04f, 0.98f, 1.0f } : XMFLOAT4{ 1.08f, 1.0f, 0.90f, 1.0f };
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

	void GenerateMap()
	{
		// 방 그래프를 만든 다음 실제 타일과 장식 데이터로 변환한다.
		g_LockedEncounterRoom = -1;
		g_EncounterBarriers.clear();
		g_StartRoomIndex = 0;
		g_FinalEncounterRoomIndex = 0;
		g_ExitRoomIndex = 0;
		GenerateRooms();
		BuildRoomConnections();
		ComputeRoomDepths(true);
		if (g_Round == GetMapData().GetBossRound())
		{
			// 마지막 라운드는 시작 방, 큰 방 두 개, 보스 방이 이미 정해져 있다.
			g_ExitRoomIndex = g_FinalEncounterRoomIndex;
		}
		else
		{
			// 그 전 라운드는 일반 방 뒤에 별도 보스 방을 붙인다.
			AddBossRoom();
		}
		BuildRoomConnections();
		ComputeRoomDepths(false);
		CarveDungeon();
		AssignGroundMaterials();
		GenerateDecorations();
	}
} // namespace ProceduralMapInternal
