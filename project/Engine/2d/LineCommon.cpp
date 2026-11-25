#include "LineCommon.h"
#include "PipelineStateUtility.h"

#include "Logger.h"

#include <cassert>

using namespace Logger;

/// <summary>
/// 共有インスタンスを取得します。
/// </summary>
LineCommon* LineCommon::GetInstance() {
	static LineCommon instance;
	return &instance;
}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
/// <param name="dxCommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
void LineCommon::Initialize(DirectXCommon* dxCommon) {
	assert(dxCommon);
	dxCommon_ = dxCommon;
	CreatePipelineState();
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void LineCommon::Finalize() {
	rootSignature.Reset();
	for (auto& depthPipelineStates : graphicsPipelineStates) {
		for (auto& pso : depthPipelineStates) {
			pso.Reset();
		}
	}
	dxCommon_ = nullptr;
}

/// <param name="blendMode">描画時に使用するブレンドモードを指定します。</param>
void LineCommon::SetDraw(uint32_t blendMode, bool ignoreDepth) {
	assert(dxCommon_);
	assert(blendMode < kCountOfBlendMode);

	auto commandList = dxCommon_->GetCommandList();
	commandList->SetPipelineState(graphicsPipelineStates[ignoreDepth ? 1 : 0][blendMode].Get());
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

/// <summary>
/// RootSignature を作成し、利用できる状態にします。
/// </summary>
void LineCommon::CreateRootSignature() {
	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	rootSignature = PipelineStateUtility::CreateRootSignature(dxCommon_->GetDevice().Get(), descriptionRootSignature);
}

/// <summary>
/// PipelineState を作成し、利用できる状態にします。
/// </summary>
void LineCommon::CreatePipelineState() {
	CreateRootSignature();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "COLOR";
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	const D3D12_RASTERIZER_DESC rasterizerDesc = PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_NONE);

	auto vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/Line.VS.hlsl", L"vs_6_0");
	auto pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/Line.PS.hlsl", L"ps_6_0");
	assert(vertexShaderBlob != nullptr);
	assert(pixelShaderBlob != nullptr);

	for (int depthMode = 0; depthMode < 2; ++depthMode) {
	for (int i = 0; i < kBlendCountblend; ++i) {
		const D3D12_BLEND_DESC blendDesc =
			PipelineStateUtility::MakeBlendDesc(static_cast<BlendMode>(i));

		const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = PipelineStateUtility::MakeDepthStencilDesc(
		    depthMode == 0, D3D12_DEPTH_WRITE_MASK_ZERO);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = rootSignature.Get();
		psoDesc.InputLayout = inputLayoutDesc;
		psoDesc.BlendState = blendDesc;
		psoDesc.RasterizerState = rasterizerDesc;
		psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
		psoDesc.DepthStencilState = depthStencilDesc;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS(&graphicsPipelineStates[depthMode][i])
		);
		assert(SUCCEEDED(hr));
	}
	}
}
