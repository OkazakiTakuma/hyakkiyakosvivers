#pragma once

#include "../../base/GameTime.h"
#include "../../flame/Component.h"
#include "../../flame/GameObject.h"
#include "ParticleManager.h"
#include <array>
#include <cmath>
#include <string>

/// <summary>
/// 1つのGameObjectの周囲へ、カメラを向く2層の加算Haloを表示します。
/// 内側は明るい芯のにじみ、外側は大きく薄い光の広がりを担当します。
/// </summary>
class GlowBillboardComponent final : public Component {
public:
	void Initialize() override {
		// GameObject名を含め、ほかのHaloとパーティクルグループが衝突しないようにする。
		groupName_ = "GlowBillboard_" + (GetOwner() ? GetOwner()->GetName() : std::string("Object"));
		EnsureParticleGroup();
	}

	void Update() override {
		EnsureParticleGroup();
		if (!ParticleManager::GetInstance()->HasGroup(groupName_) || !GetOwner()) {
			return;
		}

		// Emitを繰り返すと寿命の境界で個数や明るさが揺れるため、常に同じ2粒で置き換える。
		std::array<Particle, 2> particles{};

		pulseTime_ += GameTime::GetDeltaTime();
		const float pulse = 1.0f + std::sin(pulseTime_ * pulseSpeed_) * pulseAmount_;
		const Vector3 position = GetOwner()->GetTransform().translate + positionOffset_;

		// 外側を先に登録し、その上へ内側の高輝度な光を加算する。
		UpdateParticle(particles[0], position, outerSize_ * pulse, outerColor_, outerIntensity_);
		UpdateParticle(particles[1], position, innerSize_ * pulse, innerColor_, innerIntensity_);
		ParticleManager::GetInstance()->SetGroupParticles(groupName_, particles);
	}

	void Finalize() override {
		// シーン再読込時に古いHaloが残らないよう、このComponentが所有する粒だけを除去する。
		ParticleManager::GetInstance()->ClearGroupParticles(groupName_);
		groupName_.clear();
	}

	void SetTexture(const std::string& textureFilePath) {
		textureFilePath_ = textureFilePath;
		if (!groupName_.empty() && ParticleManager::GetInstance()->HasGroup(groupName_)) {
			ParticleManager::GetInstance()->SetGroupTexture(groupName_, textureFilePath_);
		}
	}
	const std::string& GetTexture() const { return textureFilePath_; }

	void SetInnerColor(const Vector4& color) { innerColor_ = color; }
	const Vector4& GetInnerColor() const { return innerColor_; }
	void SetOuterColor(const Vector4& color) { outerColor_ = color; }
	const Vector4& GetOuterColor() const { return outerColor_; }

	void SetInnerSize(float size) { innerSize_ = size < 0.01f ? 0.01f : size; }
	float GetInnerSize() const { return innerSize_; }
	void SetOuterSize(float size) { outerSize_ = size < 0.01f ? 0.01f : size; }
	float GetOuterSize() const { return outerSize_; }

	void SetInnerIntensity(float intensity) { innerIntensity_ = intensity < 0.0f ? 0.0f : intensity; }
	float GetInnerIntensity() const { return innerIntensity_; }
	void SetOuterIntensity(float intensity) { outerIntensity_ = intensity < 0.0f ? 0.0f : intensity; }
	float GetOuterIntensity() const { return outerIntensity_; }

	void SetPulseAmount(float amount) { pulseAmount_ = amount < 0.0f ? 0.0f : amount; }
	float GetPulseAmount() const { return pulseAmount_; }
	void SetPulseSpeed(float speed) { pulseSpeed_ = speed < 0.0f ? 0.0f : speed; }
	float GetPulseSpeed() const { return pulseSpeed_; }

	void SetPositionOffset(const Vector3& offset) { positionOffset_ = offset; }
	const Vector3& GetPositionOffset() const { return positionOffset_; }

private:
	void EnsureParticleGroup() {
		if (groupName_.empty()) {
			return;
		}
		ParticleManager* particleManager = ParticleManager::GetInstance();
		if (!particleManager->HasGroup(groupName_)) {
			particleManager->CreateParticleGroup(groupName_, textureFilePath_, kMeshTypeQuad);
		}
		// 黒い部分を背景へ足さず、明るい部分だけを加えることで光のにじみに見せる。
		particleManager->SetGroupBlendMode(groupName_, kBlendModeAdd);
	}

	static void UpdateParticle(
	    Particle& particle,
	    const Vector3& position,
	    float size,
	    const Vector4& baseColor,
	    float intensity
	) {
		particle.transform.translate = position;
		particle.transform.rotate = {0.0f, 0.0f, 0.0f};
		particle.transform.scale = {size, size, 1.0f};
		particle.startScale = particle.transform.scale;
		particle.endScale = particle.transform.scale;
		particle.velocity = {0.0f, 0.0f, 0.0f};
		particle.acceleration = {0.0f, 0.0f, 0.0f};
		particle.isBillboard = true;
		particle.isVortex = false;

		// 加算ブレンドではalphaが寄与量になるため、色のalphaと個別強度をここで合成する。
		particle.color = baseColor;
		particle.color.w *= intensity;
		particle.startColor = particle.color;
		particle.endColor = particle.color;

		// Updateのたびに0へ戻す常駐粒。Component停止時は短時間で自然に消える。
		particle.currentTime = 0.0f;
		particle.lifeTime = 0.25f;
	}

	std::string groupName_;
	std::string textureFilePath_ = "Resources/circle.png";
	Vector4 innerColor_ = {1.0f, 0.62f, 0.20f, 1.0f};
	Vector4 outerColor_ = {1.0f, 0.18f, 0.025f, 1.0f};
	Vector3 positionOffset_ = {0.0f, 0.0f, 0.0f};
	float innerSize_ = 5.2f;
	float outerSize_ = 10.5f;
	float innerIntensity_ = 0.22f;
	float outerIntensity_ = 0.07f;
	float pulseAmount_ = 0.025f;
	float pulseSpeed_ = 0.75f;
	float pulseTime_ = 0.0f;
};
