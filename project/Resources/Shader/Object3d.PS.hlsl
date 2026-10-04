#include "Object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
TextureCube<float4> gEnvironmentMap : register(t1);

struct Material
{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
    float shininess; // 光沢の強さ
    float3 emissiveColor;
    float emissiveIntensity;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

// カメラ座標用
struct CameraInfo
{
    float3 worldPosition;
    float environmentMultiplier; // ★追加：環境マップの強さ (0.0f で無効、1.0f で等倍)
    // 空と同じ色へ遠景を溶かすための大気遠近法設定。
    float3 horizonColor;
    float distanceFogDensity;
    float heightFogDensity;
    float aerialPerspectiveStrength;
    int enableAerialPerspective;
    float paddingAtmosphere;
};

// ★追加：ポイントライト用の構造体
struct PointLight
{
    float4 color; // 光の色
    float3 position; // 光の位置
    float intensity; // 光の強度
    float radius; // 光の半径
    float decay; // 光の減衰率
    float2 padding; // パディング
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<CameraInfo> gCamera : register(b3);
// ★追加：ポイントライトの定数バッファをレジスタb4に割り当て
ConstantBuffer<PointLight> gPointLight : register(b4);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    if (textureColor.a <= 0.5)
        discard;

    PixelShaderOutput output;

    if (gMaterial.enableLighting < 0)
    {
        output.color = gMaterial.color;
        output.color.a *= textureColor.a;
        return output;
    }

    if (gMaterial.enableLighting != 0)
    {
        // 共通の計算
        float3 N = normalize(input.normal); // 法線
        float3 V = normalize(gCamera.worldPosition - input.worldPosition); // 視点への方向
        
        // 1. Ambient (環境反射) は1回だけ計算
        float3 ambient = gMaterial.color.rgb * textureColor.rgb * 0.1f;

        // ==========================================
        // ディレクショナルライトの計算
        // ==========================================
        float3 L_dir = normalize(-gDirectionalLight.direction);
        float3 H_dir = normalize(L_dir + V);

        // 拡散反射
        float NdotL_dir = saturate(dot(N, L_dir));
        float3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * NdotL_dir * gDirectionalLight.intensity;

        // 鏡面反射（Blinn-Phong）
        float NdotH_dir = saturate(dot(N, H_dir));
        float specularPow_dir = pow(NdotH_dir, gMaterial.shininess);
        float3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow_dir;

        // ==========================================
        // ★追加：ポイントライトの計算
        // ==========================================
        // ライトからピクセルへの方向と距離を計算
        float3 pointLightDirection = gPointLight.position - input.worldPosition;
        float distance = length(pointLightDirection);
        float3 L_point = normalize(pointLightDirection);

        // 減衰（距離がradiusを超えたら0になるように計算）
        float factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay);

        // 光が届く範囲内(factor > 0)のときだけ計算を足し合わせる
        if (factor > 0.0f)
        {
            float3 H_point = normalize(L_point + V);

            // 拡散反射
            float NdotL_point = saturate(dot(N, L_point));
            float3 diffuse_point = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * NdotL_point * gPointLight.intensity * factor;

            // 鏡面反射（Blinn-Phong）
            float NdotH_point = saturate(dot(N, H_point));
            float specularPow_point = pow(NdotH_point, gMaterial.shininess);
            float3 specular_point = gPointLight.color.rgb * gPointLight.intensity * specularPow_point * factor;

            // ディレクショナルライトの結果に足し合わせる
            diffuse += diffuse_point;
            specular += specular_point;
        }
        output.color.rgb = ambient + diffuse + specular;

        // 環境マップによる反射
        float3 cameraTOPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraTOPosition, N);
        float3 environmentColor = gEnvironmentMap.Sample(gSampler, reflectedVector).rgb;
        
        // ★修正：環境マップの色に強度（environmentMultiplier）を掛け合わせて足す
        output.color.rgb += environmentColor.rgb * gCamera.environmentMultiplier;

        // 発光はライティング結果に依存せず、ブルーム抽出用の明るい値として加算する。
        output.color.rgb += textureColor.rgb * gMaterial.emissiveColor * gMaterial.emissiveIntensity;

        // 最終的な色を計算
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
        output.color.rgb += textureColor.rgb * gMaterial.emissiveColor * gMaterial.emissiveIntensity;
    }

    if (gCamera.enableAerialPerspective != 0)
    {
        // 遠距離ほど指数的に霞を増やし、近景のコントラストは保ちます。
        const float distanceFromCamera = length(input.worldPosition - gCamera.worldPosition);
        const float distanceFog = 1.0f - exp(-distanceFromCamera * max(gCamera.distanceFogDensity, 0.0f));
        // 地表付近へ霞を集め、高い場所まで一様に白くなることを避けます。
        const float heightFog = exp(-max(input.worldPosition.y, 0.0f) * max(gCamera.heightFogDensity, 0.0f));
        const float fogAmount = saturate(
            distanceFog * heightFog * max(gCamera.aerialPerspectiveStrength, 0.0f));
        output.color.rgb = lerp(output.color.rgb, gCamera.horizonColor, fogAmount);
    }
    return output;
}
