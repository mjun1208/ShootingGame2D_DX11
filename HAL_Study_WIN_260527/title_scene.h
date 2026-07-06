#ifndef TITLE_SCENE_H
#define TITLE_SCENE_H

#include "scene.h"
#include "texture.h"

class TitleScene final : public cScene
{
public:
	bool Initialize() override;
	void Finalize() override;
	void Update(float delta_time) override;
	void Draw() override;

private:
	int m_LogoTextureID{ TEXTURE_INVALID_ID };
};

#endif // !TITLE_SCENE_H
