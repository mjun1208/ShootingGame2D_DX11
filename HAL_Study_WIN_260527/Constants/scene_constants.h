#ifndef SCENE_CONSTANTS_H
#define SCENE_CONSTANTS_H

namespace SceneConstants
{
	namespace CameraFeedback
	{
		inline constexpr float PlayerFireShakeSize = 0.55f;
		inline constexpr float PlayerFireShakeDuration = 0.04f;

	} // namespace CameraFeedback

	namespace CameraFollow
	{
		inline constexpr float RoomCenterWeight = 0.35f;
		inline constexpr float SmoothSpeed = 9.0f;

	} // namespace CameraFollow
} // namespace SceneConstants

namespace SceneConstants::Transition
{
	inline constexpr float FadeOutDuration = 0.40f;
	inline constexpr float FadeInDuration = 0.45f;
} // namespace SceneConstants::Transition

#endif // SCENE_CONSTANTS_H
