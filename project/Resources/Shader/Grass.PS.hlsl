#include "Object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    if (textureColor.a <= 0.5f)
    {
        discard;
    }

    // 草では点光源、環境マップ、鏡面反射、フォグを省き、最低限の拡散光だけ計算する。
    float3 normal = normalize(input.normal);
    float3 lightDirection = normalize(-gDirectionalLight.direction);
    float diffuse = saturate(dot(normal, lightDirection));
    diffuse = 0.35f + diffuse * 0.65f * gDirectionalLight.intensity;

    PixelShaderOutput output;
    output.color = float4(textureColor.rgb * diffuse, textureColor.a);
    return output;
}
