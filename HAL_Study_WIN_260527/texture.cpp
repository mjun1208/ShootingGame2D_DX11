#include "texture.h"
#include "direct3d.h"
#include "WICTextureLoader11.h"
#include <string>
#include <DirectXMath.h>

using namespace DirectX;

// 管理できるテクス?ャの最大数
static constexpr int TEXTURE_MAX = 1024;

// 1つのテクス?ャが持つ情報
struct Texture
{
	std::wstring filename; // フ?イル名（重複?ェック用）
	unsigned int width = 0; // 幅
	unsigned int height = 0; // 高さ
	ID3D11Resource* pTexture = nullptr; // テクス?ャリ??ス
	ID3D11ShaderResourceView* pTextureView = nullptr; // シェ???リ??スビュ?
};

// テクス?ャ管理用の配列
static Texture g_Textures[TEXTURE_MAX];

// デバイスとコンテキストの保存用?イン?
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストを保存
	g_pDevice = pDevice;
	g_pContext = pContext;
}

void Texture_Finalize()
{
	// 全てのテクス?ャを解放
	Texture_AllRelease();
}

int Texture_Load(const wchar_t* pFileName, bool bMipMap)
{
	if (!g_pDevice || !g_pContext) 
	{
		return TEXTURE_INVALID_ID;
	}
	if (!pFileName || pFileName[0] == L'\0') 
	{
		return TEXTURE_INVALID_ID;
	}
	// ? すでに読み込まれているフ?イルか?ェック
	for (int i = 0; i < TEXTURE_MAX; i++) 
	{
		if (!g_Textures[i].pTexture) continue;
		// 同じフ?イル名が見つかったら、そのIDを返す（新しく作らない）
		if (g_Textures[i].filename == pFileName) {
			return i;
		}
	}
	// ② 新しく読み込む処理
	for (int i = 0; i < TEXTURE_MAX; i++) 
	{
		// 空いている場所を探す
		if (g_Textures[i].pTexture) continue;
		HRESULT hr;
		// This renderer writes directly to an UNORM back buffer, so keep WIC image
		// samples in their stored display-space RGB values. Auto-decoding PNG
		// sRGB/gAMA metadata without a matching sRGB render-target encode makes
		// sprites substantially darker than their source images.
		hr = CreateWICTextureFromFileEx(
			g_pDevice,
			bMipMap ? g_pContext : nullptr,
			pFileName,
			0,
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_SHADER_RESOURCE,
			0,
			0,
			WIC_LOADER_IGNORE_SRGB,
			&g_Textures[i].pTexture,
			&g_Textures[i].pTextureView);
		if (FAILED(hr)) {
			MessageBoxW(nullptr, L"テクス?ャの読み込みに失敗しました", pFileName, MB_OK | MB_ICONERROR);
			break;
		}
		// ③ 画像のサイズ（幅と高さ）を取得して保存
		ID3D11Texture2D* pTexture = (ID3D11Texture2D*)g_Textures[i].pTexture;
		D3D11_TEXTURE2D_DESC t2desc;
		pTexture->GetDesc(&t2desc);
		g_Textures[i].width = t2desc.Width;
		g_Textures[i].height = t2desc.Height;
		// フ?イル名を記?
		g_Textures[i].filename = pFileName;
		// 登?したID（配列のインデックス）を返す
		return i;
	}

	return TEXTURE_INVALID_ID;
}

void Texture_SetTexture(int texture_id)
{
	Texture_SetTextureSlot(texture_id, 0);
}

void Texture_SetTextureSlot(int texture_id, unsigned int slot)
{
	if (!g_pContext || slot >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		return;
	}

	ID3D11ShaderResourceView* texture_view = nullptr;
	if (texture_id >= 0 && texture_id < TEXTURE_MAX) {
		texture_view = g_Textures[texture_id].pTextureView;
	}

	g_pContext->PSSetShaderResources(slot, 1, &texture_view);
}
DirectX::XMUINT2 Texture_GetSize(int texture_id)
{
	if (texture_id < 0 || texture_id >= TEXTURE_MAX) {
		return { 0, 0 };
	}
	return { g_Textures[texture_id].width, g_Textures[texture_id].height };
}

unsigned int Texture_GetWidth(int texture_id)
{
	if (texture_id < 0 || texture_id >= TEXTURE_MAX) {
		return 0;
	}
	return g_Textures[texture_id].width;
}

unsigned int Texture_GetHeight(int texture_id)
{
	if (texture_id < 0 || texture_id >= TEXTURE_MAX) {
		return 0;
	}
	return g_Textures[texture_id].height;
}

void Texture_Release(int texture_id)
{
	if (texture_id < 0 || texture_id >= TEXTURE_MAX) {
		return;
	}
	Texture& t = g_Textures[texture_id];
	SAFE_RELEASE(t.pTextureView);
	SAFE_RELEASE(t.pTexture);
	t.filename.clear();
	t.width = 0;
	t.height = 0;
}

// 複数のIDをまとめて解放するオ?バ?ロ?ド
void Texture_Release(const int* pTextureIDs, int count)
{
	if (!pTextureIDs || count <= 0) {
		return;
	}
	for (int i = 0; i < count; i++) {
		if (pTextureIDs[i] < 0 || pTextureIDs[i] >= TEXTURE_MAX) {
			continue;
		}
		Texture_Release(pTextureIDs[i]);
	}
}

// 全てのテクス?ャを解放
void Texture_AllRelease()
{
	for (int i = 0; i < TEXTURE_MAX; i++) {
		Texture_Release(i);
	}
}