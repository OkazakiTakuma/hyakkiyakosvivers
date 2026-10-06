struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 world;
    float4x4 WorldInverseTranspose;
    float4x4 lightWVP;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);
StructuredBuffer<float4x4> gSkinningMatrices : register(t2);

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
    uint4 boneIndex : BONEINDEX;
    float4 boneWeight : BONEWEIGHT;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 localPosition = input.position;
    const float totalWeight = dot(input.boneWeight, 1.0f);
    if (totalWeight > 0.0f)
    {
        // 通常描画と同じボーン変形を適用し、アニメーション中も影をモデルへ追従させます。
        const float4 weight = input.boneWeight / totalWeight;
        localPosition =
            mul(input.position, gSkinningMatrices[input.boneIndex.x]) * weight.x +
            mul(input.position, gSkinningMatrices[input.boneIndex.y]) * weight.y +
            mul(input.position, gSkinningMatrices[input.boneIndex.z]) * weight.z +
            mul(input.position, gSkinningMatrices[input.boneIndex.w]) * weight.w;
        localPosition.w = 1.0f;
    }
    // WVPにはObject3d側で計算した world * lightViewProjection が入ります。
    output.position = mul(localPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
