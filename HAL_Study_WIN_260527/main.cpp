#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <combaseapi.h>
#include <algorithm>
#include "config.h"
#include "application.h"
#include "direct3d.h"
#include "debug_text.h"
#include "system_timer.h"
#include <iomanip>
#include <sstream>
#include "keyboard.h"
#include "mouse.h"

/*------------------------------------------------------------------------------
 ウィンドウ情報
------------------------------------------------------------------------------*/
static constexpr char WINDOW_CLASS[] = "GameWindow"; // メインウィンドウクラス名
static constexpr char TITLE[] = "HelloWorld"; // タイトルバーのテキスト
/*------------------------------------------------------------------------------
 ウィンドウプロシージャ プロトタイプ宣⾔
------------------------------------------------------------------------------*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
/*------------------------------------------------------------------------------
 メイン
------------------------------------------------------------------------------*/
int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	/* ウィンドウクラスの登録 */
	WNDCLASSEX wcex{};
	wcex.cbSize = sizeof(WNDCLASSEX);
	// wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr; // メニューは作らない
	wcex.lpszClassName = WINDOW_CLASS;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	RegisterClassEx(&wcex);

	// クライアント領域のサイズを持った矩形 (左からleft, top, right, bottom)
	RECT window_rect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	// ウィンドウのスタイル
	constexpr DWORD WINDOW_STYLE = WS_OVERLAPPEDWINDOW ^ (WS_THICKFRAME | WS_MAXIMIZEBOX);
	// 指定したクライアント領域を確保するために新たな矩形座標を計算
	AdjustWindowRect(&window_rect, WINDOW_STYLE, FALSE);

	const int WINDOW_WIDTH{ window_rect.right - window_rect.left };
	const int WINDOW_HEIGHT{ window_rect.bottom - window_rect.top };

	// プライマリモニターの画面解像度取得
	const int DESKTOP_WIDTH = GetSystemMetrics(SM_CXSCREEN);
	const int DESKTOP_HEIGHT = GetSystemMetrics(SM_CYSCREEN);
	// デスクトップの真ん中にウィンドウが生成されるように座標を計算
	// ※ただし万が一、デスクトップよりウィンドウが大きい場合は左上に表示
	const int WINDOW_X = std::max((DESKTOP_WIDTH - WINDOW_WIDTH) / 2, 0);
	const int WINDOW_Y = std::max((DESKTOP_HEIGHT - WINDOW_HEIGHT) / 2, 0);

	/* メインウィンドウの作成 */
	HWND hWnd = CreateWindow(
		WINDOW_CLASS,
		TITLE,
		WINDOW_STYLE,
		WINDOW_X, WINDOW_Y,
		WINDOW_WIDTH, WINDOW_HEIGHT,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (!Application_Initialize(hWnd))
	{
		return WM_QUIT;
	}

#ifdef _DEBUG
	hal::DebugText debug_text(Direct3D_GetDevice(), Direct3D_GetDeviceContext(), L"asset/texture/Sixtyfour-Regular_ascii_512.png", SCREEN_WIDTH, SCREEN_HEIGHT);
#endif

	SystemTimer_Initialize();
	LimitThreadAffinityToCurrentProc();
	SystemTimer_Start();

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	/* メッセージループ */
	MSG msg{}; // FPS表示 からの修正: 構造体をゼロクリア初期化
	// FPS表示 からの追加: 計測用変数
	double elapsed_time = 0.0; // 1フレームの経過時間（秒）
	double time_accumulator = 0.0; // 経過時間の累積値
	int frame_counter = 0; // フレーム数の累積カウント
	double fps = 0.0; // 算出されたFPS値
	static constexpr double TARGET_FPS = 60.0;
	static constexpr double FIXED_DELTA_TIME = 1.0 / TARGET_FPS; // 約0.016666秒
	double fixed_time_accumulator = 0.0;

	do {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) 
		{ // ウィンドウメッセージが来ていたら
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else 
		{ 
			// FPS表示 からの追加: 前回のフレームからの経過時間（秒）を取得
			elapsed_time = SystemTimer_GetElapsedTime();
			if (elapsed_time > 0.1)
			{
				elapsed_time = 0.1; // 最大でも100ms分として処理する制限
			}
			
			time_accumulator += elapsed_time;
			frame_counter++;
			
			// 1.0秒経過したら、その間のフレーム数からFPSを確定して累積をリセット
			if (time_accumulator >= 1.0) 
			{
				fps = frame_counter / time_accumulator;
				frame_counter = 0;
				time_accumulator = 0.0;
			}

			// ゲームの処理
			Application_Update((float)elapsed_time);

			fixed_time_accumulator += elapsed_time;

			int update_count = 0;
			while (fixed_time_accumulator >= FIXED_DELTA_TIME && update_count < 5) 
			{
				Application_FixedUpdate();
				fixed_time_accumulator -= FIXED_DELTA_TIME;
				update_count++;
			}

			Direct3D_DrawBegin();

			Application_Draw();

#ifdef _DEBUG
			debug_text.Clear();
			// FPS表示 からの追加: 計測されたFPSとフレーム経過時間(ms)をstd::stringstreamで整形
			std::stringstream ss;
			ss << "FPS: " << std::fixed << std::setprecision(2) << fps << " (" << std::fixed << std::setprecision(2) << (elapsed_time * 1000.0) << " ms)";
			debug_text.SetText(ss.str().c_str());
			debug_text.Draw();
#endif


			Direct3D_Present();
		}
	} while (msg.message != WM_QUIT);

	Application_Finalize();

	return (int)msg.wParam;
}

/*------------------------------------------------------------------------------
 ウィンドウプロシージャ
------------------------------------------------------------------------------*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rc{};

	switch (message)
	{
	case WM_DESTROY: // ウィンドウの破棄メッセージ
		PostQuitMessage(0); // WM_QUITメッセージの送信
		break;
	case WM_CLOSE: // ウィンドウを閉じるメッセージ
		if (MessageBox(hWnd, "本当に終了してよろしいですか？", "確認", MB_OKCANCEL | MB_DEFBUTTON2) == IDOK)
		{
			DestroyWindow(hWnd); // 指定のウィンドウにWM_DESTROYメッセージを送る
		}
		break; // DefWindowProc関数にメッセージを流さず終了することによって何もなかったことにする

	case WM_ACTIVATEAPP:
		Keyboard_ProcessMessage(message, wParam, lParam);
		Mouse_ProcessMessage(message, wParam, lParam);
		break;

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			SendMessage(hWnd, WM_CLOSE, 0, 0); // WM_CLOSEメッセージの送信
		}
		[[fallthrough]];
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Keyboard_ProcessMessage(message, wParam, lParam);
		break;
	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		Mouse_ProcessMessage(message, wParam, lParam);
		break;

	default:
		// 通常のメッセージ処理はこの関数に任せる
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}