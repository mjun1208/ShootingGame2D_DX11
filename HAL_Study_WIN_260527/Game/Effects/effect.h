#ifndef EFFECT_H
#define EFFECT_H

#include "sprite_instanced.h"

#include <DirectXMath.h>

struct cEffectDesc
{
	DirectX::XMFLOAT2 Position{ 0.0f, 0.0f };
	int TextureID{ -1 };
	int FrameWidth{ 0 };
	int FrameHeight{ 0 };
	int FrameCount{ 1 };
	int FrameColumns{ 1 };
	float FrameTime{ 0.05f };
	float DrawWidth{ 0.0f };
	float DrawHeight{ 0.0f };
	float Rotation{ 0.0f };
	DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT2 LightPosition{ 0.0f, 0.0f };
	DirectX::XMFLOAT3 LightColor{ 1.0f, 1.0f, 1.0f };
	float LightRadius{ 0.0f };
	float LightStrength{ 0.0f };
};

class cEffect
{
  public:
	void Play(const cEffectDesc& desc);
	void Update(float delta_time);
	void Deactivate();

	bool IsActive() const;
	int GetTextureID() const;
	bool BuildInstance(SpriteInstance& out_instance) const;
	bool BuildPointLight(SpritePointLight& out_light) const;

  private:
	int GetCurrentFrame() const;

	cEffectDesc m_Desc{};
	float m_ElapsedTime{ 0.0f };
	bool m_IsActive{ false };
};

#endif // !EFFECT_H
