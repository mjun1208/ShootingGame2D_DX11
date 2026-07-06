#include "direct3d.h"
#include <d3d11.h>
#include "debug_ostream.h"

#pragma comment (lib, "d3d11.lib")

static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;

/* バックバッファ関連 */
static ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
static ID3D11Texture2D* g_pDepthStencilBuffer = nullptr;
static ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
static D3D11_TEXTURE2D_DESC g_BackBufferDesc{};

bool CreateBackBuffer();
void ReleaseBackBuffer();

bool Direct3D_Initialize(HWND hWnd)
{
	//IDXGIFactory1* pFactory;
	//CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
	//IDXGIAdapter1* pAdapter;
	//pFactory->EnumAdapters1(1, &pAdapter);

	//DXGI_ADAPTER_DESC1 desc;
	//pAdapter->GetDesc1(&desc); // アダプタの情報を取得

	//SAFE_RELEASE(pFactory);
	//SAFE_RELEASE(pAdapter);

	/* デバイス、スワップチェーン、コンテキスト⽣成 */
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.BufferCount = 2;
	// swap_chain_desc.BufferDesc.Width = 0;
	// swap_chain_desc.BufferDesc.Height = 0;
	// ⇒ ウィンドウサイズに合わせて⾃動的に設定される
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swap_chain_desc.OutputWindow = hWnd;
	swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	UINT device_flags = 0;

#if defined(DEBUG) || defined(_DEBUG)
	device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	
	D3D_FEATURE_LEVEL levels[] = {
	D3D_FEATURE_LEVEL_11_1,
	D3D_FEATURE_LEVEL_11_0
	};

	D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		device_flags,
		levels,
		ARRAYSIZE(levels),
		D3D11_SDK_VERSION,
		&swap_chain_desc,
		&g_pSwapChain,
		&g_pDevice,
		&feature_level,
		&g_pDeviceContext);

	if (FAILED(hr)) 
	{
		// メッセージボックスなどによるエラー表⽰
		MessageBox(hWnd, "Error", "OMG", MB_OK | MB_ICONERROR);
		return false;
	}

	if (!CreateBackBuffer())
	{
		Direct3D_Finalize();
		return false;
	}

	// ViewPort
	D3D11_VIEWPORT viewport{};

	// ビューポートの設定
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<FLOAT>(g_BackBufferDesc.Width);
	viewport.Height = static_cast<FLOAT>(g_BackBufferDesc.Height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	g_pDeviceContext->RSSetViewports(1, &viewport); // ビューポートの設定

	return true;
}

void Direct3D_Finalize()
{
	ReleaseBackBuffer();

	SAFE_RELEASE(g_pSwapChain);
	SAFE_RELEASE(g_pDeviceContext);
	SAFE_RELEASE(g_pDevice);
}

void Direct3D_DrawBegin()
{
	float clear_color[4] = { 0.2f, 0.4f, 0.8f, 1.0f };
	g_pDeviceContext->ClearRenderTargetView(g_pRenderTargetView, clear_color);
	g_pDeviceContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// レンダーターゲットビューとデプスステンシルビューの設定
	g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
}

void Direct3D_Present()
{
	if (USE_VSYNC)
	{
		g_pSwapChain->Present(1, 0);
	}
	else 
	{
		g_pSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
	}
}

ID3D11Device* Direct3D_GetDevice()
{
	return g_pDevice;
}

ID3D11DeviceContext* Direct3D_GetDeviceContext()
{
	return g_pDeviceContext;
}

bool CreateBackBuffer()
{
	HRESULT hr;
	ID3D11Texture2D* back_buffer_pointer = nullptr;
	
	// バックバッファの取得
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer_pointer);
	if (FAILED(hr))
	{
		// hal::dout << "バックバッファの取得に失敗しました" << std::endl;
	}
	
	// バックバッファのレンダーターゲットビューの⽣成
	hr = g_pDevice->CreateRenderTargetView(back_buffer_pointer, nullptr,
		&g_pRenderTargetView);

	if (FAILED(hr)) {
		back_buffer_pointer->Release();
		// hal::dout << "バックバッファのレンダーターゲットビューの⽣成に失敗しました" << std::endl;
	}
	// バックバッファの状態（情報）を取得
	back_buffer_pointer->GetDesc(&g_BackBufferDesc);
	back_buffer_pointer->Release(); // バックバッファのポインタは不要なので解放

	// デプスステンシルバッファの⽣成
	D3D11_TEXTURE2D_DESC depth_stencil_desc{};
	depth_stencil_desc.Width = g_BackBufferDesc.Width;
	depth_stencil_desc.Height = g_BackBufferDesc.Height;
	depth_stencil_desc.MipLevels = 1;
	depth_stencil_desc.ArraySize = 1;
	depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_desc.SampleDesc.Count = 1;
	depth_stencil_desc.SampleDesc.Quality = 0;
	depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
	depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depth_stencil_desc.CPUAccessFlags = 0;
	depth_stencil_desc.MiscFlags = 0;

	hr = g_pDevice->CreateTexture2D(&depth_stencil_desc, nullptr, &g_pDepthStencilBuffer);
	if (FAILED(hr)) {
		hal::dout << "デプスステンシルバッファの⽣成に失敗しました" << std::endl;
		
		SAFE_RELEASE(g_pRenderTargetView);
		return false;
	}

	// デプスステンシルビューの⽣成
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = depth_stencil_desc.Format;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	depth_stencil_view_desc.Flags = 0;

	hr = g_pDevice->CreateDepthStencilView(g_pDepthStencilBuffer, &depth_stencil_view_desc, &g_pDepthStencilView);
	if (FAILED(hr)) {
		// hal::dout << "デプスステンシルビューの⽣成に失敗しました" << std::endl;

	    SAFE_RELEASE(g_pDepthStencilBuffer);
		SAFE_RELEASE(g_pRenderTargetView);
		return false;
	}

	return true;
}

void ReleaseBackBuffer()
{
	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilBuffer);
	SAFE_RELEASE(g_pRenderTargetView);
}