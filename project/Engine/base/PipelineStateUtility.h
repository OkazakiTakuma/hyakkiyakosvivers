#pragma once

#include "Logger.h"
#include "struct.h"
#include <cassert>
#include <d3d12.h>
#include <wrl.h>

/// <summary>
/// Root Signatureのシリアライズ、エラー出力、D3D12オブジェクト生成を共通化します。
/// </summary>
namespace PipelineStateUtility {

inline D3D12_BLEND_DESC MakeBlendDesc(
    BOOL enabled = FALSE,
    D3D12_BLEND source = D3D12_BLEND_ONE,
    D3D12_BLEND destination = D3D12_BLEND_ZERO,
    D3D12_BLEND_OP operation = D3D12_BLEND_OP_ADD) {
	D3D12_BLEND_DESC description{};
	D3D12_RENDER_TARGET_BLEND_DESC& target = description.RenderTarget[0];
	target.BlendEnable = enabled;
	target.SrcBlend = source;
	target.DestBlend = destination;
	target.BlendOp = operation;
	target.SrcBlendAlpha = D3D12_BLEND_ONE;
	target.DestBlendAlpha = D3D12_BLEND_ZERO;
	target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return description;
}

/// <summary>共通BlendModeをD3D12のブレンド設定へ変換します。</summary>
inline D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode) {
	// 合成方式とD3D12係数の対応を一か所に置き、各描画パイプラインの設定差を防ぐ。
	switch (mode) {
	case kBlendModeNormal:
		return MakeBlendDesc(TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA);
	case kBlendModeAdd:
		return MakeBlendDesc(TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE);
	case kBlendModeSubtract:
		return MakeBlendDesc(TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_REV_SUBTRACT);
	case kBlendModeMultiply:
		return MakeBlendDesc(TRUE, D3D12_BLEND_ZERO, D3D12_BLEND_SRC_COLOR);
	case kBlendModeScreen:
		return MakeBlendDesc(TRUE, D3D12_BLEND_INV_DEST_COLOR, D3D12_BLEND_ONE);
	case kBlendModeNone:
	default:
		return MakeBlendDesc();
	}
}

inline D3D12_RASTERIZER_DESC MakeRasterizerDesc(
    D3D12_CULL_MODE cullMode,
    BOOL depthClipEnabled = FALSE) {
	D3D12_RASTERIZER_DESC description{};
	description.FillMode = D3D12_FILL_MODE_SOLID;
	description.CullMode = cullMode;
	description.DepthClipEnable = depthClipEnabled;
	return description;
}

inline D3D12_DEPTH_STENCIL_DESC MakeDepthStencilDesc(
    BOOL depthEnabled,
    D3D12_DEPTH_WRITE_MASK writeMask,
    D3D12_COMPARISON_FUNC comparison = D3D12_COMPARISON_FUNC_LESS_EQUAL) {
	D3D12_DEPTH_STENCIL_DESC description{};
	description.DepthEnable = depthEnabled;
	description.DepthWriteMask = writeMask;
	description.DepthFunc = comparison;
	return description;
}

inline Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(
    ID3D12Device* device,
    const D3D12_ROOT_SIGNATURE_DESC& description,
    D3D_ROOT_SIGNATURE_VERSION version = D3D_ROOT_SIGNATURE_VERSION_1) {
	assert(device != nullptr);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	const HRESULT serializeResult = D3D12SerializeRootSignature(
	    &description,
	    version,
	    &signatureBlob,
	    &errorBlob);
	if (FAILED(serializeResult)) {
		if (errorBlob) {
			Logger::Log(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
		}
		assert(false);
		return nullptr;
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	const HRESULT createResult = device->CreateRootSignature(
	    0,
	    signatureBlob->GetBufferPointer(),
	    signatureBlob->GetBufferSize(),
	    IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(createResult));
	return rootSignature;
}

} // 名前空間 PipelineStateUtility
