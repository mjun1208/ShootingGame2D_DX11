#include "button_menu_controller.h"

#include "input_keyboard.h"
#include "input_mouse.h"

#include <algorithm>

void ButtonMenuController::Initialize(std::initializer_list<cButton*> buttons, int selected_index,
                                      ButtonMenuNavigation navigation, ButtonMenuBoundary boundary,
                                      bool confirm_with_space)
{
	m_Buttons.assign(buttons.begin(), buttons.end());
	m_Navigation = navigation;
	m_Boundary = boundary;
	m_ConfirmWithSpace = confirm_with_space;
	SetSelectedIndex(selected_index);
	ResetPointerTracking();
}

void ButtonMenuController::Clear()
{
	for (cButton* button : m_Buttons)
	{
		if (button)
		{
			button->SetSelected(false);
		}
	}
	m_Buttons.clear();
	m_SelectedIndex = 0;
}

int ButtonMenuController::Update()
{
	if (m_Buttons.empty())
	{
		return NO_ACTIVATION;
	}

	int clicked_index = NO_ACTIVATION;
	for (int index = 0; index < static_cast<int>(m_Buttons.size()); ++index)
	{
		cButton* button = m_Buttons[index];
		if (button && button->Update() && clicked_index == NO_ACTIVATION)
		{
			clicked_index = index;
		}
	}

	const int mouse_x = InputMouse_GetX();
	const int mouse_y = InputMouse_GetY();
	const bool mouse_moved = mouse_x != m_LastMouseX || mouse_y != m_LastMouseY;
	m_LastMouseX = mouse_x;
	m_LastMouseY = mouse_y;
	if (mouse_moved)
	{
		for (int index = 0; index < static_cast<int>(m_Buttons.size()); ++index)
		{
			const cButton* button = m_Buttons[index];
			if (button && button->IsHovered())
			{
				SetSelectedIndex(index);
				break;
			}
		}
	}

	const bool vertical = m_Navigation == ButtonMenuNavigation::Vertical || m_Navigation == ButtonMenuNavigation::Both;
	const bool horizontal =
	    m_Navigation == ButtonMenuNavigation::Horizontal || m_Navigation == ButtonMenuNavigation::Both;
	const bool move_previous = (vertical && (InputKeyboard_IsTrigger(KK_UP) || InputKeyboard_IsTrigger(KK_W))) ||
	                           (horizontal && (InputKeyboard_IsTrigger(KK_LEFT) || InputKeyboard_IsTrigger(KK_A)));
	const bool move_next = (vertical && (InputKeyboard_IsTrigger(KK_DOWN) || InputKeyboard_IsTrigger(KK_S))) ||
	                       (horizontal && (InputKeyboard_IsTrigger(KK_RIGHT) || InputKeyboard_IsTrigger(KK_D)));
	if (move_previous)
	{
		Button_PlayNavigateSound();
		MoveSelection(-1);
	}
	else if (move_next)
	{
		Button_PlayNavigateSound();
		MoveSelection(1);
	}

	if (clicked_index != NO_ACTIVATION)
	{
		SetSelectedIndex(clicked_index);
		return clicked_index;
	}

	const bool keyboard_activate =
	    InputKeyboard_IsTrigger(KK_ENTER) || (m_ConfirmWithSpace && InputKeyboard_IsTrigger(KK_SPACE));
	if (!keyboard_activate)
	{
		return NO_ACTIVATION;
	}

	Button_PlayConfirmSound();
	return m_SelectedIndex;
}

void ButtonMenuController::SetSelectedIndex(int index)
{
	if (m_Buttons.empty())
	{
		m_SelectedIndex = 0;
		return;
	}

	m_SelectedIndex = NormalizeIndex(index);
	for (int button_index = 0; button_index < static_cast<int>(m_Buttons.size()); ++button_index)
	{
		if (m_Buttons[button_index])
		{
			m_Buttons[button_index]->SetSelected(button_index == m_SelectedIndex);
		}
	}
}

int ButtonMenuController::GetSelectedIndex() const
{
	return m_Buttons.empty() ? NO_ACTIVATION : m_SelectedIndex;
}

void ButtonMenuController::ResetPointerTracking()
{
	m_LastMouseX = InputMouse_GetX();
	m_LastMouseY = InputMouse_GetY();
}

int ButtonMenuController::NormalizeIndex(int index) const
{
	const int button_count = static_cast<int>(m_Buttons.size());
	if (m_Boundary == ButtonMenuBoundary::Clamp)
	{
		return std::clamp(index, 0, button_count - 1);
	}
	return (index % button_count + button_count) % button_count;
}

void ButtonMenuController::MoveSelection(int offset)
{
	SetSelectedIndex(m_SelectedIndex + offset);
}
