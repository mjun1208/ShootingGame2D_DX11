#ifndef INGAME_SCENE_H
#define INGAME_SCENE_H

#include "camera.h"
#include "scene.h"

class IngameScene final : public cScene
{
public:
	bool Initialize() override;
	void Finalize() override;
	void Update(float delta_time) override;
	void Draw() override;

private:
	cCamera m_Camera;
	float m_FireCooldown{ 0.0f };
};

#endif // !INGAME_SCENE_H
