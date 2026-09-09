#include "keyboard.h"
#include "mouse.h"
#include <assert.h>
#include <windowsx.h>

//--------------------------------------------------------------------------------------
// File: Keyboard.cpp
//
// キーボードモジュール
//
//--------------------------------------------------------------------------------------
// 2020/06/07
//     DirectXTKより、なんちゃってC言語用にシェイプアップ改変
//
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

static_assert(sizeof(Keyboard_State) == 256 / 8, "キーボード状態構造体のサイズ不一致");

static Keyboard_State gState = {};

static void keyDown(int key)
{
	if (key < 0 || key > 0xfe)
	{
		return;
	}

	unsigned int* p = (unsigned int*)&gState;
	unsigned int bf = 1u << (key & 0x1f);
	p[(key >> 5)] |= bf;
}

static void keyUp(int key)
{
	if (key < 0 || key > 0xfe)
	{
		return;
	}

	unsigned int* p = (unsigned int*)&gState;
	unsigned int bf = 1u << (key & 0x1f);
	p[(key >> 5)] &= ~bf;
}

void Keyboard_Initialize(void)
{
	Keyboard_Reset();
}

bool Keyboard_IsKeyDown(Keyboard_Keys key, const Keyboard_State* pState)
{
	if (key <= 0xfe)
	{
		unsigned int* p = (unsigned int*)pState;
		unsigned int bf = 1u << (key & 0x1f);
		return (p[(key >> 5)] & bf) != 0;
	}
	return false;
}

bool Keyboard_IsKeyUp(Keyboard_Keys key, const Keyboard_State* pState)
{
	if (key <= 0xfe)
	{
		unsigned int* p = (unsigned int*)pState;
		unsigned int bf = 1u << (key & 0x1f);
		return (p[(key >> 5)] & bf) == 0;
	}
	return false;
}

bool Keyboard_IsKeyDown(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key, &gState);
}

bool Keyboard_IsKeyUp(Keyboard_Keys key)
{
	return Keyboard_IsKeyUp(key, &gState);
}

// キーボードの現在の状態を取得する
const Keyboard_State* Keyboard_GetState(void)
{
	return &gState;
}

void Keyboard_Reset(void)
{
	ZeroMemory(&gState, sizeof(Keyboard_State));
}

// キーボード制御のためのウォンどうメッセージプロシージャフック関数
void Keyboard_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	bool down = false;

	switch (message)
	{
	case WM_ACTIVATEAPP:
		Keyboard_Reset();
		return;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		down = true;
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		break;

	default:
		return;
	}

	int vk = (int)wParam;
	switch (vk)
	{
	case VK_SHIFT:
		vk = (int)MapVirtualKey(((unsigned int)lParam & 0x00ff0000) >> 16u, MAPVK_VSC_TO_VK_EX);
		if (!down)
		{
			// 左シフトと右シフトの両方が同時に押された場合にクリアされるようにするための回避策
			keyUp(VK_LSHIFT);
			keyUp(VK_RSHIFT);
		}
		break;

	case VK_CONTROL:
		vk = ((UINT)lParam & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
		break;

	case VK_MENU:
		vk = ((UINT)lParam & 0x01000000) ? VK_RMENU : VK_LMENU;
		break;
	}

	if (down)
	{
		keyDown(vk);
	}
	else
	{
		keyUp(vk);
	}
}

//--------------------------------------------------------------------------------------
// File: mouse.cpp
//
// 便利なマウスモジュール
//
//--------------------------------------------------------------------------------------
// 2020/02/11
//     DirectXTKより、なんちゃってC言語用にシェイプアップ改変
//
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#define SAFE_CLOSEHANDLE(h)                                                                                            \
	if (h)                                                                                                             \
	{                                                                                                                  \
		CloseHandle(h);                                                                                                \
		h = NULL;                                                                                                      \
	}

static Mouse_State gMouseState = {};
static HWND gWindow = NULL;
static Mouse_PositionMode gMode = MOUSE_POSITION_MODE_ABSOLUTE;
static HANDLE gScrollWheelValue = NULL;
static HANDLE gRelativeRead = NULL;
static HANDLE gAbsoluteMode = NULL;
static HANDLE gRelativeMode = NULL;
static int gLastX = 0;
static int gLastY = 0;
static int gRelativeX = INT32_MAX;
static int gRelativeY = INT32_MAX;
static bool gInFocus = true;
static bool gVisibleRequested = true;

static void clipToWindow(void);
static void setCursorVisibility(bool visible);

void Mouse_Initialize(HWND window)
{
	RtlZeroMemory(&gMouseState, sizeof(gMouseState));

	assert(window != NULL);

	RAWINPUTDEVICE Rid;
	Rid.usUsagePage = 0x01 /* HID_USAGE_PAGE_GENERIC */;
	Rid.usUsage = 0x02 /* HID_USAGE_GENERIC_MOUSE */;
	Rid.dwFlags = RIDEV_INPUTSINK;
	Rid.hwndTarget = window;
	RegisterRawInputDevices(&Rid, 1, sizeof(RAWINPUTDEVICE));

	gWindow = window;
	gMode = MOUSE_POSITION_MODE_ABSOLUTE;

	if (!gScrollWheelValue)
	{
		gScrollWheelValue =
		    CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
	}
	if (!gRelativeRead)
	{
		gRelativeRead = CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
	}
	if (!gAbsoluteMode)
	{
		gAbsoluteMode = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	}
	if (!gRelativeMode)
	{
		gRelativeMode = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	}

	gLastX = 0;
	gLastY = 0;
	gRelativeX = INT32_MAX;
	gRelativeY = INT32_MAX;

	gInFocus = true;
	gVisibleRequested = true;
}

void Mouse_Finalize(void)
{
	SAFE_CLOSEHANDLE(gScrollWheelValue);
	SAFE_CLOSEHANDLE(gRelativeRead);
	SAFE_CLOSEHANDLE(gAbsoluteMode);
	SAFE_CLOSEHANDLE(gRelativeMode);
}

void Mouse_GetState(Mouse_State* pState)
{
	memcpy(pState, &gMouseState, sizeof(gMouseState));
	pState->positionMode = gMode;

	DWORD Result = WaitForSingleObjectEx(gScrollWheelValue, 0, FALSE);
	if (Result == WAIT_FAILED)
	{
		return;
	}

	if (Result == WAIT_OBJECT_0)
	{

		pState->scrollWheelValue = 0;
	}

	if (pState->positionMode == MOUSE_POSITION_MODE_RELATIVE)
	{

		Result = WaitForSingleObjectEx(gRelativeRead, 0, FALSE);
		if (Result == WAIT_FAILED)
		{
			return;
		}

		if (Result == WAIT_OBJECT_0)
		{
			pState->x = 0;
			pState->y = 0;
		}
		else
		{
			SetEvent(gRelativeRead);
		}
	}
}

void Mouse_ResetScrollWheelValue(void)
{
	SetEvent(gScrollWheelValue);
}

void Mouse_SetMode(Mouse_PositionMode mode)
{
	if (gMode == mode)
	{
		return;
	}

	SetEvent((mode == MOUSE_POSITION_MODE_ABSOLUTE) ? gAbsoluteMode : gRelativeMode);

	assert(gWindow != NULL);

	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(tme);
	tme.dwFlags = TME_HOVER;
	tme.hwndTrack = gWindow;
	tme.dwHoverTime = 1;
	TrackMouseEvent(&tme);
}

bool Mouse_IsConnected(void)
{
	return GetSystemMetrics(SM_MOUSEPRESENT) != 0;
}

bool Mouse_IsVisible(void)
{
	if (gMode == MOUSE_POSITION_MODE_RELATIVE)
	{
		return false;
	}

	CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
	GetCursorInfo(&info);

	return (info.flags & CURSOR_SHOWING) != 0;
}

void Mouse_SetVisible(bool visible)
{
	gVisibleRequested = visible;

	if (gMode == MOUSE_POSITION_MODE_RELATIVE)
	{
		return;
	}

	if (!gInFocus)
	{
		return;
	}

	setCursorVisibility(visible);
}

static void setCursorVisibility(bool visible)
{
	CURSORINFO info = { sizeof(CURSORINFO), 0, nullptr, {} };
	GetCursorInfo(&info);

	bool isVisible = (info.flags & CURSOR_SHOWING) != 0;

	if (isVisible != visible)
	{
		ShowCursor(visible);
	}
}

void Mouse_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	HANDLE evts[3] = { gScrollWheelValue, gAbsoluteMode, gRelativeMode };

	switch (WaitForMultipleObjectsEx(_countof(evts), evts, FALSE, 0, FALSE))
	{
	case WAIT_OBJECT_0:
		gMouseState.scrollWheelValue = 0;
		ResetEvent(evts[0]);
		break;

	case (WAIT_OBJECT_0 + 1):
	{
		gMode = MOUSE_POSITION_MODE_ABSOLUTE;
		ClipCursor(nullptr);

		POINT point;
		point.x = gLastX;
		point.y = gLastY;

		// リモートディスクトップに対応するために移動前にカーソルを表示する
		ShowCursor(TRUE);

		if (MapWindowPoints(gWindow, nullptr, &point, 1))
		{
			SetCursorPos(point.x, point.y);
		}

		gMouseState.x = gLastX;
		gMouseState.y = gLastY;
	}
	break;

	case (WAIT_OBJECT_0 + 2):
	{
		ResetEvent(gRelativeRead);

		gMode = MOUSE_POSITION_MODE_RELATIVE;
		gMouseState.x = gMouseState.y = 0;
		gRelativeX = INT32_MAX;
		gRelativeY = INT32_MAX;

		ShowCursor(FALSE);

		clipToWindow();
	}
	break;

	case WAIT_FAILED:
		return;
	}

	switch (message)
	{
	case WM_ACTIVATEAPP:
		if (wParam)
		{

			gInFocus = true;

			if (gMode == MOUSE_POSITION_MODE_RELATIVE)
			{

				gMouseState.x = gMouseState.y = 0;
				ShowCursor(FALSE);
				clipToWindow();
			}
			else
			{
				setCursorVisibility(gVisibleRequested);
			}
		}
		else
		{
			int scrollWheel = gMouseState.scrollWheelValue;
			memset(&gMouseState, 0, sizeof(gMouseState));
			gMouseState.scrollWheelValue = scrollWheel;
			gInFocus = false;
			setCursorVisibility(true);
		}
		return;

	case WM_INPUT:
		if (gInFocus && gMode == MOUSE_POSITION_MODE_RELATIVE)
		{

			RAWINPUT raw;
			UINT rawSize = sizeof(raw);

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &rawSize, sizeof(RAWINPUTHEADER));

			if (raw.header.dwType == RIM_TYPEMOUSE)
			{

				if (!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
				{

					gMouseState.x = raw.data.mouse.lLastX;
					gMouseState.y = raw.data.mouse.lLastY;

					ResetEvent(gRelativeRead);
				}
				else if (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP)
				{

					// リモートディスクトップなどに対応
					const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
					const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

					int x = (int)((raw.data.mouse.lLastX / 65535.0f) * width);
					int y = (int)((raw.data.mouse.lLastY / 65535.0f) * height);

					if (gRelativeX == INT32_MAX)
					{
						gMouseState.x = gMouseState.y = 0;
					}
					else
					{
						gMouseState.x = x - gRelativeX;
						gMouseState.y = y - gRelativeY;
					}

					gRelativeX = x;
					gRelativeY = y;

					ResetEvent(gRelativeRead);
				}
			}
		}
		return;

	case WM_MOUSEMOVE:
		break;

	case WM_LBUTTONDOWN:
		gMouseState.leftButton = true;
		break;

	case WM_LBUTTONUP:
		gMouseState.leftButton = false;
		break;

	case WM_RBUTTONDOWN:
		gMouseState.rightButton = true;
		break;

	case WM_RBUTTONUP:
		gMouseState.rightButton = false;
		break;

	case WM_MBUTTONDOWN:
		gMouseState.middleButton = true;
		break;

	case WM_MBUTTONUP:
		gMouseState.middleButton = false;
		break;

	case WM_MOUSEWHEEL:
		gMouseState.scrollWheelValue += GET_WHEEL_DELTA_WPARAM(wParam);
		return;

	case WM_XBUTTONDOWN:
		switch (GET_XBUTTON_WPARAM(wParam))
		{
		case XBUTTON1:
			gMouseState.xButton1 = true;
			break;

		case XBUTTON2:
			gMouseState.xButton2 = true;
			break;
		}
		break;

	case WM_XBUTTONUP:
		switch (GET_XBUTTON_WPARAM(wParam))
		{
		case XBUTTON1:
			gMouseState.xButton1 = false;
			break;

		case XBUTTON2:
			gMouseState.xButton2 = false;
			break;
		}
		break;

	case WM_MOUSEHOVER:
		break;

	default:
		// マウスに対するメッセージは無かった…
		return;
	}

	if (gMode == MOUSE_POSITION_MODE_ABSOLUTE)
	{

		// すべてのマウスメッセージに対して新しい座標を取得する
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);

		gMouseState.x = gLastX = xPos;
		gMouseState.y = gLastY = yPos;
	}
}

void clipToWindow(void)
{
	assert(gWindow != NULL);

	RECT rect;
	GetClientRect(gWindow, &rect);

	POINT ul;
	ul.x = rect.left;
	ul.y = rect.top;

	POINT lr;
	lr.x = rect.right;
	lr.y = rect.bottom;

	MapWindowPoints(gWindow, NULL, &ul, 1);
	MapWindowPoints(gWindow, NULL, &lr, 1);

	rect.left = ul.x;
	rect.top = ul.y;

	rect.right = lr.x;
	rect.bottom = lr.y;

	ClipCursor(&rect);
}
