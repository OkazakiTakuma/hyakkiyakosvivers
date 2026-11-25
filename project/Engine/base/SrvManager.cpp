#include "SrvManager.h"
#include <stdexcept>
/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
/// <param name="dxcommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
void SrvManager::Initialize(DirectXCommon* dxcommon) {
	this->dxCommon = dxcommon;
	descriptorHeap = dxCommon->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	descriptrSize = dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	useIndex = 0;
	allocatedCount_ = 0;
	freeIndices_.clear();
	allocatedIndices_.fill(false);
}

uint32_t SrvManager::Allocate() {
	uint32_t index = kMaxSRVCount;
	// Releaseビルドでも境界外番号を返さず、GPUヒープ破壊より前に明示的な失敗として停止する。
	if (!TryAllocate(index)) {
		throw std::runtime_error("SRV descriptor heap is full.");
	}
	return index;
}

bool SrvManager::TryAllocate(uint32_t& index) {
	if (!freeIndices_.empty()) {
		index = freeIndices_.back();
		freeIndices_.pop_back();
	} else if (useIndex < kMaxSRVCount) {
		index = useIndex++;
	} else {
		return false;
	}
	allocatedIndices_[index] = true;
	++allocatedCount_;
	return true;
}

void SrvManager::Release(uint32_t index) {
	if (index >= kMaxSRVCount || !allocatedIndices_[index]) {
		return;
	}
	allocatedIndices_[index] = false;
	--allocatedCount_;
	freeIndices_.push_back(index);
}

bool SrvManager::IsOverAllocated() const { return allocatedCount_ > kMaxSRVCount; }

/// <param name="index">対象要素のインデックスを指定します。</param>
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptrSize * index);
	return handleCPU;
}

/// <param name="index">対象要素のインデックスを指定します。</param>
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptrSize * index);
	return handleGPU;
}

/// <summary>
/// SRVforTexture2D を作成し、利用できる状態にします。
/// </summary>
/// <param name="srvindex">対象要素のインデックスを指定します。</param>
void SrvManager::CreateSRVforTexture2D(uint32_t srvindex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = mipLevels;
	dxCommon->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvindex));
}

/// <summary>
/// SRVforStructuredBuffer を作成し、利用できる状態にします。
/// </summary>
/// <param name="srvindex">対象要素のインデックスを指定します。</param>
void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvindex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    dxCommon->GetDevice()->CreateShaderResourceView(
        pResource,
        &srvDesc,
        GetCPUDescriptorHandle(srvindex)
    );
}

void SrvManager::PreDraw() {
	ID3D12DescriptorHeap* heaps[] = {descriptorHeap.Get()};
	dxCommon->GetCommandList()->SetDescriptorHeaps(_countof(heaps), heaps);
}

/// <param name="srvIndex">対象要素のインデックスを指定します。</param>
void SrvManager::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex) {
	dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void SrvManager::Finalize() {
	descriptorHeap.Reset();
	useIndex = 0;
	allocatedCount_ = 0;
	freeIndices_.clear();
	allocatedIndices_.fill(false);
}
