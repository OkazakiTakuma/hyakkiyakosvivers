struct GrassInstanceData
{
    float3 position;
    float scale;
    float rotationY;
    float3 padding;
};

StructuredBuffer<GrassInstanceData> gInstances : register(t2);

struct GrassParameter
{
    float time;
    float windStrength;
    float windFrequency;
    float padding;
};

ConstantBuffer<GrassParameter> gGrass : register(b5);

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

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
    // ★変更点2：GPUから「自分が配列の何番目か」を教えてもらう変数を追加
    uint instanceId : SV_InstanceID;
};

struct GrassVertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float heightRate : TEXCOORD1;
    float fogAmount : TEXCOORD2;
};

GrassVertexShaderOutput main(VertexShaderInput input)
{
    GrassVertexShaderOutput output;
    
    GrassInstanceData instance = gInstances[input.instanceId];
    
    float3 localPosition = input.position.xyz;

    // 草の根元は揺らさず、先端へ行くほど大きく曲げる。
    float heightRate = saturate(localPosition.y / 0.75f);

    // 個体のワールド座標から位相をずらし、全ての草が同じ動きにならないようにする。
    float phase = instance.position.x * 0.35f + instance.position.z * 0.21f;
    float wind = sin(gGrass.time * gGrass.windFrequency + phase) * gGrass.windStrength;
    localPosition.x += wind * heightRate;
    localPosition.z += wind * 0.35f * heightRate;

    localPosition *= instance.scale;
    const float rotationCos = cos(instance.rotationY);
    const float rotationSin = sin(instance.rotationY);
    const float3 rotatedPosition = float3(
        localPosition.x * rotationCos + localPosition.z * rotationSin,
        localPosition.y,
        -localPosition.x * rotationSin + localPosition.z * rotationCos);
    float4 worldPosition = float4(rotatedPosition + instance.position, 1.0f);
    output.position = mul(worldPosition, gCamera.viewProjection);
    output.texcoord = input.texcoord;

    output.normal = normalize(float3(
        input.normal.x * rotationCos + input.normal.z * rotationSin,
        input.normal.y,
        -input.normal.x * rotationSin + input.normal.z * rotationCos));
    output.worldPosition = worldPosition.xyz;
    output.heightRate = heightRate;

    output.fogAmount = 0.0f;
    if (gCamera.enableAerialPerspective != 0)
    {
        const float distanceFromCamera = length(worldPosition.xyz - gCamera.worldPosition);
        const float distanceFog = 1.0f - exp(
            -distanceFromCamera * max(gCamera.distanceFogDensity, 0.0f));
        const float heightFog = exp(
            -max(worldPosition.y, 0.0f) * max(gCamera.heightFogDensity, 0.0f));
        output.fogAmount = saturate(
            distanceFog * heightFog * max(gCamera.aerialPerspectiveStrength, 0.0f));
    }
    
    return output;
}
