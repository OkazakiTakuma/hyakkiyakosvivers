#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
Texture2D<float> maskTexture : register(t2);
Texture2D<float4> gBloomTexture : register(t3);
Texture2D<float> gFogTexture : register(t4);
SamplerState gSampler : register(s0);

cbuffer ColorInfo : register(b0)
{
    float4 tintColor;
    int enableGrayscale;
    int enableVignetting;
    int enableSmoothing;
    int enableGaussianFilter;
    int enableRadialBlur;
    int enableRandom;
    int radialBlurSamples;
    int enableOutline;
    int enableDissolve;
    float vignetteIntensity;
    float vignetteRadius;
    float vignetteSoftness;
    float radialBlurStrength;
    float randomStrength;
    float outlineStrength;
    float outlineThreshold;
    float outlineThickness;
    float dissolveThreshold;
    float dissolveEdgeWidth;
    float time;
    float2 texelSize;
    float2 cameraNearFar;
    float4 outlineColor;
    float4 dissolveEdgeColor;
    float damageVignetteIntensity;
    float damageVignetteRadius;
    float damageVignetteSoftness;
    float paddingDamageVignette;
    int enableBloom;
    float bloomThreshold;
    float bloomIntensity;
    float bloomRadius;
    float exposure;
    int enableToneMapping;
    float bloomKnee;
    float paddingHdr;
    int enableFog;
    float fogDensity;
    float fogHeight;
    float fogNoiseScale;
    float4 fogColor;
    float fogNoiseStrength;
    float fogSpeed;
    float fogDistanceStart;
    float fogDistanceEnd;
    float fogParticleAmount;
    int fogHighQuality;
    float2 paddingFog;
};

struct PixelShaderOutPut
{
    float4 color : SV_TARGET0;
};

float Random(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898f, 78.233f)) + time * 43.758f) * 43758.5453f);
}

float4 ApplySmoothing(float2 uv)
{
    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            color += gTexture.Sample(gSampler, uv + float2(x, y) * texelSize);
        }
    }

    return color / 9.0f;
}

float4 ApplyGaussianFilter(float2 uv)
{
    static const float kernel[9] = {
        1.0f, 2.0f, 1.0f,
        2.0f, 4.0f, 2.0f,
        1.0f, 2.0f, 1.0f
    };

    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int kernelIndex = 0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            color += gTexture.Sample(gSampler, uv + float2(x, y) * texelSize) * kernel[kernelIndex];
            ++kernelIndex;
        }
    }

    return color / 16.0f;
}

// HDRの広い輝度範囲を、ハイライトの色を残しながら表示可能な0～1へ圧縮します。
float3 AcesToneMap(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float4 ApplyRadialBlur(float2 uv)
{
    const float2 center = float2(0.5f, 0.5f);
    const float2 direction = uv - center;
    const int sampleCount = clamp(radialBlurSamples, 2, 32);
    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int index = 0; index < sampleCount; ++index)
    {
        const float rate = (float)index / (float)(sampleCount - 1);
        const float2 sampleUv = uv - direction * radialBlurStrength * rate;
        color += gTexture.Sample(gSampler, sampleUv);
    }

    return color / (float)sampleCount;
}

float4 ApplyDepthOutline(float2 uv, float4 sourceColor)
{
    // UVを深度テクスチャ上の整数座標へ変換し、Loadで補間せずに深度を取得します。
    // 線形サンプリングによる境界のぼけを避け、Thicknessをピクセル単位で扱います。
    uint depthWidth;
    uint depthHeight;
    gDepthTexture.GetDimensions(depthWidth, depthHeight);
    const int2 maxPixel = int2(depthWidth, depthHeight) - 1;
    const int2 centerPixel = clamp(int2(uv * float2(depthWidth, depthHeight)), int2(0, 0), maxPixel);
    const int pixelOffset = max((int)round(outlineThickness), 1);
    const float nearClip = max(cameraNearFar.x, 0.0001f);
    const float farClip = max(cameraNearFar.y, nearClip + 0.0001f);
    const float centerDeviceDepth = gDepthTexture.Load(int3(centerPixel, 0));

    // DirectXの0～1の非線形深度を、カメラからの実距離に近い線形深度へ戻します。
    const float centerDepth = (nearClip * farClip) /
        max(farClip - centerDeviceDepth * (farClip - nearClip), 0.0001f);
    float maxRelativeDepthDifference = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            if (x == 0 && y == 0)
            {
                continue;
            }

            const int2 samplePixel = clamp(centerPixel + int2(x, y) * pixelOffset, int2(0, 0), maxPixel);
            const float sampleDeviceDepth = gDepthTexture.Load(int3(samplePixel, 0));
            const float sampleDepth = (nearClip * farClip) /
                max(farClip - sampleDeviceDepth * (farClip - nearClip), 0.0001f);

            // 絶対差では遠距離ほど反応しにくいため、近い側の深度に対する相対差で比較します。
            const float relativeDepthDifference = abs(centerDepth - sampleDepth) /
                max(min(centerDepth, sampleDepth), nearClip);
            maxRelativeDepthDifference = max(maxRelativeDepthDifference, relativeDepthDifference);
        }
    }

    // 閾値付近をsmoothstepで補間し、輪郭の点滅や極端なギザつきを抑えます。
    const float transitionWidth = max(outlineThreshold * 0.25f, 0.001f);
    const float edge = smoothstep(
        outlineThreshold,
        outlineThreshold + transitionWidth,
        maxRelativeDepthDifference
    );
    // 色のアルファを輪郭の不透明度として扱い、強度と合わせて元画像へ合成します。
    const float blend = saturate(edge * outlineStrength * outlineColor.a);
    sourceColor.rgb = lerp(sourceColor.rgb, outlineColor.rgb, blend);
    return sourceColor;
}

float4 ApplyDissolve(float2 uv, float4 sourceColor)
{
    const float maskValue = maskTexture.Sample(gSampler, uv + float2(time * 0.015f, time * 0.01f));
    if (maskValue < dissolveThreshold)
    {
        discard;
    }

    const float edgeWidth = max(dissolveEdgeWidth, 0.0001f);
    const float edge = 1.0f - smoothstep(dissolveThreshold, dissolveThreshold + edgeWidth, maskValue);
    sourceColor.rgb = lerp(sourceColor.rgb, dissolveEdgeColor.rgb, saturate(edge) * dissolveEdgeColor.a);
    return sourceColor;
}

float4 ApplyFog(float2 uv, float4 sourceColor)
{
    // 半解像度で生成済みの霧マスクを線形補間し、最終パスは1サンプルだけで合成します。
    const float fogAmount = gFogTexture.Sample(gSampler, uv);
    sourceColor.rgb = lerp(sourceColor.rgb, fogColor.rgb, fogAmount * fogColor.a);
    return sourceColor;
}

PixelShaderOutPut main(VertexShaderOutput input)
{
    PixelShaderOutPut output;

    float4 resultColor = gTexture.Sample(gSampler, input.uv);

    if (enableSmoothing != 0)
    {
        resultColor = ApplySmoothing(input.uv);
    }

    if (enableGaussianFilter != 0)
    {
        resultColor = ApplyGaussianFilter(input.uv);
    }

    if (enableRadialBlur != 0)
    {
        resultColor = ApplyRadialBlur(input.uv);
    }

    if (enableBloom != 0)
    {
        // 明部抽出とぼかしは半解像度の別パスで完了しているため、ここでは加算だけを行います。
        resultColor.rgb += gBloomTexture.Sample(gSampler, input.uv).rgb * bloomIntensity;
    }

    if (enableGrayscale != 0)
    {
        float gray = dot(resultColor.rgb, float3(0.299f, 0.587f, 0.114f));
        resultColor = float4(gray, gray, gray, resultColor.a);
    }

    if (enableOutline != 0)
    {
        resultColor = ApplyDepthOutline(input.uv, resultColor);
    }

    if (enableVignetting != 0)
    {
        float2 centeredUv = input.uv - float2(0.5f, 0.5f);
        float distanceFromCenter = length(centeredUv);
        float edgeFactor = smoothstep(vignetteRadius, vignetteRadius + vignetteSoftness, distanceFromCenter);
        float vignette = 1.0f - saturate(edgeFactor * vignetteIntensity);
        resultColor.rgb *= vignette;
    }

    if (enableRandom != 0)
    {
        const float noise = Random(input.uv) * 2.0f - 1.0f;
        resultColor.rgb += noise * randomStrength;
    }

    if (enableDissolve != 0)
    {
        resultColor = ApplyDissolve(input.uv, resultColor);
    }

    if (enableFog != 0)
    {
        resultColor = ApplyFog(input.uv, resultColor);
    }

    if (damageVignetteIntensity > 0.0f)
    {
        // 円形距離ではなく最大軸距離を使い、四隅だけでなく画面の四辺全体を赤くする。
        const float2 centeredUv = abs(input.uv - float2(0.5f, 0.5f));
        const float rectangularDistance = max(centeredUv.x, centeredUv.y);
        const float damageEdge = smoothstep(
            damageVignetteRadius,
            damageVignetteRadius + max(damageVignetteSoftness, 0.001f),
            rectangularDistance
        );
        const float damageBlend = saturate(damageEdge * damageVignetteIntensity);
        resultColor.rgb = lerp(resultColor.rgb, float3(0.72f, 0.0f, 0.0f), damageBlend);
    }

    resultColor *= tintColor;
    resultColor.rgb = max(resultColor.rgb * max(exposure, 0.0f), 0.0f);
    if (enableToneMapping != 0)
    {
        resultColor.rgb = AcesToneMap(resultColor.rgb);
    }
    output.color = resultColor;
    return output;
}
