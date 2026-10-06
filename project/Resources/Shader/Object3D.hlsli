struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0; // ★追加：ワールド空間での座標
    float4 shadowPosition : TEXCOORD1;
};
