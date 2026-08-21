#ifndef TIME_STOP_EFFECT_H
#define TIME_STOP_EFFECT_H

#include <DirectXMath.h>
#include <d3d11.h>

bool TimeStopEffect_Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* back_buffer,
    ID3D11DepthStencilView* depth_stencil,
    unsigned int width,
    unsigned int height);
void TimeStopEffect_Finalize();

void TimeStopEffect_BeginCapture();
void TimeStopEffect_EndCapture();

void TimeStopEffect_Trigger(const DirectX::XMFLOAT2& screen_position);
void TimeStopEffect_Update(float delta_time);
void TimeStopEffect_Cancel();
bool TimeStopEffect_IsActive();

#endif // TIME_STOP_EFFECT_H
