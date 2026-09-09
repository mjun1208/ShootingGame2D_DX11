#ifndef CHEST_H
#define CHEST_H

#include "interactable.h"
#include "sprite_lighting.h"

class Chest final : public IInteractable
{
  public:
	void Initialize(const DirectX::XMFLOAT2& position);
	void Finalize();
	void Reset(const DirectX::XMFLOAT2& position);
	void Update(float delta_time);
	void Draw() const;

	bool CanInteract(const DirectX::XMFLOAT2& interactor_position) const override;
	DirectX::XMFLOAT2 GetInteractionPromptPosition() const override;
	void Interact() override;
	bool IsGone() const;
	bool BuildPointLight(SpritePointLight& out_light) const;

  private:
	enum class State
	{
		Closed,
		Opening,
		Gone,
	};

	void FinishOpening();

	DirectX::XMFLOAT2 m_Position{};
	int m_TextureID{ -1 };
	int m_OpenAudioID{ -1 };
	float m_AnimationElapsedTime{ 0.0f };
	State m_State{ State::Gone };
};

#endif // !CHEST_H
