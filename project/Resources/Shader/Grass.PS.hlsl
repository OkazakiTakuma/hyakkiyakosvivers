Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);

struct CameraData
{
    float4x4 viewProjection;
    float3 worldPosition;
    float environmentMultiplier;
    float3 horizonColor;
    float distanceFogDensity;
    float heightFogDensity;
    float aerialPerspectiveStrength;
    int enableAerialPerspective;
    float paddingAtmosphere;
};

ConstantBuffer<CameraData> gCamera : register(b3);

struct GrassVertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float heightRate : TEXCOORD1;
    float fogAmount : TEXCOORD2;
};

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(GrassVertexShaderOutput input)
{
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    if (textureColor.a <= 0.5f)
    {
        discard;
    }

    // 平行光源の拡散反射に加え、根元を暗くして地面との接地感を補う。
    float3 normal = normalize(input.normal);
    float3 lightDirection = normalize(-gDirectionalLight.direction);
    float diffuse = saturate(dot(normal, lightDirection));
    diffuse = 0.35f + diffuse * 0.65f * gDirectionalLight.intensity;
    const float rootShade = lerp(0.58f, 1.0f, smoothstep(0.0f, 0.55f, input.heightRate));

    PixelShaderOutput output;
    output.color = float4(textureColor.rgb * diffuse * rootShade, textureColor.a);
    output.color.rgb = lerp(output.color.rgb, gCamera.horizonColor, input.fogAmount);
    return output;
}
