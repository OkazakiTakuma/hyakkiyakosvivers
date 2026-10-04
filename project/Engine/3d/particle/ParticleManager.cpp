#include "ParticleManager.h"
#include "PipelineStateUtility.h"
#include "../camera/Camera.h"
#include "../../base/GameTime.h"
#include <algorithm>
#include <cassert>
#include <numbers>
#include <cmath>

std::vector<VertexData> GenerateRingVerticesForParticle(uint32_t segments, float outerRadius, float innerRadius) {
	std::vector<VertexData> vertices;
	vertices.reserve(segments * 6);

	for (uint32_t i = 0; i < segments; ++i) {
		float ratio1 = static_cast<float>(i) / segments;
		float ratio2 = static_cast<float>(i + 1) / segments;

		float angle1 = ratio1 * 2.0f * std::numbers::pi_v<float>;
		float angle2 = ratio2 * 2.0f * std::numbers::pi_v<float>;

		Vector4 outer1 = { outerRadius * std::cos(angle1), outerRadius * std::sin(angle1), 0.0f, 1.0f };
		Vector4 inner1 = { innerRadius * std::cos(angle1), innerRadius * std::sin(angle1), 0.0f, 1.0f };
		Vector4 outer2 = { outerRadius * std::cos(angle2), outerRadius * std::sin(angle2), 0.0f, 1.0f };
		Vector4 inner2 = { innerRadius * std::cos(angle2), innerRadius * std::sin(angle2), 0.0f, 1.0f };

		Vector2 uvOuter1 = { ratio1, 0.0f };
		Vector2 uvInner1 = { ratio1, 1.0f };
		Vector2 uvOuter2 = { ratio2, 0.0f };
		Vector2 uvInner2 = { ratio2, 1.0f };

		Vector3 normal = { 0.0f, 0.0f, -1.0f };

		vertices.push_back({ outer1, uvOuter1, normal });
		vertices.push_back({ inner1, uvInner1, normal });
		vertices.push_back({ outer2, uvOuter2, normal });

		vertices.push_back({ inner1, uvInner1, normal });
		vertices.push_back({ inner2, uvInner2, normal });
		vertices.push_back({ outer2, uvOuter2, normal });
	}

	return vertices;
}using namespace Logger;

// 1グループあたりに保持・描画できるパーティクルの最大数です。
const uint32_t ParticleManager::kMaxParticle = 10000;

/// <summary>
/// 共有インスタンスを取得します。
/// </summary>
ParticleManager* ParticleManager::GetInstance() {
	static ParticleManager instance;
	return &instance;
}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void ParticleManager::Finalize() {
	ClearGroups();

	if (sceneResource_) {
		sceneResource_->Unmap(0, nullptr);
	}
	sceneResource_.Reset();
	sceneData_ = nullptr;

	rootSignature.Reset();
	for (auto& pso : graphicsPipelineStates) {
		pso.Reset();
	}
}

void ParticleManager::ClearGroups() {
	for (auto& group : particleGroups_) {
		group.second.instanceResource.Reset();
		group.second.vertBuff.Reset();
	}
	particleGroups_.clear();
}
/// <summary>
/// 必要なリソースを準備し、オブジェクトを初期化します。
/// </summary>
/// <param name="dxCommon">DirectX 共通処理へアクセスするための参照を指定します。</param>
void ParticleManager::Initialize(DirectXCommon* dxCommon) {

	dxCommon_ = dxCommon;
	srvManager_ = SrvManager::GetInstance();

	std::random_device seedGenerator;
	randomEngine_ = std::mt19937(seedGenerator());

	// 頂点レイアウト: POSITION(float4)、TEXCOORD(float2)、NORMAL(float3)

	vertices_.resize(6);

	vertices_[0] = VertexData{
		{-0.5f, -0.5f, 0.0f, 1.0f},
		{0.0f, 1.0f},
		{0.0f, 0.0f, -1.0f}
	};
	vertices_[1] = VertexData{
		{-0.5f, 0.5f, 0.0f, 1.0f},
		{0.0f, 0.0f},
		{0.0f, 0.0f, -1.0f}
	};
	vertices_[2] = VertexData{
		{0.5f, -0.5f, 0.0f, 1.0f},
		{1.0f, 1.0f},
		{0.0f, 0.0f, -1.0f}
	};
	vertices_[3] = VertexData{
		{-0.5f, 0.5f, 0.0f, 1.0f},
		{0.0f, 0.0f},
		{0.0f, 0.0f, -1.0f}
	};
	vertices_[4] = VertexData{
		{0.5f, 0.5f, 0.0f, 1.0f},
		{1.0f, 0.0f},
		{0.0f, 0.0f, -1.0f}
	};
	vertices_[5] = VertexData{
		{0.5f, -0.5f, 0.0f, 1.0f},
		{1.0f, 1.0f},
		{0.0f, 0.0f, -1.0f}
	};
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 6);

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	VertexData* mapped = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	std::memcpy(mapped, vertices_.data(), sizeof(VertexData) * vertices_.size());
	vertexResource_->Unmap(0, nullptr);

	sceneResource_ = dxCommon_->CreateBufferResource(sizeof(ParticleSceneForGPU));
	sceneResource_->Map(0, nullptr, reinterpret_cast<void**>(&sceneData_));
	sceneData_->viewProjection = MakeIdentity4x4();
	sceneData_->billboard = MakeIdentity4x4();

	CreatePipelineState();
}

/// <summary>
/// ParticleGroup を作成し、利用できる状態にします。
/// </summary>
/// <param name="groupName">対象となるパーティクルグループ名を指定します。</param>
/// <param name="textureFilePath">使用するテクスチャまたはモデルのファイルパスを指定します。</param>
void ParticleManager::CreateParticleGroup(const std::string& groupName, const std::string& textureFilePath, ParticleMeshType meshtype) {
	const auto [groupIt, inserted] = particleGroups_.try_emplace(groupName);
	if (!inserted) {
		assert(false && "ParticleGroup name already exists!");
		return;
	}

	ParticleGroup& group = groupIt->second;

	group.material.textureFilePath = textureFilePath;
	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture(textureFilePath);
	group.material.textureIndex = textureManager->GetTextureIndexByFilePath(textureFilePath);
	group.meshType = meshtype;

	// =========================================================
	// =========================================================
	std::vector<VertexData> vertices;
	if (meshtype == kMeshTypeRing) {
		vertices = GenerateRingVerticesForParticle(32, 1.0f, 0.2f);
	}
	else {
		vertices = {
			{ {-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
			{ {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
			{ { 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
			{ {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
			{ { 0.5f,  0.5f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
			{ { 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
		};
	}
	group.vertexCount = static_cast<uint32_t>(vertices.size());

	UINT sizeVB = static_cast<UINT>(sizeof(VertexData) * vertices.size());
	group.vertBuff = dxCommon_->CreateBufferResource(sizeVB);

	VertexData* vertMap = nullptr;
	group.vertBuff->Map(0, nullptr, reinterpret_cast<void**>(&vertMap));
	std::copy(vertices.begin(), vertices.end(), vertMap);
	group.vertBuff->Unmap(0, nullptr);

	group.vbView.BufferLocation = group.vertBuff->GetGPUVirtualAddress();
	group.vbView.SizeInBytes = sizeVB;
	group.vbView.StrideInBytes = sizeof(VertexData);
	// =========================================================

	// CPU側の上限と同じ件数をGPUへ転送できるよう、インスタンスバッファを確保します。
	const uint32_t maxInstance = kMaxParticle;
	uint32_t bufferSize = sizeof(ParticleForGPU) * maxInstance;

	group.instanceResource = dxCommon_->CreateBufferResource(bufferSize);

	group.instanceResource->Map(0, nullptr, reinterpret_cast<void**>(&group.instanceDataPtr));

	group.instanceSrvIndex = srvManager_->Allocate();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = maxInstance;
	srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	srvManager_->CreateSRVforStructuredBuffer(group.instanceSrvIndex, group.instanceResource.Get(), srvDesc.Buffer.NumElements, srvDesc.Buffer.StructureByteStride);

	group.instanceCount = 0;
}
// ==========================================
// ==========================================
/// <summary>
/// RootSignature を作成し、利用できる状態にします。
/// </summary>
void ParticleManager::CreateRootSignature() {
	D3D12_DESCRIPTOR_RANGE descriptorRangeTexture[1] = {};
	descriptorRangeTexture[0].BaseShaderRegister = 0;
	descriptorRangeTexture[0].NumDescriptors = 1;
	descriptorRangeTexture[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeTexture[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// [1] 構造化バッファー（t1）
	D3D12_DESCRIPTOR_RANGE descriptorRangeData[1] = {};
	descriptorRangeData[0].BaseShaderRegister = 1;
	descriptorRangeData[0].NumDescriptors = 1;
	descriptorRangeData[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeData[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};

	// ルートパラメーター0: テクスチャ
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangeTexture;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeTexture);

	// ルートパラメーター1: インスタンシングデータ
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeData;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeData);

	// ルートパラメーター2: カメラ行列
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[2].Descriptor.ShaderRegister = 0;

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

// ==========================================
// ==========================================
/// <summary>
/// PipelineState を作成し、利用できる状態にします。
/// </summary>
void ParticleManager::CreatePipelineState() {
	CreateRootSignature();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	const D3D12_BLEND_DESC blendDesc = PipelineStateUtility::MakeBlendDesc(
	    TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA);
	const D3D12_RASTERIZER_DESC rasterizerDesc = PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_BACK);

	// シェーダーをコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/Particle.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shader/Particle.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

	const D3D12_DEPTH_STENCIL_DESC depthStencilDesc =
	    PipelineStateUtility::MakeDepthStencilDesc(TRUE, D3D12_DEPTH_WRITE_MASK_ZERO);
	psoDesc.DepthStencilState = depthStencilDesc;

	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	for (int i = 0; i < kBlendCountblend; ++i) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC localDesc = psoDesc;

		localDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		localDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		localDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		localDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		localDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

		switch (i) {
		case kBlendModeNone:
			localDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
			break;

		case kBlendModeNormal:
			localDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			localDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			localDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			break;

		case kBlendModeAdd:
			localDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			localDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			localDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			break;

		case kBlendModeSubtract:
			localDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			localDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			localDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
			break;

		case kBlendModeMultiply:
			localDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
			localDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
			localDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			break;

		case kBlendModeScreen:
			localDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
			localDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			localDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			break;
		default:
			break;
		}

		HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&localDesc, IID_PPV_ARGS(&graphicsPipelineStates[i]));
		assert(SUCCEEDED(hr));

		if (FAILED(hr)) {
			OutputDebugStringA("Error: Failed to create GraphicsPipelineState for Particle.\n");
			assert(false);
		}
	}
}

/// <summary>
/// 現在の状態をもとに描画処理を行います。
/// </summary>
/// <param name="camera">描画や座標変換に使用するカメラを指定します。</param>
void ParticleManager::Draw(Camera* camera) {
	auto commandList = dxCommon_->GetCommandList();
	SrvManager::GetInstance()->PreDraw();

	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Matrix4x4 viewProjection = camera->GetViewProjectionMatrix();
	Matrix4x4 billboardMatrix = camera->GetWorldMatrix();
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;
	if (sceneData_) {
		sceneData_->viewProjection = viewProjection;
		sceneData_->billboard = billboardMatrix;
	}
	commandList->SetGraphicsRootConstantBufferView(2, sceneResource_->GetGPUVirtualAddress());

	for (auto& [name, group] : particleGroups_) {
		if (group.particles.empty()) {
			continue;
		}
		commandList->IASetVertexBuffers(0, 1, &group.vbView);
		commandList->SetPipelineState(graphicsPipelineStates[group.blendMode].Get());
		uint32_t numInstance = std::min<uint32_t>(static_cast<uint32_t>(group.particles.size()), kMaxParticle);
		uint32_t index = 0;

		for (const auto& particle : group.particles) {
			if (index >= numInstance)
				break;

			group.instanceDataPtr[index].translate = particle.transform.translate;
			group.instanceDataPtr[index].isBillboard = particle.isBillboard ? 1.0f : 0.0f;
			group.instanceDataPtr[index].scale = particle.transform.scale;
			group.instanceDataPtr[index].padding0 = 0.0f;
			group.instanceDataPtr[index].rotate = particle.transform.rotate;
			group.instanceDataPtr[index].padding1 = 0.0f;
			group.instanceDataPtr[index].color = particle.color;

			index++;
		}

		group.instanceCount = numInstance;

		if (group.instanceCount > 0) {
			// [0] テクスチャ
			commandList->SetGraphicsRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(group.material.textureIndex));
			// [1] インスタンシングデータ
			commandList->SetGraphicsRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.instanceSrvIndex));

			commandList->DrawInstanced(group.vertexCount, group.instanceCount, 0, 0);
		}
	}
}

/// <summary>
/// 毎フレームの状態更新を行います。
/// </summary>
void ParticleManager::Update() {
	const float deltaTime = GameTime::GetDeltaTime();
	const float frameScale = GameTime::GetFrameScale60();
	for (auto& groupPair : particleGroups_) {
		ParticleGroup& group = groupPair.second;
		group.instanceCount = 0;

		for (size_t i = 0; i < group.particles.size(); ) {

			if (group.particles[i].currentTime >= group.particles[i].lifeTime) {
				group.particles[i] = group.particles.back();
				group.particles.pop_back();

				continue;
			}

			auto& p = group.particles[i];

			// 渦運動では後段でXZ座標を円軌道から求め直すため、通常の水平速度加算は行わない。
			if (!p.isVortex) {
				p.transform.translate.x += p.velocity.x * frameScale;
				p.transform.translate.z += p.velocity.z * frameScale;
			}
			// 上昇速度と加速度は渦運動でも共通して適用し、螺旋状の流れを作る。
			p.transform.translate.y += p.velocity.y * frameScale;

			p.velocity.x += p.acceleration.x * frameScale;
			p.velocity.y += p.acceleration.y * frameScale;
			p.velocity.z += p.acceleration.z * frameScale;

			if (p.isVortex) {
				// 角速度は秒単位で更新し、フレームレートが変化しても回転速度を一定に保つ。
				p.vortexAngle += p.vortexAngularSpeed * deltaTime;
				// 現在高度を0～1へ正規化し、上へ進むほど半径が広がる円錐形状にする。
				const float height = p.transform.translate.y - p.vortexCenter.y;
				const float heightRatio = std::clamp(height / p.vortexHeight, 0.0f, 1.0f);
				const float radius =
					(p.vortexBaseRadius + (p.vortexTopRadius - p.vortexBaseRadius) * heightRatio) *
					p.vortexRadiusScale;
				// 極座標からXZ座標へ変換し、Y軸周りの円運動として反映する。
				p.transform.translate.x = p.vortexCenter.x + std::cos(p.vortexAngle) * radius;
				p.transform.translate.z = p.vortexCenter.z + std::sin(p.vortexAngle) * radius;
			}

			const float t = std::clamp(p.currentTime / p.lifeTime, 0.0f, 1.0f);

			p.transform.scale.x = p.startScale.x + (p.endScale.x - p.startScale.x) * t;
			p.transform.scale.y = p.startScale.y + (p.endScale.y - p.startScale.y) * t;
			p.transform.scale.z = p.startScale.z + (p.endScale.z - p.startScale.z) * t;

			p.color.x = p.startColor.x + (p.endColor.x - p.startColor.x) * t;
			p.color.y = p.startColor.y + (p.endColor.y - p.startColor.y) * t;
			p.color.z = p.startColor.z + (p.endColor.z - p.startColor.z) * t;
			p.color.w = p.startColor.w + (p.endColor.w - p.startColor.w) * t;

			p.currentTime += deltaTime;

			++i;
		}
	}
}
/// <summary>
/// パーティクルを発生させます。
/// </summary>
/// <param name="groupName">対象となるパーティクルグループ名を指定します。</param>
/// <param name="position">位置を指定します。</param>
/// <param name="count">処理する個数を指定します。</param>
void ParticleManager::Emit(const std::string& groupName, const Vector3& position, uint32_t count, const ParticleEmitParam& emitParam) {
	const auto groupIt = particleGroups_.find(groupName);
	if (groupIt == particleGroups_.end()) {
		return;
	}

	ParticleGroup& group = groupIt->second;
	// JSONや外部コードから上限を超える生成要求が来ても、GPUバッファの容量を超えないよう制限します。
	const uint32_t currentParticleCount = static_cast<uint32_t>(group.particles.size());
	const uint32_t availableParticleCount =
	    currentParticleCount < kMaxParticle ? kMaxParticle - currentParticleCount : 0;
	const uint32_t emitCount = (std::min)(count, availableParticleCount);

	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

	for (uint32_t i = 0; i < emitCount; ++i) {
		Particle newParticle;

		if (emitParam.isVortex) {
			// 初期角度を全周へ分散し、生成直後から竜巻全体が埋まって見えるようにする。
			const float random01 = (dist(randomEngine_) + 1.0f) * 0.5f;
			newParticle.isVortex = true;
			newParticle.vortexCenter = position;
			newParticle.vortexAngle = random01 * 2.0f * std::numbers::pi_v<float>;
			newParticle.vortexAngularSpeed = emitParam.vortexAngularSpeed;
			// 半径を82～118%へ散らし、薄い一枚の円錐面になるのを防ぐ。
			newParticle.vortexRadiusScale = 0.82f + (dist(randomEngine_) + 1.0f) * 0.18f;
			// 不正な半径や高さが渡されても除算ゼロや上下反転が起きないよう補正する。
			newParticle.vortexBaseRadius = (std::max)(0.0f, emitParam.vortexBaseRadius);
			newParticle.vortexTopRadius = (std::max)(newParticle.vortexBaseRadius, emitParam.vortexTopRadius);
			newParticle.vortexHeight = (std::max)(0.01f, emitParam.vortexHeight);

			// 高さも全域へランダム配置し、エフェクト開始時に下端から埋まる待ち時間をなくす。
			const float initialHeight = (dist(randomEngine_) + 1.0f) * 0.5f * newParticle.vortexHeight;
			const float heightRatio = initialHeight / newParticle.vortexHeight;
			const float radius =
				(newParticle.vortexBaseRadius +
				 (newParticle.vortexTopRadius - newParticle.vortexBaseRadius) * heightRatio) *
				newParticle.vortexRadiusScale;
			newParticle.transform.translate = {
				position.x + std::cos(newParticle.vortexAngle) * radius,
				position.y + initialHeight,
				position.z + std::sin(newParticle.vortexAngle) * radius};
		} else {
			// 通常パーティクルは従来どおり、直方体範囲内へランダム配置する。
			newParticle.transform.translate.x = position.x + dist(randomEngine_) * emitParam.randomPositionRange.x;
			newParticle.transform.translate.y = position.y + dist(randomEngine_) * emitParam.randomPositionRange.y;
			newParticle.transform.translate.z = position.z + dist(randomEngine_) * emitParam.randomPositionRange.z;
		}

		newParticle.transform.scale.x = emitParam.scale.x + dist(randomEngine_) * emitParam.randomScaleRange.x;
		newParticle.transform.scale.y = emitParam.scale.y + dist(randomEngine_) * emitParam.randomScaleRange.y;
		newParticle.transform.scale.z = emitParam.scale.z + dist(randomEngine_) * emitParam.randomScaleRange.z;
		newParticle.startScale = newParticle.transform.scale;
		newParticle.endScale = emitParam.endScale;
		newParticle.velocity.x = emitParam.baseVelocity.x + dist(randomEngine_) * emitParam.randomVelocityRange.x;
		newParticle.velocity.y = emitParam.baseVelocity.y + dist(randomEngine_) * emitParam.randomVelocityRange.y;
		newParticle.velocity.z = emitParam.baseVelocity.z + dist(randomEngine_) * emitParam.randomVelocityRange.z;
		newParticle.acceleration = emitParam.acceleration;

		newParticle.transform.rotate.x = emitParam.baseRotate.x + dist(randomEngine_) * emitParam.randomRotateRange.x;
		newParticle.transform.rotate.y = emitParam.baseRotate.y + dist(randomEngine_) * emitParam.randomRotateRange.y;
		newParticle.transform.rotate.z = emitParam.baseRotate.z + dist(randomEngine_) * emitParam.randomRotateRange.z;

		newParticle.lifeTime = emitParam.lifeTime;
		newParticle.currentTime = 0.0f;

		newParticle.isBillboard = emitParam.isBillboard;

		newParticle.startColor = emitParam.color;
		newParticle.endColor = emitParam.endColor;
		newParticle.color = newParticle.startColor;

		group.particles.push_back(newParticle);
	}
}
/// <param name="groupName">対象となるパーティクルグループ名を指定します。</param>
/// <param name="textureFilePath">使用するテクスチャまたはモデルのファイルパスを指定します。</param>
void ParticleManager::SetGroupTexture(const std::string& groupName, const std::string& textureFilePath) {
	const auto groupIt = particleGroups_.find(groupName);
	if (groupIt == particleGroups_.end()) {
		return;
	}

	ParticleGroup& group = groupIt->second;

	group.material.textureFilePath = textureFilePath;
	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture(textureFilePath);
	group.material.textureIndex = textureManager->GetTextureIndexByFilePath(textureFilePath);
}

/// <param name="groupName">対象となるパーティクルグループ名を指定します。</param>
/// <param name="blendMode">描画時に使用するブレンドモードを指定します。</param>
void ParticleManager::SetGroupBlendMode(const std::string& groupName, BlendMode blendMode) {
	auto it = particleGroups_.find(groupName);
	if (it != particleGroups_.end()) {
		it->second.blendMode = blendMode;
	}
}

bool ParticleManager::HasGroup(const std::string& groupName) const {
	return particleGroups_.contains(groupName);
}

void ParticleManager::SetGroupParticles(const std::string& groupName, std::span<const Particle> particles) {
	auto it = particleGroups_.find(groupName);
	if (it == particleGroups_.end()) {
		return;
	}
	it->second.particles.assign(particles.begin(), particles.end());
}

void ParticleManager::ClearGroupParticles(const std::string& groupName) {
	auto it = particleGroups_.find(groupName);
	if (it != particleGroups_.end()) {
		it->second.particles.clear();
	}
}
