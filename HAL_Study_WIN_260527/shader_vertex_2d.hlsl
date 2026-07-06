cbuffer MatrixBuffer : register(b0)
{
    float4x4 mtx;
}

cbuffer UVMatrixBuffer : register(b1)
{
    float4x4 mtx_uv;
}

// 定数バッファ
//float4x4 mtx;

struct VS_IN
{
    float4 posL : POSITION0;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH : SV_POSITION0;
    float2 uv : TEXCOORD0;
};

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out;
    
    vs_out.posH = mul(vs_in.posL, mtx);
    vs_out.uv = mul(float4(vs_in.uv, 0.0f, 1.0f), mtx_uv).xy;
    
    return vs_out;
}
