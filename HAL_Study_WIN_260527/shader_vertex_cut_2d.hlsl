// 절단 조각 정점 셰이더: 화면에 그릴 좌표와 절단 판정용 로컬 좌표를 함께 만든다.
// INSTANCE_*는 조각별 데이터이며, UV offset/scale은 스프라이트 시트의 프레임을 선택한다.

cbuffer SceneBuffer : register(b0)
{
    float4x4 view_projection;
}

struct VS_IN
{
    float3 position : POSITION0;
    float2 uv : TEXCOORD0;
    float2 instance_position : INSTANCE_POSITION0;
    float2 instance_size : INSTANCE_SIZE0;
    float instance_rotation : INSTANCE_ROTATION0;
    float4 instance_color : INSTANCE_COLOR0;
    float2 instance_texcoord_offset : INSTANCE_TEXCOORD_OFFSET0;
    float2 instance_texcoord_scale : INSTANCE_TEXCOORD_SCALE0;
    float2 instance_cut_normal : INSTANCE_CUT_NORMAL0;
    float4 instance_cut_parameters : INSTANCE_CUT_PARAMETERS0;
    float4 instance_edge_color : INSTANCE_EDGE_COLOR0;
};

struct VS_OUT
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float2 local_position : TEXCOORD1;
    float4 color : COLOR0;
    // 절단 설정은 삼각형 안에서 보간하지 않고 조각에 지정된 값을 그대로 전달한다.
    nointerpolation float2 cut_normal : TEXCOORD2;
    nointerpolation float4 cut_parameters : TEXCOORD3;
    nointerpolation float4 edge_color : COLOR1;
};

VS_OUT main(VS_IN input)
{
    VS_OUT output;
    const float sine = sin(input.instance_rotation);
    const float cosine = cos(input.instance_rotation);

    // 크기 → 회전 → 이동 순서로 배치한다. 절단 판정에는 회전 전 좌표를 따로 전달한다.
    const float2 scaled_position = input.position.xy * input.instance_size;
    const float2 rotated_position = float2(
        scaled_position.x * cosine - scaled_position.y * sine,
        scaled_position.x * sine + scaled_position.y * cosine);
    const float4 world_position = float4(
        rotated_position + input.instance_position, 0.0f, 1.0f);

    output.position = mul(world_position, view_projection);
    output.uv = input.instance_texcoord_offset +
        input.uv * input.instance_texcoord_scale;
    // 회전 전 크기만 적용한 좌표를 보내 절단선과 노이즈가 조각에 고정되게 한다.
    output.local_position = scaled_position;
    output.color = input.instance_color;
    output.cut_normal = input.instance_cut_normal;
    output.cut_parameters = input.instance_cut_parameters;
    output.edge_color = input.instance_edge_color;
    return output;
}
