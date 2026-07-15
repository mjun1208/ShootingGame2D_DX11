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
};

struct VS_OUT
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VS_OUT main(VS_IN input)
{
    VS_OUT output;
    const float sine = sin(input.instance_rotation);
    const float cosine = cos(input.instance_rotation);
    const float2 scaled = input.position.xy * input.instance_size;
    const float2 rotated = float2(
        scaled.x * cosine - scaled.y * sine,
        scaled.x * sine + scaled.y * cosine);
    const float4 world_position = float4(rotated + input.instance_position, 0.0f, 1.0f);

    output.position = mul(world_position, view_projection);
    output.uv = input.uv;
    output.color = input.instance_color;
    return output;
}
