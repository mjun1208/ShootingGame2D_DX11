struct PS_IN
{
    float4 posH : SV_POSITION0;
    float2 uv : TEXCOORD0;
};

cbuffer SpriteBuffer : register(b0)
{
    float4 color;
    float4 dissolve;
    float4 edge_color;
}

Texture2D major_texture : register(t0);
Texture2D dissolve_texture : register(t1);
SamplerState major_sampler : register(s0);

float4 main(PS_IN ps_in) : SV_TARGET
{
    float4 texture_color = major_texture.Sample(major_sampler, ps_in.uv) * color;
    if (texture_color.a <= 0.001f)
    {
        discard;
    }

    if (dissolve.x > 0.5f)
    {
        const float noise = dissolve_texture.Sample(major_sampler, ps_in.uv).r;
        const float amount = saturate(dissolve.y);
        const float edge_width = max(dissolve.z, 0.001f);
        const float edge_distance = noise - amount;

        if (edge_distance < 0.0f)
        {
            discard;
        }

        const float edge = 1.0f - smoothstep(0.0f, edge_width, edge_distance);
        texture_color.rgb += edge_color.rgb * edge_color.a * edge;
    }

    return texture_color;
}