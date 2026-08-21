#ifndef INTERACTABLE_H
#define INTERACTABLE_H

#include <DirectXMath.h>

class IInteractable
{
public:
	virtual ~IInteractable() = default;

	virtual bool CanInteract(
		const DirectX::XMFLOAT2& interactor_position) const = 0;
	virtual DirectX::XMFLOAT2 GetInteractionPromptPosition() const = 0;
	virtual void Interact() = 0;
};

#endif // !INTERACTABLE_H
