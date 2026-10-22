#pragma once
#include "../model/Model.h"
#include "../camera/Camera.h"
#include "struct.h"
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>

/// <summary>インスタンス配置、可視判定、共通GPUリソースを管理する描画基盤です。</summary>
/// <remarks>使用するパイプラインと固有リソースは派生クラスへ委譲します。</remarks>
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
    virtual ~InstancingModel();
    void SetEnvironmentMapPath(const std::string& path) { envMapTexturePath_ = path; }
    /// <summary>カメラからインスタンスを表示する最大距離を設定します。</summary>
    void SetMaxDrawDistance(float distance) {
		maxDrawDistance_ = distance;
		visibilityCacheValid_ = false;
	}
	/// <summary>インスタンス配置をカメラ周囲で繰り返すX/Z方向の周期を設定します。0の軸は繰り返しません。</summary>
	void SetLoopPeriod(float x, float z) {
		loopPeriodX_ = x;
		loopPeriodZ_ = z;
		visibilityCacheValid_ = false;
	}
	/// <summary>インスタンシング描画へ使用する平行光源を設定します。</summary>
	void SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity) {
		if (!lightData_) return;
		lightData_->color = color;
		lightData_->direction = NormalizeReturnVector(direction);
		lightData_->intensity = intensity;
	}
protected:
	/// <summary>派生レンダラーのパイプラインを設定し、固有リソースをバインドします。</summary>
	virtual void PrepareDraw(ID3D12GraphicsCommandList* commandList) = 0;
	/// <summary>可視インスタンスを派生レンダラー固有のGPU形式へ書き込みます。</summary>
	virtual void WriteVisibleInstance(
		uint32_t index, const EulerTransform& transform, const Vector3& drawPosition) = 0;

private:
    Model* model_ = nullptr;
    uint32_t maxInstanceCount_ = 1000;
    float maxDrawDistance_ = 35.0f;
	float loopPeriodX_ = 0.0f;
	float loopPeriodZ_ = 0.0f;

    struct InstanceChunk {
        int32_t x = 0;
        int32_t z = 0;
        Vector3 center{};
        Vector3 minBounds{};
        Vector3 maxBounds{};
        std::vector<EulerTransform> transforms;
    };

    static constexpr float kChunkSize = 8.0f;
    std::vector<InstanceChunk> chunks_;
    std::unordered_map<uint64_t, size_t> chunkLookup_;
    uint32_t instanceCount_ = 0;
	float modelBoundingRadius_ = 0.0f;
	// 固定配置を静止カメラで描く場合、前フレームの可視判定と行列を再利用します。
	bool visibilityCacheValid_ = false;
	Vector3 cachedCameraPosition_{};
	Matrix4x4 cachedViewProjection_{};
	uint32_t cachedVisibleInstanceCount_ = 0;

    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
    };
    struct CameraForGPU {
        Matrix4x4 viewProjection;
        Vector3 worldPosition;
        float environmentMultiplier;
		Vector3 horizonColor;
		float distanceFogDensity;
		float heightFogDensity;
		float aerialPerspectiveStrength;
		int32_t enableAerialPerspective;
		float paddingAtmosphere;
    };
	static_assert(sizeof(CameraForGPU) == 112, "CameraForGPU must match the HLSL constant-buffer layout.");

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU* cameraData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLight* pointLightData_ = nullptr;
    std::string envMapTexturePath_;
    /// <summary>
    /// ConstantBuffers を作成し、利用できる状態にします。
    /// </summary>
    void CreateConstantBuffers();
};
