struct PS_IN
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

Texture2D sprite_texture : register(t0);
SamplerState sprite_sampler : register(s0);

float4 main(PS_IN input) : SV_TARGET
{
    const float4 color = sprite_texture.Sample(sprite_sampler, input.uv) * input.color;
    if (color.a <= 0.001f)
    {
        discard;
    }
    return color;
}
