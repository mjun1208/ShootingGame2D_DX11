#ifndef DIRECT3D_H
#define DIRECT3D_H

#include <Windows.h>
#include <d3d11.h>

#define SAFE_RELEASE(o) if (o) { (o)->Release(); o = NULL; }

static constexpr bool USE_VSYNC = true;

bool Direct3D_Initialize(HWND hWnd);
void Direct3D_Finalize();

void Direct3D_DrawBegin();
void Direct3D_Present();

ID3D11Device* Direct3D_GetDevice();
ID3D11DeviceContext* Direct3D_GetDeviceContext();

#endif // DIRECT3D_H
