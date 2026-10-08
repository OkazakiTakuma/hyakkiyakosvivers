#pragma once
#include <string>
#include <vector>
#include <optional>
#include"Matrix.h"
#include <format>
#include <fstream>
#include <locale>
#include <strsafe.h>
#include <wrl.h>
#include <map>

/// <summary>オブジェクト描画時にシェーダーへ渡す座標変換行列です。</summary>
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 world;
	Matrix4x4 WorldInverseTranspose;
	// 同じ頂点をライト空間へ変換し、影合成パスへ渡します。
	Matrix4x4 lightWVP;
};

/// <summary>描画時に使用するカラー合成方式です。</summary>
enum BlendMode {
	kBlendModeNone,
	kBlendModeNormal,
	kBlendModeAdd,
	kBlendModeSubtract,
	kBlendModeMultiply,
	kBlendModeScreen,
	kBlendCountblend,
};
/// <summary>静的・スキニングモデルで共用する頂点属性です。</summary>
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	uint32_t boneIndices[4] = {};
	Vector4 boneWeights = {0.0f, 0.0f, 0.0f, 0.0f};
};

/// <summary>シェーダーへ転送するマテリアル定数です。</summary>
struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
	float shininess;
	float padding2[3];
	Vector3 emissiveColor;
	float emissiveIntensity;
};

struct VertexWeightData
{
	float weght;
	uint32_t vertexIndex;
};

struct JointWeightData
{
	Matrix4x4 inverseBindPoseMatrix = MakeIdentity4x4();
	std::vector<VertexWeightData> vertexWeights;
	uint32_t paletteIndex = 0;
};

struct MaterialData {
	std::string textureFilePath;
	uint32_t textureIndex = 0;
};

// ModelData内で完全型として扱えるよう、Nodeを先に定義します。
struct Node {
	QuaternionTransform transform;
	Matrix4x4 localMatrix;
	std::string name;
	std::vector<Node> children;
};

/// <summary>ファイルから読み込んだメッシュ、マテリアル、階層情報のまとまりです。</summary>
struct ModelData {
	std::map<std::string, JointWeightData> skinClusterData;
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	MaterialData material;
	Node rootNode;
};
/// <summary>CPU上で更新する1パーティクルの状態です。</summary>
struct Particle {
	EulerTransform transform;
	Vector3 velocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
	bool isBillboard;

	Vector3 acceleration;
	Vector4 startColor;
	Vector4 endColor;
	Vector3 startScale;
	Vector3 endScale;

	// 渦パーティクル用の実行時軌道情報。isVortex=false の場合は使用しない。
	bool isVortex = false;
	// 発生時点のエミッター位置。XZ平面上の回転中心と高さの基準にする。
	Vector3 vortexCenter = { 0.0f, 0.0f, 0.0f };
	// 現在の周回角度と1秒あたりの角速度。角速度を負にすると逆回転する。
	float vortexAngle = 0.0f;
	float vortexAngularSpeed = 0.0f;
	// 同じ円周上に粒が揃いすぎないよう、個体ごとに半径へ掛けるばらつき。
	float vortexRadiusScale = 1.0f;
	// 下端半径から上端半径まで、高さに応じて線形補間する。
	float vortexBaseRadius = 0.0f;
	float vortexTopRadius = 0.0f;
	float vortexHeight = 1.0f;
};

struct Emitter {
	EulerTransform transform;
	uint32_t count;
	float frequency;
	float frequencyTimer;
};
struct ParticleForGPU {
	Vector3 translate;
	float isBillboard;
	Vector3 scale;
	float padding0;
	Vector3 rotate;
	float padding1;
	Vector4 color;
};
/// <summary>XAudio2へ渡す音声形式とPCMバッファです。</summary>
struct SoundData {
	WAVEFORMATEX wfx;
	std::vector<BYTE> buffer;
};

struct PointLight {
	Vector4 color;
	Vector3 position;
	float intensity;
	float radius;
	float decay;
	float enabled;
	float padding;
};

/// <summary>パーティクル生成時の初期値とランダム範囲です。</summary>
struct ParticleEmitParam {
	Vector3 scale = { 1.0f, 1.0f, 1.0f };
	Vector3 endScale = { 0.0f, 0.0f, 0.0f };

	Vector3 baseVelocity = { 0.0f, 0.0f, 0.0f };
	Vector3 randomVelocityRange = { 0.1f, 0.1f, 0.1f };

	Vector3 acceleration = { 0.0f, 0.0f, 0.0f };

	Vector3 randomPositionRange = { 0.0f, 0.0f, 0.0f };
	float lifeTime = 1.0f;
	Vector3 baseRotate = { 0.0f, 0.0f, 0.0f };
	bool isRandomRotate = false;
	Vector3 randomRotateRange = { 0.0f, 0.0f, 0.0f };
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

	Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };

	Vector3 randomScaleRange = { 0.0f, 0.0f, 0.0f };
	uint32_t count = 1;
	bool isBillboard = false;

	// Y軸を中心に螺旋運動させる。falseなら従来の速度・加速度移動を使用する。
	bool isVortex = false;
	// 1秒あたりの回転量（rad）。正数は上から見て反時計回り、負数は時計回り。
	float vortexAngularSpeed = 6.0f;
	// 竜巻の下端半径、上端半径、下端から上端までの高さ。
	float vortexBaseRadius = 0.2f;
	float vortexTopRadius = 3.0f;
	float vortexHeight = 6.0f;
};

enum ParticleMeshType {
	kMeshTypeQuad,
	kMeshTypeRing,
	kMeshTypeCylinder
};

struct ParticleSetting {
	std::string groupName;
	std::string texturePath;
	ParticleMeshType meshType;
	BlendMode blendMode;
};

/// <summary>アニメーション曲線上の時刻と値の組です。</summary>
template <typename tValue>
struct Keyframe {
	float time;
	tValue value;
};
using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

template <typename tValue>
struct AnimationCurve {
	std::vector<Keyframe<tValue>> keyframes;
};
struct NodeAnimation {
	AnimationCurve<Vector3> translate;
	AnimationCurve<Quaternion> rotate;
	AnimationCurve<Vector3> scale;
};

struct Animation {
	float duration = 0.0f;
	std::map<std::string, NodeAnimation> nodeAnimations;
};

struct Joint
{
	QuaternionTransform transform;
	QuaternionTransform bindTransform;
	Matrix4x4 localMatrix;
	Matrix4x4 bindLocalMatrix;
	Matrix4x4 skeletonSpaceMatrix;
	std::string name;
	std::vector<int32_t> children;
	int32_t index;
	std::optional<int32_t> parent;
};

/// <summary>ルート、名前検索表、全ジョイントを保持するスケルトンです。</summary>
struct Skeleton
{
	int32_t root = -1;
	std::map<std::string, int32_t> jointMap;
	std::vector<Joint> joints;
};
