#include "TextureManager.h"
#include "particle/ParticleManager.h"
#include "StringUtility.h"
#include <algorithm>
#include <cstring>

using namespace StringUtility;

uint32_t TextureManager::kSRVIndexTop = 1;

TextureManager::RuntimeTextureHandle::RuntimeTextureHandle(RuntimeTextureHandle&& other) noexcept
	: owner_(other.owner_), key_(std::move(other.key_)) {
	other.owner_ = nullptr;
}

TextureManager::RuntimeTextureHandle& TextureManager::RuntimeTextureHandle::operator=(RuntimeTextureHandle&& other) noexcept {
	if (this != &other) {
		Reset();
		owner_ = other.owner_;
		key_ = std::move(other.key_);
		other.owner_ = nullptr;
	}
	return *this;
}

void TextureManager::RuntimeTextureHandle::Reset() {
	if (owner_) {
		owner_->ReleaseOwnedTexture(key_);
		owner_ = nullptr;
		key_.clear();
	}
}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
/// <param name="dxcommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
void TextureManager::Initialize(DirectXCommon* dxcommon) {
	dxCommon_ = dxcommon;

	assert(dxCommon_ != nullptr);
}

/// <summary>
/// 共有インスタンスを取得します。
/// </summary>
TextureManager* TextureManager::GetInstance() {
	static TextureManager instance;
	return &instance;
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void TextureManager::Finalize() {
	textureDatas.clear();
}

uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filepath) {
	assert(!SrvManager::GetInstance()->IsOverAllocated());
	assert(textureDatas.contains(filepath));
	return textureDatas.at(filepath).srvIndex;
}

bool TextureManager::LoadTextureImage(const std::string& filepath, DirectX::ScratchImage& mipImages) const {
	// DDSは圧縮形式を維持し、それ以外は描画用のsRGB画像として読み込む。
	DirectX::ScratchImage image;
	HRESULT hr = filepath.ends_with(".dds")
		? DirectX::LoadFromDDSFile(ConvertString(filepath).c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)
		: DirectX::LoadFromWICFile(ConvertString(filepath).c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	if (FAILED(hr)) {
		return false;
	}

	// 圧縮済み画像へのミップ生成は行わず、元データをそのまま利用する。
	if (DirectX::IsCompressed(image.GetMetadata().format)) {
		mipImages = std::move(image);
		return true;
	}

	// 非圧縮画像には縮小描画時の品質を保つためのミップマップを生成する。
	hr = DirectX::GenerateMipMaps(
		image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	return SUCCEEDED(hr);
}

bool TextureManager::AllocateSrv(TextureData& textureData) const {
	SrvManager* srvManager = SrvManager::GetInstance();
	// 同じインデックスからCPU用・GPU用の両ハンドルを取得して対応を保つ。
	if (!srvManager->TryAllocate(textureData.srvIndex)) {
		return false;
	}
	textureData.srvHandleCPU = srvManager->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager->GetGPUDescriptorHandle(textureData.srvIndex);
	return true;
}

void TextureManager::CreateSrv(const TextureData& textureData) const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// キューブマップと通常の2DテクスチャではSRVの次元設定が異なる。
	if (textureData.metadata.IsCubemap()) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT_MAX;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(textureData.metadata.mipLevels);
	}
	dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);
}

void TextureManager::UploadAndTransition(TextureData& textureData, const DirectX::ScratchImage& image) const {
	// COPY_DEST状態のGPUリソースへ、全ミップレベルの画像データを転送する。
	dxCommon_->UploadTextureData(textureData.resource, image);

	// 転送後のテクスチャをピクセルシェーダーから読み取れる状態に変更する。
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = textureData.resource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
}

/// <summary>
/// Texture を読み込み、内部データへ反映します。
/// </summary>
bool TextureManager::LoadTexture(const std::string& filepath) {
	// 同じファイルを複数回ロードしてSRVを浪費しないようにする。
	if (textureDatas.contains(filepath)) {
		return true;
	}
	DirectX::ScratchImage mipImages;
	const bool loaded = LoadTextureImage(filepath, mipImages);
	if (!loaded) {
		return false;
	}

	// CPU側の画像情報を基にGPUリソースと対応するSRVをまとめて構築する。
	TextureData& textureData = textureDatas[filepath];
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
	if (!AllocateSrv(textureData)) {
		textureDatas.erase(filepath);
		return false;
	}
	CreateSrv(textureData);
	UploadAndTransition(textureData, mipImages);
	return true;
}

void TextureManager::CreateTextureFromRGBA(const std::string& key, uint32_t width, uint32_t height, const std::vector<uint8_t>& pixels) {
	// RGBA8として必要なデータ量を満たさない入力はGPUへ送らない。
	if (!dxCommon_ || width == 0 || height == 0 || pixels.size() < static_cast<size_t>(width) * height * 4) {
		return;
	}
	DirectX::ScratchImage image;
	HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height, 1, 1);
	assert(SUCCEEDED(hr));
	const DirectX::Image* destination = image.GetImage(0, 0, 0);
	// 転送先のrowPitchを考慮し、入力ピクセルを1行ずつコピーする。
	for (uint32_t row = 0; row < height; ++row) {
		std::memcpy(destination->pixels + destination->rowPitch * row, pixels.data() + static_cast<size_t>(width) * row * 4, static_cast<size_t>(width) * 4);
	}

	auto textureIt = textureDatas.find(key);
	if (textureIt == textureDatas.end()) {
		TextureData& textureData = textureDatas[key];
		textureData.metadata = image.GetMetadata();
		textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
		if (!AllocateSrv(textureData)) {
			textureDatas.erase(key);
			return;
		}
		CreateSrv(textureData);
		UploadAndTransition(textureData, image);
		return;
	}

	// 動的文字列は同じSRVを維持してリソースだけを差し替え、値の種類だけSRVを消費しない。
	TextureData replacement = textureIt->second;
	replacement.metadata = image.GetMetadata();
	replacement.resource = dxCommon_->CreateTextureResource(replacement.metadata);
	CreateSrv(replacement);
	UploadAndTransition(replacement, image);
	textureIt->second = std::move(replacement);
}

TextureManager::RuntimeTextureHandle TextureManager::CreateOwnedTextureFromRGBA(
	const std::string& key, uint32_t width, uint32_t height, const std::vector<uint8_t>& pixels) {
	// 所有キーの重複は二重解放につながるため、新規生成時だけハンドルを発行する。
	if (textureDatas.contains(key)) {
		return {};
	}
	CreateTextureFromRGBA(key, width, height, pixels);
	return textureDatas.contains(key) ? RuntimeTextureHandle(this, key) : RuntimeTextureHandle{};
}

void TextureManager::ReleaseOwnedTexture(const std::string& key) {
	auto textureIt = textureDatas.find(key);
	if (textureIt == textureDatas.end()) {
		return;
	}
	// 描画中のSRVを即再利用しないようGPU完了を待ってから、リソースと番号をまとめて返却する。
	if (dxCommon_) {
		dxCommon_->FlushGPU();
	}
	SrvManager::GetInstance()->Release(textureIt->second.srvIndex);
	textureDatas.erase(textureIt);
}

bool TextureManager::ReloadTexture(const std::string& filepath) {
	auto textureIt = textureDatas.find(filepath);
	if (textureIt == textureDatas.end() || !dxCommon_) {
		return false;
	}

	DirectX::ScratchImage mipImages;
	if (!LoadTextureImage(filepath, mipImages)) {
		return false;
	}

	// 既存のSRVハンドルを引き継ぎ、参照元を変更せずリソースだけを差し替える。
	TextureData replacement = textureIt->second;
	replacement.metadata = mipImages.GetMetadata();
	replacement.resource = dxCommon_->CreateTextureResource(replacement.metadata);
	CreateSrv(replacement);
	UploadAndTransition(replacement, mipImages);

	textureIt->second = std::move(replacement);
	return true;
}

size_t TextureManager::ReloadAllTextures() {
	size_t reloadedCount = 0;
	// 実行時生成テクスチャを除いたスナップショットを使い、走査中の変更を避ける。
	const std::vector<std::string> filePaths = GetLoadedTextureNames();
	for (const std::string& filePath : filePaths) {
		if (ReloadTexture(filePath)) {
			++reloadedCount;
		}
	}
	return reloadedCount;
}

/// <summary>
/// 読み込み済みテクスチャのファイルパス一覧を取得します。
/// </summary>
/// <returns>読み込み済みテクスチャのファイルパス一覧を返します。</returns>
std::vector<std::string> TextureManager::GetLoadedTextureNames() const {
	std::vector<std::string> names;
	names.reserve(textureDatas.size());
	for (const auto& [filePath, textureData] : textureDatas) {
		// ファイルを持たない実行時生成テクスチャは再読み込み対象に含めない。
		if (filePath.starts_with("__runtime_")) {
			continue;
		}
		names.push_back(filePath);
	}
	std::sort(names.begin(), names.end());
	return names;
}
