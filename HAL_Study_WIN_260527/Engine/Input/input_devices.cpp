#include "input_keyboard.h"
#include "input_mouse.h"
#include "input_xinput.h"
#include <algorithm>
#include <cstring>

namespace
{
	// 키보드와 게임패드에서 공유하며 기존 반복 입력 간격을 유지한다.
	bool ConsumeRepeat(float elapsed, int& trigger_count, float delay, float interval)
	{
		if (elapsed <= 0.0f)
			return false;
		if (trigger_count == 0)
		{
			trigger_count = 1;
			return true;
		}
		if (elapsed >= delay)
		{
			const int target_count = 1 + static_cast<int>((elapsed - delay) / interval);
			if (target_count > trigger_count)
			{
				trigger_count = target_count;
				return true;
			}
		}
		return false;
	}
} // namespace

/*==============================================================================

   キーボード入力ラッパー [input_keyboard.cpp]
														 Author : Youhei Sato
--------------------------------------------------------------------------------

==============================================================================*/

// キーリピート管理用の最大キー数
static const int KEY_MAX = 0xFF;

// 現在フレームのキーボード状態
static Keyboard_State gCurrKeyState = {};

// 前フレームのキーボード状態
static Keyboard_State gPrevKeyState = {};

// 各キーの押下時間（秒、リピート用）
static float gKeyRepeatTimer[KEY_MAX] = {};

// リピートトリガーが発火した回数（インターバル制御用）
static int gKeyRepeatTriggerCount[KEY_MAX] = {};

// ========== 初期化 / 更新 ==========

void InputKeyboard_Initialize(void)
{
	Keyboard_Reset();
	std::memset(&gCurrKeyState, 0, sizeof(gCurrKeyState));
	std::memset(&gPrevKeyState, 0, sizeof(gPrevKeyState));
	std::memset(gKeyRepeatTimer, 0, sizeof(gKeyRepeatTimer));
	std::memset(gKeyRepeatTriggerCount, 0, sizeof(gKeyRepeatTriggerCount));
}

void InputKeyboard_Update(float delta_time)
{
	// 前フレームの状態を保存
	gPrevKeyState = gCurrKeyState;

	// 現在の状態を取得
	gCurrKeyState = *Keyboard_GetState();

	// --- キーリピートタイマーの更新 ---
	for (int i = 0; i < KEY_MAX; ++i)
	{
		Keyboard_Keys k = static_cast<Keyboard_Keys>(i);
		if (Keyboard_IsKeyDown(k, &gCurrKeyState))
		{
			gKeyRepeatTimer[i] += delta_time;
		}
		else
		{
			gKeyRepeatTimer[i] = 0.0f;
			gKeyRepeatTriggerCount[i] = 0;
		}
	}
}

// ========== 判定処理 ==========

bool InputKeyboard_IsPress(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key, &gCurrKeyState);
}

bool InputKeyboard_IsTrigger(Keyboard_Keys key)
{
	bool prev = Keyboard_IsKeyDown(key, &gPrevKeyState);
	bool curr = Keyboard_IsKeyDown(key, &gCurrKeyState);
	return (!prev && curr);
}

bool InputKeyboard_IsRelease(Keyboard_Keys key)
{
	bool prev = Keyboard_IsKeyDown(key, &gPrevKeyState);
	bool curr = Keyboard_IsKeyDown(key, &gCurrKeyState);
	return (prev && !curr);
}

bool InputKeyboard_IsRepeat(Keyboard_Keys key, float delay_seconds, float interval_seconds)
{
	int idx = static_cast<int>(key);
	if (idx < 0 || idx >= KEY_MAX)
	{
		return false;
	}

	return ConsumeRepeat(gKeyRepeatTimer[idx], gKeyRepeatTriggerCount[idx], delay_seconds, interval_seconds);
}

/*==============================================================================

   マウス入力ラッパー [input_mouse.cpp]
														 Author : Youhei Sato
--------------------------------------------------------------------------------

==============================================================================*/

// 前フレームのマウスボタン状態
static bool gPrevMouseButton[3] = {};

// 現在フレームのマウス状態
static Mouse_State gMouseState = {};

// ========== 初期化 / 更新 / 終了 ==========

void InputMouse_Initialize(HWND hWnd)
{
	Mouse_Initialize(hWnd);
	std::memset(&gMouseState, 0, sizeof(gMouseState));
	std::memset(gPrevMouseButton, 0, sizeof(gPrevMouseButton));
}

void InputMouse_Update(void)
{
	// 前フレームのボタン状態を保存
	gPrevMouseButton[MOUSE_BUTTON_LEFT] = gMouseState.leftButton;
	gPrevMouseButton[MOUSE_BUTTON_RIGHT] = gMouseState.rightButton;
	gPrevMouseButton[MOUSE_BUTTON_MIDDLE] = gMouseState.middleButton;

	// 現在のマウス状態を取得
	Mouse_GetState(&gMouseState);

	// スクロールホイール値のリセットを自動化
	Mouse_ResetScrollWheelValue();
}

void InputMouse_Finalize(void)
{
	Mouse_Finalize();
}

// ========== 判定処理 ==========

bool InputMouse_IsPress(MouseButton button)
{
	switch (button)
	{
	case MOUSE_BUTTON_LEFT:
		return gMouseState.leftButton;
	case MOUSE_BUTTON_RIGHT:
		return gMouseState.rightButton;
	case MOUSE_BUTTON_MIDDLE:
		return gMouseState.middleButton;
	default:
		return false;
	}
}

bool InputMouse_IsTrigger(MouseButton button)
{
	if (button < 0 || button >= 3)
		return false;
	bool prev = gPrevMouseButton[button];
	bool curr = InputMouse_IsPress(button);
	return (!prev && curr);
}

bool InputMouse_IsRelease(MouseButton button)
{
	if (button < 0 || button >= 3)
		return false;
	bool prev = gPrevMouseButton[button];
	bool curr = InputMouse_IsPress(button);
	return (prev && !curr);
}

int InputMouse_GetX(void)
{
	return gMouseState.x;
}

int InputMouse_GetY(void)
{
	return gMouseState.y;
}

int InputMouse_GetScrollWheel(void)
{
	return gMouseState.scrollWheelValue;
}

void InputMouse_SetMode(Mouse_PositionMode mode)
{
	Mouse_SetMode(mode);
}

void InputMouse_SetVisible(bool visible)
{
	Mouse_SetVisible(visible);
}

/*==============================================================================

   XInput ゲームパッド入力ラッパー [input_xinput.cpp]
														 Author : Youhei Sato
--------------------------------------------------------------------------------

==============================================================================*/

// XInputライブラリのリンク指定
#pragma comment(lib, "Xinput.lib")

// 最大コントローラー数 (XInputの仕様で最大4台)
static const int PAD_MAX = 4;
// デジタルボタン数 (16ビットのフラグ)
static const int BUTTON_MAX = 16;

// コントローラーの接続状態と状態データ
static bool gIsConnected[PAD_MAX] = {};
static XINPUT_STATE gCurrState[PAD_MAX] = {};
static XINPUT_STATE gPrevState[PAD_MAX] = {};

// ボタンリピート用のタイマーと発火回数
static float gRepeatTimer[PAD_MAX][BUTTON_MAX] = {};
static int gRepeatTriggerCount[PAD_MAX][BUTTON_MAX] = {};

// 各コントローラーの振動設定値（Update の最後に適用）
static XINPUT_VIBRATION gVibration[PAD_MAX] = {};

// ビットフラグからインデックス (0 ～ 15) への変換
static int ButtonBitToIndex(WORD buttonBitMask)
{
	for (int i = 0; i < BUTTON_MAX; ++i)
	{
		if (buttonBitMask & (1 << i))
		{
			return i;
		}
	}
	return -1;
}

// ========== 初期化 / 更新 ==========

void InputXInput_Initialize(void)
{
	std::memset(gIsConnected, 0, sizeof(gIsConnected));
	std::memset(gCurrState, 0, sizeof(gCurrState));
	std::memset(gPrevState, 0, sizeof(gPrevState));
	std::memset(gRepeatTimer, 0, sizeof(gRepeatTimer));
	std::memset(gRepeatTriggerCount, 0, sizeof(gRepeatTriggerCount));
	std::memset(gVibration, 0, sizeof(gVibration));
}

void InputXInput_Update(float delta_time)
{
	for (int pad = 0; pad < PAD_MAX; ++pad)
	{
		gPrevState[pad] = gCurrState[pad];

		// 状態取得
		XINPUT_STATE state = {};
		DWORD result = XInputGetState(pad, &state);

		if (result == ERROR_SUCCESS)
		{
			gIsConnected[pad] = true;
			gCurrState[pad] = state;

			// ボタンリピートタイマーの更新
			for (int btn = 0; btn < BUTTON_MAX; ++btn)
			{
				WORD mask = static_cast<WORD>(1 << btn);
				if (state.Gamepad.wButtons & mask)
				{
					gRepeatTimer[pad][btn] += delta_time;
				}
				else
				{
					gRepeatTimer[pad][btn] = 0.0f;
					gRepeatTriggerCount[pad][btn] = 0;
				}
			}
		}
		else
		{
			// 切断された場合はクリア
			gIsConnected[pad] = false;
			std::memset(&gCurrState[pad], 0, sizeof(XINPUT_STATE));
			std::memset(gRepeatTimer[pad], 0, sizeof(gRepeatTimer[pad]));
			std::memset(gRepeatTriggerCount[pad], 0, sizeof(gRepeatTriggerCount[pad]));
			std::memset(&gVibration[pad], 0, sizeof(XINPUT_VIBRATION));
		}

		// 振動を適用
		XInputSetState(pad, &gVibration[pad]);
	}
}

// ========== 接続確認 / ボタン判定 ==========

bool InputXInput_IsConnected(int userIndex)
{
	if (userIndex < 0 || userIndex >= PAD_MAX)
		return false;
	return gIsConnected[userIndex];
}

bool InputXInput_IsPress(int userIndex, WORD buttonBitMask)
{
	if (!InputXInput_IsConnected(userIndex))
		return false;
	return (gCurrState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
}

bool InputXInput_IsTrigger(int userIndex, WORD buttonBitMask)
{
	if (!InputXInput_IsConnected(userIndex))
		return false;
	bool prev = (gPrevState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
	bool curr = (gCurrState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
	return (!prev && curr);
}

bool InputXInput_IsRelease(int userIndex, WORD buttonBitMask)
{
	if (!InputXInput_IsConnected(userIndex))
		return false;
	bool prev = (gPrevState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
	bool curr = (gCurrState[userIndex].Gamepad.wButtons & buttonBitMask) != 0;
	return (prev && !curr);
}

bool InputXInput_IsRepeat(int userIndex, WORD buttonBitMask, float delay_seconds, float interval_seconds)
{
	if (!InputXInput_IsConnected(userIndex))
		return false;
	int btnIdx = ButtonBitToIndex(buttonBitMask);
	if (btnIdx < 0 || btnIdx >= BUTTON_MAX)
		return false;

	return ConsumeRepeat(gRepeatTimer[userIndex][btnIdx], gRepeatTriggerCount[userIndex][btnIdx], delay_seconds,
	                     interval_seconds);
}

// ========== アナログスティック (スムーズなデッドゾーンスケーリング) ==========

static void ApplyDeadzoneScaling(short rawVal, short deadzone, float& outNormalized)
{
	float raw = static_cast<float>(rawVal);
	if (raw < -32767.0f)
		raw = -32767.0f;

	float dz = static_cast<float>(deadzone);

	if (raw < -dz)
	{
		outNormalized = (raw + dz) / (32767.0f - dz);
	}
	else if (raw > dz)
	{
		outNormalized = (raw - dz) / (32767.0f - dz);
	}
	else
	{
		outNormalized = 0.0f;
	}
}

float InputXInput_GetLeftStickX(int userIndex)
{
	if (!InputXInput_IsConnected(userIndex))
		return 0.0f;
	float val = 0.0f;
	ApplyDeadzoneScaling(gCurrState[userIndex].Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, val);
	return val;
}

float InputXInput_GetLeftStickY(int userIndex)
{
	if (!InputXInput_IsConnected(userIndex))
		return 0.0f;
	float val = 0.0f;
	ApplyDeadzoneScaling(gCurrState[userIndex].Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, val);
	return val;
}

float InputXInput_GetRightStickX(int userIndex)
{
	if (!InputXInput_IsConnected(userIndex))
		return 0.0f;
	float val = 0.0f;
	ApplyDeadzoneScaling(gCurrState[userIndex].Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, val);
	return val;
}

float InputXInput_GetRightStickY(int userIndex)
{
	if (!InputXInput_IsConnected(userIndex))
		return 0.0f;
	float val = 0.0f;
	ApplyDeadzoneScaling(gCurrState[userIndex].Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, val);
	return val;
}

// ========== トリガー入力 ==========

float InputXInput_GetLeftTrigger(int userIndex)
{
	if (!InputXInput_IsConnected(userIndex))
		return 0.0f;
	BYTE raw = gCurrState[userIndex].Gamepad.bLeftTrigger;
	if (raw < XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
		return 0.0f;
	return static_cast<float>(raw - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) / (255.0f - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}

float InputXInput_GetRightTrigger(int userIndex)
{
	if (!InputXInput_IsConnected(userIndex))
		return 0.0f;
	BYTE raw = gCurrState[userIndex].Gamepad.bRightTrigger;
	if (raw < XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
		return 0.0f;
	return static_cast<float>(raw - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) / (255.0f - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}

// ========== 振動制御 ==========

void InputXInput_SetLeftVibration(int userIndex, float level)
{
	if (userIndex < 0 || userIndex >= PAD_MAX)
		return;
	float val = std::max(0.0f, std::min(1.0f, level));
	gVibration[userIndex].wLeftMotorSpeed = static_cast<WORD>(val * 65535.0f);
}

void InputXInput_SetRightVibration(int userIndex, float level)
{
	if (userIndex < 0 || userIndex >= PAD_MAX)
		return;
	float val = std::max(0.0f, std::min(1.0f, level));
	gVibration[userIndex].wRightMotorSpeed = static_cast<WORD>(val * 65535.0f);
}
