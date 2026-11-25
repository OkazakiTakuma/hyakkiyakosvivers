static const float PI = 3.14159265f;

struct AtmosphereData
{
    float3 sunDirection;
    float sunAngularRadius;
    float3 rayleighColor;
    float rayleighStrength;
    float3 mieColor;
    float mieStrength;
    float mieG;
    float atmosphereDensity;
    float sunIntensity;
    float padding;
};

ConstantBuffer<AtmosphereData> gAtmosphere : register(b1);

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float3 viewDirection : TEXCOORD0;
};

// 微小な空気分子が作る広い散乱です。太陽の正面と反対側の両方へ緩やかに広がります。
float RayleighPhase(float cosTheta)
{
    return (3.0f / (16.0f * PI)) * (1.0f + cosTheta * cosTheta);
}

// 埃や水滴が作る前方散乱です。gが1へ近いほど太陽周辺へ光が集中します。
float MiePhase(float cosTheta, float g)
{
    const float g2 = g * g;
    const float denominator = max(1.0f + g2 - 2.0f * g * cosTheta, 0.0001f);
    return (1.0f - g2) / (4.0f * PI * pow(denominator, 1.5f));
}

float4 main(PixelShaderInput input) : SV_TARGET
{
    const float3 viewDirection = normalize(input.viewDirection);
    const float3 sunDirection = normalize(gAtmosphere.sunDirection);
    const float cosTheta = dot(viewDirection, sunDirection);

    // 地平線方向ほど視線が通る空気の距離が長いため、散乱量を増やします。
    const float horizonAmount = pow(1.0f - saturate(viewDirection.y), 2.2f);
    const float opticalDepth = 0.16f + horizonAmount * max(gAtmosphere.atmosphereDensity, 0.0f);
    // 太陽が地平線へ近づくほど青を減らし、橙から赤の成分を強くします。
    // 地平線より十分下では夕焼けを終え、夜空へ切り替えます。
    const float nightAmount = 1.0f - smoothstep(-0.24f, -0.04f, sunDirection.y);
    const float sunsetAmount = (1.0f - smoothstep(-0.08f, 0.25f, sunDirection.y)) * (1.0f - nightAmount);
    const float3 sunsetRayleigh = float3(1.0f, 0.16f, 0.028f);
    const float3 rayleighTint = lerp(gAtmosphere.rayleighColor, sunsetRayleigh, sunsetAmount);

    const float3 rayleigh = rayleighTint * RayleighPhase(cosTheta) *
        opticalDepth * gAtmosphere.rayleighStrength;
    const float3 mie = gAtmosphere.mieColor * MiePhase(cosTheta, gAtmosphere.mieG) *
        opticalDepth * gAtmosphere.mieStrength;

    // 散乱だけでは暗くなりすぎる天頂へ、夕方の青紫を残す基礎色を与えます。
    const float3 zenithColor = lerp(float3(0.055f, 0.16f, 0.42f),
        float3(0.055f, 0.025f, 0.16f), sunsetAmount);
    const float3 horizonColor = lerp(float3(0.34f, 0.47f, 0.64f),
        float3(0.88f, 0.14f, 0.025f), sunsetAmount);
    const float3 daytimeSkyBase = lerp(zenithColor, horizonColor, saturate(horizonAmount));
    const float3 nightSkyBase = lerp(float3(0.006f, 0.012f, 0.045f),
        float3(0.018f, 0.032f, 0.080f), saturate(horizonAmount));
    const float3 skyBase = lerp(daytimeSkyBase, nightSkyBase, nightAmount);

    // 球体モデルを使わず、視線と太陽方向の角度だけから輪郭の柔らかい円盤を作ります。
    const float diskInner = cos(max(gAtmosphere.sunAngularRadius, 0.001f));
    const float diskOuter = cos(max(gAtmosphere.sunAngularRadius * 1.18f, 0.0012f));
    const float sunDisk = smoothstep(diskOuter, diskInner, cosTheta);
    const float3 sunColor = lerp(float3(1.0f, 0.94f, 0.78f),
        float3(1.0f, 0.40f, 0.08f), sunsetAmount);

    const float daylight = 1.0f - nightAmount;
    const float3 result = skyBase + (rayleigh + mie) * daylight +
        sunColor * sunDisk * gAtmosphere.sunIntensity * daylight;
    return float4(result, 1.0f);
}
