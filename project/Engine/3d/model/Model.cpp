#include "Model.h"
#include "../../2d/TextureManager.h"
#include "ModelCommon.h"
#include "../../base/Logger.h"
#include "../../base/StringUtility.h"
#include <assimp/Importer.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace Logger;
using StringUtility::PathToUtf8;
using StringUtility::Utf8ToPath;

namespace {
class Utf8AssimpStream final : public Assimp::IOStream {
public:
	explicit Utf8AssimpStream(FILE* file) : file_(file) {}
	~Utf8AssimpStream() override {
		if (file_) {
			std::fclose(file_);
		}
	}

	size_t Read(void* buffer, size_t size, size_t count) override { return std::fread(buffer, size, count, file_); }
	size_t Write(const void* buffer, size_t size, size_t count) override { return std::fwrite(buffer, size, count, file_); }
	aiReturn Seek(size_t offset, aiOrigin origin) override {
		const int origins[] = {SEEK_SET, SEEK_CUR, SEEK_END};
		return _fseeki64(file_, static_cast<__int64>(offset), origins[origin]) == 0 ? aiReturn_SUCCESS : aiReturn_FAILURE;
	}
	size_t Tell() const override { return static_cast<size_t>(_ftelli64(file_)); }
	size_t FileSize() const override {
		const __int64 position = _ftelli64(file_);
		_fseeki64(file_, 0, SEEK_END);
		const __int64 size = _ftelli64(file_);
		_fseeki64(file_, position, SEEK_SET);
		return size >= 0 ? static_cast<size_t>(size) : 0;
	}
	void Flush() override { std::fflush(file_); }

private:
	FILE* file_ = nullptr;
};

// Assimp's default Windows backend uses fopen(), which interprets filenames in
// the active code page.  Its public path contract remains UTF-8, while this
// adapter converts to UTF-16 only at the Windows file boundary.
class Utf8AssimpIOSystem final : public Assimp::IOSystem {
public:
	bool Exists(const char* file) const override {
		std::error_code error;
		return file && std::filesystem::exists(Utf8ToPath(file), error);
	}
	char getOsSeparator() const override { return '\\'; }
	Assimp::IOStream* Open(const char* file, const char* mode = "rb") override {
		if (!file || !mode) {
			return nullptr;
		}
		FILE* handle = nullptr;
		const std::wstring nativeMode = StringUtility::ConvertString(std::string(mode));
		if (_wfopen_s(&handle, Utf8ToPath(file).c_str(), nativeMode.c_str()) != 0) {
			return nullptr;
		}
		return new Utf8AssimpStream(handle);
	}
	void Close(Assimp::IOStream* file) override { delete file; }
};

void EnableUtf8Paths(Assimp::Importer& importer) {
	importer.SetIOHandler(new Utf8AssimpIOSystem());
}

// OBJの四角形・多角形も既存の三角形描画処理で扱えるよう、読み込み時に三角形化する。
constexpr unsigned int kAssimpModelImportFlags =
    aiProcess_Triangulate |
    aiProcess_FlipWindingOrder |
    aiProcess_FlipUVs |
    aiProcess_LimitBoneWeights;

int HexDigitValue(char character) {
	if (character >= '0' && character <= '9') {
		return character - '0';
	}
	if (character >= 'a' && character <= 'f') {
		return character - 'a' + 10;
	}
	if (character >= 'A' && character <= 'F') {
		return character - 'A' + 10;
	}
	return -1;
}

std::string DecodeUriPath(const std::string& uriPath) {
	std::string decodedPath;
	decodedPath.reserve(uriPath.size());
	for (size_t index = 0; index < uriPath.size(); ++index) {
		if (uriPath[index] == '%' && index + 2 < uriPath.size()) {
			const int high = HexDigitValue(uriPath[index + 1]);
			const int low = HexDigitValue(uriPath[index + 2]);
			if (high >= 0 && low >= 0) {
				decodedPath.push_back(static_cast<char>((high << 4) | low));
				index += 2;
				continue;
			}
		}
		decodedPath.push_back(uriPath[index]);
	}
	return decodedPath;
}

/// <summary>
/// AffineMatrix を生成して返します。
/// </summary>
/// <param name="scale">拡大率を指定します。</param>
/// <param name="rotate">回転量を指定します。</param>
/// <param name="translate">位置を指定します。</param>
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate) {
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
	Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);
	return Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
}

Quaternion ConvertAssimpQuaternion(const aiQuaternion& quaternion) {
	return Normalize({-quaternion.x, quaternion.y, quaternion.z, quaternion.w});
}

/// <param name="matrix">計算に使用する行列を指定します。</param>
Matrix4x4 ConvertAssimpMatrix(const aiMatrix4x4& matrix) {
	Matrix4x4 result{};
	result.m[0][0] = matrix.a1;
	result.m[0][1] = matrix.b1;
	result.m[0][2] = matrix.c1;
	result.m[0][3] = matrix.d1;
	result.m[1][0] = matrix.a2;
	result.m[1][1] = matrix.b2;
	result.m[1][2] = matrix.c2;
	result.m[1][3] = matrix.d2;
	result.m[2][0] = matrix.a3;
	result.m[2][1] = matrix.b3;
	result.m[2][2] = matrix.c3;
	result.m[2][3] = matrix.d3;
	result.m[3][0] = matrix.a4;
	result.m[3][1] = matrix.b4;
	result.m[3][2] = matrix.c4;
	result.m[3][3] = matrix.d4;

	const Matrix4x4 mirrorX = MakeScaleMatrix({-1.0f, 1.0f, 1.0f});
	return Multiply(mirrorX, Multiply(result, mirrorX));
}

/// <param name="matrix">計算に使用する行列を指定します。</param>
Vector3 TransformNormal(const Vector3& normal, const Matrix4x4& matrix) {
	Vector3 result{};
	result.x = normal.x * matrix.m[0][0] + normal.y * matrix.m[1][0] + normal.z * matrix.m[2][0];
	result.y = normal.x * matrix.m[0][1] + normal.y * matrix.m[1][1] + normal.z * matrix.m[2][1];
	result.z = normal.x * matrix.m[0][2] + normal.y * matrix.m[1][2] + normal.z * matrix.m[2][2];
	return result;
}

void AddJointInfluence(VertexData& vertex, uint32_t paletteIndex, float weight) {
	if (weight <= 0.0f) {
		return;
	}

	float* weights[] = {
	    &vertex.boneWeights.x,
	    &vertex.boneWeights.y,
	    &vertex.boneWeights.z,
	    &vertex.boneWeights.w,
	};
	for (uint32_t index = 0; index < 4; index++) {
		if (*weights[index] > 0.0f && vertex.boneIndices[index] == paletteIndex) {
			*weights[index] += weight;
			return;
		}
	}

	for (uint32_t index = 0; index < 4; index++) {
		if (*weights[index] <= 0.0f) {
			vertex.boneIndices[index] = paletteIndex;
			*weights[index] = weight;
			return;
		}
	}

	uint32_t lightestIndex = 0;
	for (uint32_t index = 1; index < 4; index++) {
		if (*weights[index] < *weights[lightestIndex]) {
			lightestIndex = index;
		}
	}
	if (weight > *weights[lightestIndex]) {
		vertex.boneIndices[lightestIndex] = paletteIndex;
		*weights[lightestIndex] = weight;
	}
}

/// <summary>
/// 値を正規化して扱いやすい状態にします。
/// </summary>
void NormalizeJointInfluences(VertexData& vertex) {
	const float totalWeight =
	    vertex.boneWeights.x +
	    vertex.boneWeights.y +
	    vertex.boneWeights.z +
	    vertex.boneWeights.w;
	if (totalWeight <= 0.0f) {
		return;
	}

	vertex.boneWeights.x /= totalWeight;
	vertex.boneWeights.y /= totalWeight;
	vertex.boneWeights.z /= totalWeight;
	vertex.boneWeights.w /= totalWeight;
}

/// <summary>
/// 頂点に採用された正規化済みウェイトから、ジョイント側の参照リストを再構築します。
/// </summary>
/// <param name="modelData">再構築対象のモデルデータを指定します。</param>
void RebuildJointWeightsFromVertexInfluences(ModelData& modelData) {
	std::map<uint32_t, JointWeightData*> jointWeightsByPaletteIndex;
	for (auto& jointWeightPair : modelData.skinClusterData) {
		JointWeightData& jointWeightData = jointWeightPair.second;
		jointWeightData.vertexWeights.clear();
		jointWeightsByPaletteIndex[jointWeightData.paletteIndex] = &jointWeightData;
	}

	for (uint32_t vertexIndex = 0; vertexIndex < modelData.vertices.size(); vertexIndex++) {
		const VertexData& vertex = modelData.vertices[vertexIndex];
		const float weights[] = {
		    vertex.boneWeights.x,
		    vertex.boneWeights.y,
		    vertex.boneWeights.z,
		    vertex.boneWeights.w,
		};

		for (uint32_t influenceIndex = 0; influenceIndex < 4; influenceIndex++) {
			if (weights[influenceIndex] <= 0.0f) {
				continue;
			}

			auto jointWeightItr = jointWeightsByPaletteIndex.find(vertex.boneIndices[influenceIndex]);
			if (jointWeightItr == jointWeightsByPaletteIndex.end()) {
				continue;
			}

			jointWeightItr->second->vertexWeights.push_back({
			    weights[influenceIndex],
			    vertexIndex,
			});
		}
	}
}

}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename, const bool isAnimation) {
	this->modelCommon_ = modelCommon;
	this->isAnimation_ = isAnimation;
	modelData = LoadModelFile(directoryPath, filename);
	if (isAnimation) {
		// 1ファイル内の全クリップを読み込み、従来API向けの既定クリップには先頭を使用する。
		animations_ = LoadAnimations(directoryPath, filename);
		if (!animationNames_.empty()) {
			animation = animations_.at(animationNames_.front());
		}
	}
	CreateVertexdata();
	CreateIndexData();
	CreateMaterialData();
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData.material.textureFilePath);
}

/// <summary>
/// 破棄時に必要な解放処理を行います。
/// </summary>
Model::~Model() {
	Finalize();
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void Model::Finalize() {
	if (vertexResource) {
		vertexResource->Unmap(0, nullptr);
	}
	if (materialResource) {
		materialResource->Unmap(0, nullptr);
	}
	if (indexResource) {
		indexResource->Unmap(0, nullptr);
	}

	vertexResource.Reset();
	indexResource.Reset();
	materialResource.Reset();

	materialData = nullptr;
	vertexData = nullptr;
	modelCommon_ = nullptr;

	vertexBufferView = {};
	indexBufferView = {};
}

/// <summary>
/// 現在の状態をもとに描画処理を行います。
/// </summary>
void Model::Draw(D3D12_GPU_VIRTUAL_ADDRESS overrideMaterialAddress, const std::string& overrideTextureFilePath) {
	modelCommon_->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	modelCommon_->GetDxCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView);
	const D3D12_GPU_VIRTUAL_ADDRESS activeMaterialAddress =
		overrideMaterialAddress != 0 ? overrideMaterialAddress : materialResource->GetGPUVirtualAddress();
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, activeMaterialAddress);
	const std::string& activeTextureFilePath = overrideTextureFilePath.empty() ? modelData.material.textureFilePath : overrideTextureFilePath;
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(3, TextureManager::GetInstance()->GetSRVHandleGPU(activeTextureFilePath));

	modelCommon_->GetDxCommon()->GetCommandList()->DrawIndexedInstanced(UINT(modelData.indices.size()), 1, 0, 0, 0);
}

/// <summary>
/// モデル描画に使用するテクスチャを変更します。
/// </summary>
/// <param name="textureFilePath">使用するテクスチャのファイルパスを指定します。</param>
void Model::SetTextureFilePath(const std::string& textureFilePath) {
	if (textureFilePath.empty()) {
		return;
	}

	TextureManager::GetInstance()->LoadTexture(textureFilePath);
	modelData.material.textureFilePath = textureFilePath;
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
}

/// <summary>
/// ModelFile を読み込み、内部データへ反映します。
/// </summary>
ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;
	Assimp::Importer importer;
	EnableUtf8Paths(importer);
	const std::filesystem::path nativeFilePath = Utf8ToPath(directoryPath) / Utf8ToPath(filename);
	const std::string filePath = PathToUtf8(nativeFilePath);
	importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);

	const aiScene* scene = importer.ReadFile(filePath.c_str(), kAssimpModelImportFlags);
	assert(scene != nullptr && scene->HasMeshes());

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		const uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

			VertexData vertex;
			vertex.position = { position.x * -1.0f, position.y, position.z, 1.0f };
			vertex.normal = { normal.x * -1.0f, normal.y, normal.z };
			vertex.texcoord = { texcoord.x, texcoord.y };

			modelData.vertices.push_back(vertex);
		}

		uint32_t nextPaletteIndex = static_cast<uint32_t>(modelData.skinClusterData.size());
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
			aiBone* bone = mesh->mBones[boneIndex];
			const std::string boneName = bone->mName.C_Str();
		auto jointWeightItr = modelData.skinClusterData.find(boneName);
		if (jointWeightItr == modelData.skinClusterData.end()) {
			jointWeightItr = modelData.skinClusterData.emplace(boneName, JointWeightData{}).first;
				jointWeightItr->second.paletteIndex = nextPaletteIndex++;
			}

		JointWeightData& jointWeightData = jointWeightItr->second;
			jointWeightData.inverseBindPoseMatrix = ConvertAssimpMatrix(bone->mOffsetMatrix);

			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++) {
				const aiVertexWeight& weight = bone->mWeights[weightIndex];
				const uint32_t vertexIndex = vertexOffset + weight.mVertexId;
				jointWeightData.vertexWeights.push_back({
				    weight.mWeight,
				    vertexIndex
				});
				if (vertexIndex < modelData.vertices.size()) {
					AddJointInfluence(modelData.vertices[vertexIndex], jointWeightData.paletteIndex, weight.mWeight);
				}
			}
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3);

			for (uint32_t element = 0; element < 3; element++) {
				modelData.indices.push_back(vertexOffset + face.mIndices[element]);
			}
		}
	}

	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
			modelData.material.textureFilePath = PathToUtf8(Utf8ToPath(directoryPath) / Utf8ToPath(DecodeUriPath(texturePath.C_Str())));
			break;
		}
		if (material->GetTextureCount(aiTextureType_BASE_COLOR) != 0) {
			aiString texturePath;
			material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath);
			modelData.material.textureFilePath = PathToUtf8(Utf8ToPath(directoryPath) / Utf8ToPath(DecodeUriPath(texturePath.C_Str())));
			break;
		}
	}
	if (modelData.material.textureFilePath.empty()) {
		modelData.material.textureFilePath = "Resources/uvChecker.png";
	}

	if (scene->mRootNode != nullptr) {
		modelData.rootNode = ReadNode(scene->mRootNode);
	}

	for (VertexData& vertex : modelData.vertices) {
		NormalizeJointInfluences(vertex);
	}
	RebuildJointWeightsFromVertexInfluences(modelData);

	return modelData;
}

std::map<std::string, Animation> Model::LoadAnimations(const std::string& directoryPath, const std::string& filename) {
	// 名前検索用mapと、ファイル内の並び順を維持する名前配列を同時に構築する。
	std::map<std::string, Animation> loadedAnimations;
	animationNames_.clear();
	Assimp::Importer importer;
	EnableUtf8Paths(importer);
	const std::string filePath = PathToUtf8(Utf8ToPath(directoryPath) / Utf8ToPath(filename));

	importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
	const aiScene* scene = importer.ReadFile(filePath.c_str(), kAssimpModelImportFlags);
	assert(scene != nullptr && scene->mNumAnimations != 0);

	for (uint32_t animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex) {
		// Assimpが保持する各クリップを個別のAnimationへ変換する。
		aiAnimation* animationAssimp = scene->mAnimations[animationIndex];
		Animation loadedAnimation;
		const double ticksPerSecond = animationAssimp->mTicksPerSecond != 0.0 ? animationAssimp->mTicksPerSecond : 1.0;
		loadedAnimation.duration = static_cast<float>(animationAssimp->mDuration / ticksPerSecond);

		for (uint32_t channeIndex = 0; channeIndex < animationAssimp->mNumChannels; channeIndex++) {
			aiNodeAnim* nodeAnimtionAssimp = animationAssimp->mChannels[channeIndex];
			NodeAnimation& nodeAnimation = loadedAnimation.nodeAnimations[nodeAnimtionAssimp->mNodeName.C_Str()];

			for (uint32_t keyIndex = 0; keyIndex < nodeAnimtionAssimp->mNumPositionKeys; keyIndex++) {
				aiVectorKey& keyAssimp = nodeAnimtionAssimp->mPositionKeys[keyIndex];
				KeyframeVector3 keyframe;
				keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
				keyframe.value = {-keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
				nodeAnimation.translate.keyframes.push_back(keyframe);
			}

			for (uint32_t keyIndex = 0; keyIndex < nodeAnimtionAssimp->mNumRotationKeys; keyIndex++) {
				aiQuatKey& keyAssimp = nodeAnimtionAssimp->mRotationKeys[keyIndex];
				KeyframeQuaternion keyframe;
				keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);

				keyframe.value = ConvertAssimpQuaternion(keyAssimp.mValue);
				nodeAnimation.rotate.keyframes.push_back(keyframe);
			}

			for (uint32_t keyIndex = 0; keyIndex < nodeAnimtionAssimp->mNumScalingKeys; keyIndex++) {
				aiVectorKey& keyAssimp = nodeAnimtionAssimp->mScalingKeys[keyIndex];
				KeyframeVector3 keyframe;
				keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);

				keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
				nodeAnimation.scale.keyframes.push_back(keyframe);
			}
		}

		// 無名クリップと重複名を補正し、インスペクターと保存データで安定して参照できる名前にする。
		std::string animationName = animationAssimp->mName.length > 0
		                                ? animationAssimp->mName.C_Str()
		                                : "Animation_" + std::to_string(animationIndex + 1);
		const std::string baseName = animationName;
		uint32_t duplicateIndex = 2;
		while (loadedAnimations.contains(animationName)) {
			animationName = baseName + "_" + std::to_string(duplicateIndex++);
		}
		animationNames_.push_back(animationName);
		loadedAnimations.emplace(animationName, std::move(loadedAnimation));
	}

	return loadedAnimations;
}

Animation Model::LoadAnimation(const std::string& directoryPath, const std::string& filename) {
	const std::map<std::string, Animation> loadedAnimations = LoadAnimations(directoryPath, filename);
	return animationNames_.empty() ? Animation{} : loadedAnimations.at(animationNames_.front());
}

const Animation* Model::FindAnimation(const std::string& animationName) const {
	// 呼び出し側へ所有権を渡さず、Modelが保持するクリップを読み取り専用で返す。
	const auto animationIterator = animations_.find(animationName);
	return animationIterator != animations_.end() ? &animationIterator->second : nullptr;
}
Node Model::ReadNode(aiNode* aiNode) {
	Node result;
	aiVector3D scale;
	aiQuaternion rotate;
	aiVector3D translate;
	aiNode->mTransformation.Decompose(scale, rotate, translate);

	result.transform.scale = {scale.x, scale.y, scale.z};
	result.transform.rotate = ConvertAssimpQuaternion(rotate);
	result.transform.translate = {-translate.x, translate.y, translate.z};
	result.localMatrix = MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);

	result.name = aiNode->mName.C_Str();
	result.children.reserve(aiNode->mNumChildren);
	for (uint32_t childIndex = 0; childIndex < aiNode->mNumChildren; childIndex++) {
		result.children.push_back(ReadNode(aiNode->mChildren[childIndex]));
	}

	return result;
}
/// <summary>
/// MaterialTemplateFile を読み込み、内部データへ反映します。
/// </summary>
MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData;
	std::string line;
	std::ifstream file(Utf8ToPath(directoryPath) / Utf8ToPath(filename));
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string idenfire;
		std::istringstream s(line);
		s >> idenfire;

		if (idenfire == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath = PathToUtf8(Utf8ToPath(directoryPath) / Utf8ToPath(textureFilename));
		}
	}
	return materialData;
}

/// <summary>
/// Vertexdata を作成し、利用できる状態にします。
/// </summary>
void Model::CreateVertexdata() {
	originalVertices_ = modelData.vertices;
	skinnedVertices_ = originalVertices_;
	skinWeights_.assign(originalVertices_.size(), 0.0f);
	vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());


	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
}

/// <summary>
/// IndexData を作成し、利用できる状態にします。
/// </summary>
void Model::CreateIndexData() {
	indexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * modelData.indices.size());

	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* indexDataModel = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexDataModel));
	std::memcpy(indexDataModel, modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());
}

/// <summary>
/// MaterialData を作成し、利用できる状態にします。
/// </summary>
void Model::CreateMaterialData() {
	materialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));

	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = 1;
	materialData->uvTransform = MakeIdentity4x4();

	materialData->shininess = 20.0f;
	materialData->emissiveColor = {0.0f, 0.0f, 0.0f};
	materialData->emissiveIntensity = 0.0f;
}

uint32_t Model::GetSkinningPaletteSize() const {
	return modelData.skinClusterData.empty() ? 1u : static_cast<uint32_t>(modelData.skinClusterData.size());
}

/// <summary>
/// SkinningPalette を構築します。
/// </summary>
void Model::BuildSkinningPalette(const Skeleton& skeleton, std::vector<Matrix4x4>& palette) const {
	const uint32_t paletteSize = GetSkinningPaletteSize();
	if (palette.size() != paletteSize) {
		palette.resize(paletteSize);
	}
	for (Matrix4x4& matrix : palette) {
		matrix = MakeIdentity4x4();
	}

	for (const auto& [jointName, jointWeightData] : modelData.skinClusterData) {
		if (jointWeightData.paletteIndex >= palette.size()) {
			continue;
		}

		const auto jointItr = skeleton.jointMap.find(jointName);
		if (jointItr == skeleton.jointMap.end()) {
			continue;
		}

		const int32_t jointIndex = jointItr->second;
		if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
			continue;
		}

		palette[jointWeightData.paletteIndex] = Multiply(
		    jointWeightData.inverseBindPoseMatrix,
		    skeleton.joints[jointIndex].skeletonSpaceMatrix
		);
	}
}

/// <summary>
/// Skinning を現在の状態へ反映します。
/// </summary>
void Model::ApplySkinning(const Skeleton& skeleton) {
	if (modelData.skinClusterData.empty() || originalVertices_.empty() || !vertexData) {
		return;
	}

	if (skinnedVertices_.size() != originalVertices_.size()) {
		skinnedVertices_.resize(originalVertices_.size());
	}
	if (skinWeights_.size() != originalVertices_.size()) {
		skinWeights_.resize(originalVertices_.size());
	}

	std::copy(originalVertices_.begin(), originalVertices_.end(), skinnedVertices_.begin());
	std::fill(skinWeights_.begin(), skinWeights_.end(), 0.0f);

	for (VertexData& vertex : skinnedVertices_) {
		vertex.position = {0.0f, 0.0f, 0.0f, 0.0f};
		vertex.normal = {0.0f, 0.0f, 0.0f};
	}

	for (const auto& [jointName, jointWeightData] : modelData.skinClusterData) {
		const auto jointItr = skeleton.jointMap.find(jointName);
		if (jointItr == skeleton.jointMap.end()) {
			continue;
		}

		const int32_t jointIndex = jointItr->second;
		if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
			continue;
		}

		const Matrix4x4 skinningMatrix = Multiply(
		    jointWeightData.inverseBindPoseMatrix,
		    skeleton.joints[jointIndex].skeletonSpaceMatrix
		);
		for (const VertexWeightData& vertexWeight : jointWeightData.vertexWeights) {
			if (vertexWeight.vertexIndex >= originalVertices_.size()) {
				continue;
			}

			const VertexData& sourceVertex = originalVertices_[vertexWeight.vertexIndex];
			const Vector3 skinnedPosition = Transformation(
			    {sourceVertex.position.x, sourceVertex.position.y, sourceVertex.position.z},
			    skinningMatrix
			);
			const Vector3 skinnedNormal = TransformNormal(sourceVertex.normal, skinningMatrix);

			VertexData& destinationVertex = skinnedVertices_[vertexWeight.vertexIndex];
			destinationVertex.position.x += skinnedPosition.x * vertexWeight.weght;
			destinationVertex.position.y += skinnedPosition.y * vertexWeight.weght;
			destinationVertex.position.z += skinnedPosition.z * vertexWeight.weght;
			destinationVertex.position.w += sourceVertex.position.w * vertexWeight.weght;
			destinationVertex.normal.x += skinnedNormal.x * vertexWeight.weght;
			destinationVertex.normal.y += skinnedNormal.y * vertexWeight.weght;
			destinationVertex.normal.z += skinnedNormal.z * vertexWeight.weght;
			skinWeights_[vertexWeight.vertexIndex] += vertexWeight.weght;
		}
	}

	for (size_t vertexIndex = 0; vertexIndex < skinnedVertices_.size(); vertexIndex++) {
		if (skinWeights_[vertexIndex] <= 0.0f) {
			skinnedVertices_[vertexIndex] = originalVertices_[vertexIndex];
			continue;
		}

		skinnedVertices_[vertexIndex].position.w = 1.0f;
		skinnedVertices_[vertexIndex].normal = NormalizeReturnVector(skinnedVertices_[vertexIndex].normal);
	}

	std::memcpy(vertexData, skinnedVertices_.data(), sizeof(VertexData) * skinnedVertices_.size());
}
