// 번개 픽셀 셰이더: 텍스처 알파에서 외곽 광채·몸통·흰 중심을 만들어 빛을 합산한다.
// 인스턴싱 정점 셰이더의 출력을 받으며 color_mask는 여기서는 사용하지 않는다.

struct PS_IN
{
    float4 position : SV_POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float color_mask : COLOR1;
};

Texture2D lightning_texture : register(t0);
SamplerState lightning_sampler : register(s0);

// 좌우 같은 거리의 알파를 차례로 합쳐 원래 샘플 순서와 두께를 유지한다.
float ExpandAlphaMask(float current_mask, float2 uv, float2 offset)
{
    current_mask = max(current_mask, lightning_texture.Sample(lightning_sampler, uv - offset).a);
    current_mask = max(current_mask, lightning_texture.Sample(lightning_sampler, uv + offset).a);
    return current_mask;
}

float4 main(PS_IN input) : SV_TARGET
{
    // 텍스처 알파와 주변 샘플에서 번개의 중심/몸통 폭을 구한다.
    uint texture_width;
    uint texture_height;
    lightning_texture.GetDimensions(texture_width, texture_height);
    // 화면 픽셀이 아닌 텍스처 한 칸의 UV 폭. 확대해도 같은 원본 이웃을 읽는다.
    const float2 horizontal_texel = float2(
        1.0f / max(float(texture_width), 1.0f), 0.0f);

    const float source_mask = lightning_texture.Sample(lightning_sampler, input.uv).a;
    // 좌우 2/4텍셀의 최댓값으로 중심 마스크를 넓히고, 6텍셀까지 읽어 몸통을 만든다.
    float core_mask = source_mask;
    core_mask = ExpandAlphaMask(core_mask, input.uv, horizontal_texel * 2.0f);
    core_mask = ExpandAlphaMask(core_mask, input.uv, horizontal_texel * 4.0f);

    float body_mask = core_mask;
    body_mask = ExpandAlphaMask(body_mask, input.uv, horizontal_texel * 6.0f);
    if (body_mask <= 0.002f)
    {
        discard;
    }

    // 낮은 알파는 부드러운 광채, 중간은 몸통, 높은 알파는 흰 중심으로 나눈다.
    const float glow = pow(saturate(max(source_mask, body_mask * 0.48f)), 0.72f);
    const float body = smoothstep(0.10f, 0.48f, body_mask);
    const float core = smoothstep(0.58f, 0.92f, core_mask);
    const float fade = saturate(input.color.a);

    // 바깥은 지정색, 몸통은 청록색, 중심은 흰색으로 합성해 전기처럼 빛나게 한다.
    const float3 glow_color = input.color.rgb;
    const float3 body_color = lerp(glow_color, float3(0.45f, 0.95f, 1.0f), 0.55f);
    const float3 core_color = float3(1.0f, 1.0f, 1.0f);
    const float3 emission =
        glow_color * glow * 0.42f +
        body_color * body * 0.92f +
        core_color * core * 1.85f;

    // ONE + ONE 가산 블렌딩이므로 수명에 따른 밝기 감소를 RGB에도 직접 적용한다.
    return float4(emission * fade, saturate((glow + body + core) * fade));
}
