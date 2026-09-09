#ifndef INGAME_MENU_CONTROLLER_H
#define INGAME_MENU_CONTROLLER_H

#include "game_bullet.h"

#include <memory>

enum class IngamePauseAction
{
	None,
	Resume,
	Retry,
	Title,
	Exit,
};

enum class IngameAugmentChoice
{
	None,
	MultiShot,
	Overdrive,
	Power,
};

enum class IngameAugmentReason
{
	LevelUp,
	RoundClear,
};

struct IngameAugmentSelection
{
	BulletType WeaponType{ BulletType::Count };
	IngameAugmentChoice Choice{ IngameAugmentChoice::None };
	IngameAugmentReason Reason{ IngameAugmentReason::LevelUp };
};

class IngameMenuController final
{
  public:
	IngameMenuController();
	~IngameMenuController();

	IngameMenuController(const IngameMenuController&) = delete;
	IngameMenuController& operator=(const IngameMenuController&) = delete;

	bool Initialize();
	void Finalize();
	void Reset();

	void OpenPause();
	void ClosePause();
	bool IsPauseOpen() const;
	IngamePauseAction UpdatePause();

	// 보상에 사용할 보유 무기가 없으면 false를 반환한다.
	bool OpenAugment(IngameAugmentReason reason = IngameAugmentReason::LevelUp);
	void CloseAugment();
	bool IsAugmentOpen() const;
	IngameAugmentSelection UpdateAugment();

	void Draw(int overlay_texture_id);

  private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;
};

#endif // !INGAME_MENU_CONTROLLER_H
