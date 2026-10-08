#pragma once
#include "../model/Model.h"
#include "../camera/Camera.h"
#include "struct.h"
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>

struct InstancingMatrixData {
    Matrix4x4 WVP;
    Matrix4x4 world;
    Matrix4x4 WorldInverseTranspose;
};

struct GrassParameter {
	float time;
	float windStrength;
	float windFrequency;
	float padding;
};

class InstancingModel {
public:
    /// <summary>
    /// 必要なリソースを準備し、オブジェクトを初期化します。
    /// </summary>
    void Initialize(Model* model, uint32_t maxInstanceCount);

    void AddInstance(const EulerTransform& transform);

    /// <summary>
    /// 現在の状態をもとに描画処理を行います。
    /// </summary>
    /// <param name="camera">描画や座標変換に使用するカメラを指定します。</param>
    void Draw(Camera* camera);

    /// <summary>
    /// 破棄時に必要な解放処理を行います。
    /// </summary>
    ~InstancingModel();
    void SetEnvironmentMapPath(const std::string& path) { envMapTexturePath_ = path; }
    /// <summary>カメラからインスタンスを表示する最大距離を設定します。</summary>
    void SetMaxDrawDistance(float distance) { maxDrawDistance_ = distance; }
private:
    Model* model_ = nullptr;
    uint32_t maxInstanceCount_ = 1000;
    float maxDrawDistance_ = 35.0f;

    struct InstanceChunk {
        int32_t x = 0;
        int32_t z = 0;
        Vector3 center{};
        std::vector<EulerTransform> transforms;
    };

    static constexpr float kChunkSize = 8.0f;
    std::vector<InstanceChunk> chunks_;
    std::unordered_map<uint64_t, size_t> chunkLookup_;
    uint32_t instanceCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
    InstancingMatrixData* mappedData_ = nullptr;

    /// <summary>
    /// InstanceBuffer を作成し、利用できる状態にします。
    /// </summary>
    void CreateInstanceBuffer();

    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
    };
    struct CameraForGPU {
        Vector3 worldPosition;
        float environmentMultiplier;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU* cameraData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLight* pointLightData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> grassResource_;
    GrassParameter* grassData_ = nullptr;
    std::string envMapTexturePath_;
    /// <summary>
    /// ConstantBuffers を作成し、利用できる状態にします。
    /// </summary>
    void CreateConstantBuffers();
};
