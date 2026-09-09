#ifndef MAP_CONSTANTS_H
#define MAP_CONSTANTS_H

namespace MapConstants::Generation
{
	inline constexpr int RoomGridMinX = -2;
	inline constexpr int RoomGridMaxX = 2;
	inline constexpr int RoomGridMinY = -2;
	inline constexpr int RoomGridMaxY = 2;
	inline constexpr int CorridorRadius = 2;
	inline constexpr int RoomConnectionLength = 6;
} // namespace MapConstants::Generation

namespace MapConstants::EncounterBarrier
{
	inline constexpr float Thickness = 14.0f;
	inline constexpr float EndpointExtension = 4.0f;
} // namespace MapConstants::EncounterBarrier

namespace MapConstants::Decoration
{
	inline constexpr float TorchFrameTime = 0.12f;
} // namespace MapConstants::Decoration

#endif // MAP_CONSTANTS_H
