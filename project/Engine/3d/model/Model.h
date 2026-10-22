#pragma once
#include "Matrix.h"
#include "struct.h"
#include <Windows.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <d3d12.h>
#include <map>
#include <string>
#include <vector>
#include <wrl.h>

// このヘッダーからAssimpのヘッダーを除外するため、ノード型を前方宣言します。

class ModelCommon;
class Model {
public:
	friend class InstancingModel;

	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename,const bool isAnimation);
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize();
	/// <summary>
	/// 現在の状態をもとに描画処理を行います。
	/// </summary>
	/// <param name="overrideMaterialAddress">0以外なら、この描画だけ別のマテリアルCBVを使用します。</param>
	/// <param name="overrideTextureFilePath">指定時は共有モデルを変更せず、この描画だけ別のテクスチャを使用します。</param>
	void Draw(D3D12_GPU_VIRTUAL_ADDRESS overrideMaterialAddress = 0, const std::string& overrideTextureFilePath = {});
	/// <summary>
	/// 破棄時に必要な解放処理を行います。
	/// </summary>
	~Model();
	const Node& GetRootNode() const { return modelData.rootNode; }
	const ModelData& GetModelData() const { return modelData; }
	uint32_t GetVertexCount() const {
		return static_cast<uint32_t>(modelData.vertices.size());
	}
	/// <summary>
	/// Animation を読み込み、内部データへ反映します。
	/// </summary>
	Animation LoadAnimation(const std::string& directoryPath, const std::string& filename);
	std::map<std::string, Animation> LoadAnimations(const std::string& directoryPath, const std::string& filename);
	Animation GetAnimation() const { return animation; };
	const Animation* FindAnimation(const std::string& animationName) const;
	const std::vector<std::string>& GetAnimationNames() const { return animationNames_; }
	const bool GetIsAnimation() { return isAnimation_; };
	bool HasSkinCluster() const { return !modelData.skinClusterData.empty(); }
	void SetTextureFilePath(const std::string& textureFilePath);
	const std::string& GetTextureFilePath() const { return modelData.material.textureFilePath; }
	uint32_t GetSkinningPaletteSize() const;
	/// <summary>
	/// SkinningPalette を構築します。
	/// </summary>
	void BuildSkinningPalette(const Skeleton& skeleton, std::vector<Matrix4x4>& palette) const;
	/// <summary>
	/// Skinning を現在の状態へ反映します。
	/// </summary>
	void ApplySkinning(const Skeleton& skeleton);

private:
	ModelCommon* modelCommon_ = nullptr;
	ModelData modelData;
	std::vector<VertexData> originalVertices_;
	std::vector<VertexData> skinnedVertices_;
	std::vector<float> skinWeights_;
	Animation animation;
	std::map<std::string, Animation> animations_;
	std::vector<std::string> animationNames_;
	bool isAnimation_;
	/// <summary>
	/// ModelFile を読み込み、内部データへ反映します。
	/// </summary>
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);
	static Node ReadNode(aiNode* aiNode);
	/// <summary>
	/// MaterialTemplateFile を読み込み、内部データへ反映します。
	/// </summary>
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;
	/// <summary>
	/// MaterialData を作成し、利用できる状態にします。
	/// </summary>
	void CreateMaterialData();
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	VertexData* vertexData = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	/// <summary>
	/// Vertexdata を作成し、利用できる状態にします。
	/// </summary>
	void CreateVertexdata();
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	/// <summary>
	/// IndexData を作成し、利用できる状態にします。
	/// </summary>
	void CreateIndexData();

};
