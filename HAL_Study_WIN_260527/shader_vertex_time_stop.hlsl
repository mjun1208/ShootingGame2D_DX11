// 시간 정지 후처리 정점 셰이더: 정점 버퍼 없이 화면 전체를 덮는 삼각형을 만든다.
// CPU가 정점 3개를 그리면 SV_VertexID로 0, 1, 2를 받는다.

struct VertexOutput
{
    float4 clip_position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput main(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    // 비트 연산으로 UV (0,0), (2,0), (0,2)를 생성한다.
    // 화면 밖까지 뻗은 삼각형이 화면을 덮으며, 보이는 영역의 UV는 0~1로 보간된다.
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    // UV를 클립 좌표로 변환한다. 화면 UV의 아래쪽 +Y를 클립 좌표의 위쪽 +Y로 뒤집는다.
    output.clip_position = float4(
        output.uv.x * 2.0f - 1.0f,
        1.0f - output.uv.y * 2.0f,
        0.0f,
        1.0f);
    return output;
}
