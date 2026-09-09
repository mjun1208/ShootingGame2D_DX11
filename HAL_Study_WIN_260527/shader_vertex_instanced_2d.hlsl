// 인스턴싱 정점 셰이더: 하나의 사각형을 공유하면서 개체별 위치·크기·색상을 적용한다.
// POSITION/UV는 공통 꼭짓점 데이터, INSTANCE_*는 스프라이트마다 달라지는 데이터다.

cbuffer SceneBuffer : register(b0)
{
    // CPU가 설정한 화면 변환/투영 행렬. 월드 위치를 클립 좌표로 보낸다.
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
    float instance_color_mask : INSTANCE_COLOR_MASK0;
};

struct VS_OUT
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float color_mask : COLOR1;
};

VS_OUT main(VS_IN input)
{
    VS_OUT output;
    // 크기 적용 → 라디안 회전 → 월드 위치 이동 순서로 사각형을 배치한다.
    const float sine = sin(input.instance_rotation);
    const float cosine = cos(input.instance_rotation);
    const float2 scaled_position = input.position.xy * input.instance_size;
    const float2 rotated_position = float2(
        scaled_position.x * cosine - scaled_position.y * sine,
        scaled_position.x * sine + scaled_position.y * cosine);
    const float4 world_position = float4(rotated_position + input.instance_position, 0.0f, 1.0f);

    // 위치는 클립 좌표로, UV는 시트 내부 프레임 영역으로 변환한다.
    output.position = mul(world_position, view_projection);
    output.uv = input.instance_texcoord_offset + input.uv * input.instance_texcoord_scale;
    output.color = input.instance_color;
    output.color_mask = input.instance_color_mask;
    return output;
}
