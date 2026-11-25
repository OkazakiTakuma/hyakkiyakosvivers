#pragma once
#include "DirectXCommon.h"
#include <array>
#include <vector>

class SrvManager {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	/// <param name="dxcommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
	void Initialize(DirectXCommon* dxcommon);
	static constexpr uint32_t kMaxSRVCount = 512;
	/// <summary>
	/// 共有インスタンスを取得します。
	/// </summary>
	static SrvManager* GetInstance() {
		static SrvManager instance;
		return &instance;
	}
	uint32_t Allocate();
	/// <summary>空きSRVを確保し、満杯の場合はfalseを返します。</summary>
	bool TryAllocate(uint32_t& index);
	/// <summary>GPUでの利用完了が保証されたSRVを再利用可能な状態へ戻します。</summary>
	void Release(uint32_t index);
	bool IsOverAllocated() const;
	uint32_t GetAllocatedCount() const { return allocatedCount_; }

	/// <param name="index">対象要素のインデックスを指定します。</param>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() { return descriptorHeap; }
	
	/// <param name="index">対象要素のインデックスを指定します。</param>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);
	/// <summary>
	/// SRVforTexture2D を作成し、利用できる状態にします。
	/// </summary>
	/// <param name="srvindex">対象要素のインデックスを指定します。</param>
	void CreateSRVforTexture2D(uint32_t srvindex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);
	/// <summary>
	/// SRVforStructuredBuffer を作成し、利用できる状態にします。
	/// </summary>
	/// <param name="srvindex">対象要素のインデックスを指定します。</param>
	void CreateSRVforStructuredBuffer(uint32_t srvindex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
	void PreDraw();
	/// <param name="srvIndex">対象要素のインデックスを指定します。</param>
	void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize();

private:
	DirectXCommon* dxCommon = nullptr;
	uint32_t descriptrSize = 0;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	uint32_t useIndex = 0;
	uint32_t allocatedCount_ = 0;
	std::vector<uint32_t> freeIndices_;
	std::array<bool, kMaxSRVCount> allocatedIndices_{};

};

