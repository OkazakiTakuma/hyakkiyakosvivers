#pragma once
#include "../flame/Component.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include <memory>
#include <string>

class SpriteComponent : public Component {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	void Initialize() override {
		sprite_ = std::make_unique<Sprite>();
	}

	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	/// <param name="textureFilePath">使用するテクスチャまたはモデルのファイルパスを指定します。</param>
	void Initialize(const std::string& textureFilePath) {
		if (!sprite_) {
			sprite_ = std::make_unique<Sprite>();
		}
		sprite_->Initialize(textureFilePath);
	}

	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update() override {
		if (sprite_) {
			SyncOwnerTransformToSprite();
			sprite_->Update();
		}
	}

	/// <summary>
	/// 2D 要素の描画処理を行います。
	/// </summary>
	void Draw2D() override {
		if (sprite_) {
			SpriteCommon::GetInstance()->SetDraw(kBlendModeNone);
			sprite_->Draw();
		}
	}

	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize() override {
		sprite_.reset();
	}

	Sprite* GetSprite() { return sprite_.get(); }
	const Sprite* GetSprite() const { return sprite_.get(); }
	void SetTexture(const std::string& textureFilePath) {
		if (sprite_) {
			sprite_->SetTexture(textureFilePath);
		}
	}
	std::string GetTextureFilePath() const { return sprite_ ? sprite_->GetTextureFilePath() : std::string(); }

	const EulerTransform& GetTransform() { return sprite_->GetTransform(); }
	void SetTransform(const EulerTransform& transform) { sprite_->SetTransform(transform); }
	const EulerTransform& GetUVTransform() { return sprite_->GetUVTransform(); }
	void SetUVTransform(const EulerTransform& uvTransform) { sprite_->SetUVTransform(uvTransform); }
	const Vector4& GetColor() const { return sprite_->GetColor(); }
	void SetColor(const Vector4& color) { sprite_->SetColor(color); }
	const Vector2 GetSize() const { return sprite_->GetSize(); }
	void SetSize(const Vector2& size) { sprite_->SetSize(size); }
	const Vector2& GetAnchorPoint() const { return sprite_->GetAnchorPoint(); }
	void SetAnchorPoint(const Vector2& anchorPoint) { sprite_->SetAnchorPoint(anchorPoint); }
	const bool GetIsFlipX() const { return sprite_->GetIsFlipX(); }
	void SetIsFlipX(bool flipX) { sprite_->SetIsFlipX(flipX); }
	const bool GetIsFlipY() const { return sprite_->GetIsFlipY(); }
	void SetIsFlipY(bool flipY) { sprite_->SetIsFlipY(flipY); }
	const Vector2& GetTextureLeftTop() const { return sprite_->GetTextureLeftTop(); }
	void SetTextureLeftTop(const Vector2& leftTop) { sprite_->SetTextureLeftTop(leftTop); }
	const Vector2& GetTextureSize() const { return sprite_->GetTextureSize(); }
	void SetTextureSize(const Vector2& size) { sprite_->SetTextureSize(size); }

private:
	void SyncOwnerTransformToSprite() {
		if (!sprite_ || GetOwner() == nullptr) {
			return;
		}
		sprite_->SetTransform(GetOwner()->GetTransform());
	}

	std::unique_ptr<Sprite> sprite_;
};
