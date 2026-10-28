#include "AtmosphereSky.h"

#include "PipelineStateUtility.h"
#include "SkyBoxCommon.h"
#include "camera/Camera.h"
#include <cassert>
#include <cstring>

void AtmosphereSky::Initialize() {
	dxCommon_ = SkyBoxCommon::GetInstance()->GetDxCommon();
	assert(dxCommon_);
	CreateRootSignature();
	CreatePipelineState();
	CreateGeometry();
	CreateConstantBuffers();
}

void AtmosphereSky::CreateRootSignature() {
	D3D12_ROOT_PARAMETER rootParameters[2]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	D3D12_ROOT_SIGNATURE_DESC description{};
	description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	description.pParameters = rootParameters;
	description.NumParameters = _countof(rootParameters);
	rootSignature_ = PipelineStateUtility::CreateRootSignature(dxCommon_->GetDevice().Get(), description);
}

void AtmosphereSky::CreatePipelineState() {
	D3D12_INPUT_ELEMENT_DESC inputElement{};
	inputElement.SemanticName = "POSITION";
	inputElement.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayout{};
	inputLayout.pInputElementDescs = &inputElement;
	inputLayout.NumElements = 1;

	auto vertexShader = dxCommon_->CompileShader(L"Resources/Shader/AtmosphereSky.VS.hlsl", L"vs_6_0");
	auto pixelShader = dxCommon_->CompileShader(L"Resources/Shader/AtmosphereSky.PS.hlsl", L"ps_6_0");
	assert(vertexShader && pixelShader);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
	description.pRootSignature = rootSignature_.Get();
	description.InputLayout = inputLayout;
	description.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
	description.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
	description.BlendState = PipelineStateUtility::MakeBlendDesc();
	// カメラが立方体の内側にいるため、表面側をカリングします。
	description.RasterizerState = PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_FRONT);
	description.DepthStencilState = PipelineStateUtility::MakeDepthStencilDesc(TRUE, D3D12_DEPTH_WRITE_MASK_ZERO);
	description.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	description.NumRenderTargets = 1;
	description.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	description.SampleDesc.Count = 1;
	description.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	const HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
	    &description, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(result));
}

void AtmosphereSky::CreateGeometry() {
	constexpr float size = 1.0f;
	const Vertex vertices[] = {
	    {{-size, -size, -size, 1.0f}}, {{-size, size, -size, 1.0f}},
	    {{size, size, -size, 1.0f}},   {{size, -size, -size, 1.0f}},
	    {{-size, -size, size, 1.0f}},  {{-size, size, size, 1.0f}},
	    {{size, size, size, 1.0f}},    {{size, -size, size, 1.0f}},
	};
	const uint16_t indices[] = {
	    3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
	    1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7,
	    0, 1, 2, 0, 2, 3, 7, 6, 5, 7, 5, 4,
	};

	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(vertices));
	Vertex* mappedVertices = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
	std::memcpy(mappedVertices, vertices, sizeof(vertices));
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(vertices);
	vertexBufferView_.StrideInBytes = sizeof(Vertex);

	indexResource_ = dxCommon_->CreateBufferResource(sizeof(indices));
	uint16_t* mappedIndices = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
	std::memcpy(mappedIndices, indices, sizeof(indices));
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(indices);
	indexBufferView_.Format = DXGI_FORMAT_R16_UINT;
}

void AtmosphereSky::CreateConstantBuffers() {
	transformResource_ = dxCommon_->CreateBufferResource(sizeof(TransformForGPU));
	transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
	transformData_->WVP = MakeIdentity4x4();

	atmosphereResource_ = dxCommon_->CreateBufferResource(sizeof(AtmosphereForGPU));
	atmosphereResource_->Map(0, nullptr, reinterpret_cast<void**>(&atmosphereData_));
	*atmosphereData_ = {};
}

void AtmosphereSky::Update(const Camera* camera) {
	if (!camera || !transformData_ || !atmosphereData_) {
		return;
	}

	// Skyはカメラの回転だけに追従させ、移動しても無限遠に見えるよう平行移動を除去します。
	Matrix4x4 view = camera->GetViewMatrix();
	view.m[3][0] = 0.0f;
	view.m[3][1] = 0.0f;
	view.m[3][2] = 0.0f;
	transformData_->WVP = Multiply(MakeScaleMatrix({500.0f, 500.0f, 500.0f}),
	    Multiply(view, camera->GetProjectionMatrix()));

	const AtmosphereSettings& settings = AtmosphereSystem::GetInstance()->GetSettings();
	atmosphereData_->sunDirection = Normalize(settings.sunDirection);
	atmosphereData_->sunAngularRadius = settings.sunAngularRadius;
	atmosphereData_->rayleighColor = settings.rayleighColor;
	atmosphereData_->rayleighStrength = settings.rayleighStrength;
	atmosphereData_->mieColor = settings.mieColor;
	atmosphereData_->mieStrength = settings.mieStrength;
	atmosphereData_->mieG = settings.mieG;
	atmosphereData_->atmosphereDensity = settings.atmosphereDensity;
	atmosphereData_->sunIntensity = settings.sunIntensity;
}

void AtmosphereSky::Draw() {
	const AtmosphereSettings& settings = AtmosphereSystem::GetInstance()->GetSettings();
	Camera* camera = SkyBoxCommon::GetInstance()->GetDefaultCamera();
	if (!settings.enabled || !camera || !pipelineState_) {
		return;
	}
	Update(camera);

	auto commandList = dxCommon_->GetCommandList();
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, transformResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, atmosphereResource_->GetGPUVirtualAddress());
	commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

void AtmosphereSky::Finalize() {
	atmosphereData_ = nullptr;
	transformData_ = nullptr;
	atmosphereResource_.Reset();
	transformResource_.Reset();
	indexResource_.Reset();
	vertexResource_.Reset();
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
}
