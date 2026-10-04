#pragma once
#include "../../flame/Component.h"
#include "../../flame/GameObject.h"
#include "Camera.h"
#include <memory>
#include <string>

class CameraComponent : public Component {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	void Initialize() override {
		camera_ = std::make_unique<Camera>();
		SyncOwnerTransformToCamera();
	}

	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update() override {
		if (!camera_) {
			return;
		}

		if (followTarget_) {
			EulerTransform& ownerTransform = GetOwner()->GetTransform();
			const EulerTransform& targetTransform = followTarget_->GetTransform();
			ownerTransform.translate = targetTransform.translate + followOffset_;
			ownerTransform.rotate = useOverrideRotation_ ? overrideRotation_ : targetTransform.rotate;
		}

		SyncOwnerTransformToCamera();
		camera_->Update();
	}

	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize() override {
		camera_.reset();
		followTarget_ = nullptr;
	}

	Camera* GetCamera() { return camera_.get(); }
	const Camera* GetCamera() const { return camera_.get(); }

	void SetFollowTarget(GameObject* target) {
		followTarget_ = target;
		followTargetName_ = target ? target->GetName() : "";
	}
	GameObject* GetFollowTarget() const { return followTarget_; }
	const std::string& GetFollowTargetName() const { return followTargetName_; }
	void SetFollowTargetName(const std::string& name) { followTargetName_ = name; }
	void SetFollowOffset(const Vector3& offset) { followOffset_ = offset; }
	const Vector3& GetFollowOffset() const { return followOffset_; }
	void SetLocalOffset(const Vector3& offset) { localOffset_ = offset; }
	const Vector3& GetLocalOffset() const { return localOffset_; }
	void SetOverrideRotationEnabled(bool isEnabled) { useOverrideRotation_ = isEnabled; }
	bool GetOverrideRotationEnabled() const { return useOverrideRotation_; }
	void SetOverrideRotation(const Vector3& rotate) { overrideRotation_ = rotate; }
	const Vector3& GetOverrideRotation() const { return overrideRotation_; }

	float GetFovY() const { return camera_ ? camera_->GetFovY() : 0.45f; }
	void SetFovY(float fovY) {
		if (camera_) {
			camera_->SetfovY(fovY);
		}
	}
	float GetNearClip() const { return camera_ ? camera_->GetNearClip() : 0.1f; }
	void SetNearClip(float nearClip) {
		if (camera_) {
			camera_->SetNearClip(nearClip);
		}
	}
	float GetFarClip() const { return camera_ ? camera_->GetFarClip() : 100.0f; }
	void SetFarClip(float farClip) {
		if (camera_) {
			camera_->SetFarClip(farClip);
		}
	}

private:
	void SyncOwnerTransformToCamera() {
		if (!camera_ || GetOwner() == nullptr) {
			return;
		}

		const EulerTransform& transform = GetOwner()->GetTransform();
		camera_->SetTranslate(transform.translate + localOffset_);
		camera_->SetRotate(useOverrideRotation_ ? overrideRotation_ : transform.rotate);
	}

	std::unique_ptr<Camera> camera_;
	GameObject* followTarget_ = nullptr;
	std::string followTargetName_;
	Vector3 followOffset_{0.0f, 0.0f, 0.0f};
	Vector3 localOffset_{0.0f, 0.0f, 0.0f};
	Vector3 overrideRotation_{0.0f, 0.0f, 0.0f};
	bool useOverrideRotation_ = false;
};
