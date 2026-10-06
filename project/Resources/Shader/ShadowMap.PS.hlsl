Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float main(PixelShaderInput input) : SV_TARGET
{
    // 透明部分は影を落とさないよう、通常描画と同じアルファ判定を使います。
    if (gTexture.Sample(gSampler, input.texcoord).a <= 0.5f)
        discard;
    // SV_POSITION.zは射影後の0～1深度です。R32_FLOATへ保存し、受け側パスで比較します。
    return input.position.z;
}
