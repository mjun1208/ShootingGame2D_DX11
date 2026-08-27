/*==============================================================================

   システ??イ?? [system_timer.cpp]
                                                         Author : Youhei Sato
                                                         Date   : 2018/06/17
--------------------------------------------------------------------------------

==============================================================================*/
#include <Windows.h>


/*------------------------------------------------------------------------------
   グロ?バル変数
------------------------------------------------------------------------------*/
static bool g_bTimerStopped = true; // ストップフラグ
static LONGLONG g_TicksPerSec = 0;  // 1秒間の周波数
static LONGLONG g_StopTime;         // ストップ時の時間
static LONGLONG g_LastElapsedTime;  // 前回計測した時間
static LONGLONG g_BaseTime;         // 基?時間


/*------------------------------------------------------------------------------
   プロト?イプ宣言
------------------------------------------------------------------------------*/
// 停?中であれば停?時間を、動作中であれば現在時間を取得
static LARGE_INTEGER GetAdjustedCurrentTime(void);


/*------------------------------------------------------------------------------
   関数定?
------------------------------------------------------------------------------*/

// システ??イ??の初期化
void SystemTimer_Initialize(void)
{
    g_bTimerStopped = true;
    g_TicksPerSec = 0;
    g_StopTime = 0;
    g_LastElapsedTime = 0;
    g_BaseTime = 0;

    // 高精度パフォ??ンスカウン?の周波数を取得
    LARGE_INTEGER ticksPerSec = { 0 };
    QueryPerformanceFrequency(&ticksPerSec);
    g_TicksPerSec = ticksPerSec.QuadPart;
}

// システ??イ??のリセット
void SystemTimer_Reset(void)
{
    LARGE_INTEGER time = GetAdjustedCurrentTime();

    g_BaseTime = g_LastElapsedTime = time.QuadPart;
    g_StopTime = 0;
    g_bTimerStopped = false;
}

// システ??イ??のス??ト
void SystemTimer_Start(void)
{
    // 現在時間を取得
    LARGE_INTEGER time = { 0 };
    QueryPerformanceCounter(&time);

    // 停?していた?イ??を再ス??トする場合
    if( g_bTimerStopped ) {
        // 停?していた時間分だけ基?時間を更新
        g_BaseTime += time.QuadPart - g_StopTime;
    }

    g_StopTime = 0;
    g_LastElapsedTime = time.QuadPart;
    g_bTimerStopped = false;
}

// システ??イ??のストップ
void SystemTimer_Stop(void)
{
    if( g_bTimerStopped ) return;

    LARGE_INTEGER time = { 0 };
    QueryPerformanceCounter(&time);

    g_LastElapsedTime = g_StopTime = time.QuadPart; // 停?時間を記?
    g_bTimerStopped = true;
}

// システ??イ??を0.1秒進める
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

// 前回の呼び出しからの経過時間を取得（フレ??間差分）
double SystemTimer_GetElapsedTime(void)
{
    LARGE_INTEGER time = GetAdjustedCurrentTime();

    double elapsed_time = (double)(time.QuadPart - g_LastElapsedTime) / (double)g_TicksPerSec;
    g_LastElapsedTime = time.QuadPart;

    // ?イ??の巻き戻り現象対策（PCの低電力モ?ドや別コアへのスレッド移動により起こり得る）
    // elapsed_timeが負になった場合、前回のフレ??より前の時間に戻っていることを示す。
    // これを防ぐため、メインスレッドで SetThreadAffinityMask を呼び出して実行コアを固定することが推奨される。
    // ワ?カ?プロセッサ間の?イ??の同期ずれから生じる時間の逆流を防ぐための安全装置。
    if( elapsed_time < 0.0 ) {
        elapsed_time = 0.0;
    }

    return elapsed_time;
}

// システ??イ??が停?しているか？
bool SystemTimer_IsStoped(void)
{
    return g_bTimerStopped;
}

// 現在のスレッドの実行を特定のプロセッサコア（メインコア）に制限する
void LimitThreadAffinityToCurrentProc(void)
{
    HANDLE hCurrentProcess = GetCurrentProcess();

    // プロセスのアフィニティ?スクを取得
    DWORD_PTR dwProcessAffinityMask = 0;
    DWORD_PTR dwSystemAffinityMask = 0;

    if( GetProcessAffinityMask(hCurrentProcess, &dwProcessAffinityMask, &dwSystemAffinityMask) != 0 && dwProcessAffinityMask ) {
        // プロセスが実行を許可されている最も下位のコアを選択
        DWORD_PTR dwAffinityMask = (dwProcessAffinityMask & ((~dwProcessAffinityMask) + 1));

        // 現在実行中のスレッドをその特定のコアに固定
        // これはプロセスのアフィニティ?スクのサブセットでなければならない
        HANDLE hCurrentThread = GetCurrentThread();
        if( INVALID_HANDLE_VALUE != hCurrentThread ) {
            SetThreadAffinityMask(hCurrentThread, dwAffinityMask);
        }
    }
}

// 停?中であれば停?時間を、動作中であれば現在時間を取得
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