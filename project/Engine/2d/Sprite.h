#pragma once
#include "Matrix.h"
#include "Vector.h"
#include "struct.h"
#include <Windows.h>
#include <d3d12.h>
#include <string>
#include <wrl.h>

class Sprite {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	/// <param name="textureFilePath">使用するテクスチャまたはモデルのファイルパスを指定します。</param>
	void Initialize(const std::string& textureFilePath);
	/// <summary>
	/// 使用するテクスチャを変更します。
	/// </summary>
	/// <param name="textureFilePath">使用するテクスチャのファイルパスを指定します。</param>
	void SetTexture(const std::string& textureFilePath);
	const std::string& GetTextureFilePath() const { return filepath; }
	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update();
	/// <summary>
	/// 現在の状態をもとに描画処理を行います。
	/// </summary>
	void Draw();

	const EulerTransform& GetTransform() { return transform; };
	void SetTransform(const EulerTransform& newTransform) { transform = newTransform; }
	const EulerTransform& GetUVTransform() { return uvTransform; }
	void SetUVTransform(const EulerTransform& newUVTransform) { uvTransform = newUVTransform; }
	const Vector4& GetColor() const { return materialData->color; }
	void SetColor(const Vector4& newColor) { materialData->color = newColor; }
	const Vector2 GetSize() const { return size; }
	void SetSize(const Vector2& newSize) { size = newSize; }
	const Vector2& GetAnchorPoint() const { return anchorPoint; }
	void SetAnchorPoint(const Vector2& newAnchorPoint) { anchorPoint = newAnchorPoint; }
	const bool GetIsFlipX() const { return isFlipX; }
	void SetIsFlipX(bool flipX) { isFlipX = flipX; }
	const bool GetIsFlipY() const { return isFlipY; }
	void SetIsFlipY(bool flipY) { isFlipY = flipY; }
	const Vector2& GetTextureLeftTop() const { return textureLeftTop; }
	void SetTextureLeftTop(const Vector2& leftTop) { textureLeftTop = leftTop; }
	const Vector2& GetTextureSize() const { return textureSize; }
	void SetTextureSize(const Vector2& size) { textureSize = size; }

	/// <summary>
	/// 破棄時に必要な解放処理を行います。
	/// </summary>
	~Sprite();

private:
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	

	std::string filepath;
	uint32_t* indexData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	VertexData* vertexData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferview{};
	Material* materialData = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;

	EulerTransform transform{
	    {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };
	EulerTransform uvTransform{
	    {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };


	TransformationMatrix* transformationMatrixData = nullptr;
	Vector2 size = {0, 0};
	uint32_t textureIndex = 0;
	Vector2 anchorPoint = {0.0f, 0.0f};
	bool isFlipX = false;
	bool isFlipY = false;
	Vector2 textureLeftTop = {0.0f, 0.0f};
	Vector2 textureSize = {512.0f, 512.0f};

	void AdjustTextureSize();
};
