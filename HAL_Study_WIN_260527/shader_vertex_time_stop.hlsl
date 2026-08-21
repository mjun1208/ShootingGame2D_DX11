struct VertexOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VertexOutput main(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    output.TexCoord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.Position = float4(
        output.TexCoord.x * 2.0f - 1.0f,
        1.0f - output.TexCoord.y * 2.0f,
        0.0f,
        1.0f);
    return output;
}
