#include "sprite.h"
#include "direct3d.h"
#include "debug_ostream.h"
#include "config.h"
#include "shader.h"
#include "WICTextureLoader11.h"
#include "texture.h"

#include <algorithm>

using namespace DirectX;

static constexpr int NUM_VERTEX{ 4 };

static ID3D11Buffer* g_pVertexBuffer = nullptr; //?頂?バッフ?
static ID3D11SamplerState* g_pSamplerState_Point = nullptr;
static ID3D11SamplerState* g_pSamplerState_Linear = nullptr;
static ID3D11BlendState* g_pBlendState = nullptr;
static ID3D11DepthStencilState* g_pDepthStencilState = nullptr;
static ID3D11RasterizerState* g_pRasterizerState = nullptr;

static ID3D11Buffer* g_pVSConstantBuffer = nullptr; //?定数バッフ?
static ID3D11Buffer* g_pPSConstantBuffer = nullptr; //?定数バッフ?
static XMFLOAT4X4 g_ViewMatrix{};

// 頂??造体
struct Vertex
{
    XMFLOAT3 position; // 頂?座標
    XMFLOAT2 texcoord;
};

struct SpritePixelConstants
{
    XMFLOAT4 color;
    XMFLOAT4 dissolve;
    XMFLOAT4 edge_color;
};

static void Sprite_SetPixelConstants(
    ID3D11DeviceContext* context,
    const XMFLOAT4& color,
    const XMFLOAT4& dissolve,
    const XMFLOAT4& edge_color)
{
    SpritePixelConstants constants{};
    constants.color = color;
    constants.dissolve = dissolve;
    constants.edge_color = edge_color;

    context->UpdateSubresource(g_pPSConstantBuffer, 0, nullptr, &constants, 0, 0);
    context->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer);
}

bool Sprite_Initialize()
{
    Sprite_ResetViewMatrix();

    // 頂?バッフ?生成
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

	// 頂?バッフ?へ送るゲ???の作成
    Vertex v[NUM_VERTEX]{};

	// 画面の左上から右下に向かう線分を?画する
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
        hal::dout << "Polygon.cpp : 頂?バッフ?の生成に失敗しました。";
        return false;
    }

    // 頂?バッフ?生成
    D3D11_BUFFER_DESC vs_cb{};
    vs_cb.ByteWidth = sizeof(XMFLOAT4X4);
    vs_cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = Direct3D_GetDevice()->CreateBuffer(&vs_cb, nullptr, &g_pVSConstantBuffer);

    // 頂?バッフ?生成
    D3D11_BUFFER_DESC ps_cb{};
    ps_cb.ByteWidth = sizeof(SpritePixelConstants);
    ps_cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = Direct3D_GetDevice()->CreateBuffer(&ps_cb, nullptr, &g_pPSConstantBuffer);


    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    // sd.Filter = D3D11_FILTER_ANISOTROPIC; // フィル?リング設定 (MIPMAPリニア)
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // 範囲外の扱い (横方向：クランプ)
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; // 範囲外の扱い (縦方向：クランプ)
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; // 範囲外の扱い (奥行方向：クランプ)
    sd.MaxAnisotropy = 16;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;

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

    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; // ?画する色の係数
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; // すでにある色の係数
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = Direct3D_GetDevice()->CreateBlendState(&blend_desc, &g_pBlendState);
    if (FAILED(hr))
    {
        hal::dout << "Polygon.cpp : BlendState creation failed.";
        SAFE_RELEASE(g_pSamplerState_Point);
        SAFE_RELEASE(g_pSamplerState_Linear);
        SAFE_RELEASE(g_pVertexBuffer);
        return false;
    }

    // デプスステンシルステ?トの設定
    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = FALSE; // デプステストを無効化
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // デプス書き込みを無効化 (書き込まない)
    dsd.DepthFunc = D3D11_COMPARISON_NEVER; // 比較処理を行わない
    dsd.StencilEnable = FALSE; // ステンシルテスト無効
    
    // ステ?トオブジェクトの作成
    hr = Direct3D_GetDevice()->CreateDepthStencilState(&dsd, &g_pDepthStencilState);
    if (FAILED(hr)) 
    {
        MessageBox(nullptr, "デプスステンシルステ?トの作成に失敗しました", "エラ?",
            MB_OK | MB_ICONERROR);
    }

    D3D11_RASTERIZER_DESC rasterizer_desc{};
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.DepthClipEnable = TRUE;

    Direct3D_GetDevice()->CreateRasterizerState(&rasterizer_desc, &g_pRasterizerState);

    return true;
}

void Sprite_Finalize()
{
    SAFE_RELEASE(g_pVSConstantBuffer);
    SAFE_RELEASE(g_pPSConstantBuffer);
    SAFE_RELEASE(g_pRasterizerState);
    SAFE_RELEASE(g_pDepthStencilState);
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

void Sprite_Draw(int texture_Id, float pos_X, float pos_Y)
{
    float width = static_cast<float>(Texture_GetWidth(texture_Id));
    float height = static_cast<float>(Texture_GetHeight(texture_Id));

    Sprite_Draw(texture_Id, pos_X, pos_Y, width, height);
}

void Sprite_Draw(int texture_Id, const XMFLOAT2& pos)
{
    Sprite_Draw(texture_Id, pos.x, pos.y);
}

void Sprite_Draw(int texture_Id, float pos_X, float pos_Y, float width, float height)
{
    Sprite_Draw(texture_Id, pos_X, pos_Y, width, height, { 1.0f, 1.0f, 1.0f, 1.0f });
}

void Sprite_Draw(
    int texture_Id,
    float pos_X,
    float pos_Y,
    float width,
    float height,
    const XMFLOAT4& color)
{
    Sprite_Draw(texture_Id, pos_X, pos_Y, width, height, 0.0f, color);
}

void Sprite_Draw(
    int texture_Id,
    float pos_X,
    float pos_Y,
    float width,
    float height,
    float rotation_radian,
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

    Sprite_SetPixelConstants(
        context,
        color,
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f });

    // 頂?バッフ?を?画パイプラインに設定
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // プリ?ティブト?ロジ?の設定
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    Sprite_SetFilter(kPOINT);

    Texture_SetTexture(texture_Id);

    context->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);

    // デプスステンシルステ?トをパイプライン（OMステ?ジ）に設定
    context->OMSetDepthStencilState(g_pDepthStencilState, 0);

    context->RSSetState(g_pRasterizerState);

    // ?リゴン?画命令発行
    context->Draw(NUM_VERTEX, 0);
}

void Sprite_DrawDissolve(
    int texture_Id,
    int noise_texture_Id,
    float pos_X,
    float pos_Y,
    float width,
    float height,
    float dissolve_amount,
    float edge_width,
    const XMFLOAT4& edge_color)
{
    if (texture_Id == TEXTURE_INVALID_ID || noise_texture_Id == TEXTURE_INVALID_ID)
    {
        Sprite_Draw(texture_Id, pos_X, pos_Y, width, height);
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

    const float dissolve = std::clamp(dissolve_amount, 0.0f, 1.0f);
    const float edge = std::max(edge_width, 0.001f);
    Sprite_SetPixelConstants(
        context,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, dissolve, edge, 0.0f },
        edge_color);

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
void Sprite_Draw(
    int texture_Id,
    float pos_X,
    float pos_Y,
    float width,
    float height,
    int texture_X,
    int texture_Y,
    int texture_Width,
    int texture_Height,
    const XMFLOAT4& color)
{
    float texture_width = static_cast<float>(Texture_GetWidth(texture_Id));
    float texture_height = static_cast<float>(Texture_GetHeight(texture_Id));

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

    float tx = texture_X / texture_width;
    float ty = texture_Y / texture_height;
    float tw = texture_Width / texture_width;
    float th = texture_Height / texture_height;
    mtxScaling = XMMatrixScaling(tw, th, 1.0f);
    mtxTranslation = XMMatrixTranslation(tx, ty, 0.0f);

    XMFLOAT4X4 mtxUV;
    XMStoreFloat4x4(&mtxUV, XMMatrixTranspose(mtxScaling * mtxTranslation));
    context->UpdateSubresource(g_pVSConstantBuffer, 0, nullptr, &mtxUV, 0, 0);
    context->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer);

    Sprite_SetPixelConstants(
        context,
        color,
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f });

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    Sprite_SetFilter(kPOINT);
    Texture_SetTexture(texture_Id);
    context->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);
    context->OMSetDepthStencilState(g_pDepthStencilState, 0);
    context->RSSetState(g_pRasterizerState);
    context->Draw(NUM_VERTEX, 0);
}
void Sprite_Draw(int texture_Id, float pos_X, float pos_Y, 
    int texture_X, int texture_Y, int texture_Width, int texture_Height)
{
    float width = static_cast<float>(Texture_GetWidth(texture_Id));
    float height = static_cast<float>(Texture_GetHeight(texture_Id));

    Shader_Begin();

    XMVECTOR RotationQuaternion{};

    XMVECTOR axisZ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR qRot = XMQuaternionRotationAxis(axisZ, XMConvertToRadians(45.0f));

    // 変換行列を作成
    XMMATRIX mtxScaling = XMMatrixScaling(static_cast<float>(texture_Width), static_cast<float>(texture_Height), 1.0f);
    XMMATRIX mtxRotation = XMMatrixRotationQuaternion(qRot);
    XMMATRIX mtxTranslation = XMMatrixTranslation(pos_X, pos_Y, 0);
    XMMATRIX mtxView = XMLoadFloat4x4(&g_ViewMatrix);
    XMMATRIX mtxProjection = XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);

    XMMATRIX mtx = mtxScaling * mtxRotation * mtxTranslation * mtxView * mtxProjection;

    Shader_SetMatrix(mtx);

    ID3D11DeviceContext* context = Direct3D_GetDeviceContext();

    float tx = texture_X / (float)width;
    float ty = texture_Y / (float)height;
    float tw = texture_Width / (float)width;
    float th = texture_Height / (float)height;
    mtxScaling = XMMatrixScaling(tw, th, 1.0f);
    mtxTranslation = XMMatrixTranslation(tx, ty, 0.0f);

    XMFLOAT4X4 mtxUV;
    XMStoreFloat4x4(&mtxUV, XMMatrixTranspose(mtxScaling * mtxTranslation));
    context->UpdateSubresource(g_pVSConstantBuffer, 0, nullptr, &mtxUV, 0, 0);
    context->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer);

    XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    Sprite_SetPixelConstants(
        context,
        color,
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f });

    // 頂?バッフ?を?画パイプラインに設定
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // プリ?ティブト?ロジ?の設定
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    Sprite_SetFilter(kPOINT);

    Texture_SetTexture(texture_Id);

    context->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);

    // デプスステンシルステ?トをパイプライン（OMステ?ジ）に設定
    context->OMSetDepthStencilState(g_pDepthStencilState, 0);

    context->RSSetState(g_pRasterizerState);

    // ?リゴン?画命令発行
    context->Draw(NUM_VERTEX, 0);
}
