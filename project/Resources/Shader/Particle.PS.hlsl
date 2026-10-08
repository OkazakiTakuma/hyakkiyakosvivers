#include "Particle.hlsli"

// テクスチャ (t0)
// ※ここはグローバル領域（波括弧の外）になければなりません
Texture2D<float4> gTexture : register(t0);

// サンプラー (s0)
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 頂点色とテクスチャ色を乗算
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * input.color;

    // 0.5固定では円形・煙テクスチャの柔らかい外周まで破棄され、明るい背景上で
    // 実際の表示面積が極端に小さくなる。ほぼ透明な画素だけを除外する。
    if (textureColor.a <= 0.01f)
        discard;

    // アルファテスト（透明なら描画しない）
    if (output.color.a <= 0.001f)
    {
        discard;
    }

    return output;
}
