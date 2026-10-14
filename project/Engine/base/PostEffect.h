#pragma once
#include "DirectXCommon.h"
#include "struct.h"
#include <array>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl/client.h>

class PostEffect {
public:
	/// <summary>
	/// 共有インスタンスを取得します。
	/// </summary>
	static PostEffect* GetInstance();

	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	/// <param name="dxCommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
	void Initialize(DirectXCommon* dxCommon);

	void PreDrawScene();

	void PostDrawScene();

	/// <summary>
	/// 現在の状態をもとに描画処理を行います。
	/// </summary>
	void Draw();
	void ResizeIfNeeded();

	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize();
	/// <summary>変更されたHLSLを反映するため、ポストエフェクト用PSOを再生成します。</summary>
	void ReloadPipelineState() { CreatePipelineState(); }

	/// <summary>
	/// ImGui によるデバッグ用 UI の表示と編集処理を行います。
	/// </summary>
	void DrawImGui();

	void UpdateHotkeys();
	/// <summary>プレイヤー被弾時の赤い画面端エフェクトを開始します。</summary>
	void TriggerDamageVignette();
	/// <summary>深度と画面位置から合成する低層霧の有効状態と外観を設定します。</summary>
	void SetFogSettings(bool enabled, const Vector4& color, float density, float height,
		float noiseScale, float noiseStrength, float particleAmount, float speed,
		float distanceStart, float distanceEnd, bool highQuality = false);

	bool IsActive() const { return isActive_; }

private:
	PostEffect() = default;
	~PostEffect() = default;
	PostEffect(const PostEffect&) = delete;
	PostEffect& operator=(const PostEffect&) = delete;

	/// <summary>
	/// TextureResource を作成し、利用できる状態にします。
	/// </summary>
	void CreateTextureResource();
	/// <summary>
	/// Rtv を作成し、利用できる状態にします。
	/// </summary>
	void CreateRtv();
	/// <summary>
	/// Dsv を作成し、利用できる状態にします。
	/// </summary>
	void CreateDsv();
	/// <summary>
	/// Srv を作成し、利用できる状態にします。
	/// </summary>
	void CreateSrv();
	/// <summary>半解像度Bloomで使用する2枚のHDR描画先とSRVを作成します。</summary>
	void CreateBloomResources();
	void CreateDissolveMask();
	/// <summary>霧の模様に使用するタイル可能なノイズテクスチャを読み込みます。</summary>
	void CreateFogNoiseTexture();
	/// <summary>霧だけを半解像度で生成する描画先を作成します。</summary>
	void CreateFogResources();
	/// <summary>
	/// RootSignature を作成し、利用できる状態にします。
	/// </summary>
	void CreateRootSignature();
	/// <summary>
	/// PipelineState を作成し、利用できる状態にします。
	/// </summary>
	void CreatePipelineState();
	/// <summary>輝度抽出と縦横ぼかしに共通使用するBloomパイプラインを作成します。</summary>
	void CreateBloomPipelineState();
	void CreateFogPipelineState();
	/// <summary>HDRシーンから明部を抽出し、半解像度で縦横にぼかします。</summary>
	void DrawBloom();
	/// <summary>深度とノイズから半解像度の霧マスクを生成します。</summary>
	void DrawFog();

	/// <summary>
	/// ColorBuffer を作成し、利用できる状態にします。
	/// </summary>
	void CreateColorBuffer();
	/// <summary>
	/// SettingsToBuffer を現在の状態へ反映します。
	/// </summary>
	void ApplySettingsToBuffer();
	void ResizeResources(int32_t width, int32_t height);

private:
	DirectXCommon* dxCommon_ = nullptr;

	bool isActive_ = true;

	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_ = nullptr;

	uint32_t srvIndex_ = UINT32_MAX;
	uint32_t depthSrvIndex_ = UINT32_MAX;
	uint32_t dissolveMaskSrvIndex_ = UINT32_MAX;
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
	D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandleGPU_{};
	D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskSrvHandleGPU_{};
	D3D12_GPU_DESCRIPTOR_HANDLE fogNoiseSrvHandleGPU_{};
	uint32_t fogSrvIndex_ = UINT32_MAX;
	D3D12_GPU_DESCRIPTOR_HANDLE fogSrvHandleGPU_{};
	std::array<uint32_t, 2> bloomSrvIndices_{UINT32_MAX, UINT32_MAX};
	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 2> bloomSrvHandlesGPU_{};
	int32_t renderWidth_ = 1;
	int32_t renderHeight_ = 1;
	int32_t bloomWidth_ = 1;
	int32_t bloomHeight_ = 1;
	int32_t fogWidth_ = 1;
	int32_t fogHeightPixels_ = 1;
	bool sceneTextureReadyAsSrv_ = false;
	bool depthTextureReadyAsSrv_ = false;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> bloomRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> fogRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> fogPipelineState_ = nullptr;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> bloomResources_{};
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> bloomRtvHeap_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> fogResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> fogRtvHeap_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> colorBuffer_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> dissolveMaskResource_ = nullptr;

	struct ColorData {
		float r, g, b, a;
		int32_t enableGrayscale;
		int32_t enableVignetting;
		int32_t enableSmoothing;
		int32_t enableGaussianFilter;
		int32_t enableRadialBlur;
		int32_t enableRandom;
		int32_t radialBlurSamples;
		int32_t enableOutline;
		int32_t enableDissolve;
		float vignetteIntensity;
		float vignetteRadius;
		float vignetteSoftness;
		float radialBlurStrength;
		float randomStrength;
		float outlineStrength;
		float outlineThreshold;
		float outlineThickness;
		float dissolveThreshold;
		float dissolveEdgeWidth;
		float time;
		float texelSize[2];
		// 深度値をビュー空間の距離へ戻すための Near / Far クリップ距離です。
		float cameraNearFar[2];
		float outlineColor[4];
		float dissolveEdgeColor[4];
		// 通常ビネットと独立して合成する、被弾専用の赤い画面端エフェクト設定。
		float damageVignetteIntensity;
		float damageVignetteRadius;
		float damageVignetteSoftness;
		float paddingDamageVignette;
		int32_t enableBloom;
		float bloomThreshold;
		float bloomIntensity;
		float bloomRadius;
		float exposure;
		int32_t enableToneMapping;
		float bloomKnee;
		float paddingHdr;
		int32_t enableFog;
		float fogDensity;
		float fogHeight;
		float fogNoiseScale;
		float fogColor[4];
		float fogNoiseStrength;
		float fogSpeed;
		float fogDistanceStart;
		float fogDistanceEnd;
		float fogParticleAmount;
		int32_t fogHighQuality;
		float paddingFog[2];
	};
	static_assert(sizeof(ColorData) == 256, "ColorData must match the FullScreen.PS.hlsl constant buffer layout");

	ColorData* colorData_ = nullptr;

	float tintColor_[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool enableGrayscale_ = false;
	bool enableVignetting_ = false;
	bool enableSmoothing_ = false;
	bool enableGaussianFilter_ = false;
	bool enableRadialBlur_ = false;
	bool enableRandom_ = false;
	bool enableOutline_ = false;
	bool enableDissolve_ = false;
	float vignetteIntensity_ = 0.9f;
	float vignetteRadius_ = 0.05f;
	float vignetteSoftness_ = 0.2f;
	float radialBlurStrength_ = 0.16f;
	int radialBlurSamples_ = 16;
	float randomStrength_ = 0.1f;
	// 深度差アウトラインの描画濃度、検出感度、サンプリング間隔です。
	float outlineStrength_ = 1.6f;
	float outlineThreshold_ = 0.05f;
	float outlineThickness_ = 2.0f;
	float outlineColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float dissolveThreshold_ = 0.5f;
	float dissolveEdgeWidth_ = 0.14f;
	float dissolveEdgeColor_[4] = { 1.0f, 0.25f, 0.02f, 1.0f };
	float time_ = 0.0f;
	// TriggerDamageVignetteでタイマーを戻し、二次曲線で素早く減衰させる。
	float damageVignetteTimer_ = 0.0f;
	float damageVignetteDuration_ = 0.45f;
	float damageVignetteMaxIntensity_ = 0.95f;
	float damageVignetteCurrentIntensity_ = 0.0f;
	float damageVignetteRadius_ = 0.24f;
	float damageVignetteSoftness_ = 0.24f;
	bool enableBloom_ = true;
	// HDRでは1.0を通常白の基準とし、それを越える太陽や発光を主なBloom対象にします。
	float bloomThreshold_ = 1.0f;
	float bloomIntensity_ = 0.40f;
	float bloomRadius_ = 1.5f;
	float bloomKnee_ = 0.50f;
	float exposure_ = 1.0f;
	bool enableToneMapping_ = true;
	bool enableFog_ = false;
	float fogDensity_ = 0.72f;
	float fogHeight_ = 0.50f;
	float fogNoiseScale_ = 3.5f;
	float fogColor_[4] = {0.30f, 0.36f, 0.46f, 1.0f};
	float fogNoiseStrength_ = 0.42f;
	float fogParticleAmount_ = 0.08f;
	float fogSpeed_ = 0.035f;
	float fogDistanceStart_ = 5.0f;
	float fogDistanceEnd_ = 48.0f;
	bool fogHighQuality_ = false;
};
