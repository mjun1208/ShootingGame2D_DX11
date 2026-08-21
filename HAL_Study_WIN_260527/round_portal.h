#ifndef ROUND_PORTAL_H
#define ROUND_PORTAL_H

#include "interactable.h"

class RoundPortal final : public IInteractable
{
public:
	void Initialize(const DirectX::XMFLOAT2& position);
	void Finalize();
	void Reset(const DirectX::XMFLOAT2& position);
	void Update(float delta_time, bool open);
	void Draw() const;

	bool CanInteract(
		const DirectX::XMFLOAT2& interactor_position) const override;
	DirectX::XMFLOAT2 GetInteractionPromptPosition() const override;
	void Interact() override;

	bool ConsumeActivation();

private:
	DirectX::XMFLOAT2 m_Position{};
	int m_OpenTextureID{ -1 };
	int m_IdleTextureID{ -1 };
	float m_AnimationElapsedTime{ 0.0f };
	bool m_IsOpen{ false };
	bool m_IsActivated{ false };
};

#endif // !ROUND_PORTAL_H
