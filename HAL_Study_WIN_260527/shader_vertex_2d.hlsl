// 기본 스프라이트 정점 셰이더: 위치와 UV를 각각 변환해 픽셀 셰이더에 전달한다.
// 정점 셰이더는 꼭짓점마다 실행된다. SV_POSITION은 클립 좌표, TEXCOORD는 전달할 UV를 뜻한다.
// register(bN)은 CPU가 연결하는 상수 버퍼 슬롯이며 정점/픽셀 단계별로 독립적이다.

cbuffer MatrixBuffer : register(b0)
{
    // 로컬 위치를 클립 좌표로 보내는 합성 행렬.
    float4x4 local_to_clip;
}

cbuffer UVMatrixBuffer : register(b1)
{
    // 0~1 UV를 사용할 텍스처 영역으로 이동/확대·축소하는 행렬.
    float4x4 uv_transform;
}

struct VS_IN
{
    float4 local_position : POSITION0;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 clip_position : SV_POSITION0;
    float2 uv : TEXCOORD0;
};

VS_OUT main(VS_IN input)
{
    VS_OUT output;

    // 위치는 행벡터 × 행렬 순서로 변환한다. UV의 w=1은 평행 이동을 반영한다.
    output.clip_position = mul(input.local_position, local_to_clip);
    output.uv = mul(float4(input.uv, 0.0f, 1.0f), uv_transform).xy;

    return output;
}
