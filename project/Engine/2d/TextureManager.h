#pragma once
#include "DirectXTex.h"
#include "d3dx12.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>
#include <utility>

/// <summary>
/// テクスチャの読み込み、GPUリソース、SRVとファイルパスの対応を一元管理します。
/// 同じテクスチャの重複読み込みを防ぎ、描画機能へ参照情報を提供します。
/// </summary>
class TextureManager {
public:
	/// <summary>UIインスタンスと同じ寿命で実行時テクスチャを解放するムーブ専用ハンドルです。</summary>
	class RuntimeTextureHandle {
	public:
		RuntimeTextureHandle() = default;
		~RuntimeTextureHandle() { Reset(); }
		RuntimeTextureHandle(const RuntimeTextureHandle&) = delete;
		RuntimeTextureHandle& operator=(const RuntimeTextureHandle&) = delete;
		RuntimeTextureHandle(RuntimeTextureHandle&& other) noexcept;
		RuntimeTextureHandle& operator=(RuntimeTextureHandle&& other) noexcept;
		void Reset();
		bool IsValid() const { return owner_ != nullptr; }
		const std::string& GetKey() const { return key_; }

	private:
		friend class TextureManager;
		RuntimeTextureHandle(TextureManager* owner, std::string key)
			: owner_(owner), key_(std::move(key)) {}
		TextureManager* owner_ = nullptr;
		std::string key_;
	};

	/// <summary>
	/// 共有インスタンスを取得します。
	/// </summary>
	static TextureManager* GetInstance();
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	/// <param name="dxcommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
	void Initialize(DirectXCommon* dxcommon);
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize();
	/// <summary>
	/// Texture を読み込み、内部データへ反映します。
	/// </summary>
	/// <returns>既に読み込み済み、または新規読み込みに成功した場合は true を返します。</returns>
	bool LoadTexture(const std::string& filepath);
	/// <summary>
	/// RGBA8形式のピクセル列から実行時テクスチャを生成します。
	/// </summary>
	/// <param name="key">生成したテクスチャを識別する一意なキーです。</param>
	/// <param name="width">テクスチャの幅です。</param>
	/// <param name="height">テクスチャの高さです。</param>
	/// <param name="pixels">1ピクセル4バイトのRGBAデータです。</param>
	void CreateTextureFromRGBA(const std::string& key, uint32_t width, uint32_t height, const std::vector<uint8_t>& pixels);
	/// <summary>呼び出し元の寿命に追従する実行時テクスチャを生成します。</summary>
	RuntimeTextureHandle CreateOwnedTextureFromRGBA(
		const std::string& key, uint32_t width, uint32_t height, const std::vector<uint8_t>& pixels);
	/// <summary>
	/// 登録済みのSRVを維持したまま、ファイルからGPUリソースを再作成します。
	/// </summary>
	bool ReloadTexture(const std::string& filepath);
	/// <summary>
	/// ファイルから読み込んだ全テクスチャを再読み込みします。
	/// </summary>
	/// <returns>再読み込みに成功したテクスチャ数を返します。</returns>
	size_t ReloadAllTextures();
	/// <summary>SRV領域でテクスチャに使用する先頭インデックスです。</summary>
	static uint32_t kSRVIndexTop;
	uint32_t GetTextureIndexByFilePath(const std::string& filepath);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU(const std::string& filePath) {
		assert(!SrvManager::GetInstance()->IsOverAllocated());
		assert(textureDatas.contains(filePath));
		return textureDatas.at(filePath).srvHandleGPU;
	}
	const DirectX::TexMetadata& GetTextureMetadata(const std::string& filePath) {
		assert(!SrvManager::GetInstance()->IsOverAllocated());
		assert(textureDatas.contains(filePath));
		return textureDatas.at(filePath).metadata;
	}
	/// <summary>
	/// 読み込み済みテクスチャのファイルパス一覧を取得します。
	/// </summary>
	/// <returns>読み込み済みテクスチャのファイルパス一覧を返します。</returns>
	std::vector<std::string> GetLoadedTextureNames() const;

private:
	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	/// <summary>1枚のテクスチャに対応するCPU・GPU側の管理情報です。</summary>
	struct TextureData {
		/// <summary>画像サイズ、フォーマット、ミップ数などの情報です。</summary>
		DirectX::TexMetadata metadata;
		/// <summary>GPU上に確保されたテクスチャリソースです。</summary>
		Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
		/// <summary>SRVディスクリプタヒープ内の割り当て位置です。</summary>
		uint32_t srvIndex=0;
		/// <summary>SRV作成時にCPUから参照するディスクリプタハンドルです。</summary>
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		/// <summary>描画時にシェーダーから参照するディスクリプタハンドルです。</summary>
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	/// <summary>画像を読み込み、必要に応じてミップマップを生成します。</summary>
	bool LoadTextureImage(const std::string& filepath, DirectX::ScratchImage& mipImages) const;
	/// <summary>テクスチャ用SRVを1つ確保し、CPU/GPUハンドルを保存します。</summary>
	bool AllocateSrv(TextureData& textureData) const;
	/// <summary>所有ハンドルから返却された実行時テクスチャとSRVを安全に解放します。</summary>
	void ReleaseOwnedTexture(const std::string& key);
	/// <summary>メタデータに合わせたSRVを、確保済みディスクリプタへ作成します。</summary>
	void CreateSrv(const TextureData& textureData) const;
	/// <summary>画像をGPUへ転送し、描画可能なリソース状態へ遷移させます。</summary>
	void UploadAndTransition(TextureData& textureData, const DirectX::ScratchImage& image) const;

	/// <summary>識別キーと、読み込み済みテクスチャ情報の対応表です。</summary>
	std::unordered_map<std::string, TextureData> textureDatas;
	/// <summary>GPUリソースの作成とコマンド発行に使用する共通処理です。</summary>
	DirectXCommon* dxCommon_ = nullptr;
};
