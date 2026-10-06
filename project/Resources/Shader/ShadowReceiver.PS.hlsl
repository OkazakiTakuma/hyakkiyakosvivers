#include "Object3D.hlsli"

Texture2D<float4> gTexture : register(t0);
// 通常パスの環境マップと同じt1を、影合成パス中だけシャドウマップとして利用します。
// RootSignatureへ新しいスロットを増やさず、既存の描画構成との互換性を保つためです。
Texture2D<float> gShadowMap : register(t1);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // 葉や髪などのアルファ抜き部分に、板ポリゴン形状の影が付くのを防ぎます。
    if (gTexture.Sample(gSampler, input.texcoord).a <= 0.5f)
        discard;
    // ライトの背面や射影不能な頂点は、影なしとして扱います。
    if (input.shadowPosition.w <= 0.00001f)
        return 0.0f;

    // ライトのクリップ座標(-1～1)をテクスチャUV(0～1)へ変換します。
    // 画面座標系とテクスチャ座標系ではY方向が逆なので符号を反転します。
    const float3 projected = input.shadowPosition.xyz / input.shadowPosition.w;
    const float2 uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);
    if (projected.z <= 0.0f || projected.z >= 1.0f || any(uv < 0.0f) || any(uv > 1.0f))
        return 0.0f;

    // 実際のテクスチャサイズから安全な整数座標を求め、範囲外Loadを防ぎます。
    uint width, height;
    gShadowMap.GetDimensions(width, height);
    const int2 texel = clamp(int2(uv * float2(width, height)), int2(0, 0), int2(width - 1, height - 1));
    const float storedDepth = gShadowMap.Load(int3(texel, 0));
    // 現在の面がライトから見た最前面より奥なら、ほかの形状に遮られています。
    // 小さなバイアスは浮動小数点誤差による自己シャドウを抑えるためのものです。
    const bool inShadow = projected.z - 0.0008f > storedDepth;
    // 黒の半透明を通常描画の上へ重ね、影にならないピクセルは完全透明にします。
    return inShadow ? float4(0.0f, 0.0f, 0.0f, 0.45f) : 0.0f;
}
