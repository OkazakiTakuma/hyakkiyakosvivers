#include "Object3d.hlsli"

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

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 localPosition = input.position;
    float3 localNormal = input.normal;
    const float totalWeight = input.boneWeight.x + input.boneWeight.y + input.boneWeight.z + input.boneWeight.w;

    if (totalWeight > 0.0f)
    {
        const float4 skinWeight = input.boneWeight / totalWeight;

        localPosition =
            mul(input.position, gSkinningMatrices[input.boneIndex.x]) * skinWeight.x +
            mul(input.position, gSkinningMatrices[input.boneIndex.y]) * skinWeight.y +
            mul(input.position, gSkinningMatrices[input.boneIndex.z]) * skinWeight.z +
            mul(input.position, gSkinningMatrices[input.boneIndex.w]) * skinWeight.w;
        localPosition.w = 1.0f;

        localNormal =
            mul(input.normal, (float3x3)gSkinningMatrices[input.boneIndex.x]) * skinWeight.x +
            mul(input.normal, (float3x3)gSkinningMatrices[input.boneIndex.y]) * skinWeight.y +
            mul(input.normal, (float3x3)gSkinningMatrices[input.boneIndex.z]) * skinWeight.z +
            mul(input.normal, (float3x3)gSkinningMatrices[input.boneIndex.w]) * skinWeight.w;
        localNormal = normalize(localNormal);
    }

    output.position = mul(localPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(localNormal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(localPosition, gTransformationMatrix.world).xyz;
    output.shadowPosition = mul(localPosition, gTransformationMatrix.lightWVP);

    return output;
}
