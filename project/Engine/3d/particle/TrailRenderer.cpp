#include "TrailRenderer.h"
#include "PipelineStateUtility.h"

#include "Logger.h"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace {
constexpr float kEpsilon = 0.00001f;

Vector4 LerpColor(const Vector4& from, const Vector4& to, float t) {
	// 軌跡上の位置と寿命に応じた色を成分ごとに補間する。
	t = (std::clamp)(t, 0.0f, 1.0f);
	return {
	    from.x + (to.x - from.x) * t,
	    from.y + (to.y - from.y) * t,
	    from.z + (to.z - from.z) * t,
	    from.w + (to.w - from.w) * t
	};
}
}

TrailRenderer* TrailRenderer::GetInstance() {
	static TrailRenderer instance;
	return &instance;
}

void TrailRenderer::Initialize(DirectXCommon* dxCommon) {
	// 最大頂点数分の動的バッファと、描画に必要なパイプラインを一度だけ作成する。
	assert(dxCommon);
	dxCommon_ = dxCommon;
	CreateRootSignature();
	CreatePipelineState();

	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * kMaxVertexCount);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(Vertex) * kMaxVertexCount;
	vertexBufferView_.StrideInBytes = sizeof(Vertex);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	cameraResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
}

void TrailRenderer::Finalize() {
	requests_.clear();
	if (vertexResource_ && vertexData_) {
		vertexResource_->Unmap(0, nullptr);
	}
	if (cameraResource_ && cameraData_) {
		cameraResource_->Unmap(0, nullptr);
	}
	vertexData_ = nullptr;
	cameraData_ = nullptr;
	vertexResource_.Reset();
	cameraResource_.Reset();
	pipelineState_.Reset();
	rootSignature_.Reset();
	dxCommon_ = nullptr;
}

void TrailRenderer::Submit(
    const std::vector<TrailRenderPoint>& points,
    float width,
    const Vector4& headColor,
    const Vector4& tailColor
) {
	// 頂点化はカメラ位置が確定するDrawまで遅延し、要求だけを保存する。
	if (points.size() < 2 || width <= 0.0f) {
		return;
	}
	requests_.push_back({points, width, headColor, tailColor});
}

void TrailRenderer::Draw(Camera* camera) {
	// 全要求を1つの頂点バッファへまとめ、描画コールを1回に集約する。
	if (!camera || requests_.empty() || !dxCommon_) {
		requests_.clear();
		return;
	}

	vertexCount_ = 0;
	for (const Request& request : requests_) {
		AppendRequestVertices(request, camera->GetTranslate());
	}
	requests_.clear();
	if (vertexCount_ == 0) {
		return;
	}

	*cameraData_ = camera->GetViewProjectionMatrix();
	auto commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, cameraResource_->GetGPUVirtualAddress());
	commandList->DrawInstanced(vertexCount_, 1, 0, 0);
}

void TrailRenderer::AppendRequestVertices(const Request& request, const Vector3& cameraPosition) {
	// 各線分とカメラ方向の外積から帯の左右頂点を求める。
	const size_t pointCount = request.points.size();
	std::vector<Vector3> sides(pointCount);
	std::vector<Vertex> left(pointCount);
	std::vector<Vertex> right(pointCount);

	for (size_t i = 0; i < pointCount; ++i) {
		const Vector3 previous = request.points[i == 0 ? i : i - 1].position;
		const Vector3 next = request.points[i + 1 < pointCount ? i + 1 : i].position;
		Vector3 tangent = next - previous;
		if (Length(tangent) <= kEpsilon) {
			continue;
		}
		tangent = NormalizeReturnVector(tangent);
		Vector3 viewDirection = cameraPosition - request.points[i].position;
		// 進行方向と視線方向の両方に直交する向きを帯の横方向として使う。
		Vector3 side = Cross(tangent, viewDirection);
		// カメラが軌跡の延長線上にある場合は外積が0になるため、固定軸で退避方向を作る。
		if (Length(side) <= kEpsilon) {
			side = Cross(tangent, {0.0f, 1.0f, 0.0f});
		}
		if (Length(side) <= kEpsilon) {
			side = Cross(tangent, {1.0f, 0.0f, 0.0f});
		}
		sides[i] = NormalizeReturnVector(side);
	}

	for (size_t i = 0; i < pointCount; ++i) {
		if (Length(sides[i]) <= kEpsilon) {
			sides[i] = i > 0 ? sides[i - 1] : Vector3{1.0f, 0.0f, 0.0f};
		}
		const float lifeRate = (std::clamp)(request.points[i].lifeRate, 0.0f, 1.0f);
		// 古い点ほど幅とアルファを小さくし、末端が細く消える形状にする。
		const float halfWidth = request.width * 0.5f * lifeRate;
		Vector4 color = LerpColor(request.tailColor, request.headColor, lifeRate);
		// tail/head のアルファ補間ですでに寿命に応じて減衰しているため、lifeRateを重ねて
		// 掛けない。二重減衰を避け、明るい背景でも中間部分の色と輪郭を残す。
		left[i] = {request.points[i].position + halfWidth * sides[i], color};
		right[i] = {request.points[i].position + (-halfWidth) * sides[i], color};
	}

	for (size_t i = 0; i + 1 < pointCount; ++i) {
		// 1区間は left/right の4点を共有する2三角形（6頂点）として出力する。
		if (vertexCount_ + 6 > kMaxVertexCount) {
			return;
		}
		vertexData_[vertexCount_++] = left[i];
		vertexData_[vertexCount_++] = right[i];
		vertexData_[vertexCount_++] = left[i + 1];
		vertexData_[vertexCount_++] = left[i + 1];
		vertexData_[vertexCount_++] = right[i];
		vertexData_[vertexCount_++] = right[i + 1];
	}
}

void TrailRenderer::CreateRootSignature() {
	// 軌跡描画ではカメラ行列だけをルート定数バッファとして公開する。
	D3D12_ROOT_PARAMETER rootParameter{};
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameter.Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = &rootParameter;
	desc.NumParameters = 1;

	rootSignature_ = PipelineStateUtility::CreateRootSignature(dxCommon_->GetDevice().Get(), desc);
}

void TrailRenderer::CreatePipelineState() {
	// 半透明の帯を前面から重ねるためのブレンド・深度設定を構築する。
	D3D12_INPUT_ELEMENT_DESC inputElements[2]{};
	inputElements[0].SemanticName = "POSITION";
	inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElements[1].SemanticName = "COLOR";
	inputElements[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	// 発光する軌跡として背景へ光を重ねる。中間部のアルファを二重減衰させないことで、
	// 加算合成の見栄えを維持しながら明所でも帯の長さを読み取りやすくする。
	const D3D12_BLEND_DESC blendDesc = PipelineStateUtility::MakeBlendDesc(
	    TRUE, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE);
	const D3D12_RASTERIZER_DESC rasterizerDesc =
	    PipelineStateUtility::MakeRasterizerDesc(D3D12_CULL_MODE_NONE, TRUE);
	// 既に描画済みの3Dオブジェクトより奥にあるトレイル画素は深度テストで破棄する。
	// 深度への書き込みは行わないため、トレイル同士や後続の描画へ深度を残さない。
	const D3D12_DEPTH_STENCIL_DESC depthDesc =
	    PipelineStateUtility::MakeDepthStencilDesc(TRUE, D3D12_DEPTH_WRITE_MASK_ZERO);

	auto vertexShader = dxCommon_->CompileShader(L"Resources/Shader/Line.VS.hlsl", L"vs_6_0");
	auto pixelShader = dxCommon_->CompileShader(L"Resources/Shader/Line.PS.hlsl", L"ps_6_0");
	assert(vertexShader && pixelShader);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.InputLayout = {inputElements, _countof(inputElements)};
	desc.BlendState = blendDesc;
	desc.RasterizerState = rasterizerDesc;
	desc.DepthStencilState = depthDesc;
	desc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
	desc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}
