#ifndef BUTTON_MENU_CONTROLLER_H
#define BUTTON_MENU_CONTROLLER_H

#include "button.h"

#include <initializer_list>
#include <vector>

enum class ButtonMenuNavigation
{
	Vertical,
	Horizontal,
	Both,
};

enum class ButtonMenuBoundary
{
	Wrap,
	Clamp,
};

class ButtonMenuController final
{
  public:
	static constexpr int NO_ACTIVATION = -1;

	void Initialize(std::initializer_list<cButton*> buttons, int selected_index = 0,
	                ButtonMenuNavigation navigation = ButtonMenuNavigation::Vertical,
	                ButtonMenuBoundary boundary = ButtonMenuBoundary::Wrap, bool confirm_with_space = true);
	void Clear();

	// 모든 버튼을 갱신하고 선택한 버튼의 인덱스를 반환한다. 선택이 없으면 NO_ACTIVATION을 반환한다.
	int Update();
	void SetSelectedIndex(int index);
	int GetSelectedIndex() const;
	void ResetPointerTracking();

  private:
	int NormalizeIndex(int index) const;
	void MoveSelection(int offset);

	std::vector<cButton*> m_Buttons;
	int m_SelectedIndex{ 0 };
	int m_LastMouseX{ 0 };
	int m_LastMouseY{ 0 };
	ButtonMenuNavigation m_Navigation{ ButtonMenuNavigation::Vertical };
	ButtonMenuBoundary m_Boundary{ ButtonMenuBoundary::Wrap };
	bool m_ConfirmWithSpace{ true };
};

#endif // BUTTON_MENU_CONTROLLER_H
