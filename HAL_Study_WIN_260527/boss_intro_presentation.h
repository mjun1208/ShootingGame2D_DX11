#ifndef BOSS_INTRO_PRESENTATION_H
#define BOSS_INTRO_PRESENTATION_H

#include <memory>

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
	struct Impl;
	std::unique_ptr<Impl> m_Impl;
};

#endif // !BOSS_INTRO_PRESENTATION_H
