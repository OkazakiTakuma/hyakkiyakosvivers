#include "CopyImage.hlsli"

Texture2D<float> gDepthTexture : register(t0);
Texture2D<float4> gFogNoiseTexture : register(t1);
SamplerState gDepthSampler : register(s0);
SamplerState gFogSampler : register(s1);

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

float main(VertexShaderOutput input) : SV_TARGET
{
    const float deviceDepth = gDepthTexture.Sample(gDepthSampler, input.uv);
    const float nearClip = max(cameraNearFar.x, 0.0001f);
    const float farClip = max(cameraNearFar.y, nearClip + 0.0001f);
    const float linearDepth = (nearClip * farClip) /
        max(farClip - deviceDepth * (farClip - nearClip), 0.0001f);
    const float distanceFog = smoothstep(fogDistanceStart, fogDistanceEnd, linearDepth);
    const float lowLayer = smoothstep(fogHeight - 0.20f, min(fogHeight + 0.42f, 1.0f), input.uv.y);
    const float2 wind = float2(time * fogSpeed, time * fogSpeed * 0.27f);
    const float broadNoise = gFogNoiseTexture.Sample(
        gFogSampler, input.uv * fogNoiseScale * 0.22f + wind).r;
    float detailNoise = broadNoise;
    if (fogHighQuality != 0)
    {
        detailNoise = gFogNoiseTexture.Sample(
            gFogSampler, input.uv * fogNoiseScale * 0.53f - wind * 0.65f + float2(0.37f, 0.61f)).r;
    }
    const float detailWeight = fogHighQuality != 0 ? 0.28f : 0.0f;
    const float shapedNoise = saturate(
        broadNoise * (1.0f - detailWeight) + detailNoise * detailWeight);
    const float densityVariation = lerp(
        1.0f - fogNoiseStrength, 1.0f + fogNoiseStrength, shapedNoise);
    float particleFog = 0.0f;
    if (fogHighQuality != 0)
    {
        const float particleShape = smoothstep(0.48f, 0.82f, detailNoise) *
            (0.55f + 0.45f * smoothstep(0.35f, 0.72f, broadNoise));
        particleFog = particleShape * lowLayer * fogParticleAmount *
            (0.22f + distanceFog * 0.38f);
    }
    return saturate(
        (distanceFog * 0.58f + lowLayer * 0.52f) * fogDensity * densityVariation + particleFog);
}
