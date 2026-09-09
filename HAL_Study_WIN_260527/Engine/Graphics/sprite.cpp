#include "sprite.h"

#include "math_utils.h"
#include "WICTextureLoader11.h"
#include "config.h"
#include "debug_ostream.h"
#include "direct3d.h"
#include "render_state_utils.h"
#include "shader.h"
#include "texture.h"

#include <algorithm>

using namespace DirectX;

static constexpr int NUM_VERTEX{ 4 };

static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
static ID3D11SamplerState* g_pSamplerState_Point = nullptr;
static ID3D11SamplerState* g_pSamplerState_Linear = nullptr;
static ID3D11BlendState* g_pBlendState = nullptr;
static ID3D11BlendState* g_pAdditiveBlendState = nullptr;
static ID3D11DepthStencilState* g_pDepthStencilState = nullptr;
static ID3D11RasterizerState* g_pRasterizerState = nullptr;

static ID3D11Buffer* g_pVSConstantBuffer = nullptr; // 定数バッファ
static ID3D11Buffer* g_pPSConstantBuffer = nullptr; // 定数バッファ
static ID3D11Buffer* g_pLightConstantBuffer = nullptr;
static XMFLOAT4X4 g_ViewMatrix{};

// 頂点構造体
struct Vertex
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT2 texcoord;
};

struct SpritePixelConstants
{
	XMFLOAT4 color;
	XMFLOAT4 dissolve;
	XMFLOAT4 edge_color;
};

static bool g_LightingEnabled = true;

static void Sprite_SetPixelConstants(ID3D11DeviceContext* context, const XMFLOAT4& color, const XMFLOAT4& dissolve,
                                     const XMFLOAT4& edge_color)
{
	SpritePixelConstants constants{};
	constants.color = color;
	constants.dissolve = dissolve;
	constants.edge_color = edge_color;

	context->UpdateSubresource(g_pPSConstantBuffer, 0, nullptr, &constants, 0, 0);
	context->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer);
	const SpriteLightConstants light_constants = SpriteLighting_BuildConstants(g_ViewMatrix, g_LightingEnabled, false);
	context->UpdateSubresource(g_pLightConstantBuffer, 0, nullptr, &light_constants, 0, 0);
	context->PSSetConstantBuffers(1, 1, &g_pLightConstantBuffer);
}

bool Sprite_Initialize()
{
	Sprite_ResetViewMatrix();

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	// 頂点バッファへ送るデータの作成
	Vertex v[NUM_VERTEX]{};

	// 画面の左上から右下に向かう線分を描画する
	v[0].position = { -0.5f, -0.5f, 0.0f };
	v[1].position = { 0.5f, -0.5f, 0.0f };
	v[2].position = { -0.5f, 0.5f, 0.0f };
	v[3].position = { 0.5f, 0.5f, 0.0f };

	v[0].texcoord = { 0.0f, 0.0f };
	v[1].texcoord = { 1.0f, 0.0f };
	v[2].texcoord = { 0.0f, 1.0f };
	v[3].texcoord = { 1.0f, 1.0f };

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = v;

	HRESULT hr = Direct3D_GetDevice()->CreateBuffer(&bd, &initData, &g_pVertexBuffer);

	if (FAILED(hr))
	{
		hal::dout << "Polygon.cpp : 頂点バッファの生成に失敗しました。";
		return false;
	}

	// 頂点バッファ生成
	D3D11_BUFFER_DESC vs_cb{};
	vs_cb.ByteWidth = sizeof(XMFLOAT4X4);
	vs_cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = Direct3D_GetDevice()->CreateBuffer(&vs_cb, nullptr, &g_pVSConstantBuffer);

	// 頂点バッファ生成
	D3D11_BUFFER_DESC ps_cb{};
	ps_cb.ByteWidth = sizeof(SpritePixelConstants);
	ps_cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = Direct3D_GetDevice()->CreateBuffer(&ps_cb, nullptr, &g_pPSConstantBuffer);
	if (FAILED(hr))
	{
		SAFE_RELEASE(g_pVSConstantBuffer);
		SAFE_RELEASE(g_pVertexBuffer);
		return false;
	}

	ps_cb.ByteWidth = sizeof(SpriteLightConstants);
	hr = Direct3D_GetDevice()->CreateBuffer(&ps_cb, nullptr, &g_pLightConstantBuffer);
	if (FAILED(hr))
	{
		SAFE_RELEASE(g_pPSConstantBuffer);
		SAFE_RELEASE(g_pVSConstantBuffer);
		SAFE_RELEASE(g_pVertexBuffer);
		return false;
	}

	D3D11_SAMPLER_DESC sd =
	    hal::CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_COMPARISON_NEVER, 16);

	hr = Direct3D_GetDevice()->CreateSamplerState(&sd, &g_pSamplerState_Point);
	if (FAILED(hr))
	{
		hal::dout << "Polygon.cpp : SamplerState creation failed.";
		SAFE_RELEASE(g_pVertexBuffer);
		return false;
	}

	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

	hr = Direct3D_GetDevice()->CreateSamplerState(&sd, &g_pSamplerState_Linear);
	if (FAILED(hr))
	{
		hal::dout << "Polygon.cpp : SamplerState creation failed.";
		SAFE_RELEASE(g_pVertexBuffer);
		return false;
	}

	D3D11_BLEND_DESC blend_desc = hal::CreateAlphaBlendDesc();
	hr = Direct3D_GetDevice()->CreateBlendState(&blend_desc, &g_pBlendState);
	if (FAILED(hr))
	{
		hal::dout << "Polygon.cpp : BlendState creation failed.";
		SAFE_RELEASE(g_pSamplerState_Point);
		SAFE_RELEASE(g_pSamplerState_Linear);
		SAFE_RELEASE(g_pVertexBuffer);
		return false;
	}

	// デプスステンシルステートの設定
	D3D11_BLEND_DESC additive_blend_desc = hal::CreateAdditiveBlendDesc();
	hr = Direct3D_GetDevice()->CreateBlendState(&additive_blend_desc, &g_pAdditiveBlendState);
	if (FAILED(hr))
	{
		hal::dout << "Sprite.cpp : Additive blend state creation failed.";
		SAFE_RELEASE(g_pAdditiveBlendState);
		SAFE_RELEASE(g_pBlendState);
		SAFE_RELEASE(g_pSamplerState_Point);
		SAFE_RELEASE(g_pSamplerState_Linear);
		SAFE_RELEASE(g_pVertexBuffer);
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC dsd = hal::CreateDepthDisabledDesc(D3D11_COMPARISON_NEVER);
	// ステートオブジェクトの作成
	hr = Direct3D_GetDevice()->CreateDepthStencilState(&dsd, &g_pDepthStencilState);
	if (FAILED(hr))
	{
		MessageBoxW(nullptr, L"デプスステンシルステートの作成に失敗しました", L"エラー", MB_OK | MB_ICONERROR);
	}

	D3D11_RASTERIZER_DESC rasterizer_desc = hal::CreateRasterizerDesc(D3D11_CULL_NONE);
	Direct3D_GetDevice()->CreateRasterizerState(&rasterizer_desc, &g_pRasterizerState);

	return true;
}

void Sprite_Finalize()
{
	SAFE_RELEASE(g_pLightConstantBuffer);
	SAFE_RELEASE(g_pVSConstantBuffer);
	SAFE_RELEASE(g_pPSConstantBuffer);
	SAFE_RELEASE(g_pRasterizerState);
	SAFE_RELEASE(g_pDepthStencilState);
	SAFE_RELEASE(g_pAdditiveBlendState);
	SAFE_RELEASE(g_pBlendState);
	SAFE_RELEASE(g_pSamplerState_Point);
	SAFE_RELEASE(g_pSamplerState_Linear);
	SAFE_RELEASE(g_pVertexBuffer);
}

void Sprite_SetFilter(SpriteFilter filter)
{
	// PixelShaderにSampler設定
	switch (filter)
	{
	case kPOINT:
		Direct3D_GetDeviceContext()->PSSetSamplers(0, 1, &g_pSamplerState_Point);
		break;
	case kLINEAR:
		Direct3D_GetDeviceContext()->PSSetSamplers(0, 1, &g_pSamplerState_Linear);
		break;
	default:
		break;
	}
}

void Sprite_SetViewMatrix(const XMMATRIX& view_matrix)
{
	XMStoreFloat4x4(&g_ViewMatrix, view_matrix);
}

void Sprite_ResetViewMatrix()
{
	XMStoreFloat4x4(&g_ViewMatrix, XMMatrixIdentity());
}

bool Sprite_SetLightingEnabled(bool enabled)
{
	const bool previous = g_LightingEnabled;
	g_LightingEnabled = enabled;
	return previous;
}

void Sprite_Draw(int texture_Id, float pos_X, float pos_Y, float rotation_radian, float scale)
{
	const float width = static_cast<float>(Texture_GetWidth(texture_Id)) * scale;
	const float height = static_cast<float>(Texture_GetHeight(texture_Id)) * scale;

	Sprite_DrawSized(texture_Id, pos_X, pos_Y, width, height, rotation_radian, { 1.0f, 1.0f, 1.0f, 1.0f });
}

void Sprite_DrawSized(int texture_Id, float pos_X, float pos_Y, float width, float height)
{
	Sprite_DrawSized(texture_Id, pos_X, pos_Y, width, height, { 1.0f, 1.0f, 1.0f, 1.0f });
}

void Sprite_DrawSized(int texture_Id, float pos_X, float pos_Y, float width, float height, const XMFLOAT4& color)
{
	Sprite_DrawSized(texture_Id, pos_X, pos_Y, width, height, 0.0f, color);
}

void Sprite_DrawSized(int texture_Id, float pos_X, float pos_Y, float width, float height, float rotation_radian,
                      const XMFLOAT4& color)
{
	Shader_Begin();

	XMVECTOR RotationQuaternion{};

	XMVECTOR axisZ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR qRot = XMQuaternionRotationAxis(axisZ, rotation_radian);

	// 変換行列を作成
	XMMATRIX mtxScaling = XMMatrixScaling(width, height, 1.0f);
	XMMATRIX mtxRotation = XMMatrixRotationQuaternion(qRot);
	XMMATRIX mtxTranslation = XMMatrixTranslation(pos_X, pos_Y, 0);
	XMMATRIX mtxView = XMLoadFloat4x4(&g_ViewMatrix);
	XMMATRIX mtxProjection = XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);

	XMMATRIX mtx = mtxScaling * mtxRotation * mtxTranslation * mtxView * mtxProjection;

	Shader_SetMatrix(mtx);

	ID3D11DeviceContext* context = Direct3D_GetDeviceContext();

	XMMATRIX mtxIdentity = XMMatrixIdentity();
	XMFLOAT4X4 mtxUV;
	XMStoreFloat4x4(&mtxUV, mtxIdentity);
	context->UpdateSubresource(g_pVSConstantBuffer, 0, nullptr, &mtxUV, 0, 0);
	context->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer);

	Sprite_SetPixelConstants(context, color, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f });

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// プリミティブトポロジーの設定
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Sprite_SetFilter(kPOINT);

	Texture_SetTexture(texture_Id);

	context->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);

	// デプスステンシルステートをパイプライン（OMステージ）に設定
	context->OMSetDepthStencilState(g_pDepthStencilState, 0);

	context->RSSetState(g_pRasterizerState);

	// ポリゴン描画命令発行
	context->Draw(NUM_VERTEX, 0);
}

void Sprite_DrawDissolve(int texture_Id, int noise_texture_Id, float pos_X, float pos_Y, float width, float height,
                         float dissolve_amount, float edge_width, const XMFLOAT4& edge_color)
{
	if (texture_Id == TEXTURE_INVALID_ID || noise_texture_Id == TEXTURE_INVALID_ID)
	{
		Sprite_DrawSized(texture_Id, pos_X, pos_Y, width, height);
		return;
	}

	Shader_Begin();

	XMVECTOR axisZ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR qRot = XMQuaternionRotationAxis(axisZ, 0.0f);

	XMMATRIX mtxScaling = XMMatrixScaling(width, height, 1.0f);
	XMMATRIX mtxRotation = XMMatrixRotationQuaternion(qRot);
	XMMATRIX mtxTranslation = XMMatrixTranslation(pos_X, pos_Y, 0);
	XMMATRIX mtxView = XMLoadFloat4x4(&g_ViewMatrix);
	XMMATRIX mtxProjection = XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);

	XMMATRIX mtx = mtxScaling * mtxRotation * mtxTranslation * mtxView * mtxProjection;

	Shader_SetMatrix(mtx);

	ID3D11DeviceContext* context = Direct3D_GetDeviceContext();

	XMFLOAT4X4 mtxUV;
	XMStoreFloat4x4(&mtxUV, XMMatrixIdentity());
	context->UpdateSubresource(g_pVSConstantBuffer, 0, nullptr, &mtxUV, 0, 0);
	context->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer);

	const float dissolve = Saturate(dissolve_amount);
	const float edge = std::max(edge_width, 0.001f);
	Sprite_SetPixelConstants(context, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, dissolve, edge, 0.0f }, edge_color);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Sprite_SetFilter(kPOINT);
	Texture_SetTextureSlot(texture_Id, 0);
	Texture_SetTextureSlot(noise_texture_Id, 1);

	context->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);
	context->OMSetDepthStencilState(g_pDepthStencilState, 0);
	context->RSSetState(g_pRasterizerState);
	context->Draw(NUM_VERTEX, 0);
}

void Sprite_DrawRegion(int texture_id, const XMFLOAT2& position, const XMFLOAT2& size,
                       const SpriteRegion& source, const XMFLOAT4& color)
{
	Sprite_DrawRegionRotated(texture_id, {
		.Position = position,
		.Size = size,
		.Rotation = 0.0f,
		.Source = source,
		.Color = color,
	});
}

void Sprite_DrawRegionRotated(int texture_id, const SpriteRegionDraw& draw)
{
	float texture_width = static_cast<float>(Texture_GetWidth(texture_id));
	float texture_height = static_cast<float>(Texture_GetHeight(texture_id));

	Shader_Begin();

	XMVECTOR axisZ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR qRot = XMQuaternionRotationAxis(axisZ, draw.Rotation);

	XMMATRIX mtxScaling = XMMatrixScaling(draw.Size.x, draw.Size.y, 1.0f);
	XMMATRIX mtxRotation = XMMatrixRotationQuaternion(qRot);
	XMMATRIX mtxTranslation = XMMatrixTranslation(draw.Position.x, draw.Position.y, 0);
	XMMATRIX mtxView = XMLoadFloat4x4(&g_ViewMatrix);
	XMMATRIX mtxProjection = XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);

	XMMATRIX mtx = mtxScaling * mtxRotation * mtxTranslation * mtxView * mtxProjection;

	Shader_SetMatrix(mtx);

	ID3D11DeviceContext* context = Direct3D_GetDeviceContext();

	float tx = draw.Source.X / texture_width;
	float ty = draw.Source.Y / texture_height;
	float tw = draw.Source.Width / texture_width;
	float th = draw.Source.Height / texture_height;
	mtxScaling = XMMatrixScaling(tw, th, 1.0f);
	mtxTranslation = XMMatrixTranslation(tx, ty, 0.0f);

	XMFLOAT4X4 mtxUV;
	XMStoreFloat4x4(&mtxUV, XMMatrixTranspose(mtxScaling * mtxTranslation));
	context->UpdateSubresource(g_pVSConstantBuffer, 0, nullptr, &mtxUV, 0, 0);
	context->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer);

	Sprite_SetPixelConstants(context, draw.Color, { 0.0f, 0.0f, 0.0f, draw.AlphaMask ? 1.0f : 0.0f },
	                         { 0.0f, 0.0f, 0.0f, 0.0f });

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Sprite_SetFilter(kPOINT);
	Texture_SetTexture(texture_id);
	context->OMSetBlendState(draw.Additive ? g_pAdditiveBlendState : g_pBlendState, nullptr, 0xffffffff);
	context->OMSetDepthStencilState(g_pDepthStencilState, 0);
	context->RSSetState(g_pRasterizerState);
	context->Draw(NUM_VERTEX, 0);
}
