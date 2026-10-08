#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 world;
    float4x4 WorldInverseTranspose;
};

// ★変更点1：ConstantBuffer から StructuredBuffer（構造化バッファの配列）に変更！
// ピクセルシェーダーで t0(テクスチャ), t1(環境マップ) を使っているため、ここは t2 にします。
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t2);

struct GrassParameter
{
    float time;
    float windStrength;
    float windFrequency;
    float padding;
};

ConstantBuffer<GrassParameter> gGrass : register(b5);

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
    // ★変更点2：GPUから「自分が配列の何番目か」を教えてもらう変数を追加
    uint instanceId : SV_InstanceID;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // ★変更点3：自分のIDを使って、配列から自分専用の行列データを取り出す
    TransformationMatrix mat = gTransformationMatrices[input.instanceId];
    
    float3 localPosition = input.position.xyz;

    // 草の根元は揺らさず、先端へ行くほど大きく曲げる。
    float heightRate = saturate(localPosition.y / 0.75f);

    // 個体のワールド座標から位相をずらし、全ての草が同じ動きにならないようにする。
    float phase = mat.world[3][0] * 0.35f + mat.world[3][2] * 0.21f;
    float wind = sin(gGrass.time * gGrass.windFrequency + phase) * gGrass.windStrength;
    localPosition.x += wind * heightRate;
    localPosition.z += wind * 0.35f * heightRate;

    float4 animatedPosition = float4(localPosition, 1.0f);
    output.position = mul(animatedPosition, mat.WVP);
    output.texcoord = input.texcoord;
    
    output.normal = normalize(mul(input.normal, (float3x3) mat.WorldInverseTranspose));
    output.worldPosition = mul(animatedPosition, mat.world).xyz;
    
    return output;
}
