#pragma once
#include "../../flame/Component.h"
#include "../../flame/GameObject.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include <memory>

class Object3dComponent : public Component {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	void Initialize() override {
		object3d_ = std::make_unique<Object3d>();
		object3d_->Initialize();
		SyncOwnerTransformToObject();
	}

	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update() override {
		if (!object3d_) {
			return;
		}
		SyncOwnerTransformToObject();
		if (object3d_->GetIsPointLightSet()) {
			object3d_->SetPointLight(
			    object3d_->GetPointLightColor(),
			    GetOwner()->GetTransform().translate,
			    object3d_->GetPointLightIntensity(),
			    object3d_->GetPointLightRadius(),
			    object3d_->GetPointLightDecay()
			);
		}
		object3d_->Update();
	}

	/// <summary>
	/// 3D 要素の描画処理を行います。
	/// </summary>
	void Draw3D() override {
		if (object3d_) {
			Object3dCommon::GetInstance()->SetDraw();
			object3d_->Draw();
		}
	}
	/// <summary>所有する3Dオブジェクトを影生成パスへ描画します。</summary>
	void DrawShadowMap() { if (object3d_) object3d_->DrawShadowMap(); }

	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize() override {
		object3d_.reset();
	}

	Object3d* GetObject3d() { return object3d_.get(); }
	const Object3d* GetObject3d() const { return object3d_.get(); }

	void SetModel(Model* model) { object3d_->SetModel(model); }
	void SetModel(const std::string& filePath) { object3d_->SetModel(filePath); }
	/// <summary>
	/// Cylinder を作成し、利用できる状態にします。
	/// </summary>
	/// <param name="radius">半径を指定します。</param>
	/// <param name="height">高さを指定します。</param>
	void CreateCylinder(float radius = 1.0f, float height = 2.0f, uint32_t subdivision = 16, bool createTopCap = true, bool createBottomCap = true) {
		object3d_->CreateCylinder(radius, height, subdivision, createTopCap, createBottomCap);
	}
	void SetTexture(const std::string& textureFilePath) { object3d_->SetTexture(textureFilePath); }
	void SetModelTexture(const std::string& textureFilePath) { object3d_->SetModelTexture(textureFilePath); }
	void SetModelTextureOverride(const std::string& textureFilePath) { object3d_->SetModelTextureOverride(textureFilePath); }
	std::string GetModelTextureFilePath() const { return object3d_ ? object3d_->GetModelTextureFilePath() : std::string(); }

	const Vector3& GetTranslate() { return GetOwner()->GetTransform().translate; }
	/// <param name="translate">位置を指定します。</param>
	void SetTranslate(const Vector3& translate) {
		GetOwner()->GetTransform().translate = translate;
		SyncOwnerTransformToObject();
	}
	const Vector3& GetRotate() { return GetOwner()->GetTransform().rotate; }
	/// <param name="rotate">回転量を指定します。</param>
	void SetRotate(const Vector3& rotate) {
		GetOwner()->GetTransform().rotate = rotate;
		SyncOwnerTransformToObject();
	}
	const Vector3& GetScale() { return GetOwner()->GetTransform().scale; }
	/// <param name="scale">拡大率を指定します。</param>
	void SetScale(const Vector3& scale) {
		GetOwner()->GetTransform().scale = scale;
		SyncOwnerTransformToObject();
	}

	void SetCamera(Camera* camera) { object3d_->SetCamera(camera); }
	/// <param name="color">色を指定します。</param>
	/// <param name="intensity">強度を指定します。</param>
	void SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity) {
		object3d_->SetDirectionalLight(color, direction, intensity);
	}
	/// <param name="color">色を指定します。</param>
	/// <param name="position">位置を指定します。</param>
	/// <param name="intensity">強度を指定します。</param>
	/// <param name="radius">半径を指定します。</param>
	void SetPointLight(const Vector4& color, const Vector3& position, float intensity, float radius, float decay) {
		object3d_->SetPointLight(color, position, intensity, radius, decay);
	}
	void SetEnvironmentMultiplier(float multiplier) { object3d_->SetEnvironmentMultiplier(multiplier); }
	void SetColor(const Vector4& color) { if (object3d_) object3d_->SetColor(color); }
	/// <summary>ライトの影響を受けない自己発光色と強度を設定します。</summary>
	void SetEmission(const Vector3& color, float intensity) {
		if (object3d_) object3d_->SetEmission(color, intensity);
	}
	/// <summary>演出用に指定ボーンの回転オフセットを設定します。</summary>
	void SetJointRotationOffset(const std::string& jointName, const Quaternion& rotationOffset) {
		if (object3d_) object3d_->SetJointRotationOffset(jointName, rotationOffset);
	}
	void ClearJointRotationOffsets() {
		if (object3d_) object3d_->ClearJointRotationOffsets();
	}
	std::vector<std::string> GetJointNames() const {
		return object3d_ ? object3d_->GetJointNames() : std::vector<std::string>{};
	}
	void SetJointRotationOffsetEuler(const std::string& jointName, const Vector3& rotation) {
		if (object3d_) object3d_->SetJointRotationOffsetEuler(jointName, rotation);
	}
	Vector3 GetJointRotationOffsetEuler(const std::string& jointName) const {
		return object3d_ ? object3d_->GetJointRotationOffsetEuler(jointName) : Vector3{};
	}
	Vector3 GetEmissionColor() const {
		return object3d_ ? object3d_->GetEmissionColor() : Vector3{0.0f, 0.0f, 0.0f};
	}
	float GetEmissionIntensity() const { return object3d_ ? object3d_->GetEmissionIntensity() : 0.0f; }
	void SetLightingEnabled(bool enabled) { if (object3d_) object3d_->SetLightingEnabled(enabled); }
	bool GetLightingEnabled() const { return object3d_ && object3d_->GetLightingEnabled(); }
	void SetShadowEnabled(bool enabled) { if (object3d_) object3d_->SetShadowEnabled(enabled); }
	bool GetShadowEnabled() const { return object3d_ && object3d_->GetShadowEnabled(); }
	Vector4 GetColor() const { return object3d_ ? object3d_->GetColor() : Vector4{1.0f, 1.0f, 1.0f, 1.0f}; }
	void IsPointLightSet(bool isSet) { object3d_->IsPointLightSet(isSet); }
	void SetEnvironmentMap(const std::string& textureFilePath) { object3d_->SetEnvironmentMap(textureFilePath); }
	void SetDrawSkeleton(bool isDraw) { object3d_->SetDrawSkeleton(isDraw); }
	bool GetDrawSkeleton() const { return object3d_->GetDrawSkeleton(); }
	bool HasSkeleton() const { return object3d_ && object3d_->HasSkeleton(); }
	bool HasModel() const { return object3d_ && object3d_->HasModel(); }
	bool HasAnimation() const { return object3d_ && object3d_->HasAnimation(); }
	void SetAnimationPlaying(bool isPlaying) { object3d_->SetAnimationPlaying(isPlaying); }
	bool GetAnimationPlaying() const { return object3d_ && object3d_->GetAnimationPlaying(); }
	void RestartAnimation() { object3d_->RestartAnimation(); }
	void ResetAnimationPoseToInitial() { object3d_->ResetAnimationPoseToInitial(); }
	bool SetAnimation(const std::string& animationName, bool restart = true) { return object3d_ && object3d_->SetAnimation(animationName, restart); }
	const std::string& GetAnimationName() const {
		static const std::string emptyName;
		return object3d_ ? object3d_->GetAnimationName() : emptyName;
	}
	const std::vector<std::string>& GetAnimationNames() const {
		static const std::vector<std::string> emptyNames;
		return object3d_ ? object3d_->GetAnimationNames() : emptyNames;
	}
	float GetAnimationTime() const { return object3d_ ? object3d_->GetAnimationTime() : 0.0f; }
	float GetAnimationDuration() const { return object3d_ ? object3d_->GetAnimationDuration() : 0.0f; }

	const Vector4 GetLightColor() const { return object3d_->GetLightColor(); }
	const Vector3 GetLightDirection() const { return object3d_->GetLightDirection(); }
	const float GetLightIntensity() const { return object3d_->GetLightIntensity(); }
	const Vector4 GetPointLightColor() const { return object3d_->GetPointLightColor(); }
	const Vector3 GetPointLightPosition() const { return object3d_->GetPointLightPosition(); }
	const float GetPointLightIntensity() const { return object3d_->GetPointLightIntensity(); }
	const float GetPointLightRadius() const { return object3d_->GetPointLightRadius(); }
	const float GetPointLightDecay() const { return object3d_->GetPointLightDecay(); }
	const bool GetIsPointLightSet() const { return object3d_->GetIsPointLightSet(); }
	const float GetEnvironmentMultiplier() const { return object3d_->GetEnvironmentMultiplier(); }

private:
	void SyncOwnerTransformToObject() {
		if (!object3d_ || GetOwner() == nullptr) {
			return;
		}

		const EulerTransform& transform = GetOwner()->GetTransform();
		object3d_->SetScale(transform.scale);
		object3d_->SetRotate(transform.rotate);
		object3d_->SetTranslate(transform.translate);
	}

	std::unique_ptr<Object3d> object3d_;
};
