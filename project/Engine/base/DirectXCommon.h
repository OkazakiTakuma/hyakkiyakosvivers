#pragma once
#include <d3d12.h>
#include "DirectXTex.h"
#include "Matrix.h"
#include "ScreenCapture.h"
#include "Vector.h"
#include "Logger.h"
#include "StringUtility.h"
#include "WinApp.h"
#include <array>
#include <cassert>
#include <chrono>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <format>
#include <wrl.h>
#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

/// <summary>
/// Direct3D 12デバイス、スワップチェーン、コマンド実行、描画先リソースのライフサイクルを管理します。
/// エンジン内の描画機能へ共通のDirectX基盤を提供します。
/// </summary>
class DirectXCommon {
public:
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 world;
	};

	// RTV、DSV、またはシェーダーから参照可能なディスクリプターヒープを作成します。
	/// <summary>
	/// DescriptorHeap を作成し、利用できる状態にします。
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible);

	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	/// <param name="winApp">ウィンドウ管理オブジェクトを指定します。</param>
	void Initialize(WinApp* winApp);
	/// <summary>
	/// 描画に使用するバックバッファの完了を待ち、そのフレーム用のコマンド記録を開始します。
	/// </summary>
	void BeginFrame();
	void PreDraw();
	/// <summary>
	/// 録画用フレームを確定し、その後に画面専用オーバーレイを描ける状態へ戻します。
	/// </summary>
	void CaptureFrameBeforeOverlay();
	void PostDraw();
	/// <summary>次の完成フレームを PNG として保存するよう要求します。</summary>
	void RequestScreenshot() { screenCapture_.RequestScreenshot(); }
	/// <summary>現在の描画サイズで画面録画の開始と停止を切り替えます。</summary>
	void ToggleScreenRecording() { screenCapture_.ToggleRecording(static_cast<uint32_t>(renderWidth_), static_cast<uint32_t>(renderHeight_)); }
	/// <summary>画面録画中かどうかを返します。</summary>
	bool IsScreenRecording() const { return screenCapture_.IsRecording(); }
	void ResizeIfNeeded();
	int32_t GetRenderWidth() const { return renderWidth_; }
	int32_t GetRenderHeight() const { return renderHeight_; }
	/// <summary>
	/// 指定された HLSL シェーダーをコンパイルします。
	/// </summary>
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filepath, const wchar_t* profile);
	/// <summary>
	/// アプリケーションを停止させずにHLSLをコンパイルします。
	/// ホットリロード時の編集途中エラーはerrorMessageへ返し、失敗時はnullptrを返します。
	/// </summary>
	Microsoft::WRL::ComPtr<IDxcBlob> TryCompileShader(const std::wstring& filepath, const wchar_t* profile, std::string& errorMessage);
	/// <summary>実行中のGPU処理が完了するまで待機し、リソース交換を安全にします。</summary>
	void FlushGPU() { WaitForGPU(); }
	/// <summary>
	/// BufferResource を作成し、利用できる状態にします。
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
	/// <summary>
	/// TextureResource を作成し、利用できる状態にします。
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metaData);

	Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() { return device.Get(); }
	IDxcCompiler3* GetDxcCompiler() { return dxcCompiler.Get(); }
	IDxcUtils* GetDxcUtils() { return dxcUtils.Get(); }
	IDxcIncludeHandler* GetIncludeHandler() { return includeHandler.Get(); }
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() { return commandList.Get(); }
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetCommandAllocator() {
		return frameResources_[currentFrameIndex_].commandAllocator.Get();
	}
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() { return commandQueue.Get(); }
	size_t GetSwapChainResourceCount() const { return _countof(swapChainResources); }


	void UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

	/// <param name="index">対象要素のインデックスを指定します。</param>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <param name="index">対象要素のインデックスを指定します。</param>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRTVDescriptorHeap() { return rtvDescriptorHeap; }
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDSVDescriptorHeap() { return dsvDescriptorHeap; }
	void Release();

	/// <summary>
	/// RenderTextureResource を作成し、利用できる状態にします。
	/// </summary>
	/// <param name="width">幅を指定します。</param>
	/// <param name="height">高さを指定します。</param>
	/// <param name="color">色を指定します。</param>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, int32_t width, int32_t height, DXGI_FORMAT format, const Vector4 color);

private:
	/// <summary>
	/// Device を作成し、利用できる状態にします。
	/// </summary>
	void CreateDevice();

	/// <summary>
	/// Command を作成し、利用できる状態にします。
	/// </summary>
	void CreateCommand();

	/// <summary>
	/// SwapChain を作成し、利用できる状態にします。
	/// </summary>
	void CreateSwapChain();

	/// <summary>
	/// DepthBuffer を作成し、利用できる状態にします。
	/// </summary>
	void CreateDepthBuffer();

	/// <summary>
	/// DescriptorHeap を作成し、利用できる状態にします。
	/// </summary>
	void CreateDescriptorHeap();


	/// <summary>
	/// RenderTargetView を作成し、利用できる状態にします。
	/// </summary>
	void CreateRenderTargetView();

	/// <summary>
	/// DepthStencilView を作成し、利用できる状態にします。
	/// </summary>
	void CreateDepthStencilView();

	/// <summary>
	/// Fence を作成し、利用できる状態にします。
	/// </summary>
	void CreateFence();

	/// <summary>
	/// ViewportRect を作成し、利用できる状態にします。
	/// </summary>
	void CreateViewportRect();

	/// <summary>
	/// ScissorRect を作成し、利用できる状態にします。
	/// </summary>
	void CreateScissorRect();
	void WaitForGPU();
	void WaitForFrame(uint32_t frameIndex);
	void WaitForFenceValue(uint64_t waitValue);
	void LimitFrameRate();
	void ResizeBackBuffers(int32_t width, int32_t height);

	/// <summary>
	/// DXCompiler を作成し、利用できる状態にします。
	/// </summary>
	void CreateDXCompiler();

	/// <summary>
	/// XAudio2 を作成し、利用できる状態にします。
	/// </summary>
	void CreateXAudio2();

	WinApp* winApp = nullptr;
	static constexpr uint32_t kFrameCount = 2;
	struct FrameResource {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		uint64_t fenceValue = 0;
	};

	Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
	std::array<FrameResource, kFrameCount> frameResources_{};
	uint32_t currentFrameIndex_ = 0;
	bool frameStarted_ = false;
	// オーバーレイ前に録画済みなら PostDraw で同一フレームを二重保存しない。
	bool captureProcessedThisFrame_ = false;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResorceModel;
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResorce;
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
	uint32_t descroptorSizeRTV;
	uint32_t descroptorSizeDSV;
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[kFrameCount] = {nullptr};
	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	uint64_t lastSubmittedFenceValue_ = 0;
	std::chrono::steady_clock::time_point nextFrameTime_{};
	HANDLE fenceEvent = nullptr;
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};
	int32_t renderWidth_ = WinApp::kClientWidth;
	int32_t renderHeight_ = WinApp::kClientHeight;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils = nullptr;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[kFrameCount];

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masteringVoice = nullptr;
	// バックバッファの読み戻しと PNG／MP4 のファイル出力を担当する。
	ScreenCapture screenCapture_;
};
