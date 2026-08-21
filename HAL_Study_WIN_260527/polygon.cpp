#include "polygon.h"
#include <DirectXMath.h>
#include "direct3d.h"
#include "debug_ostream.h"
#include "config.h"
#include "shader.h"
#include "WICTextureLoader11.h"
#include "texture.h"

using namespace DirectX;

static constexpr int NUM_VERTEX{ 4 };

static ID3D11Buffer* g_pVertexBuffer = nullptr; //?頂?バッフ?
static ID3D11SamplerState* g_pSamplerState = nullptr;
static ID3D11BlendState* g_pBlendState = nullptr;

static int g_TextureID = TEXTURE_INVALID_ID;

// 頂??造体
struct Vertex
{
    XMFLOAT3 position; // 頂?座標
    XMFLOAT4 color;
    XMFLOAT2 texcoord;
};

bool Polygon_Initialize()
{
    // 頂?バッフ?生成
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = Direct3D_GetDevice()->CreateBuffer(&bd, NULL, &g_pVertexBuffer);

    if (FAILED(hr))
    {
        hal::dout << "Polygon.cpp : 頂?バッフ?の生成に失敗しました。";
        return false;
    }

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // フィル?リング設定 (MIPMAPリニア)
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // 範囲外の扱い (横方向：クランプ)
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; // 範囲外の扱い (縦方向：クランプ)
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; // 範囲外の扱い (奥行方向：クランプ)
    sd.MaxAnisotropy = 16;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;

    hr = Direct3D_GetDevice()->CreateSamplerState(&sd, &g_pSamplerState);
    if (FAILED(hr))
    {
        hal::dout << "Polygon.cpp : SamplerState creation failed.";
        SAFE_RELEASE(g_pVertexBuffer);
        return false;
    }
    g_TextureID = Texture_Load(L"logo.png");
    if (g_TextureID == TEXTURE_INVALID_ID)
    {
        SAFE_RELEASE(g_pSamplerState);
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
        Texture_Release(g_TextureID);
        g_TextureID = TEXTURE_INVALID_ID;
        SAFE_RELEASE(g_pSamplerState);
        SAFE_RELEASE(g_pVertexBuffer);
        return false;
    }

    return true;
}

void Polygon_Finalize()
{
    Texture_Release(g_TextureID);
    SAFE_RELEASE(g_pBlendState);
    SAFE_RELEASE(g_pSamplerState);
    SAFE_RELEASE(g_pVertexBuffer);
}

void Polygon_Draw()
{
    Shader_Begin();

    Shader_SetMatrix(XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f));

    // 頂?バッフ?をロックする
    D3D11_MAPPED_SUBRESOURCE msr;
    ID3D11DeviceContext* context = Direct3D_GetDeviceContext();
    context->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

    // 頂?バッフ?への仮想?イン?を取得
    Vertex* v = (Vertex*)msr.pData;

    // 画面の左上から右下に向かう線分を?画する
    v[0].position = { 100.0f, 100.0f, 0.0f };
    v[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
    v[0].texcoord = { 0.0f, 0.0f };

    v[1].position = { 500.0f, 100.0f, 0.0f };
    v[1].color = { 1.0f, 0.0f, 0.0f, 1.0f };
    v[1].texcoord = { 1.0f, 0.0f };

    v[2].position = { 100.0f, 500.0f, 0.0f };
    v[2].color = { 0.0f, 1.0f, 0.0f, 1.0f };
    v[2].texcoord = { 0.0f, 1.0f };

    v[3].position = { 600.0f, 600.0f, 0.0f };
    v[3].color = { 0.0f, 0.0f, 1.0f, 1.0f };
    v[3].texcoord = { 1.0f, 1.0f };

    // 頂?バッフ?のロックを解除
    context->Unmap(g_pVertexBuffer, 0);

    // 頂?バッフ?を?画パイプラインに設定
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // プリ?ティブト?ロジ?の設定
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    
    // PixelShaderにSampler設定
    context->PSSetSamplers(0, 1, &g_pSamplerState);

    Texture_SetTexture(g_TextureID);

    context->OMSetBlendState(g_pBlendState, nullptr, 0xffffffff);

    // ?リゴン?画命令発行
    context->Draw(NUM_VERTEX, 0);
}
