#pragma once
#include "DirectXTex.h"
#include "d3dx12.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "struct.h"
#include "TextureManager.h"
#include <numbers>
#include <random>
#include <span>
#include <string>
#include <vector>
#include <list>
#include "../camera/Camera.h"


class Camera;
/// <summary>
/// パーティクルグループ、インスタンス描画リソース、更新・描画処理を一元管理します。
/// ParticleEmitterからの生成要求を受け、GPUへ送るインスタンスデータを構築します。
/// </summary>
class ParticleManager {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	/// <param name="dxCommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
	void Initialize(DirectXCommon* dxCommon);
	/// <summary>
	/// 共有インスタンスを取得します。
	/// </summary>
	static ParticleManager* GetInstance();
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize();
	/// <summary>変更されたHLSLを反映するため、パーティクル用PSOを再生成します。</summary>
	void ReloadPipelineState() { CreatePipelineState(); }
	void ClearGroups();
	static const uint32_t kMaxParticle;
	/// <summary>
	/// ParticleGroup を作成し、利用できる状態にします。
	/// </summary>
	/// <param name="groupName">対象となるパーティクルグループ名を指定します。</param>
	/// <param name="textureFilePath">使用するテクスチャまたはモデルのファイルパスを指定します。</param>
	void CreateParticleGroup(const std::string& groupName, const std::string& textureFilePath, ParticleMeshType meshType = kMeshTypeQuad);
	/// <summary>
	/// 現在の状態をもとに描画処理を行います。
	/// </summary>
	/// <param name="camera">描画や座標変換に使用するカメラを指定します。</param>
	void Draw(Camera* camera);
	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update();
	void SetCamera(Camera* camera) { camera_ = camera; }
	/// <param name="groupName">対象となるパーティクルグループ名を指定します。</param>
	/// <param name="textureFilePath">使用するテクスチャまたはモデルのファイルパスを指定します。</param>
	void SetGroupTexture(const std::string& groupName, const std::string& textureFilePath);
	/// <summary>
	/// パーティクルを発生させます。
	/// </summary>
	/// <param name="groupName">対象となるパーティクルグループ名を指定します。</param>
	/// <param name="position">位置を指定します。</param>
	/// <param name="count">処理する個数を指定します。</param>
	void Emit(const std::string& groupName, const Vector3& position, uint32_t count, const ParticleEmitParam& emitParam);
	/// <param name="groupName">対象となるパーティクルグループ名を指定します。</param>
	/// <param name="blendMode">描画時に使用するブレンドモードを指定します。</param>
	void SetGroupBlendMode(const std::string& groupName, BlendMode blendMode);
	bool HasGroup(const std::string& groupName) const;
	void SetGroupParticles(const std::string& groupName, std::span<const Particle> particles);
	void ClearGroupParticles(const std::string& groupName);

private:
	struct ParticleGroup {
		MaterialData material;
		std::vector<Particle> particles;
		uint32_t instanceSrvIndex = 0;
		Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource;
		uint32_t instanceCount = 0;
		ParticleForGPU* instanceDataPtr;

		BlendMode blendMode = kBlendModeNormal;
		ParticleMeshType meshType = kMeshTypeQuad;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertBuff = nullptr;
		D3D12_VERTEX_BUFFER_VIEW vbView{};
		uint32_t vertexCount = 0;
	};
	struct ParticleSceneForGPU {
		Matrix4x4 viewProjection;
		Matrix4x4 billboard;
	};
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;
	Camera* camera_ = nullptr;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, kBlendCountblend> graphicsPipelineStates;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	std::mt19937 randomEngine_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> sceneResource_;
	ParticleSceneForGPU* sceneData_ = nullptr;
	std::vector<VertexData> vertices_;
	float deltaTime = 0;
	/// <summary>
	/// RootSignature を作成し、利用できる状態にします。
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// PipelineState を作成し、利用できる状態にします。
	/// </summary>
	void CreatePipelineState();

	std::unordered_map<std::string, ParticleGroup> particleGroups_;
};
