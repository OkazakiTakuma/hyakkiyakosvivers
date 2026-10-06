#pragma once
#include "../camera/Camera.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "struct.h"
#include <assert.h>
#include <d3d12.h>
#include <wrl.h>

class Object3dCommon {
public:
	/// <summary>
	/// 共有インスタンスを取得します。
	/// </summary>
	static Object3dCommon* GetInstance();

	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	/// <param name="dxCommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
	void Initialize(DirectXCommon* dxCommon);
	/// <summary>
	/// DirectX共通コンテキストが有効であることを保証します。
	/// </summary>
	void EnsureInitialized(DirectXCommon* dxCommon);
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize();
	/// <summary>変更されたHLSLを反映するため、3Dオブジェクト用PSOを再生成します。</summary>
	void ReloadPipelineState() { CreatePipelineState(); }

	void SetDraw();
	void SetShadowDraw();
	/// <summary>ライト視点の深度を書き込むPSOを設定します。</summary>
	void SetShadowMapDraw();
	/// <summary>通常描画済みの面へ影だけを合成するPSOを設定します。</summary>
	void SetShadowReceiverDraw();
	/// <summary>環境マップと同じt1スロットへ、影合成中だけシャドウマップを設定します。</summary>
	void BindShadowMap();
	/// <summary>影用テクスチャを描画先へ切り替え、毎フレーム初期化します。</summary>
	void BeginShadowMapPass();
	/// <summary>影用テクスチャを後続シェーダーから読める状態へ戻します。</summary>
	void EndShadowMapPass();
	/// <summary>平行光源が光を送る方向を設定します。</summary>
	void SetShadowLightDirection(const Vector3& direction);
	const Matrix4x4& GetLightViewProjectionMatrix() const { return lightViewProjectionMatrix_; }

	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	void SetDefaultCamera(Camera* cmr) { defaultCamera = cmr; }
	Camera* GetDefaultCamera() { return defaultCamera; }

	~Object3dCommon() = default;
private:
	Object3dCommon() = default;

	Object3dCommon(const Object3dCommon&) = delete;
	Object3dCommon& operator=(const Object3dCommon&) = delete;

	/// <summary>
	/// RootSignature を作成し、利用できる状態にします。
	/// </summary>
	void CreateRootSignature();
	/// <summary>
	/// PipelineState を作成し、利用できる状態にします。
	/// </summary>
	void CreatePipelineState();
	/// <summary>シーンの床高に依存しない影情報を保存するテクスチャを作成します。</summary>
	void CreateShadowMapResource();
	/// <summary>現在のカメラを中心にライト用ビュー射影行列を更新します。</summary>
	void UpdateLightViewProjectionMatrix();

private:
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowMapPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowReceiverPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> shadowMapResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> shadowDepthResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shadowRtvHeap_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shadowDsvHeap_ = nullptr;
	uint32_t shadowSrvIndex_ = 0;
	bool shadowMapIsShaderResource_ = true;
	static constexpr uint32_t kShadowMapSize = 2048;
	Vector3 shadowLightDirection_ = {0.0f, -1.0f, 0.0f};
	Matrix4x4 lightViewProjectionMatrix_ = MakeIdentity4x4();

	Camera* defaultCamera = nullptr;
};
