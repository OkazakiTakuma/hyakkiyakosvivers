#include "PostEffect.h"
#include "PipelineStateUtility.h"
#include "Input.h"
#include "SrvManager.h"
#include "WinApp.h"
#include "imGuiManager.h"
#include "GameTime.h"
#include "TextureManager.h"
#include "object/Object3dCommon.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

/// <summary>
/// 共有インスタンスを取得します。
/// </summary>
PostEffect* PostEffect::GetInstance() {
	static PostEffect instance;
	return &instance;
}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
/// <param name="dxCommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
void PostEffect::Initialize(DirectXCommon* dxCommon) {
	assert(dxCommon);
	dxCommon_ = dxCommon;
	renderWidth_ = dxCommon_->GetRenderWidth();
	renderHeight_ = dxCommon_->GetRenderHeight();

	CreateTextureResource();
	CreateRtv();
	CreateDsv();
	CreateSrv();
	CreateBloomResources();
	CreateDissolveMask();
	CreateFogNoiseTexture();
	CreateFogResources();
	CreateRootSignature();
	CreatePipelineState();
	CreateBloomPipelineState();
	CreateFogPipelineState();

	CreateColorBuffer();
}

/// <summary>
/// TextureResource を作成し、利用できる状態にします。
/// </summary>
void PostEffect::CreateTextureResource() {
	D3D12_RESOURCE_DESC textureDesc{};
	textureDesc.Width = renderWidth_;
	textureDesc.Height = renderHeight_;
	textureDesc.MipLevels = 1;
	textureDesc.DepthOrArraySize = 1;
	// 1.0を超える太陽輝度をBloomまで保持するため、3DシーンはFP16 HDRへ描画します。
	textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	clearValue.Color[0] = 0.1f;
	clearValue.Color[1] = 0.25f;
	clearValue.Color[2] = 0.5f;
	clearValue.Color[3] = 1.0f;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&textureResource_));
	assert(SUCCEEDED(hr));
}

/// <summary>
/// Rtv を作成し、利用できる状態にします。
/// </summary>
void PostEffect::CreateRtv() {
	rtvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	dxCommon_->GetDevice()->CreateRenderTargetView(textureResource_.Get(), &rtvDesc, rtvHeap_->GetCPUDescriptorHandleForHeapStart());
}

/// <summary>
/// Dsv を作成し、利用できる状態にします。
/// </summary>
void PostEffect::CreateDsv() {
	D3D12_RESOURCE_DESC depthDesc{};
	depthDesc.Width = renderWidth_;
	depthDesc.Height = renderHeight_;
	depthDesc.MipLevels = 1;
	depthDesc.DepthOrArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&depthBuffer_));
	assert(SUCCEEDED(hr));

	dsvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	dxCommon_->GetDevice()->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}

/// <summary>
/// Srv を作成し、利用できる状態にします。
/// </summary>
void PostEffect::CreateSrv() {
	SrvManager* srvManager = SrvManager::GetInstance();
	if (srvIndex_ == UINT32_MAX) {
		srvIndex_ = srvManager->Allocate();
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	dxCommon_->GetDevice()->CreateShaderResourceView(textureResource_.Get(), &srvDesc, srvManager->GetCPUDescriptorHandle(srvIndex_));

	srvHandleGPU_ = srvManager->GetGPUDescriptorHandle(srvIndex_);

	if (depthSrvIndex_ == UINT32_MAX) {
		depthSrvIndex_ = srvManager->Allocate();
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
	depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthSrvDesc.Texture2D.MipLevels = 1;
	dxCommon_->GetDevice()->CreateShaderResourceView(depthBuffer_.Get(), &depthSrvDesc, srvManager->GetCPUDescriptorHandle(depthSrvIndex_));
	depthSrvHandleGPU_ = srvManager->GetGPUDescriptorHandle(depthSrvIndex_);
}

void PostEffect::CreateBloomResources() {
	bloomWidth_ = (std::max)(renderWidth_ / 2, 1);
	bloomHeight_ = (std::max)(renderHeight_ / 2, 1);
	bloomRtvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	const UINT rtvIncrement = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	SrvManager* srvManager = SrvManager::GetInstance();

	for (uint32_t index = 0; index < bloomResources_.size(); ++index) {
		D3D12_RESOURCE_DESC description{};
		description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		description.Width = static_cast<uint64_t>(bloomWidth_);
		description.Height = static_cast<uint32_t>(bloomHeight_);
		description.DepthOrArraySize = 1;
		description.MipLevels = 1;
		description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		description.SampleDesc.Count = 1;
		description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		const HRESULT result = dxCommon_->GetDevice()->CreateCommittedResource(
		    &heapProperties, D3D12_HEAP_FLAG_NONE, &description,
		    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
		    IID_PPV_ARGS(&bloomResources_[index]));
		assert(SUCCEEDED(result));

		D3D12_RENDER_TARGET_VIEW_DESC rtvDescription{};
		rtvDescription.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		rtvDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = bloomRtvHeap_->GetCPUDescriptorHandleForHeapStart();
		rtv.ptr += static_cast<SIZE_T>(index) * rtvIncrement;
		dxCommon_->GetDevice()->CreateRenderTargetView(bloomResources_[index].Get(), &rtvDescription, rtv);

		if (bloomSrvIndices_[index] == UINT32_MAX) {
			bloomSrvIndices_[index] = srvManager->Allocate();
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription{};
		srvDescription.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		srvDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDescription.Texture2D.MipLevels = 1;
		dxCommon_->GetDevice()->CreateShaderResourceView(
		    bloomResources_[index].Get(), &srvDescription,
		    srvManager->GetCPUDescriptorHandle(bloomSrvIndices_[index]));
		bloomSrvHandlesGPU_[index] = srvManager->GetGPUDescriptorHandle(bloomSrvIndices_[index]);
	}
}

void PostEffect::CreateDissolveMask() {
	if (dissolveMaskResource_) {
		return;
	}

	constexpr uint32_t maskSize = 256;
	D3D12_RESOURCE_DESC textureDesc{};
	textureDesc.Width = maskSize;
	textureDesc.Height = maskSize;
	textureDesc.MipLevels = 1;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
	    &heapProps,
	    D3D12_HEAP_FLAG_NONE,
	    &textureDesc,
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	    nullptr,
	    IID_PPV_ARGS(&dissolveMaskResource_)
	);
	assert(SUCCEEDED(hr));

	std::vector<uint8_t> mask(maskSize * maskSize);
	for (uint32_t y = 0; y < maskSize; ++y) {
		for (uint32_t x = 0; x < maskSize; ++x) {
			const float wave = std::sin(static_cast<float>(x) * 0.13f) + std::sin(static_cast<float>(y) * 0.17f);
			const uint32_t hash = (x * 1973u) ^ (y * 9277u) ^ ((x + y) * 26699u);
			const float noise = static_cast<float>((hash ^ (hash >> 13)) & 0xffu) / 255.0f;
			const float value = std::clamp((wave * 0.18f) + (noise * 0.82f), 0.0f, 1.0f);
			mask[(y * maskSize) + x] = static_cast<uint8_t>(value * 255.0f);
		}
	}

	hr = dissolveMaskResource_->WriteToSubresource(0, nullptr, mask.data(), maskSize, maskSize * maskSize);
	assert(SUCCEEDED(hr));

	SrvManager* srvManager = SrvManager::GetInstance();
	if (dissolveMaskSrvIndex_ == UINT32_MAX) {
		dissolveMaskSrvIndex_ = srvManager->Allocate();
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	dxCommon_->GetDevice()->CreateShaderResourceView(dissolveMaskResource_.Get(), &srvDesc, srvManager->GetCPUDescriptorHandle(dissolveMaskSrvIndex_));
	dissolveMaskSrvHandleGPU_ = srvManager->GetGPUDescriptorHandle(dissolveMaskSrvIndex_);
}

void PostEffect::CreateFogNoiseTexture() {
	static constexpr const char* kFogNoiseTexturePath = "Resources/Texture/fog_noise.png";
	TextureManager* textureManager = TextureManager::GetInstance();
	const bool loaded = textureManager->LoadTexture(kFogNoiseTexturePath);
	assert(loaded);
	if (loaded) {
		fogNoiseSrvHandleGPU_ = textureManager->GetSRVHandleGPU(kFogNoiseTexturePath);
	}
}

void PostEffect::CreateFogResources() {
	fogWidth_ = (std::max)(renderWidth_ / 2, 1);
	fogHeightPixels_ = (std::max)(renderHeight_ / 2, 1);
	fogRtvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);

	D3D12_RESOURCE_DESC description{};
	description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	description.Width = static_cast<uint64_t>(fogWidth_);
	description.Height = static_cast<uint32_t>(fogHeightPixels_);
	description.DepthOrArraySize = 1;
	description.MipLevels = 1;
	description.Format = DXGI_FORMAT_R8_UNORM;
	description.SampleDesc.Count = 1;
	description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_R8_UNORM;
	const HRESULT result = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &description,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
		IID_PPV_ARGS(&fogResource_));
	assert(SUCCEEDED(result));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDescription{};
	rtvDescription.Format = DXGI_FORMAT_R8_UNORM;
	rtvDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	dxCommon_->GetDevice()->CreateRenderTargetView(
		fogResource_.Get(), &rtvDescription, fogRtvHeap_->GetCPUDescriptorHandleForHeapStart());

	SrvManager* srvManager = SrvManager::GetInstance();
	if (fogSrvIndex_ == UINT32_MAX) {
		fogSrvIndex_ = srvManager->Allocate();
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription{};
	srvDescription.Format = DXGI_FORMAT_R8_UNORM;
	srvDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescription.Texture2D.MipLevels = 1;
	dxCommon_->GetDevice()->CreateShaderResourceView(
		fogResource_.Get(), &srvDescription, srvManager->GetCPUDescriptorHandle(fogSrvIndex_));
	fogSrvHandleGPU_ = srvManager->GetGPUDescriptorHandle(fogSrvIndex_);
}

/// <summary>
/// RootSignature を作成し、利用できる状態にします。
/// </summary>
void PostEffect::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE descriptorRanges[5] = {};
	for (uint32_t index = 0; index < _countof(descriptorRanges); ++index) {
		descriptorRanges[index].BaseShaderRegister = index;
		descriptorRanges[index].NumDescriptors = 1;
		descriptorRanges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRanges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	D3D12_ROOT_PARAMETER rootParameters[6] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRanges[2];
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

	// 半解像度でぼかしたBloom結果をt3から最終合成します。
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].DescriptorTable.pDescriptorRanges = &descriptorRanges[3];
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;

	// タイル可能な霧ノイズをt4から参照します。
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].DescriptorTable.pDescriptorRanges = &descriptorRanges[4];
	rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionSignature{};
	descriptionSignature.pParameters = rootParameters;
	descriptionSignature.NumParameters = _countof(rootParameters);
	descriptionSignature.pStaticSamplers = staticSamplers;
	descriptionSignature.NumStaticSamplers = _countof(staticSamplers);
	descriptionSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	rootSignature_ = PipelineStateUtility::CreateRootSignature(dxCommon_->GetDevice().Get(), descriptionSignature);
}

/// <summary>
/// PipelineState を作成し、利用できる状態にします。
/// </summary>
void PostEffect::CreatePipelineState() {
	auto vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/CopyImage.VS.hlsl", L"vs_6_0");
	auto pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/FullScreen.PS.hlsl", L"ps_6_0");
	assert(vertexShaderBlob && pixelShaderBlob);

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = nullptr;
	inputLayoutDesc.NumElements = 0;

	const D3D12_BLEND_DESC blendDesc = PipelineStateUtility::MakeBlendDesc();
	const D3D12_RASTERIZER_DESC rasterizerDesc = PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_NONE);
	const D3D12_DEPTH_STENCIL_DESC depthStencilDesc =
	    PipelineStateUtility::MakeDepthStencilDesc(FALSE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_NEVER);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void PostEffect::CreateBloomPipelineState() {
	D3D12_DESCRIPTOR_RANGE sourceRange{};
	sourceRange.BaseShaderRegister = 0;
	sourceRange.NumDescriptors = 1;
	sourceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	sourceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER parameters[2]{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	parameters[0].DescriptorTable.pDescriptorRanges = &sourceRange;
	parameters[0].DescriptorTable.NumDescriptorRanges = 1;
	// パスごとの値はRoot Constantsとしてコマンドへコピーし、同じCBを上書きする競合を避けます。
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	parameters[1].Constants.ShaderRegister = 0;
	parameters[1].Constants.Num32BitValues = 8;

	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootDescription{};
	rootDescription.pParameters = parameters;
	rootDescription.NumParameters = _countof(parameters);
	rootDescription.pStaticSamplers = &sampler;
	rootDescription.NumStaticSamplers = 1;
	bloomRootSignature_ = PipelineStateUtility::CreateRootSignature(dxCommon_->GetDevice().Get(), rootDescription);

	auto vertexShader = dxCommon_->CompileShader(L"Resources/Shader/CopyImage.VS.hlsl", L"vs_6_0");
	auto pixelShader = dxCommon_->CompileShader(L"Resources/Shader/Bloom.PS.hlsl", L"ps_6_0");
	assert(vertexShader && pixelShader);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription{};
	pipelineDescription.pRootSignature = bloomRootSignature_.Get();
	pipelineDescription.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
	pipelineDescription.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
	pipelineDescription.BlendState = PipelineStateUtility::MakeBlendDesc();
	pipelineDescription.RasterizerState = PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_NONE);
	pipelineDescription.DepthStencilState = PipelineStateUtility::MakeDepthStencilDesc(
	    FALSE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_NEVER);
	pipelineDescription.NumRenderTargets = 1;
	pipelineDescription.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	pipelineDescription.SampleDesc.Count = 1;
	pipelineDescription.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	const HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
	    &pipelineDescription, IID_PPV_ARGS(&bloomPipelineState_));
	assert(SUCCEEDED(result));
}

void PostEffect::CreateFogPipelineState() {
	D3D12_DESCRIPTOR_RANGE ranges[2]{};
	for (uint32_t index = 0; index < _countof(ranges); ++index) {
		ranges[index].BaseShaderRegister = index;
		ranges[index].NumDescriptors = 1;
		ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}
	D3D12_ROOT_PARAMETER parameters[3]{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	parameters[0].DescriptorTable.pDescriptorRanges = &ranges[0];
	parameters[0].DescriptorTable.NumDescriptorRanges = 1;
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	parameters[1].DescriptorTable.pDescriptorRanges = &ranges[1];
	parameters[1].DescriptorTable.NumDescriptorRanges = 1;
	parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	parameters[2].Descriptor.ShaderRegister = 0;

	D3D12_STATIC_SAMPLER_DESC samplers[2]{};
	for (auto& sampler : samplers) {
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}
	samplers[0].ShaderRegister = 0;
	samplers[1].ShaderRegister = 1;
	samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	D3D12_ROOT_SIGNATURE_DESC rootDescription{};
	rootDescription.pParameters = parameters;
	rootDescription.NumParameters = _countof(parameters);
	rootDescription.pStaticSamplers = samplers;
	rootDescription.NumStaticSamplers = _countof(samplers);
	fogRootSignature_ = PipelineStateUtility::CreateRootSignature(dxCommon_->GetDevice().Get(), rootDescription);

	auto vertexShader = dxCommon_->CompileShader(L"Resources/Shader/CopyImage.VS.hlsl", L"vs_6_0");
	auto pixelShader = dxCommon_->CompileShader(L"Resources/Shader/FogMask.PS.hlsl", L"ps_6_0");
	assert(vertexShader && pixelShader);
	D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
	description.pRootSignature = fogRootSignature_.Get();
	description.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
	description.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
	description.BlendState = PipelineStateUtility::MakeBlendDesc();
	description.RasterizerState = PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_NONE);
	description.DepthStencilState = PipelineStateUtility::MakeDepthStencilDesc(
		FALSE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_NEVER);
	description.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	description.NumRenderTargets = 1;
	description.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
	description.SampleDesc.Count = 1;
	const HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&description, IID_PPV_ARGS(&fogPipelineState_));
	assert(SUCCEEDED(result));
}

/// <summary>
/// ColorBuffer を作成し、利用できる状態にします。
/// </summary>
void PostEffect::CreateColorBuffer() {
	uint32_t sizeIB = (sizeof(ColorData) + 0xff) & ~0xff;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeIB;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&colorBuffer_));

	if (FAILED(hr)) {
		assert(false && "Failed to create ColorBuffer");
		return;
	}

	D3D12_RANGE readRange{};
	readRange.Begin = 0;
	readRange.End = 0;

	hr = colorBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&colorData_));

	if (FAILED(hr) || colorData_ == nullptr) {
		assert(false && "Failed to map ColorBuffer");
		return;
	}

	ApplySettingsToBuffer();
}

/// <summary>
/// SettingsToBuffer を現在の状態へ反映します。
/// </summary>
void PostEffect::ApplySettingsToBuffer() {
	if (colorData_ == nullptr) {
		return;
	}

	colorData_->r = tintColor_[0];
	colorData_->g = tintColor_[1];
	colorData_->b = tintColor_[2];
	colorData_->a = tintColor_[3];

	// HDRから画面へ変換する処理は常時必要ですが、Master OFF時は演出系だけを停止します。
	colorData_->enableGrayscale = isActive_ && enableGrayscale_ ? 1 : 0;
	colorData_->enableVignetting = isActive_ && enableVignetting_ ? 1 : 0;
	colorData_->enableSmoothing = isActive_ && enableSmoothing_ ? 1 : 0;
	colorData_->enableGaussianFilter = isActive_ && enableGaussianFilter_ ? 1 : 0;
	colorData_->enableRadialBlur = isActive_ && enableRadialBlur_ ? 1 : 0;
	colorData_->enableRandom = isActive_ && enableRandom_ ? 1 : 0;
	colorData_->radialBlurSamples = radialBlurSamples_;
	colorData_->enableOutline = isActive_ && enableOutline_ ? 1 : 0;
	colorData_->enableDissolve = isActive_ && enableDissolve_ ? 1 : 0;
	colorData_->vignetteIntensity = vignetteIntensity_;
	colorData_->vignetteRadius = vignetteRadius_;
	colorData_->vignetteSoftness = vignetteSoftness_;
	colorData_->radialBlurStrength = radialBlurStrength_;
	colorData_->randomStrength = randomStrength_;
	colorData_->outlineStrength = outlineStrength_;
	colorData_->outlineThreshold = outlineThreshold_;
	colorData_->outlineThickness = outlineThickness_;
	colorData_->dissolveThreshold = dissolveThreshold_;
	colorData_->dissolveEdgeWidth = dissolveEdgeWidth_;
	colorData_->time = time_;
	colorData_->texelSize[0] = 1.0f / static_cast<float>(renderWidth_);
	colorData_->texelSize[1] = 1.0f / static_cast<float>(renderHeight_);

	// デバイス深度は非線形なので、シェーダーへ現在のカメラの Near / Far を渡して線形化します。
	// カメラ未設定時もアウトライン処理を継続できるよう、安全な既定値を使用します。
	const Camera* camera = Object3dCommon::GetInstance()->GetDefaultCamera();
	const float nearClip = camera ? camera->GetNearClip() : 0.1f;
	const float farClip = camera ? camera->GetFarClip() : 1000.0f;
	colorData_->cameraNearFar[0] = (std::max)(nearClip, 0.0001f);
	colorData_->cameraNearFar[1] = (std::max)(farClip, colorData_->cameraNearFar[0] + 0.0001f);
	colorData_->outlineColor[0] = outlineColor_[0];
	colorData_->outlineColor[1] = outlineColor_[1];
	colorData_->outlineColor[2] = outlineColor_[2];
	colorData_->outlineColor[3] = outlineColor_[3];
	colorData_->dissolveEdgeColor[0] = dissolveEdgeColor_[0];
	colorData_->dissolveEdgeColor[1] = dissolveEdgeColor_[1];
	colorData_->dissolveEdgeColor[2] = dissolveEdgeColor_[2];
	colorData_->dissolveEdgeColor[3] = dissolveEdgeColor_[3];
	colorData_->damageVignetteIntensity = damageVignetteCurrentIntensity_;
	colorData_->damageVignetteRadius = damageVignetteRadius_;
	colorData_->damageVignetteSoftness = damageVignetteSoftness_;
	colorData_->paddingDamageVignette = 0.0f;
	colorData_->enableBloom = isActive_ && enableBloom_ ? 1 : 0;
	colorData_->bloomThreshold = bloomThreshold_;
	colorData_->bloomIntensity = bloomIntensity_;
	colorData_->bloomRadius = bloomRadius_;
	colorData_->exposure = exposure_;
	colorData_->enableToneMapping = enableToneMapping_ ? 1 : 0;
	colorData_->bloomKnee = bloomKnee_;
	colorData_->paddingHdr = 0.0f;
	colorData_->enableFog = isActive_ && enableFog_ ? 1 : 0;
	colorData_->fogDensity = fogDensity_;
	colorData_->fogHeight = fogHeight_;
	colorData_->fogNoiseScale = fogNoiseScale_;
	for (size_t index = 0; index < 4; ++index) {
		colorData_->fogColor[index] = fogColor_[index];
	}
	colorData_->fogNoiseStrength = fogNoiseStrength_;
	colorData_->fogSpeed = fogSpeed_;
	colorData_->fogDistanceStart = fogDistanceStart_;
	colorData_->fogDistanceEnd = fogDistanceEnd_;
	colorData_->fogParticleAmount = fogParticleAmount_;
	colorData_->fogHighQuality = fogHighQuality_ ? 1 : 0;
	colorData_->paddingFog[0] = 0.0f;
	colorData_->paddingFog[1] = 0.0f;
}

void PostEffect::SetFogSettings(bool enabled, const Vector4& color, float density, float height,
	float noiseScale, float noiseStrength, float particleAmount, float speed,
	float distanceStart, float distanceEnd, bool highQuality) {
	enableFog_ = enabled;
	fogColor_[0] = color.x;
	fogColor_[1] = color.y;
	fogColor_[2] = color.z;
	fogColor_[3] = color.w;
	fogDensity_ = std::clamp(density, 0.0f, 2.0f);
	fogHeight_ = std::clamp(height, 0.0f, 1.0f);
	fogNoiseScale_ = (std::max)(noiseScale, 0.01f);
	fogNoiseStrength_ = std::clamp(noiseStrength, 0.0f, 1.0f);
	fogParticleAmount_ = std::clamp(particleAmount, 0.0f, 1.0f);
	fogSpeed_ = speed;
	fogDistanceStart_ = (std::max)(distanceStart, 0.0f);
	fogDistanceEnd_ = (std::max)(distanceEnd, fogDistanceStart_ + 0.01f);
	fogHighQuality_ = highQuality;
	ApplySettingsToBuffer();
}

void PostEffect::ResizeIfNeeded() {
	if (!dxCommon_) {
		return;
	}

	const int32_t width = dxCommon_->GetRenderWidth();
	const int32_t height = dxCommon_->GetRenderHeight();
	if (width == renderWidth_ && height == renderHeight_) {
		return;
	}

	ResizeResources(width, height);
}

void PostEffect::ResizeResources(int32_t width, int32_t height) {
	if (width <= 0 || height <= 0) {
		return;
	}

	renderWidth_ = width;
	renderHeight_ = height;
	sceneTextureReadyAsSrv_ = false;
	depthTextureReadyAsSrv_ = false;
	textureResource_.Reset();
	depthBuffer_.Reset();
	for (auto& resource : bloomResources_) {
		resource.Reset();
	}
	fogResource_.Reset();
	rtvHeap_.Reset();
	dsvHeap_.Reset();
	bloomRtvHeap_.Reset();
	fogRtvHeap_.Reset();

	CreateTextureResource();
	CreateRtv();
	CreateDsv();
	CreateSrv();
	CreateBloomResources();
	CreateFogResources();
	ApplySettingsToBuffer();
}

void PostEffect::UpdateHotkeys() {
	if (damageVignetteTimer_ > 0.0f) {
		damageVignetteTimer_ = (std::max)(0.0f, damageVignetteTimer_ - GameTime::GetDeltaTime());
		const float normalizedTime = damageVignetteDuration_ > 0.0f
			? damageVignetteTimer_ / damageVignetteDuration_
			: 0.0f;
		// 二次減衰により、被弾直後を強く見せつつ後半の赤みを素早く消す。
		damageVignetteCurrentIntensity_ = damageVignetteMaxIntensity_ * normalizedTime * normalizedTime;
	} else {
		damageVignetteCurrentIntensity_ = 0.0f;
	}

	Input* input = Input::GetInstance();
	const auto triggered = [input](BYTE key, BYTE numpadKey) {
		return input->TriggerKey(key) || input->TriggerKey(numpadKey);
	};

	if (triggered(DIK_1, DIK_NUMPAD1)) {
		isActive_ = !isActive_;
	}
	if (triggered(DIK_2, DIK_NUMPAD2)) {
		enableGrayscale_ = !enableGrayscale_;
	}
	if (triggered(DIK_3, DIK_NUMPAD3)) {
		enableSmoothing_ = !enableSmoothing_;
	}
	if (triggered(DIK_4, DIK_NUMPAD4)) {
		enableGaussianFilter_ = !enableGaussianFilter_;
	}
	if (triggered(DIK_5, DIK_NUMPAD5)) {
		enableRadialBlur_ = !enableRadialBlur_;
	}
	if (triggered(DIK_6, DIK_NUMPAD6)) {
		enableRandom_ = !enableRandom_;
	}
	if (triggered(DIK_7, DIK_NUMPAD7)) {
		enableVignetting_ = !enableVignetting_;
	}
	if (triggered(DIK_8, DIK_NUMPAD8)) {
		enableOutline_ = !enableOutline_;
	}
	if (triggered(DIK_9, DIK_NUMPAD9)) {
		enableDissolve_ = !enableDissolve_;
	}

	ApplySettingsToBuffer();
}

void PostEffect::TriggerDamageVignette() {
	// 連続被弾時も現在値へ加算せず、常に同じ最大強度から再生する。
	damageVignetteTimer_ = damageVignetteDuration_;
	damageVignetteCurrentIntensity_ = damageVignetteMaxIntensity_;
	ApplySettingsToBuffer();
}
void PostEffect::PreDrawScene() {
	ResizeIfNeeded();
	auto commandList = dxCommon_->GetCommandList();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = textureResource_.Get();
	barrier.Transition.StateBefore = sceneTextureReadyAsSrv_ ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	if (barrier.Transition.StateBefore != barrier.Transition.StateAfter) {
		commandList->ResourceBarrier(1, &barrier);
	}

	if (depthTextureReadyAsSrv_) {
		D3D12_RESOURCE_BARRIER depthBarrier{};
		depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		depthBarrier.Transition.pResource = depthBuffer_.Get();
		depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &depthBarrier);
		depthTextureReadyAsSrv_ = false;
	}

	sceneTextureReadyAsSrv_ = false;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(renderWidth_);
	viewport.Height = static_cast<float>(renderHeight_);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &viewport);

	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = renderWidth_;
	scissorRect.bottom = renderHeight_;
	commandList->RSSetScissorRects(1, &scissorRect);
}

void PostEffect::PostDrawScene() {
	auto commandList = dxCommon_->GetCommandList();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = textureResource_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &barrier);
	sceneTextureReadyAsSrv_ = true;

	D3D12_RESOURCE_BARRIER depthBarrier{};
	depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	depthBarrier.Transition.pResource = depthBuffer_.Get();
	depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &depthBarrier);
	depthTextureReadyAsSrv_ = true;

	// シーンがHDR SRVへ遷移した直後にBloomを生成し、最終合成時は読み取りだけにします。
	if (isActive_ && enableBloom_) {
		DrawBloom();
	}

	// バックバッファへ切り替える前に、深度を使う半解像度の霧マスクを完成させます。
	time_ += GameTime::GetDeltaTime();
	ApplySettingsToBuffer();
	DrawFog();
}

void PostEffect::DrawBloom() {
	if (!bloomPipelineState_ || !bloomResources_[0] || !bloomResources_[1]) {
		return;
	}

	auto commandList = dxCommon_->GetCommandList();
	SrvManager::GetInstance()->PreDraw();
	commandList->SetGraphicsRootSignature(bloomRootSignature_.Get());
	commandList->SetPipelineState(bloomPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(bloomWidth_);
	viewport.Height = static_cast<float>(bloomHeight_);
	viewport.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &viewport);
	D3D12_RECT scissor{0, 0, bloomWidth_, bloomHeight_};
	commandList->RSSetScissorRects(1, &scissor);

	const UINT rtvIncrement = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const auto transition = [commandList](ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
	};

	struct BloomConstants {
		float threshold;
		float knee;
		float radius;
		float padding;
		float texelSize[2];
		int32_t mode;
		float padding2;
	};
	static_assert(sizeof(BloomConstants) == sizeof(uint32_t) * 8);

	const auto drawPass = [&](uint32_t targetIndex, D3D12_GPU_DESCRIPTOR_HANDLE source,
	                          int32_t mode, float sourceWidth, float sourceHeight) {
		transition(bloomResources_[targetIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		    D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = bloomRtvHeap_->GetCPUDescriptorHandleForHeapStart();
		rtv.ptr += static_cast<SIZE_T>(targetIndex) * rtvIncrement;
		commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		BloomConstants constants{};
		constants.threshold = bloomThreshold_;
		constants.knee = bloomKnee_;
		constants.radius = bloomRadius_;
		constants.texelSize[0] = 1.0f / (std::max)(sourceWidth, 1.0f);
		constants.texelSize[1] = 1.0f / (std::max)(sourceHeight, 1.0f);
		constants.mode = mode;
		commandList->SetGraphicsRootDescriptorTable(0, source);
		commandList->SetGraphicsRoot32BitConstants(1, 8, &constants, 0);
		commandList->DrawInstanced(3, 1, 0, 0);
		transition(bloomResources_[targetIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
		    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	};

	// 0: HDR明部を半解像度へ縮小、1: 横ぼかし、2: 縦ぼかし。
	drawPass(0, srvHandleGPU_, 0, static_cast<float>(renderWidth_), static_cast<float>(renderHeight_));
	drawPass(1, bloomSrvHandlesGPU_[0], 1, static_cast<float>(bloomWidth_), static_cast<float>(bloomHeight_));
	drawPass(0, bloomSrvHandlesGPU_[1], 2, static_cast<float>(bloomWidth_), static_cast<float>(bloomHeight_));
}

void PostEffect::DrawFog() {
	if (!fogPipelineState_ || !fogResource_ || !enableFog_ || !isActive_) {
		return;
	}
	auto commandList = dxCommon_->GetCommandList();
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = fogResource_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = fogRtvHeap_->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	const float clearColor[4]{};
	commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(fogWidth_);
	viewport.Height = static_cast<float>(fogHeightPixels_);
	viewport.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &viewport);
	D3D12_RECT scissor{0, 0, fogWidth_, fogHeightPixels_};
	commandList->RSSetScissorRects(1, &scissor);
	commandList->SetGraphicsRootSignature(fogRootSignature_.Get());
	commandList->SetPipelineState(fogPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootDescriptorTable(0, depthSrvHandleGPU_);
	commandList->SetGraphicsRootDescriptorTable(1, fogNoiseSrvHandleGPU_);
	commandList->SetGraphicsRootConstantBufferView(2, colorBuffer_->GetGPUVirtualAddress());
	commandList->DrawInstanced(3, 1, 0, 0);

	std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
	commandList->ResourceBarrier(1, &barrier);
}

/// <summary>
/// 現在の状態をもとに描画処理を行います。
/// </summary>
void PostEffect::Draw() {
	ResizeIfNeeded();
	auto commandList = dxCommon_->GetCommandList();
	SrvManager::GetInstance()->PreDraw();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootDescriptorTable(0, srvHandleGPU_);
	commandList->SetGraphicsRootConstantBufferView(1, colorBuffer_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, depthSrvHandleGPU_);
	commandList->SetGraphicsRootDescriptorTable(3, dissolveMaskSrvHandleGPU_);
	commandList->SetGraphicsRootDescriptorTable(4, bloomSrvHandlesGPU_[0]);
	commandList->SetGraphicsRootDescriptorTable(5, fogSrvHandleGPU_);

	commandList->DrawInstanced(3, 1, 0, 0);
}

/// <summary>
/// ImGui によるデバッグ用 UI の表示と編集処理を行います。
/// </summary>
void PostEffect::DrawImGui() {
#ifdef USE_IMGUI
#ifndef IMGUI_HAS_DOCK
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float width = displaySize.x > 0.0f ? displaySize.x : 1280.0f;
	const float panelWidth = width < 900.0f ? 280.0f : 320.0f;
	const float leftWidth = width * 0.18f;
	const float x = (width - panelWidth) * 0.5f;

	ImGui::SetNextWindowPos(ImVec2(x > leftWidth ? x : leftWidth, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, 420.0f), ImGuiCond_Always);
	ImGui::Begin(
	    "PostEffect Settings",
	    nullptr,
	    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings
	);
#else
	ImGui::Begin("PostEffect Settings");
#endif

	const ImVec4 enabledColor{0.2f, 1.0f, 0.35f, 1.0f};
	const ImVec4 disabledColor{1.0f, 0.25f, 0.2f, 1.0f};
	const auto drawEffectToggle = [&enabledColor, &disabledColor](const char* label, bool* enabled) {
		const bool changed = ImGui::Checkbox(label, enabled);
		ImGui::SameLine();
		ImGui::TextColored(*enabled ? enabledColor : disabledColor, *enabled ? "[ON]" : "[OFF]");
		return changed;
	};

	const int enabledEffectCount =
		static_cast<int>(enableGrayscale_) + static_cast<int>(enableSmoothing_) +
		static_cast<int>(enableGaussianFilter_) + static_cast<int>(enableRadialBlur_) +
		static_cast<int>(enableRandom_) + static_cast<int>(enableOutline_) +
		static_cast<int>(enableDissolve_) + static_cast<int>(enableVignetting_) +
		static_cast<int>(enableFog_);

	drawEffectToggle("Enable PostEffect", &isActive_);
	ImGui::TextColored(
		isActive_ ? enabledColor : disabledColor,
		isActive_ ? "APPLYING: %d effect(s)" : "MASTER OFF: effects are not applied",
		enabledEffectCount
	);
	ImGui::TextDisabled("Hotkeys: 1 Master / 2-9 Effects");
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Bloom Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (drawEffectToggle("Apply Bloom", &enableBloom_)) {
			colorData_->enableBloom = enableBloom_ ? 1 : 0;
		}
		if (!enableBloom_) {
			ImGui::BeginDisabled();
		}
		ImGui::SliderFloat("Bloom Threshold", &bloomThreshold_, 0.0f, 1.5f);
		ImGui::SliderFloat("Bloom Intensity", &bloomIntensity_, 0.0f, 2.0f);
		ImGui::SliderFloat("Bloom Radius", &bloomRadius_, 0.5f, 4.0f);
		ImGui::SliderFloat("Bloom Soft Knee", &bloomKnee_, 0.01f, 1.0f);
		if (!enableBloom_) {
			ImGui::EndDisabled();
		}
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("HDR / Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable ACES Tone Mapping", &enableToneMapping_);
		ImGui::SliderFloat("Exposure", &exposure_, 0.1f, 4.0f);
		ImGui::TextDisabled("Tone mapping remains available when the effect master is off.");
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Fog Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		drawEffectToggle("Apply Depth Fog", &enableFog_);
		if (!enableFog_) {
			ImGui::BeginDisabled();
		}
		ImGui::ColorEdit4("Fog Color", fogColor_);
		ImGui::SliderFloat("Fog Density", &fogDensity_, 0.0f, 2.0f);
		ImGui::SliderFloat("Fog Screen Height", &fogHeight_, 0.0f, 1.0f);
		ImGui::SliderFloat("Fog Noise Scale", &fogNoiseScale_, 0.1f, 12.0f);
		ImGui::SliderFloat("Fog Noise Strength", &fogNoiseStrength_, 0.0f, 1.0f);
		ImGui::SliderFloat("Fog Particle Amount", &fogParticleAmount_, 0.0f, 1.0f);
		ImGui::SliderFloat("Fog Wind Speed", &fogSpeed_, -0.2f, 0.2f);
		ImGui::SliderFloat("Fog Distance Start", &fogDistanceStart_, 0.0f, 100.0f);
		ImGui::SliderFloat("Fog Distance End", &fogDistanceEnd_, fogDistanceStart_ + 0.01f, 200.0f);
		ImGui::Checkbox("Fog High Quality", &fogHighQuality_);
		if (!enableFog_) {
			ImGui::EndDisabled();
		}
	}
	ImGui::Separator();

	// 小さいウィンドウでも見つけやすいよう、アウトライン設定を先頭の展開セクションに配置します。
	if (ImGui::CollapsingHeader("Outline Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (drawEffectToggle("Apply Outline", &enableOutline_)) {
			colorData_->enableOutline = enableOutline_ ? 1 : 0;
		}
		if (!enableOutline_) {
			ImGui::BeginDisabled();
		}
		ImGui::TextDisabled("Depth-based outline (lower threshold = more edges)");
		ImGui::ColorEdit4("Outline Color", outlineColor_);
		ImGui::SliderFloat("Outline Strength", &outlineStrength_, 0.0f, 2.0f);
		ImGui::SliderFloat("Outline Threshold", &outlineThreshold_, 0.001f, 0.5f, "%.3f");
		ImGui::SliderFloat("Outline Thickness", &outlineThickness_, 1.0f, 8.0f, "%.1f px");
		// 調整中に基準値へすぐ戻せるよう、アウトライン項目だけを初期化します。
		if (ImGui::Button("Reset Outline Settings")) {
			outlineColor_[0] = 0.0f;
			outlineColor_[1] = 0.0f;
			outlineColor_[2] = 0.0f;
			outlineColor_[3] = 1.0f;
			outlineStrength_ = 1.0f;
			outlineThreshold_ = 0.05f;
			outlineThickness_ = 2.0f;
		}
		if (!enableOutline_) {
			ImGui::EndDisabled();
		}
	}
	ImGui::Separator();

	drawEffectToggle("Apply Grayscale", &enableGrayscale_);

	if (ImGui::ColorEdit4("Tint Color", tintColor_)) {
		colorData_->r = tintColor_[0];
		colorData_->g = tintColor_[1];
		colorData_->b = tintColor_[2];
		colorData_->a = tintColor_[3];
	}

	ImGui::Separator();
	if (drawEffectToggle("Apply Smoothing", &enableSmoothing_)) {
		colorData_->enableSmoothing = enableSmoothing_ ? 1 : 0;
	}

	if (drawEffectToggle("Apply Gaussian Filter", &enableGaussianFilter_)) {
		colorData_->enableGaussianFilter = enableGaussianFilter_ ? 1 : 0;
	}

	if (drawEffectToggle("Apply Radial Blur", &enableRadialBlur_)) {
		colorData_->enableRadialBlur = enableRadialBlur_ ? 1 : 0;
	}
	if (!enableRadialBlur_) {
		ImGui::BeginDisabled();
	}
	if (ImGui::SliderFloat("Radial Strength", &radialBlurStrength_, 0.0f, 0.3f)) {
		colorData_->radialBlurStrength = radialBlurStrength_;
	}
	if (ImGui::SliderInt("Radial Samples", &radialBlurSamples_, 2, 32)) {
		colorData_->radialBlurSamples = radialBlurSamples_;
	}
	if (!enableRadialBlur_) {
		ImGui::EndDisabled();
	}

	if (drawEffectToggle("Apply Random", &enableRandom_)) {
		colorData_->enableRandom = enableRandom_ ? 1 : 0;
	}
	if (!enableRandom_) {
		ImGui::BeginDisabled();
	}
	if (ImGui::SliderFloat("Random Strength", &randomStrength_, 0.0f, 0.3f)) {
		colorData_->randomStrength = randomStrength_;
	}
	if (!enableRandom_) {
		ImGui::EndDisabled();
	}

	ImGui::Separator();
	if (drawEffectToggle("Apply Dissolve", &enableDissolve_)) {
		colorData_->enableDissolve = enableDissolve_ ? 1 : 0;
	}
	if (!enableDissolve_) {
		ImGui::BeginDisabled();
	}
	ImGui::SliderFloat("Dissolve Threshold", &dissolveThreshold_, 0.0f, 1.0f);
	ImGui::SliderFloat("Dissolve Edge Width", &dissolveEdgeWidth_, 0.001f, 0.3f);
	ImGui::ColorEdit4("Dissolve Edge Color", dissolveEdgeColor_);
	if (!enableDissolve_) {
		ImGui::EndDisabled();
	}

	ImGui::Separator();
	if (drawEffectToggle("Apply Vignetting", &enableVignetting_)) {
		colorData_->enableVignetting = enableVignetting_ ? 1 : 0;
	}
	if (!enableVignetting_) {
		ImGui::BeginDisabled();
	}
	if (ImGui::SliderFloat("Vignette Intensity", &vignetteIntensity_, 0.0f, 1.0f)) {
		colorData_->vignetteIntensity = vignetteIntensity_;
	}
	if (ImGui::SliderFloat("Vignette Radius", &vignetteRadius_, 0.0f, 1.5f)) {
		colorData_->vignetteRadius = vignetteRadius_;
	}
	if (ImGui::SliderFloat("Vignette Softness", &vignetteSoftness_, 0.01f, 1.0f)) {
		colorData_->vignetteSoftness = vignetteSoftness_;
	}
	if (!enableVignetting_) {
		ImGui::EndDisabled();
	}

	ImGui::Separator();
	ImGui::Text("Damage Vignette");
	ImGui::SliderFloat("Damage Intensity", &damageVignetteMaxIntensity_, 0.0f, 1.0f);
	ImGui::SliderFloat("Damage Duration", &damageVignetteDuration_, 0.05f, 2.0f);
	ImGui::SliderFloat("Damage Radius", &damageVignetteRadius_, 0.0f, 0.5f);
	ImGui::SliderFloat("Damage Softness", &damageVignetteSoftness_, 0.01f, 0.5f);
	if (ImGui::Button("Test Damage Vignette")) {
		TriggerDamageVignette();
	}

	ApplySettingsToBuffer();
	ImGui::End();
#endif
}
/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void PostEffect::Finalize() {
	textureResource_.Reset();
	depthBuffer_.Reset();
	for (auto& resource : bloomResources_) {
		resource.Reset();
	}
	dissolveMaskResource_.Reset();
	fogResource_.Reset();
	rtvHeap_.Reset();
	bloomRtvHeap_.Reset();
	fogRtvHeap_.Reset();
	dsvHeap_.Reset();
	rootSignature_.Reset();
	graphicsPipelineState_.Reset();
	bloomRootSignature_.Reset();
	bloomPipelineState_.Reset();
	fogRootSignature_.Reset();
	fogPipelineState_.Reset();
	colorBuffer_.Reset();
}
