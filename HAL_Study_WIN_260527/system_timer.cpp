/*==============================================================================

   システムタイマー [system_timer.cpp]
                                                         Author : Youhei Sato
                                                         Date   : 2018/06/17
--------------------------------------------------------------------------------

==============================================================================*/
#include <Windows.h>


/*------------------------------------------------------------------------------
   グローバル変数
------------------------------------------------------------------------------*/
static bool g_bTimerStopped = true; // ストップフラグ
static LONGLONG g_TicksPerSec = 0;  // 1秒間の周波数
static LONGLONG g_StopTime;         // ストップ時の時間
static LONGLONG g_LastElapsedTime;  // 前回計測した時間
static LONGLONG g_BaseTime;         // 基準時間


/*------------------------------------------------------------------------------
   プロトタイプ宣言
------------------------------------------------------------------------------*/
// 停止中であれば停止時間を、動作中であれば現在時間を取得
static LARGE_INTEGER GetAdjustedCurrentTime(void);


/*------------------------------------------------------------------------------
   関数定義
------------------------------------------------------------------------------*/

// システムタイマーの初期化
void SystemTimer_Initialize(void)
{
    g_bTimerStopped = true;
    g_TicksPerSec = 0;
    g_StopTime = 0;
    g_LastElapsedTime = 0;
    g_BaseTime = 0;

    // 高精度パフォーマンスカウンタの周波数を取得
    LARGE_INTEGER ticksPerSec = { 0 };
    QueryPerformanceFrequency(&ticksPerSec);
    g_TicksPerSec = ticksPerSec.QuadPart;
}

// システムタイマーのリセット
void SystemTimer_Reset(void)
{
    LARGE_INTEGER time = GetAdjustedCurrentTime();

    g_BaseTime = g_LastElapsedTime = time.QuadPart;
    g_StopTime = 0;
    g_bTimerStopped = false;
}

// システムタイマーのスタート
void SystemTimer_Start(void)
{
    // 現在時間を取得
    LARGE_INTEGER time = { 0 };
    QueryPerformanceCounter(&time);

    // 停止していたタイマーを再スタートする場合
    if( g_bTimerStopped ) {
        // 停止していた時間分だけ基準時間を更新
        g_BaseTime += time.QuadPart - g_StopTime;
    }

    g_StopTime = 0;
    g_LastElapsedTime = time.QuadPart;
    g_bTimerStopped = false;
}

// システムタイマーのストップ
void SystemTimer_Stop(void)
{
    if( g_bTimerStopped ) return;

    LARGE_INTEGER time = { 0 };
    QueryPerformanceCounter(&time);

    g_LastElapsedTime = g_StopTime = time.QuadPart; // 停止時間を記録
    g_bTimerStopped = true;
}

// システムタイマーを0.1秒進める
void SystemTimer_Advance(void)
{
    g_StopTime += g_TicksPerSec / 10;
}

// 起動してからの経過時間を取得
double SystemTimer_GetTime(void)
{
    LARGE_INTEGER time = GetAdjustedCurrentTime();

    return (double)(time.QuadPart - g_BaseTime) / (double)g_TicksPerSec;
}

// 絶対時間を取得
double SystemTimer_GetAbsoluteTime(void)
{
    LARGE_INTEGER time = { 0 };
    QueryPerformanceCounter(&time);

    return time.QuadPart / (double)g_TicksPerSec;
}

// 前回の呼び出しからの経過時間を取得（フレーム間差分）
double SystemTimer_GetElapsedTime(void)
{
    LARGE_INTEGER time = GetAdjustedCurrentTime();

    double elapsed_time = (double)(time.QuadPart - g_LastElapsedTime) / (double)g_TicksPerSec;
    g_LastElapsedTime = time.QuadPart;

    // タイマーの巻き戻り現象対策（PCの低電力モードや別コアへのスレッド移動により起こり得る）
    // elapsed_timeが負になった場合、前回のフレームより前の時間に戻っていることを示す。
    // これを防ぐため、メインスレッドで SetThreadAffinityMask を呼び出して実行コアを固定することが推奨される。
    // ワーカープロセッサ間のタイマーの同期ずれから生じる時間の逆流を防ぐための安全装置。
    if( elapsed_time < 0.0 ) {
        elapsed_time = 0.0;
    }

    return elapsed_time;
}

// システムタイマーが停止しているか？
bool SystemTimer_IsStoped(void)
{
    return g_bTimerStopped;
}

// 現在のスレッドの実行を特定のプロセッサコア（メインコア）に制限する
void LimitThreadAffinityToCurrentProc(void)
{
    HANDLE hCurrentProcess = GetCurrentProcess();

    // プロセスのアフィニティマスクを取得
    DWORD_PTR dwProcessAffinityMask = 0;
    DWORD_PTR dwSystemAffinityMask = 0;

    if( GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinityMask, &dwSystemAffinityMask) != 0 && dwProcessAffinityMask ) {
        // プロセスが実行を許可されている最も下位のコアを選択
        DWORD_PTR dwAffinityMask = (dwProcessAffinityMask & ((~dwProcessAffinityMask) + 1));

        // 現在実行中のスレッドをその特定のコアに固定
        // これはプロセスのアフィニティマスクのサブセットでなければならない
        HANDLE hCurrentThread = GetCurrentThread();
        if( INVALID_HANDLE_VALUE != hCurrentThread ) {
            SetThreadAffinityMask(hCurrentThread, dwAffinityMask);
        }
    }
}

// 停止中であれば停止時間を、動作中であれば現在時間を取得
LARGE_INTEGER GetAdjustedCurrentTime(void)
{
    LARGE_INTEGER time;
    if( g_StopTime != 0 ) {
        time.QuadPart = g_StopTime;
    }
    else {
        QueryPerformanceCounter(&time);
    }

    return time;
}