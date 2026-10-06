#include "Object3dCommon.h"
#include "PipelineStateUtility.h"
#include "../../base/SrvManager.h"
#include <cmath>
#include <cstddef>

using namespace Logger;

/// <summary>
/// 共有インスタンスを取得します。
/// </summary>
Object3dCommon* Object3dCommon::GetInstance() {
	static Object3dCommon instance;
	return &instance;
}

/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
/// <param name="dxCommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
void Object3dCommon::Initialize(DirectXCommon* dxCommon) {
	assert(dxCommon);
	this->dxCommon_ = dxCommon;

	CreatePipelineState();
	CreateShadowMapResource();
}

void Object3dCommon::EnsureInitialized(DirectXCommon* dxCommon) {
	assert(dxCommon);
	if (dxCommon_ == dxCommon && rootSignature && graphicsPipelineState && shadowPipelineState && shadowMapPipelineState_ && shadowReceiverPipelineState_ && shadowMapResource_) {
		return;
	}
	Initialize(dxCommon);
}

void Object3dCommon::SetDraw() {
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetShadowDraw() {
	dxCommon_->GetCommandList()->SetPipelineState(shadowPipelineState.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetShadowMapDraw() {
	dxCommon_->GetCommandList()->SetPipelineState(shadowMapPipelineState_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetShadowReceiverDraw() {
	dxCommon_->GetCommandList()->SetPipelineState(shadowReceiverPipelineState_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::BindShadowMap() {
	if (shadowMapIsShaderResource_) {
		// root parameter 6は通常パスでは環境マップ、影合成パスでは影マップを指します。
		SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(6, shadowSrvIndex_);
	}
}

void Object3dCommon::SetShadowLightDirection(const Vector3& direction) {
	const Vector3 normalized = NormalizeReturnVector(direction);
	if (Length(normalized) > 0.0f) shadowLightDirection_ = normalized;
}

void Object3dCommon::UpdateLightViewProjectionMatrix() {
	// 床の高さではなくカメラ周辺の空間全体を覆うので、坂や段差も同じ方法で扱えます。
	const Vector3 focus = defaultCamera ? defaultCamera->GetTranslate() : Vector3{};
	Vector3 forward = NormalizeReturnVector(shadowLightDirection_);
	if (Length(forward) <= 0.0f) forward = {0.0f, -1.0f, 0.0f};
	Vector3 up = std::fabs(forward.y) > 0.98f ? Vector3{0.0f, 0.0f, 1.0f} : Vector3{0.0f, 1.0f, 0.0f};
	const Vector3 right = NormalizeReturnVector(Cross(up, forward));
	up = NormalizeReturnVector(Cross(forward, right));
	// ライトは方向だけを持つため、カメラ中心から十分離れた仮想位置を作ります。
	const Vector3 lightPosition = focus - 70.0f * forward;
	Matrix4x4 view = MakeIdentity4x4();
	view.m[0][0] = right.x; view.m[1][0] = right.y; view.m[2][0] = right.z; view.m[3][0] = -Dot(lightPosition, right);
	view.m[0][1] = up.x; view.m[1][1] = up.y; view.m[2][1] = up.z; view.m[3][1] = -Dot(lightPosition, up);
	view.m[0][2] = forward.x; view.m[1][2] = forward.y; view.m[2][2] = forward.z; view.m[3][2] = -Dot(lightPosition, forward);
	// カメラ中心の70x70領域を直交投影します。遠近で影サイズが変わらない平行光源向けです。
	Matrix4x4 projection{};
	projection.m[0][0] = 1.0f / 35.0f;
	projection.m[1][1] = 1.0f / 35.0f;
	projection.m[2][2] = 1.0f / (140.0f - 0.1f);
	projection.m[3][2] = -0.1f / (140.0f - 0.1f);
	projection.m[3][3] = 1.0f;
	lightViewProjectionMatrix_ = Multiply(view, projection);
}

void Object3dCommon::BeginShadowMapPass() {
	if (!shadowMapResource_) return;
	UpdateLightViewProjectionMatrix();
	auto commandList = dxCommon_->GetCommandList();
	if (shadowMapIsShaderResource_) {
		// 前フレームの読み取り状態から、今フレームの書き込み状態へ切り替えます。
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = shadowMapResource_.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		shadowMapIsShaderResource_ = false;
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = shadowRtvHeap_->GetCPUDescriptorHandleForHeapStart();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = shadowDsvHeap_->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	// 1.0は最遠方を表し、何も描かれていない場所を「遮蔽物なし」にします。
	const float clearDepth[] = {1.0f, 1.0f, 1.0f, 1.0f};
	commandList->ClearRenderTargetView(rtv, clearDepth, 0, nullptr);
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	const D3D12_VIEWPORT viewport{0.0f, 0.0f, float(kShadowMapSize), float(kShadowMapSize), 0.0f, 1.0f};
	const D3D12_RECT scissor{0, 0, LONG(kShadowMapSize), LONG(kShadowMapSize)};
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	SetShadowMapDraw();
}

void Object3dCommon::EndShadowMapPass() {
	if (!shadowMapResource_ || shadowMapIsShaderResource_) return;
	// 通常描画のピクセルシェーダーから参照できる読み取り状態へ戻します。
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = shadowMapResource_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	shadowMapIsShaderResource_ = true;
}

void Object3dCommon::CreateShadowMapResource() {
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	// 比較に使う深度値をそのまま保存できる単チャンネル32bit浮動小数点形式です。
	D3D12_RESOURCE_DESC description{};
	description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	description.Width = kShadowMapSize;
	description.Height = kShadowMapSize;
	description.DepthOrArraySize = 1;
	description.MipLevels = 1;
	description.Format = DXGI_FORMAT_R32_FLOAT;
	description.SampleDesc.Count = 1;
	description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_R32_FLOAT;
	clearValue.Color[0] = 1.0f;
	const HRESULT result = dxCommon_->GetDevice()->CreateCommittedResource(
	    &heapProperties, D3D12_HEAP_FLAG_NONE, &description,
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
	    IID_PPV_ARGS(&shadowMapResource_));
	assert(SUCCEEDED(result));
	shadowRtvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDescription{};
	rtvDescription.Format = DXGI_FORMAT_R32_FLOAT;
	rtvDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	dxCommon_->GetDevice()->CreateRenderTargetView(
	    shadowMapResource_.Get(), &rtvDescription, shadowRtvHeap_->GetCPUDescriptorHandleForHeapStart());

	// R32の保存先とは別にD32の深度判定用バッファを持ち、最前面だけを書き込みます。
	D3D12_RESOURCE_DESC depthDescription = description;
	depthDescription.Format = DXGI_FORMAT_D32_FLOAT;
	depthDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE depthClear{};
	depthClear.Format = DXGI_FORMAT_D32_FLOAT;
	depthClear.DepthStencil.Depth = 1.0f;
	const HRESULT depthResult = dxCommon_->GetDevice()->CreateCommittedResource(
	    &heapProperties, D3D12_HEAP_FLAG_NONE, &depthDescription,
	    D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear, IID_PPV_ARGS(&shadowDepthResource_));
	assert(SUCCEEDED(depthResult));
	shadowDsvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDescription{};
	dsvDescription.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dxCommon_->GetDevice()->CreateDepthStencilView(
	    shadowDepthResource_.Get(), &dsvDescription, shadowDsvHeap_->GetCPUDescriptorHandleForHeapStart());
	// 同じR32リソースを、影合成パスではSRVとして読み取ります。
	shadowSrvIndex_ = SrvManager::GetInstance()->Allocate();
	SrvManager::GetInstance()->CreateSRVforTexture2D(
	    shadowSrvIndex_, shadowMapResource_.Get(), DXGI_FORMAT_R32_FLOAT, 1);
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void Object3dCommon::Finalize() {
	shadowMapResource_.Reset();
	shadowDepthResource_.Reset();
	shadowRtvHeap_.Reset();
	shadowDsvHeap_.Reset();
	rootSignature.Reset();
	graphicsPipelineState.Reset();
	shadowPipelineState.Reset();
	shadowMapPipelineState_.Reset();
	shadowReceiverPipelineState_.Reset();
	dxCommon_ = nullptr;
	defaultCamera = nullptr;
}

/// <summary>
/// RootSignature を作成し、利用できる状態にします。
/// </summary>
void Object3dCommon::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeEnvMap[1] = {};
	descriptorRangeEnvMap[0].BaseShaderRegister = 1;
	descriptorRangeEnvMap[0].NumDescriptors = 1;
	descriptorRangeEnvMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeEnvMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[8] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].Descriptor.ShaderRegister = 2;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 3; // b3

	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 4; // b4

	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].DescriptorTable.pDescriptorRanges = descriptorRangeEnvMap;
	rootParameters[6].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeEnvMap);

	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[7].Descriptor.ShaderRegister = 2;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	rootSignature = PipelineStateUtility::CreateRootSignature(dxCommon_->GetDevice().Get(), descriptionRootSignature);
}
/// <summary>
/// PipelineState を作成し、利用できる状態にします。
/// </summary>
void Object3dCommon::CreatePipelineState() {
	HRESULT hr;
	CreateRootSignature();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[5] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = static_cast<UINT>(offsetof(::VertexData, position));

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = static_cast<UINT>(offsetof(::VertexData, texcoord));

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = static_cast<UINT>(offsetof(::VertexData, normal));

	inputElementDescs[3].SemanticName = "BONEINDEX";
	inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_UINT;
	inputElementDescs[3].AlignedByteOffset = static_cast<UINT>(offsetof(::VertexData, boneIndices));

	inputElementDescs[4].SemanticName = "BONEWEIGHT";
	inputElementDescs[4].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[4].AlignedByteOffset = static_cast<UINT>(offsetof(::VertexData, boneWeights));

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	const D3D12_BLEND_DESC blendDesc = PipelineStateUtility::MakeBlendDesc();
	const D3D12_BLEND_DESC shadowBlendDesc = PipelineStateUtility::MakeBlendDesc(
	    TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA);
	const D3D12_RASTERIZER_DESC rasterizerDesc = PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_NONE);

	auto vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/Object3d.VS.hlsl", L"vs_6_0");
	auto pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/Object3d.PS.hlsl", L"ps_6_0");
	auto shadowMapVertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/ShadowMap.VS.hlsl", L"vs_6_0");
	auto shadowMapPixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/ShadowMap.PS.hlsl", L"ps_6_0");
	auto shadowReceiverPixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/ShadowReceiver.PS.hlsl", L"ps_6_0");
	assert(vertexShaderBlob != nullptr);
	assert(pixelShaderBlob != nullptr);
	assert(shadowMapVertexShaderBlob != nullptr);
	assert(shadowMapPixelShaderBlob != nullptr);
	assert(shadowReceiverPixelShaderBlob != nullptr);

	// 深度ステンシルステート
	const D3D12_DEPTH_STENCIL_DESC depthStencilDesc =
	    PipelineStateUtility::MakeDepthStencilDesc(TRUE, D3D12_DEPTH_WRITE_MASK_ALL);

	// パイプラインステート
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
	psoDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	// 1.0を超える発光と太陽光をPostEffectまで保持するHDR描画先です。
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = psoDesc;
	D3D12_DEPTH_STENCIL_DESC shadowDepthStencilDesc = depthStencilDesc;
	shadowDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	shadowPsoDesc.BlendState = shadowBlendDesc;
	shadowPsoDesc.DepthStencilState = shadowDepthStencilDesc;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&shadowPipelineState));
	assert(SUCCEEDED(hr));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowMapPsoDesc = psoDesc;
	D3D12_RASTERIZER_DESC shadowMapRasterizer = PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_NONE, TRUE);
	// ライトに対して斜めの面で起きる自己シャドウを、深度バイアスで抑えます。
	shadowMapRasterizer.DepthBias = 1000;
	shadowMapRasterizer.SlopeScaledDepthBias = 1.0f;
	shadowMapRasterizer.DepthBiasClamp = 0.01f;
	shadowMapPsoDesc.RasterizerState = shadowMapRasterizer;
	shadowMapPsoDesc.BlendState = PipelineStateUtility::MakeBlendDesc();
	shadowMapPsoDesc.VS = {shadowMapVertexShaderBlob->GetBufferPointer(), shadowMapVertexShaderBlob->GetBufferSize()};
	shadowMapPsoDesc.PS = {shadowMapPixelShaderBlob->GetBufferPointer(), shadowMapPixelShaderBlob->GetBufferSize()};
	shadowMapPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	shadowMapPsoDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&shadowMapPsoDesc, IID_PPV_ARGS(&shadowMapPipelineState_));
	assert(SUCCEEDED(hr));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC receiverPsoDesc = psoDesc;
	receiverPsoDesc.PS = {shadowReceiverPixelShaderBlob->GetBufferPointer(), shadowReceiverPixelShaderBlob->GetBufferSize()};
	receiverPsoDesc.BlendState = shadowBlendDesc;
	receiverPsoDesc.DepthStencilState = PipelineStateUtility::MakeDepthStencilDesc(
	    TRUE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_LESS_EQUAL);
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&receiverPsoDesc, IID_PPV_ARGS(&shadowReceiverPipelineState_));
	assert(SUCCEEDED(hr));
}
