#ifndef BOSS_INTRO_PRESENTATION_H
#define BOSS_INTRO_PRESENTATION_H

#include "texture.h"

#include <memory>

namespace hal
{
	class BitmapText;
}

class BossIntroPresentation final
{
  public:
	BossIntroPresentation();
	~BossIntroPresentation();

	BossIntroPresentation(const BossIntroPresentation&) = delete;
	BossIntroPresentation& operator=(const BossIntroPresentation&) = delete;

	bool Initialize();
	void Finalize();
	void Reset();
	bool Begin();
	void Update(float delta_time);
	void Draw(int overlay_texture_id);

	bool IsActive() const;
	bool HasPlayed() const;

  private:
	int m_DangerTextureID{ TEXTURE_INVALID_ID };
	int m_WarningAudioID{ -1 };
	std::unique_ptr<hal::BitmapText> m_BossNameText;
	float m_ElapsedTime{ 0.0f };
	bool m_Active{ false };
	bool m_Played{ false };
};

#endif // !BOSS_INTRO_PRESENTATION_H
