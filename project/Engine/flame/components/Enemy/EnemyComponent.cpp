#include "components/Enemy/EnemyComponent.h"
#include "object/Object3dComponent.h"
#include "particle/ParticleManager.h"
#include "collision/OBBColliderComponent.h"
#include "collision/SphereColliderComponent.h"
#include "PostEffect.h"

namespace {

EnemyBehaviorPresentation MakeColoredPresentation(const char* modelFilePath, const Vector4& color) {
	EnemyBehaviorPresentation result;
	result.modelFilePath = modelFilePath;
	result.color = color;
	result.overrideColor = true;
	return result;
}

/// <summary>プレイヤーの実コライダー中心へ向く射撃方向を返します。</summary>
Vector3 MakeProjectileDirection(
	const Vector3& shotPosition, Entity* target, const Vector3& fallbackDirection) {
	if (!target) {
		return fallbackDirection;
	}

	Vector3 targetPosition = target->GetTransform().translate + Vector3{0.0f, 0.35f, 0.0f};
	if (const OBBColliderComponent* collider = target->GetComponent<OBBColliderComponent>();
	    collider && collider->IsEnabled()) {
		targetPosition = collider->GetWorldOBB().center;
	} else if (const SphereColliderComponent* collider = target->GetComponent<SphereColliderComponent>();
	           collider && collider->IsEnabled()) {
		targetPosition = collider->GetWorldSphere().center;
	}

	const Vector3 toTarget = targetPosition - shotPosition;
	return Length(toTarget) > MathConstants::kDirectionEpsilon
		? Normalize(toTarget)
		: fallbackDirection;
}

/// <summary>頭上カウントダウン用の数字を、カメラ側を向く7セグメント線として描画します。</summary>
void DrawCountdownDigit(int digit, const Vector3& center, const Vector3& right, float scale, const Vector4& color) {
	static constexpr unsigned char kDigitMasks[10] = {
		0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
	static constexpr float kSegments[7][4] = {
		{-0.5f, 1.0f, 0.5f, 1.0f}, {0.5f, 1.0f, 0.5f, 0.0f},
		{0.5f, 0.0f, 0.5f, -1.0f}, {-0.5f, -1.0f, 0.5f, -1.0f},
		{-0.5f, 0.0f, -0.5f, -1.0f}, {-0.5f, 1.0f, -0.5f, 0.0f},
		{-0.5f, 0.0f, 0.5f, 0.0f}};
	if (digit < 0 || digit > 9) {
		return;
	}
	const Vector3 up{0.0f, 1.0f, 0.0f};
	for (int segment = 0; segment < 7; ++segment) {
		if ((kDigitMasks[digit] & (1u << segment)) == 0) {
			continue;
		}
		const float* points = kSegments[segment];
		const Vector3 start = center + scale * (points[0] * right + points[1] * up);
		const Vector3 end = center + scale * (points[2] * right + points[3] * up);
		// 線を3本重ね、背景が明るい場所でも残り時間を読み取れる太さにする。
		const Vector3 thickness = 0.025f * scale * right;
		LineDrawer::GetInstance()->DrawLine(start - thickness, end - thickness, color, true);
		LineDrawer::GetInstance()->DrawLine(start, end, color, true);
		LineDrawer::GetInstance()->DrawLine(start + thickness, end + thickness, color, true);
	}
}

void DrawCountdownNumber(int value, const Vector3& center, const Vector3& right, float scale, const Vector4& color) {
	value = (std::clamp)(value, 0, 99);
	if (value < 10) {
		DrawCountdownDigit(value, center, right, scale, color);
		return;
	}
	DrawCountdownDigit(value / 10, center - 0.65f * scale * right, right, scale, color);
	DrawCountdownDigit(value % 10, center + 0.65f * scale * right, right, scale, color);
}

}

/// <summary>
/// Stateへ渡す、そのフレームの夜叉ボス情報です。
/// EnemyComponent自体を公開せず、状態判断に必要な値だけへ依存を限定します。
/// </summary>
struct NightSlashStateContext {
	const EnemyStats& stats;
	EulerTransform& transform;
	const Vector3& direction;
	float distance = 0.0f;
	float frameScale = 0.0f;
	float stateTimer = 0.0f;
	Vector3 dashDirection{};
	int comboDashIndex = 0;
	int bossPatternIndex = 0;
	int rangedWaveIndex = 0;
};

/// <summary>StateがComponentへ返す状態値と、遷移・攻撃の要求です。</summary>
struct NightSlashStateUpdate {
	bool hasTransition = false;
	NightSlashState nextState = NightSlashState::Approach;
	bool beginDash = false;
	bool emitRangedWave = false;
	bool resetStateTimer = false;
	int comboDashIndex = 0;
	int bossPatternIndex = 0;
	int rangedWaveIndex = 0;
};

NightSlashStateUpdate MakeNightSlashUpdate(const NightSlashStateContext& context) {
	NightSlashStateUpdate update;
	update.comboDashIndex = context.comboDashIndex;
	update.bossPatternIndex = context.bossPatternIndex;
	update.rangedWaveIndex = context.rangedWaveIndex;
	return update;
}

/// <summary>夜叉ボスの1状態が移動と次の操作要求を計算します。</summary>
class INightSlashState {
public:
	virtual ~INightSlashState() = default;
	virtual NightSlashStateUpdate Update(const NightSlashStateContext& context) const = 0;
};

class NightSlashApproachState final : public INightSlashState {
public:
	NightSlashStateUpdate Update(const NightSlashStateContext& context) const override {
		NightSlashStateUpdate update = MakeNightSlashUpdate(context);
		// 攻撃範囲までは追跡し、現在の攻撃パターンに対応する予兆Stateへ遷移する。
		if (context.distance > context.stats.comboTriggerDistance) {
			context.transform.translate = context.transform.translate +
				(context.stats.speed * context.frameScale) * context.direction;
			return update;
		}
		update.comboDashIndex = 0;
		update.rangedWaveIndex = 0;
		update.hasTransition = true;
		update.nextState = context.bossPatternIndex == 0
			? NightSlashState::Windup
			: NightSlashState::RangedWindup;
		return update;
	}
};

class NightSlashWindupState final : public INightSlashState {
public:
	NightSlashStateUpdate Update(const NightSlashStateContext& context) const override {
		NightSlashStateUpdate update = MakeNightSlashUpdate(context);
		// 予兆完了時点の標的位置を使い、回避可能な固定ダッシュ方向を決める。
		if (context.stateTimer >= context.stats.comboWindup) {
			update.beginDash = true;
		}
		return update;
	}
};

class NightSlashDashingState final : public INightSlashState {
public:
	NightSlashStateUpdate Update(const NightSlashStateContext& context) const override {
		NightSlashStateUpdate update = MakeNightSlashUpdate(context);
		const bool isFinisher = context.comboDashIndex + 1 >=
			(std::max)(1, context.stats.comboDashCount);
		const float speedMultiplier = isFinisher ? context.stats.finisherSpeedMultiplier : 1.0f;
		context.transform.translate = context.transform.translate +
			(context.stats.comboDashSpeed * speedMultiplier * context.frameScale) * context.dashDirection;
		context.transform.rotate.y = std::atan2(context.dashDirection.x, context.dashDirection.z);
		if (context.stateTimer >= context.stats.comboDashDuration) {
			update.hasTransition = true;
			update.nextState = NightSlashState::Slashing;
		}
		return update;
	}
};

class NightSlashSlashingState final : public INightSlashState {
public:
	NightSlashStateUpdate Update(const NightSlashStateContext& context) const override {
		NightSlashStateUpdate update = MakeNightSlashUpdate(context);
		if (context.stateTimer < context.stats.comboSlashPause) {
			return update;
		}
		++update.comboDashIndex;
		if (update.comboDashIndex >= (std::max)(1, context.stats.comboDashCount)) {
			update.hasTransition = true;
			update.nextState = NightSlashState::Recovering;
		} else {
			// 次の斬撃ごとに標的位置を取り直し、左右交互のダッシュを開始する。
			update.beginDash = true;
		}
		return update;
	}
};

class NightSlashRangedWindupState final : public INightSlashState {
public:
	NightSlashStateUpdate Update(const NightSlashStateContext& context) const override {
		NightSlashStateUpdate update = MakeNightSlashUpdate(context);
		if (context.stateTimer < context.stats.bossRangedWindup) {
			return update;
		}
		update.rangedWaveIndex = 0;
		update.hasTransition = true;
		update.nextState = NightSlashState::RangedFiring;
		// 1ウェーブ目は射撃Stateへ入った瞬間に発射する。
		update.emitRangedWave = true;
		return update;
	}
};

class NightSlashRangedFiringState final : public INightSlashState {
public:
	NightSlashStateUpdate Update(const NightSlashStateContext& context) const override {
		NightSlashStateUpdate update = MakeNightSlashUpdate(context);
		if (context.stateTimer < context.stats.bossRangedInterval) {
			return update;
		}
		++update.rangedWaveIndex;
		if (update.rangedWaveIndex >= (std::max)(1, context.stats.bossRangedWaves)) {
			update.hasTransition = true;
			update.nextState = NightSlashState::Recovering;
			return update;
		}
		// 同じState内で次のウェーブを待つため、経過時間だけをリセットする。
		update.resetStateTimer = true;
		update.emitRangedWave = true;
		return update;
	}
};

class NightSlashRecoveringState final : public INightSlashState {
public:
	NightSlashStateUpdate Update(const NightSlashStateContext& context) const override {
		NightSlashStateUpdate update = MakeNightSlashUpdate(context);
		if (context.stateTimer < context.stats.comboRecovery) {
			return update;
		}
		// 0=連続斬り、1=全方位弾幕、2=扇状連射を固定順で循環する。
		update.bossPatternIndex = (context.bossPatternIndex + 1) % 3;
		update.hasTransition = true;
		update.nextState = NightSlashState::Approach;
		return update;
	}
};

void EnemyComponent::Initialize() {
	currentHealth_ = stats_.health;
	deathEffectTimer_ = 0.0f;
	deathEffectStarted_ = false;
	usesAuthoredDeathAnimation_ = false;
	bossDefeatPresentationTimer_ = 0.0f;
	knockbackElapsedSeconds_ = 0.0f;
	knockbackActive_ = false;
}

void EnemyComponent::Update() {
	damageFlashTimer_ = (std::max)(0.0f, damageFlashTimer_ - GameTime::GetDeltaTime());

	// 敵タイプは実行中に頻繁に変わらないため、薄いStrategyを介さず直接ディスパッチする。
	// 複雑な時間遷移を持つ夜叉ボスだけは、この先でState Patternへ委譲する。
	Entity* owner = GetOwner();
	if (!owner) {
		return;
	}

	EulerTransform& transform = owner->GetTransform();
	if (currentHealth_ <= 0.0f) {
		if (!deathEffectStarted_) {
			BeginDeathEffect(transform);
		}
		UpdateDeathEffect(transform);
		return;
	}
	if (knockbackActive_) {
		UpdateKnockback(transform, GameTime::GetDeltaTime());
		// 補間中は追跡や突進による移動を止め、ノックバックの軌道を維持する。
		return;
	}
	if (!target_) {
		return;
	}
	const Vector3 targetPosition = target_->GetTransform().translate;
	Vector3 direction = targetPosition - transform.translate;
	direction.y = 0.0f;
	const float distance = Length(direction);
	const Vector3 normalized = distance > MathConstants::kDirectionEpsilon ? Normalize(direction) : Vector3{0.0f, 0.0f, 1.0f};
	transform.rotate.y = std::atan2(normalized.x, normalized.z);

	switch (stats_.behavior) {
	case EnemyBehaviorType::Shooter:
		UpdateShooter(transform, normalized, distance);
		break;
	case EnemyBehaviorType::Charger:
		UpdateCharger(transform, normalized, distance);
		break;
	case EnemyBehaviorType::NightSlashBoss:
		UpdateNightSlashBoss(transform, normalized, distance);
		UpdateNightSlashBossMotion(transform);
		break;
	case EnemyBehaviorType::SelfDestruct:
		UpdateSelfDestruct(transform, normalized, distance);
		break;
	case EnemyBehaviorType::TornadoBoss:
		UpdateTornadoBoss(transform, normalized, distance);
		break;
	case EnemyBehaviorType::BurstShooter:
		UpdateBurstShooter(transform, normalized, distance);
		break;
	case EnemyBehaviorType::BarrierBoss:
		UpdateBarrierBoss(transform, normalized, distance);
		break;
	case EnemyBehaviorType::Chase:
	default:
		if (distance > MathConstants::kDirectionEpsilon) {
			transform.translate = transform.translate +
				(stats_.speed * GameTime::GetFrameScale60()) * normalized;
		}
		break;
	}
}

void EnemyComponent::Draw3D() {
		if (IsDying() && GetOwner()) {
			// 消滅点から二重の光輪を広げ、モデルが小さくなっても撃破位置を読み取れるようにする。
			const float progress = (std::clamp)(deathEffectTimer_ / GetDeathEffectDuration(), 0.0f, 1.0f);
			const float alpha = (1.0f - progress) * (1.0f - progress);
			const Vector3 center = deathStartPosition_ + Vector3{0.0f, 0.18f, 0.0f};
			constexpr int kSegments = 28;
			for (int ringIndex = 0; ringIndex < 2; ++ringIndex) {
				const float delayedProgress = (std::clamp)(progress * 1.25f - 0.18f * ringIndex, 0.0f, 1.0f);
				const float radius = stats_.sizeScale * (0.25f + (1.25f + 0.35f * ringIndex) * delayedProgress);
				const Vector4 color = ringIndex == 0
					? Vector4{1.0f, 0.88f, 0.28f, alpha}
					: Vector4{1.0f, 0.34f, 0.08f, alpha * 0.72f};
				for (int index = 0; index < kSegments; ++index) {
					const float angleA = MathConstants::kTwoPi * static_cast<float>(index) / kSegments;
					const float angleB = MathConstants::kTwoPi * static_cast<float>(index + 1) / kSegments;
					LineDrawer::GetInstance()->DrawLine(
						center + Vector3{std::cos(angleA) * radius, 0.0f, std::sin(angleA) * radius},
						center + Vector3{std::cos(angleB) * radius, 0.0f, std::sin(angleB) * radius},
						color,
						true);
				}
			}
			return;
		}
		// 攻撃予兆または攻撃中だけ、地面の警告円と照準線を描画する。
		if ((!IsChargeWarningActive() && !IsSelfDestructArmed() && !IsTornadoWarningActive() && !IsBarrierWarningActive() &&
		     !IsNightSlashWarningActive() && !IsNightSlashAttacking() &&
		     !IsBossRangedWarningActive() && !IsBossRangedAttacking()) ||
		    !GetOwner()) {
			return;
		}
		Vector3 center = GetOwner()->GetTransform().translate + Vector3{0.0f, 0.06f, 0.0f};
		const bool isNightSlash = stats_.behavior == EnemyBehaviorType::NightSlashBoss;
		const bool isSelfDestruct = stats_.behavior == EnemyBehaviorType::SelfDestruct;
		const bool isTornadoBoss = stats_.behavior == EnemyBehaviorType::TornadoBoss;
		const bool isBarrierBoss = stats_.behavior == EnemyBehaviorType::BarrierBoss;
		if (isSelfDestruct && IsSelfDestructArmed()) {
			Vector3 toTarget = target_
				? target_->GetTransform().translate - GetOwner()->GetTransform().translate
				: Vector3{0.0f, 0.0f, -1.0f};
			toTarget.y = 0.0f;
			const Vector3 facing = Length(toTarget) > MathConstants::kDirectionEpsilon
				? Normalize(toTarget) : Vector3{0.0f, 0.0f, -1.0f};
			const Vector3 right{facing.z, 0.0f, -facing.x};
			const float remainingSeconds = (std::max)(0.0f, stats_.selfDestructFuseDuration - stateTimer_);
			const int displayedSeconds = static_cast<int>(std::ceil(remainingSeconds));
			const Vector3 timerCenter = GetOwner()->GetTransform().translate +
				Vector3{0.0f, 1.35f * stats_.sizeScale, 0.0f};
			const float warningPulse = 0.65f + 0.35f * std::sin(GetSelfDestructProgress() * 20.0f * MathConstants::kPi);
			DrawCountdownNumber(
				displayedSeconds, timerCenter, right, 0.34f * stats_.sizeScale,
				{1.0f, 0.12f + 0.62f * (1.0f - warningPulse), 0.02f, 1.0f});
		}
		if (isTornadoBoss && tornadoPatternIndex_ == 1 && target_) {
			center = target_->GetTransform().translate + Vector3{0.0f, 0.06f, 0.0f};
		}
		if (isBarrierBoss && target_) {
			center = target_->GetTransform().translate + Vector3{0.0f, 0.06f, 0.0f};
		}
		const bool isRangedPattern = IsBossRangedWarningActive() || IsBossRangedAttacking();
		const float progress = isRangedPattern
		    ? GetBossRangedProgress()
		    : isNightSlash ? GetNightSlashProgress()
		    : isSelfDestruct ? GetSelfDestructProgress()
		    : isBarrierBoss ? GetBarrierWarningProgress()
		    : isTornadoBoss ? GetTornadoWarningProgress() : GetChargeProgress();
		const float radius = isNightSlash
		    ? 1.35f + 0.45f * progress
		    : isSelfDestruct ? stats_.selfDestructRadius * (0.75f + 0.25f * progress)
		    : isBarrierBoss ? stats_.bossConvergingTornadoInitialRadius
		    : isTornadoBoss ? (tornadoPatternIndex_ == 0
		        ? stats_.bossTornadoInitialRadius + 0.6f * progress
		        : tornadoPatternIndex_ == 1
		            ? stats_.bossConvergingTornadoInitialRadius
		            : stats_.bossGiantTornadoSize * (1.0f + 0.25f * progress))
		    : 0.9f + 0.35f * progress;
		const Vector4 color = isRangedPattern
			? Vector4{0.12f, 0.45f + 0.45f * progress, 1.0f, 1.0f}
			: isNightSlash ? Vector4{0.82f, 0.08f + 0.30f * progress, 1.0f, 1.0f}
			: isSelfDestruct ? Vector4{1.0f, 0.85f * (1.0f - progress), 0.02f, 1.0f}
			: isBarrierBoss ? Vector4{0.72f + 0.25f * progress, 0.10f, 1.0f, 1.0f}
			: isTornadoBoss ? (tornadoPatternIndex_ == 0
			    ? Vector4{0.20f, 0.75f + 0.25f * progress, 1.0f, 1.0f}
			    : tornadoPatternIndex_ == 1
			        ? Vector4{0.75f + 0.25f * progress, 0.25f, 1.0f, 1.0f}
			        : Vector4{0.20f, 1.0f, 0.40f + 0.35f * progress, 1.0f})
			: Vector4{1.0f, 0.05f + 0.25f * progress, 0.02f, 1.0f};
		constexpr int segmentCount = 24;
		for (int index = 0; index < segmentCount; ++index) {
			const float angleA = static_cast<float>(index) * (2.0f * MathConstants::kPi / static_cast<float>(segmentCount));
			const float angleB = static_cast<float>(index + 1) * (2.0f * MathConstants::kPi / static_cast<float>(segmentCount));
			const Vector3 pointA = center + Vector3{std::cos(angleA) * radius, 0.0f, std::sin(angleA) * radius};
			const Vector3 pointB = center + Vector3{std::cos(angleB) * radius, 0.0f, std::sin(angleB) * radius};
			LineDrawer::GetInstance()->DrawLine(pointA, pointB, color, true);
		}
		if (target_) {
			Vector3 direction = target_->GetTransform().translate - center;
			direction.y = 0.0f;
			if (Length(direction) > MathConstants::kDirectionEpsilon) {
				LineDrawer::GetInstance()->DrawLine(center, center + 5.0f * Normalize(direction), color, true);
				if (isNightSlash && !isRangedPattern) {
					// 連続斬りでは標的を中心としたX字を表示し、斬撃範囲と回避タイミングを伝える。
					const Vector3 targetCenter = target_->GetTransform().translate + Vector3{0.0f, 0.08f, 0.0f};
					const Vector3 right{direction.z, 0.0f, -direction.x};
					const Vector3 normalizedRight = Length(right) > MathConstants::kDirectionEpsilon ? Normalize(right) : Vector3{1.0f, 0.0f, 0.0f};
					const float slashRadius = 1.25f + 0.35f * progress;
					LineDrawer::GetInstance()->DrawLine(
						targetCenter - slashRadius * normalizedRight - Vector3{0.0f, 0.0f, slashRadius},
						targetCenter + slashRadius * normalizedRight + Vector3{0.0f, 0.0f, slashRadius},
						color,
						true
					);
					LineDrawer::GetInstance()->DrawLine(
						targetCenter + slashRadius * normalizedRight - Vector3{0.0f, 0.0f, slashRadius},
						targetCenter - slashRadius * normalizedRight + Vector3{0.0f, 0.0f, slashRadius},
						color,
						true
					);
				}
			}
		}
	}

void EnemyComponent::ApplyStats(const EnemyStats& stats) {
		const bool isFirstStatsApplication = !hasAppliedStats_;
		const bool behaviorChanged = hasAppliedStats_ && stats_.behavior != stats.behavior;
		stats_ = stats;
		stats_.knockbackResistance = (std::clamp)(stats_.knockbackResistance, 0, 3);
		hasAppliedStats_ = true;
		if (behaviorChanged) {
			shootTimer_ = 0.0f;
			stateTimer_ = 0.0f;
			chargeState_ = ChargeState::Approach;
			nightSlashState_ = NightSlashState::Approach;
			// 行動種別を戻した場合も必ずApproach Stateから再構築する。
			nightSlashStateObject_ = nullptr;
			previousNightSlashMotionState_ = NightSlashState::Approach;
			nightSlashMotionInitialized_ = false;
			nightSlashMotionTime_ = 0.0f;
			tornadoBossState_ = TornadoBossState::Approach;
			barrierBossState_ = BarrierBossState::Approach;
			tornadoPatternIndex_ = 0;
			comboDashIndex_ = 0;
			bossPatternIndex_ = 0;
			rangedWaveIndex_ = 0;
			selfDestructArmed_ = false;
			selfDestructRequested_ = false;
			pendingShotRequests_.clear();
		}
		if (isFirstStatsApplication || currentHealth_ <= 0.0f || currentHealth_ > stats_.health) {
			currentHealth_ = stats_.health;
		}
	}

const EnemyStats& EnemyComponent::GetStats() const { return stats_; }

void EnemyComponent::SetEnemyTypeName(const std::string& enemyTypeName) { enemyTypeName_ = enemyTypeName; }

const std::string& EnemyComponent::GetEnemyTypeName() const { return enemyTypeName_; }

void EnemyComponent::SetTarget(Entity* target) { target_ = target; }

Entity* EnemyComponent::GetTarget() const { return target_; }

void EnemyComponent::SetTargetName(const std::string& targetName) {
	if (targetName_ != targetName) {
		targetName_ = targetName;
		// 設定名変更後に以前のEntityを使い続けず、次回のリンク解決へ委ねる。
		if (!target_ || (!targetName.empty() && target_->GetName() != targetName)) {
			target_ = nullptr;
		}
	}
}

const std::string& EnemyComponent::GetTargetName() const { return targetName_; }

void EnemyComponent::SetCurrentHealth(float health) { currentHealth_ = health < 0.0f ? 0.0f : health; }

float EnemyComponent::GetCurrentHealth() const { return currentHealth_; }

void EnemyComponent::TakeDamage(float damage) {
	if (damage <= 0.0f || currentHealth_ <= 0.0f) {
		return;
	}
	SetCurrentHealth(currentHealth_ - damage);
	damageFlashTimer_ = kDamageFlashDuration;
}

void EnemyComponent::ApplyKnockback(const Vector3& direction, int level) {
	Entity* owner = GetOwner();
	const int effectiveLevel = (std::clamp)(level - stats_.knockbackResistance, 0, 3);
	if (!owner || currentHealth_ <= 0.0f || effectiveLevel == 0) {
		return;
	}

	Vector3 horizontalDirection = direction;
	horizontalDirection.y = 0.0f;
	if (Length(horizontalDirection) <= MathConstants::kDirectionEpsilon) {
		return;
	}

	// 段階差を視認しやすくしつつ、1回の命中で敵が画面外まで飛ばない距離に抑える。
	constexpr float kKnockbackDistances[] = {0.0f, 0.45f, 0.90f, 1.50f};
	knockbackStartPosition_ = owner->GetTransform().translate;
	knockbackTargetPosition_ = knockbackStartPosition_ +
		kKnockbackDistances[effectiveLevel] * Normalize(horizontalDirection);
	knockbackElapsedSeconds_ = 0.0f;
	knockbackActive_ = true;
}

void EnemyComponent::UpdateKnockback(EulerTransform& transform, float deltaTime) {
	knockbackElapsedSeconds_ += (std::max)(0.0f, deltaTime);
	const float progress = (std::clamp)(knockbackElapsedSeconds_ / kKnockbackDuration, 0.0f, 1.0f);
	transform.translate = Leap(knockbackStartPosition_, knockbackTargetPosition_, progress);
	if (progress >= 1.0f) {
		transform.translate = knockbackTargetPosition_;
		knockbackActive_ = false;
	}
}

bool EnemyComponent::IsDamageFlashActive() const { return damageFlashTimer_ > 0.0f; }

bool EnemyComponent::IsDying() const { return currentHealth_ <= 0.0f && deathEffectStarted_; }

bool EnemyComponent::IsDeathEffectFinished() const {
	return deathEffectStarted_ && deathEffectTimer_ >= GetDeathEffectDuration();
}

bool EnemyComponent::IsBoss() const {
	return stats_.dropsBossUpgrade ||
		stats_.behavior == EnemyBehaviorType::NightSlashBoss ||
		stats_.behavior == EnemyBehaviorType::TornadoBoss;
}

float EnemyComponent::GetDeathEffectDuration() const {
	return IsBoss() ? kBossDeathEffectDuration : kDeathEffectDuration;
}

bool EnemyComponent::UsesDefeatCinematic() const {
	// dropsBossUpgradeだけを持つ中ボスは、倒れ込みを等速再生して戦闘テンポを維持する。
	return stats_.behavior == EnemyBehaviorType::NightSlashBoss ||
		stats_.behavior == EnemyBehaviorType::TornadoBoss;
}

void EnemyComponent::BeginDeathEffect(EulerTransform& transform) {
	deathEffectStarted_ = true;
	deathEffectTimer_ = 0.0f;
	deathStartPosition_ = transform.translate;
	deathStartRotation_ = transform.rotate;
	deathStartScale_ = transform.scale;
	pendingShotRequests_.clear();
	usesAuthoredDeathAnimation_ = false;
	if (IsBoss()) {
		if (!stats_.defeatAnimationName.empty()) {
			if (Object3dComponent* object3d = GetOwner()->GetComponent<Object3dComponent>()) {
				usesAuthoredDeathAnimation_ = object3d->SetAnimation(stats_.defeatAnimationName, true);
				if (usesAuthoredDeathAnimation_) {
					object3d->SetAnimationLooping(false);
					object3d->RestartAnimation();
				}
			}
		}
		if (UsesDefeatCinematic()) {
			// 最終打撃を画面へ刻み、次フレーム以降は実時間タイマーでスローへつなげる。
			bossDefeatPresentationTimer_ = 0.0f;
			GameTime::SetTimeScale(0.0f);
			PostEffect::GetInstance()->SetCinematicRadialBlur(0.24f);
		}
	}

	// エミッター自体をシーンへ残さず、ParticleManagerへ一度だけ破片を登録する。
	constexpr const char* kDeathParticleGroup = "EnemyDeathBurst";
	ParticleManager* particleManager = ParticleManager::GetInstance();
	if (!particleManager->HasGroup(kDeathParticleGroup)) {
		particleManager->CreateParticleGroup(kDeathParticleGroup, "Resources/circle.png", kMeshTypeQuad);
		particleManager->SetGroupBlendMode(kDeathParticleGroup, kBlendModeAdd);
	}
	ParticleEmitParam particle{};
	const float visualScale = (std::max)(0.75f, stats_.sizeScale);
	particle.scale = {0.16f * visualScale, 0.16f * visualScale, 1.0f};
	particle.endScale = {0.015f, 0.015f, 1.0f};
	particle.randomScaleRange = {0.08f * visualScale, 0.08f * visualScale, 0.0f};
	particle.baseVelocity = {0.0f, 1.15f * visualScale, 0.0f};
	particle.randomVelocityRange = {2.1f * visualScale, 1.35f * visualScale, 2.1f * visualScale};
	particle.randomPositionRange = {0.22f * visualScale, 0.24f * visualScale, 0.22f * visualScale};
	particle.acceleration = {0.0f, -2.8f, 0.0f};
	particle.lifeTime = 0.72f;
	particle.color = {1.0f, 0.72f, 0.12f, 0.95f};
	particle.endColor = {1.0f, 0.08f, 0.01f, 0.0f};
	particle.count = static_cast<uint32_t>((std::clamp)(12.0f + stats_.sizeScale * 6.0f, 14.0f, 30.0f));
	particleManager->Emit(
		kDeathParticleGroup,
		deathStartPosition_ + Vector3{0.0f, 0.28f * visualScale, 0.0f},
		particle.count,
		particle);
}

void EnemyComponent::UpdateDeathEffect(EulerTransform& transform) {
	if (UsesDefeatCinematic()) {
		constexpr float kHitStopEnd = 0.12f;
		constexpr float kSlowMotionEnd = 1.15f;
		constexpr float kRecoveryEnd = 1.55f;
		bossDefeatPresentationTimer_ += GameTime::GetUnscaledDeltaTime();

		if (bossDefeatPresentationTimer_ < kHitStopEnd) {
			GameTime::SetTimeScale(0.0f);
			PostEffect::GetInstance()->SetCinematicRadialBlur(0.24f);
		} else if (bossDefeatPresentationTimer_ < kSlowMotionEnd) {
			const float phase = (bossDefeatPresentationTimer_ - kHitStopEnd) /
				(kSlowMotionEnd - kHitStopEnd);
			const float eased = phase * phase * (3.0f - 2.0f * phase);
			GameTime::SetTimeScale(0.12f + 0.18f * eased);
			PostEffect::GetInstance()->SetCinematicRadialBlur(0.22f - 0.165f * eased);
		} else if (bossDefeatPresentationTimer_ < kRecoveryEnd) {
			const float phase = (bossDefeatPresentationTimer_ - kSlowMotionEnd) /
				(kRecoveryEnd - kSlowMotionEnd);
			const float eased = phase * phase * (3.0f - 2.0f * phase);
			GameTime::SetTimeScale(0.30f + 0.70f * eased);
			PostEffect::GetInstance()->SetCinematicRadialBlur(0.055f * (1.0f - eased));
		} else {
			GameTime::SetTimeScale(1.0f);
			PostEffect::GetInstance()->SetCinematicRadialBlur(0.0f);
		}
	}

	const float deathDuration = GetDeathEffectDuration();
	deathEffectTimer_ = (std::min)(deathDuration, deathEffectTimer_ + GameTime::GetDeltaTime());
	const float progress = (std::clamp)(deathEffectTimer_ / deathDuration, 0.0f, 1.0f);
	const float smoothProgress = progress * progress * (3.0f - 2.0f * progress);

	if (IsBoss()) {
		// ボスは即座に回転消滅せず、衝撃でのけぞってから横へ重く倒れ、最後だけ消滅する。
		const float recoilProgress = (std::clamp)(progress / 0.16f, 0.0f, 1.0f);
		const float fallProgress = (std::clamp)((progress - 0.12f) / 0.58f, 0.0f, 1.0f);
		const float smoothFall = fallProgress * fallProgress * (3.0f - 2.0f * fallProgress);
		const float vanishProgress = (std::clamp)((progress - 0.68f) / 0.32f, 0.0f, 1.0f);
		const float smoothVanish = vanishProgress * vanishProgress * (3.0f - 2.0f * vanishProgress);
		const float recoilScale = 1.0f + 0.10f * std::sin(MathConstants::kPi * recoilProgress);
		const float remainScale = (std::max)(0.015f, 1.0f - smoothVanish);

		if (usesAuthoredDeathAnimation_) {
			// 倒れ込み自体はbin内のボーンアニメーションへ任せ、終端の消滅だけEntity側で加える。
			transform.scale = {
				deathStartScale_.x * remainScale,
				deathStartScale_.y * remainScale,
				deathStartScale_.z * remainScale};
			transform.rotate = deathStartRotation_;
			transform.translate = deathStartPosition_;
		} else {
			// 専用クリップを持たない中ボスでも、同じ演出シーケンスを破綻させないフォールバックです。
			transform.scale = {
				deathStartScale_.x * recoilScale * remainScale,
				deathStartScale_.y * recoilScale * remainScale * (1.0f - 0.28f * smoothFall),
				deathStartScale_.z * recoilScale * remainScale};
			transform.rotate = deathStartRotation_ + Vector3{
				-0.18f * recoilProgress + 0.34f * smoothFall,
				0.30f * smoothFall,
				1.32f * smoothFall};
			transform.translate = deathStartPosition_ + Vector3{
				0.18f * stats_.sizeScale * smoothFall,
				0.20f * std::sin(MathConstants::kPi * recoilProgress) - 0.48f * smoothFall,
				-0.12f * stats_.sizeScale * recoilProgress};
		}

		if (Object3dComponent* object3d = GetOwner()->GetComponent<Object3dComponent>()) {
			const float impactFlash = 1.0f - recoilProgress;
			object3d->SetColor({
				1.0f,
				0.24f + 0.76f * impactFlash,
				0.08f + 0.92f * impactFlash,
				1.0f - smoothVanish});
			object3d->SetEmission(
				{1.0f, 0.16f + 0.74f * impactFlash, 0.02f},
				2.4f * (1.0f - smoothProgress));
		}
		return;
	}

	const float flashScale = 1.0f + 0.12f * std::sin(MathConstants::kPi * (std::min)(progress / 0.22f, 1.0f));
	const float disappearScale = (std::max)(0.015f, 1.0f - smoothProgress);

	transform.scale = {
		deathStartScale_.x * flashScale * disappearScale,
		deathStartScale_.y * flashScale * disappearScale * disappearScale,
		deathStartScale_.z * flashScale * disappearScale};
	transform.rotate = deathStartRotation_ + Vector3{progress * 0.55f, progress * 5.4f, progress * 1.15f};
	transform.translate = deathStartPosition_ + Vector3{
		0.0f,
		0.30f * std::sin(MathConstants::kPi * progress) - 0.42f * smoothProgress,
		0.0f};

	if (Object3dComponent* object3d = GetOwner()->GetComponent<Object3dComponent>()) {
		const float flash = 1.0f - (std::min)(1.0f, progress / 0.16f);
		object3d->SetColor({1.0f, 0.22f + 0.78f * flash, 0.06f + 0.94f * flash, 1.0f - smoothProgress});
		object3d->SetEmission({1.0f, 0.18f + 0.75f * flash, 0.02f}, 1.8f * (1.0f - progress));
	}
}

void EnemyComponent::SetRuntimeSpawned(bool runtimeSpawned) { runtimeSpawned_ = runtimeSpawned; }

bool EnemyComponent::GetRuntimeSpawned() const { return runtimeSpawned_; }

std::vector<EnemyShotRequest> EnemyComponent::ConsumeShotRequests() {
		std::vector<EnemyShotRequest> requests;
		requests.swap(pendingShotRequests_);
		return requests;
	}

bool EnemyComponent::IsChargeWarningActive() const { return chargeState_ == ChargeState::Charging; }

float EnemyComponent::GetChargeProgress() const {
		return stats_.chargeDuration > 0.0f ? (std::min)(1.0f, stateTimer_ / stats_.chargeDuration) : 1.0f;
	}

bool EnemyComponent::IsNightSlashWarningActive() const { return nightSlashState_ == NightSlashState::Windup; }

bool EnemyComponent::IsSelfDestructArmed() const { return selfDestructArmed_; }

float EnemyComponent::GetSelfDestructProgress() const {
		return stats_.selfDestructFuseDuration > 0.0f
		    ? (std::min)(1.0f, stateTimer_ / stats_.selfDestructFuseDuration)
		    : 1.0f;
	}

bool EnemyComponent::IsTornadoWarningActive() const { return tornadoBossState_ == TornadoBossState::Windup; }

int EnemyComponent::GetTornadoPatternIndex() const { return tornadoPatternIndex_; }

float EnemyComponent::GetTornadoWarningProgress() const {
		return stats_.bossTornadoWindup > 0.0f
		    ? (std::min)(1.0f, stateTimer_ / stats_.bossTornadoWindup)
		    : 1.0f;
	}

bool EnemyComponent::IsBarrierWarningActive() const { return barrierBossState_ == BarrierBossState::Windup; }

float EnemyComponent::GetBarrierWarningProgress() const {
	return stats_.bossTornadoWindup > 0.0f
		? (std::min)(1.0f, stateTimer_ / stats_.bossTornadoWindup)
		: 1.0f;
}

bool EnemyComponent::ConsumeSelfDestructRequest() {
		if (!selfDestructRequested_) {
			return false;
		}
		selfDestructRequested_ = false;
		currentHealth_ = 0.0f;
		return true;
	}

bool EnemyComponent::IsBossRangedWarningActive() const { return nightSlashState_ == NightSlashState::RangedWindup; }

bool EnemyComponent::IsBossRangedAttacking() const { return nightSlashState_ == NightSlashState::RangedFiring; }

float EnemyComponent::GetBossRangedProgress() const {
		if (nightSlashState_ == NightSlashState::RangedWindup) {
			return stats_.bossRangedWindup > 0.0f
			    ? (std::min)(1.0f, stateTimer_ / stats_.bossRangedWindup)
			    : 1.0f;
		}
		return stats_.bossRangedWaves > 0
		    ? (std::min)(1.0f, static_cast<float>(rangedWaveIndex_ + 1) / static_cast<float>(stats_.bossRangedWaves))
		    : 1.0f;
	}

bool EnemyComponent::IsNightSlashAttacking() const {
		// Dashingと直後のSlashingだけを近接攻撃判定として扱う。
		return nightSlashState_ == NightSlashState::Dashing || nightSlashState_ == NightSlashState::Slashing;
	}

float EnemyComponent::GetNightSlashProgress() const {
		if (nightSlashState_ == NightSlashState::Windup) {
			return stats_.comboWindup > 0.0f ? (std::min)(1.0f, stateTimer_ / stats_.comboWindup) : 1.0f;
		}
		return stats_.comboDashCount > 0
			? (std::min)(1.0f, static_cast<float>(comboDashIndex_ + 1) / static_cast<float>(stats_.comboDashCount))
			: 1.0f;
	}

bool EnemyComponent::CanDealContactDamage() const {
	if (stats_.behavior == EnemyBehaviorType::SelfDestruct || stats_.behavior == EnemyBehaviorType::TornadoBoss ||
		stats_.behavior == EnemyBehaviorType::BarrierBoss) {
		return false;
	}
	return stats_.behavior != EnemyBehaviorType::NightSlashBoss || IsNightSlashAttacking();
}

float EnemyComponent::GetContactAttackDamage() const {
	const bool isNightSlashFinisher =
		stats_.behavior == EnemyBehaviorType::NightSlashBoss && IsNightSlashAttacking() &&
		comboDashIndex_ + 1 >= (std::max)(1, stats_.comboDashCount);
	return stats_.attack * (isNightSlashFinisher ? 1.6f : 1.0f);
}

EnemyBehaviorPresentation EnemyComponent::GetBehaviorPresentation() {
	switch (stats_.behavior) {
	case EnemyBehaviorType::Shooter:
		return MakeColoredPresentation("enemy_shooter.gltf", {0.25f, 0.55f, 1.0f, 1.0f});
	case EnemyBehaviorType::Charger: {
		if (IsChargeWarningActive()) {
			const float pulse = 0.45f + 0.55f * std::sin(GetChargeProgress() * 18.0f * MathConstants::kPi);
			return MakeColoredPresentation("enemy_charger.gltf", {1.0f, 0.05f + 0.25f * pulse, 0.02f, 1.0f});
		}
		return MakeColoredPresentation("enemy_charger.gltf", {1.0f, 0.35f, 0.08f, 1.0f});
	}
	case EnemyBehaviorType::BurstShooter:
		return MakeColoredPresentation("enemy_shooter.gltf", {0.08f, 0.86f, 0.62f, 1.0f});
	case EnemyBehaviorType::BarrierBoss: {
		const float pulse = IsBarrierWarningActive()
			? 0.45f + 0.55f * std::sin(GetBarrierWarningProgress() * 14.0f * MathConstants::kPi)
			: 0.0f;
		return MakeColoredPresentation("enemy_shooter.gltf", {0.62f + 0.30f * pulse, 0.08f, 0.92f, 1.0f});
	}
	case EnemyBehaviorType::SelfDestruct: {
		const float pulse = IsSelfDestructArmed()
			? 0.45f + 0.55f * std::sin(GetSelfDestructProgress() * 20.0f * MathConstants::kPi)
			: 0.0f;
		return MakeColoredPresentation("enemy_bomber.gltf", {1.0f, 0.72f * (1.0f - pulse), 0.02f, 1.0f});
	}
	case EnemyBehaviorType::TornadoBoss: {
		if (!IsTornadoWarningActive()) {
			return MakeColoredPresentation("enemy_shooter.gltf", {0.08f, 0.48f, 0.78f, 1.0f});
		}
		const float pulse = 0.45f + 0.55f * std::sin(GetTornadoWarningProgress() * 16.0f * MathConstants::kPi);
		const Vector4 color = tornadoPatternIndex_ == 0
			? Vector4{0.08f, 0.60f + 0.35f * pulse, 1.0f, 1.0f}
			: tornadoPatternIndex_ == 1
				? Vector4{0.65f + 0.30f * pulse, 0.12f, 1.0f, 1.0f}
				: Vector4{0.08f, 0.65f + 0.30f * pulse, 0.30f, 1.0f};
		return MakeColoredPresentation("enemy_shooter.gltf", color);
	}
	case EnemyBehaviorType::NightSlashBoss: {
		Vector4 color{0.42f, 0.06f, 0.62f, 1.0f};
		if (IsBossRangedWarningActive()) {
			const float pulse = 0.45f + 0.55f * std::sin(GetBossRangedProgress() * 14.0f * MathConstants::kPi);
			color = {0.05f, 0.35f + 0.35f * pulse, 1.0f, 1.0f};
		} else if (IsBossRangedAttacking()) {
			color = {0.12f, 0.70f, 1.0f, 1.0f};
		} else if (IsNightSlashWarningActive()) {
			const float pulse = 0.45f + 0.55f * std::sin(GetNightSlashProgress() * 14.0f * MathConstants::kPi);
			color = {0.62f + 0.28f * pulse, 0.04f, 0.82f + 0.18f * pulse, 1.0f};
		} else if (IsNightSlashAttacking()) {
			color = {0.95f, 0.18f, 1.0f, 1.0f};
		}
		EnemyBehaviorPresentation result = MakeColoredPresentation("enemy_charger.gltf", color);
		result.addAttackTrail = true;
		result.attackTrailEmitting = IsNightSlashAttacking();
		return result;
	}
	case EnemyBehaviorType::Chase:
	default:
		EnemyBehaviorPresentation result;
		result.modelFilePath = "enemy_chaser.gltf";
		return result;
	}
}

void EnemyComponent::UpdateShooter(EulerTransform& transform, const Vector3& direction, float distance) {
		const float frameScale = GameTime::GetFrameScale60();
		// 距離帯の外側では接近、内側では後退し、射撃に適した間合いを維持する。
		const float minimumDistance = (std::max)(0.0f, stats_.preferredDistance - stats_.distanceTolerance);
		const float maximumDistance = stats_.preferredDistance + stats_.distanceTolerance;
		if (distance > maximumDistance) {
			transform.translate = transform.translate + (stats_.speed * frameScale) * direction;
		} else if (distance < minimumDistance) {
			transform.translate = transform.translate - (stats_.speed * frameScale) * direction;
		}

		// 射程へ到達する前はタイマーを進めず、画面外から弾が飛んでくる状況を避ける。
		if (stats_.shootingInterval <= 0.0f || distance <= MathConstants::kDirectionEpsilon || distance > maximumDistance) {
			return;
		}
		shootTimer_ += GameTime::GetDeltaTime();
		if (shootTimer_ >= stats_.shootingInterval) {
			shootTimer_ = 0.0f;
			// sceneObjects_を走査している最中に追加しないよう、生成要求だけをキューへ積む。
			const Vector3 shotPosition =
				transform.translate + Vector3{0.0f, 0.25f, 0.0f} + 0.65f * direction;
			const Vector3 shotDirection = MakeProjectileDirection(shotPosition, target_, direction);
			pendingShotRequests_.push_back({
				shotPosition,
				shotDirection,
				stats_.projectileSpeed,
				stats_.attack,
				stats_.projectileSize,
				stats_.projectileLifeTime
			});
		}
	}

void EnemyComponent::UpdateCharger(EulerTransform& transform, const Vector3& direction, float distance) {
		const float deltaTime = GameTime::GetDeltaTime();
		const float frameScale = GameTime::GetFrameScale60();
		stateTimer_ += deltaTime;
		switch (chargeState_) {
		case ChargeState::Approach:
			if (distance <= stats_.chargeTriggerDistance) {
				chargeState_ = ChargeState::Charging;
				stateTimer_ = 0.0f;
			} else {
				transform.translate = transform.translate + (stats_.speed * frameScale) * direction;
			}
			break;
		case ChargeState::Charging:
			if (stateTimer_ >= stats_.chargeDuration) {
				// 溜め完了時の方向を固定し、突進中にプレイヤーを追尾しない回避可能な攻撃にする。
				dashDirection_ = direction;
				chargeState_ = ChargeState::Dashing;
				stateTimer_ = 0.0f;
			}
			break;
		case ChargeState::Dashing:
			transform.translate = transform.translate + (stats_.dashSpeed * frameScale) * dashDirection_;
			transform.rotate.y = std::atan2(dashDirection_.x, dashDirection_.z);
			if (stateTimer_ >= stats_.dashDuration) {
				chargeState_ = ChargeState::Recovering;
				stateTimer_ = 0.0f;
			}
			break;
		case ChargeState::Recovering:
			// 突進直後に反撃できる時間を保証してから、再び接近状態へ戻す。
			if (stateTimer_ >= stats_.dashRecovery) {
				chargeState_ = ChargeState::Approach;
				stateTimer_ = 0.0f;
			}
			break;
		}
	}

void EnemyComponent::UpdateBurstShooter(EulerTransform& transform, const Vector3& direction, float distance) {
		const float frameScale = GameTime::GetFrameScale60();
		const float minimumDistance = (std::max)(0.0f, stats_.preferredDistance - stats_.distanceTolerance);
		const float maximumDistance = stats_.preferredDistance + stats_.distanceTolerance;
		if (distance > maximumDistance) {
			transform.translate = transform.translate + (stats_.speed * frameScale) * direction;
		} else if (distance < minimumDistance) {
			transform.translate = transform.translate - (stats_.speed * frameScale) * direction;
		}

		if (stats_.shootingInterval <= 0.0f || distance <= MathConstants::kDirectionEpsilon || distance > maximumDistance) {
			return;
		}
		shootTimer_ += GameTime::GetDeltaTime();
		if (shootTimer_ < stats_.shootingInterval) {
			return;
		}
		shootTimer_ = 0.0f;

		// 奇数・偶数のどちらでもプレイヤー方向を扇の中央として等間隔に配置する。
		const int shotCount = (std::max)(1, stats_.burstShotCount);
		const float centerIndex = static_cast<float>(shotCount - 1) * 0.5f;
		const Vector3 shotPosition =
			transform.translate + Vector3{0.0f, 0.35f, 0.0f} + 0.8f * direction;
		const Vector3 centerDirection = MakeProjectileDirection(shotPosition, target_, direction);
		for (int index = 0; index < shotCount; ++index) {
			const float angle = (static_cast<float>(index) - centerIndex) * stats_.burstSpreadAngle;
			const float sine = std::sin(angle);
			const float cosine = std::cos(angle);
			const Vector3 shotDirection{
				centerDirection.x * cosine + centerDirection.z * sine,
				centerDirection.y,
				centerDirection.z * cosine - centerDirection.x * sine
			};
			pendingShotRequests_.push_back({
				shotPosition,
				shotDirection,
				stats_.projectileSpeed,
				stats_.attack,
				stats_.projectileSize,
				stats_.projectileLifeTime
			});
		}
	}

void EnemyComponent::UpdateSelfDestruct(EulerTransform& transform, const Vector3& direction, float distance) {
		if (selfDestructRequested_) {
			return;
		}
		if (!selfDestructArmed_) {
			// 起爆距離までは通常追跡し、一度起動した導火線はプレイヤーが離れても解除しない。
			if (distance <= stats_.selfDestructTriggerDistance) {
				selfDestructArmed_ = true;
				stateTimer_ = 0.0f;
			} else if (distance > MathConstants::kDirectionEpsilon) {
				transform.translate = transform.translate + (stats_.speed * GameTime::GetFrameScale60()) * direction;
			}
			return;
		}

		stateTimer_ += GameTime::GetDeltaTime();
		if (stateTimer_ >= stats_.selfDestructFuseDuration) {
			selfDestructRequested_ = true;
		}
	}

void EnemyComponent::UpdateTornadoBoss(EulerTransform& transform, const Vector3& direction, float distance) {
		// 接近 -> 予告 -> 攻撃後硬直を全パターンで共有し、攻撃内容だけを順番に切り替える。
		stateTimer_ += GameTime::GetDeltaTime();
		switch (tornadoBossState_) {
		case TornadoBossState::Approach:
			if (distance <= stats_.bossTornadoTriggerDistance) {
				tornadoBossState_ = TornadoBossState::Windup;
				stateTimer_ = 0.0f;
			} else {
				transform.translate = transform.translate + (stats_.speed * GameTime::GetFrameScale60()) * direction;
			}
			break;
		case TornadoBossState::Windup:
			if (stateTimer_ >= stats_.bossTornadoWindup) {
				// 0: 拡散、1: 包囲収束、2: 巨大追尾の固定順で繰り返す。
				if (tornadoPatternIndex_ == 0) {
					EmitBossTornadoPattern(transform);
				} else if (tornadoPatternIndex_ == 1) {
					EmitBossConvergingTornadoPattern(transform);
				} else {
					EmitBossGiantTornadoPattern(transform);
				}
				tornadoPatternIndex_ = (tornadoPatternIndex_ + 1) % 3;
				tornadoBossState_ = TornadoBossState::Recovering;
				stateTimer_ = 0.0f;
			}
			break;
		case TornadoBossState::Recovering:
			if (stateTimer_ >= stats_.bossTornadoRecovery) {
				tornadoBossState_ = TornadoBossState::Approach;
				stateTimer_ = 0.0f;
			}
			break;
		}
	}

void EnemyComponent::UpdateBarrierBoss(EulerTransform& transform, const Vector3& direction, float distance) {
	stateTimer_ += GameTime::GetDeltaTime();
	switch (barrierBossState_) {
	case BarrierBossState::Approach:
		if (distance <= stats_.bossTornadoTriggerDistance) {
			barrierBossState_ = BarrierBossState::Windup;
			stateTimer_ = 0.0f;
			shootTimer_ = 0.0f;
		} else {
			transform.translate = transform.translate + (stats_.speed * GameTime::GetFrameScale60()) * direction;
		}
		break;
	case BarrierBossState::Windup:
		if (stateTimer_ >= stats_.bossTornadoWindup) {
			// 発動時のプレイヤー位置を収束弾で囲み、徐々に狭くなる戦闘領域を作る。
			EmitBossConvergingTornadoPattern(transform);
			barrierBossState_ = BarrierBossState::Active;
			stateTimer_ = 0.0f;
			shootTimer_ = 0.0f;
		}
		break;
	case BarrierBossState::Active:
		// 結界が縮む間は距離を保ち、内側へ扇状弾を撃って回避経路を制限する。
		UpdateBurstShooter(transform, direction, distance);
		if (stateTimer_ >= stats_.bossConvergingTornadoLifeTime) {
			barrierBossState_ = BarrierBossState::Recovering;
			stateTimer_ = 0.0f;
		}
		break;
	case BarrierBossState::Recovering:
		if (stateTimer_ >= stats_.bossTornadoRecovery) {
			barrierBossState_ = BarrierBossState::Approach;
			stateTimer_ = 0.0f;
		}
		break;
	}
}

void EnemyComponent::UpdateNightSlashBoss(EulerTransform& transform, const Vector3& direction, float distance) {
	// 夜叉ボスだけは状態数と責務が多いため、状態固有処理をStateオブジェクトへ委譲する。
	if (!nightSlashStateObject_) {
		ChangeNightSlashState(NightSlashState::Approach);
	}
	stateTimer_ += GameTime::GetDeltaTime();
	const NightSlashStateContext context{
		stats_,
		transform,
		direction,
		distance,
		GameTime::GetFrameScale60(),
		stateTimer_,
		dashDirection_,
		comboDashIndex_,
		bossPatternIndex_,
		rangedWaveIndex_};
	const NightSlashStateUpdate update = nightSlashStateObject_->Update(context);
	comboDashIndex_ = update.comboDashIndex;
	bossPatternIndex_ = update.bossPatternIndex;
	rangedWaveIndex_ = update.rangedWaveIndex;

	// Stateはprivate関数を直接呼ばず、要求だけを返す。副作用の適用順はComponentが管理する。
	if (update.resetStateTimer) {
		stateTimer_ = 0.0f;
	}
	if (update.beginDash) {
		BeginNightSlashDash(transform);
	} else if (update.hasTransition) {
		ChangeNightSlashState(update.nextState);
	}
	if (update.emitRangedWave) {
		EmitBossRangedWave(transform);
	}
}

void EnemyComponent::ChangeNightSlashState(NightSlashState state) {
	// Stateオブジェクトは内部データを持たないため全個体で共有し、遷移時の動的確保を避ける。
	static const NightSlashApproachState approachState;
	static const NightSlashWindupState windupState;
	static const NightSlashDashingState dashingState;
	static const NightSlashSlashingState slashingState;
	static const NightSlashRangedWindupState rangedWindupState;
	static const NightSlashRangedFiringState rangedFiringState;
	static const NightSlashRecoveringState recoveringState;

	nightSlashState_ = state;
	stateTimer_ = 0.0f;
	switch (state) {
	case NightSlashState::Approach: nightSlashStateObject_ = &approachState; break;
	case NightSlashState::Windup: nightSlashStateObject_ = &windupState; break;
	case NightSlashState::Dashing: nightSlashStateObject_ = &dashingState; break;
	case NightSlashState::Slashing: nightSlashStateObject_ = &slashingState; break;
	case NightSlashState::RangedWindup: nightSlashStateObject_ = &rangedWindupState; break;
	case NightSlashState::RangedFiring: nightSlashStateObject_ = &rangedFiringState; break;
	case NightSlashState::Recovering: nightSlashStateObject_ = &recoveringState; break;
	}
}

void EnemyComponent::UpdateNightSlashBossMotion(EulerTransform& transform) {
		if (!nightSlashMotionInitialized_) {
			nightSlashBaseScale_ = transform.scale;
			previousNightSlashMotionState_ = nightSlashState_;
			nightSlashMotionInitialized_ = true;
		}

		nightSlashMotionTime_ += GameTime::GetDeltaTime();
		const bool stateChanged = previousNightSlashMotionState_ != nightSlashState_;
		Object3dComponent* object3d = GetOwner() ? GetOwner()->GetComponent<Object3dComponent>() : nullptr;
		if (object3d && stateChanged) {
			// 高速行動へ入る瞬間に歩行クリップを先頭へ戻し、踏み込みのタイミングを揃える。
			if (nightSlashState_ == NightSlashState::Dashing || nightSlashState_ == NightSlashState::RangedFiring) {
				object3d->RestartAnimation();
			}
		}
		previousNightSlashMotionState_ = nightSlashState_;

		const auto normalizedProgress = [this](float duration) {
			return duration > 0.0f ? std::clamp(stateTimer_ / duration, 0.0f, 1.0f) : 1.0f;
		};
		const auto smoothStep = [](float value) {
			return value * value * (3.0f - 2.0f * value);
		};
		const float side = comboDashIndex_ % 2 == 0 ? 1.0f : -1.0f;

		// 各状態の変形は必ず基準姿勢から作り、状態をまたいだ拡縮・傾きの蓄積を防ぐ。
		transform.scale = nightSlashBaseScale_;
		transform.rotate.x = 0.0f;
		transform.rotate.z = 0.0f;

		switch (nightSlashState_) {
		case NightSlashState::Approach: {
			// 歩行クリップに呼吸と足取りの上下動を足し、接近中も大型の猫らしい重さを出す。
			if (object3d) object3d->SetAnimationPlaying(true);
			const float stride = std::sin(nightSlashMotionTime_ * 7.0f);
			transform.scale.x *= 1.0f - stride * 0.018f;
			transform.scale.y *= 1.0f + stride * 0.035f;
			transform.scale.z *= 1.0f - stride * 0.018f;
			transform.rotate.z = std::sin(nightSlashMotionTime_ * 3.5f) * 0.025f;
			break;
		}
		case NightSlashState::Windup: {
			// 連続斬りの直前は地面へ伏せ、左右どちらへ飛ぶかを体の傾きで予告する。
			if (object3d) object3d->SetAnimationPlaying(false);
			const float progress = smoothStep(normalizedProgress(stats_.comboWindup));
			transform.scale.x *= 1.0f + 0.10f * progress;
			transform.scale.y *= 1.0f - 0.22f * progress;
			transform.scale.z *= 1.0f + 0.12f * progress;
			transform.rotate.x = -0.18f * progress;
			transform.rotate.z = side * 0.10f * progress;
			break;
		}
		case NightSlashState::Dashing: {
			// 突進方向へ胴体を伸ばし、通常斬りより最終斬りを大きく傾ける。
			if (object3d) object3d->SetAnimationPlaying(true);
			const float progress = normalizedProgress(stats_.comboDashDuration);
			const float stretch = std::sin(progress * MathConstants::kPi);
			const bool isFinisher = comboDashIndex_ + 1 >= (std::max)(1, stats_.comboDashCount);
			transform.scale.x *= 1.0f - 0.10f * stretch;
			transform.scale.y *= 1.0f - 0.08f * stretch;
			transform.scale.z *= 1.0f + (isFinisher ? 0.32f : 0.22f) * stretch;
			transform.rotate.x = -(isFinisher ? 0.20f : 0.12f) * stretch;
			transform.rotate.z = side * 0.12f * stretch;
			break;
		}
		case NightSlashState::Slashing: {
			// ダッシュ直後に上体を左右へ鋭く返し、交互の爪振りとフィニッシュを見せる。
			if (object3d) object3d->SetAnimationPlaying(false);
			const float progress = normalizedProgress(stats_.comboSlashPause);
			const float snap = std::sin(progress * MathConstants::kPi);
			const bool isFinisher = comboDashIndex_ + 1 >= (std::max)(1, stats_.comboDashCount);
			transform.scale.x *= 1.0f + (isFinisher ? 0.20f : 0.12f) * snap;
			transform.scale.z *= 1.0f - 0.08f * snap;
			transform.rotate.y += side * (isFinisher ? 0.82f : 0.55f) * snap;
			transform.rotate.z = side * (isFinisher ? 0.42f : 0.28f) * snap;
			break;
		}
		case NightSlashState::RangedWindup: {
			// 弾幕前は体を高く起こして溜め、近接攻撃とは異なる予備動作にする。
			if (object3d) object3d->SetAnimationPlaying(false);
			const float progress = smoothStep(normalizedProgress(stats_.bossRangedWindup));
			const float pulse = std::sin(progress * MathConstants::kPi * 3.0f) * 0.025f;
			transform.scale.x *= 1.0f - 0.06f * progress + pulse;
			transform.scale.y *= 1.0f + 0.18f * progress;
			transform.scale.z *= 1.0f - 0.08f * progress + pulse;
			transform.rotate.x = 0.20f * progress;
			break;
		}
		case NightSlashState::RangedFiring: {
			// 各ウェーブの発射間隔に同期して前後へ反動を付ける。
			if (object3d) object3d->SetAnimationPlaying(true);
			const float progress = normalizedProgress(stats_.bossRangedInterval);
			const float recoil = std::sin(progress * MathConstants::kPi);
			transform.scale.x *= 1.0f + 0.08f * recoil;
			transform.scale.y *= 1.0f + 0.10f * recoil;
			transform.scale.z *= 1.0f - 0.14f * recoil;
			transform.rotate.x = -0.16f * recoil;
			transform.rotate.z = (rangedWaveIndex_ % 2 == 0 ? 1.0f : -1.0f) * 0.07f * recoil;
			break;
		}
		case NightSlashState::Recovering: {
			// 攻撃後は低く沈んで息を整え、プレイヤーの反撃時間を姿勢でも示す。
			if (object3d) object3d->SetAnimationPlaying(false);
			const float remaining = 1.0f - smoothStep(normalizedProgress(stats_.comboRecovery));
			const float breath = std::sin(nightSlashMotionTime_ * 5.0f) * 0.018f * remaining;
			transform.scale.x *= 1.0f + 0.07f * remaining - breath;
			transform.scale.y *= 1.0f - 0.14f * remaining + breath;
			transform.scale.z *= 1.0f + 0.07f * remaining - breath;
			transform.rotate.x = 0.16f * remaining;
			break;
		}
		}
	}

void EnemyComponent::EmitBossRangedWave(const EulerTransform& transform) {
		const Vector3 shotPosition = transform.translate + Vector3{0.0f, 0.35f, 0.0f};
		const float shotAttack = stats_.attack * stats_.bossProjectileAttackMultiplier;
		if (bossPatternIndex_ == 1) {
			// ウェーブごとに角度を少しずらし、同じ射線が重なり続けない全方位弾幕を作る。
			const int shotCount = (std::max)(1, stats_.bossRadialShotCount);
			const float waveRotation = static_cast<float>(rangedWaveIndex_) * 0.16f;
			for (int index = 0; index < shotCount; ++index) {
				const float angle =
				    (static_cast<float>(index) / static_cast<float>(shotCount)) * 2.0f * MathConstants::kPi + waveRotation;
				const Vector3 direction{std::sin(angle), 0.0f, std::cos(angle)};
				pendingShotRequests_.push_back({
					shotPosition, direction, stats_.projectileSpeed, shotAttack,
					stats_.projectileSize, stats_.projectileLifeTime
				});
			}
			return;
		}

		// パターン2は発射時点のプレイヤー方向を中心として、左右対称の扇状弾を生成する。
		Vector3 forward = target_->GetTransform().translate - transform.translate;
		forward.y = 0.0f;
		forward = Length(forward) > MathConstants::kDirectionEpsilon
		    ? Normalize(forward)
		    : Vector3{0.0f, 0.0f, 1.0f};
		const int shotCount = (std::max)(1, stats_.bossAimedShotCount);
		const float centerIndex = static_cast<float>(shotCount - 1) * 0.5f;
		for (int index = 0; index < shotCount; ++index) {
			const float angle = (static_cast<float>(index) - centerIndex) * stats_.bossAimedSpreadAngle;
			const float sine = std::sin(angle);
			const float cosine = std::cos(angle);
			const Vector3 direction{
				forward.x * cosine + forward.z * sine,
				0.0f,
				forward.z * cosine - forward.x * sine
			};
			pendingShotRequests_.push_back({
				shotPosition, direction, stats_.projectileSpeed, shotAttack,
				stats_.projectileSize, stats_.projectileLifeTime
			});
		}
	}

void EnemyComponent::EmitBossTornadoPattern(const EulerTransform& transform) {
		// ボスを中心に等角度で配置し、同じ向きへ回転しながら外側へ広げる。
		const int tornadoCount = (std::max)(1, stats_.bossTornadoCount);
		for (int index = 0; index < tornadoCount; ++index) {
			const float angle =
			    (static_cast<float>(index) / static_cast<float>(tornadoCount)) * 2.0f * MathConstants::kPi;
			EnemyShotRequest request{};
			request.position = transform.translate + Vector3{
				std::sin(angle) * stats_.bossTornadoInitialRadius,
				0.35f,
				std::cos(angle) * stats_.bossTornadoInitialRadius
			};
			request.direction = {std::cos(angle), 0.0f, -std::sin(angle)};
			request.attack = stats_.attack * stats_.bossTornadoAttackMultiplier;
			request.size = stats_.bossTornadoSize;
			request.lifeTime = stats_.bossTornadoLifeTime;
			request.motionType = EnemyAttackEntityMotionType::ExpandingOrbit;
			request.orbitCenter = transform.translate;
			request.orbitAngle = angle;
			request.orbitInitialRadius = stats_.bossTornadoInitialRadius;
			request.orbitAngularSpeed = stats_.bossTornadoAngularSpeed;
			request.orbitRadialSpeed = stats_.bossTornadoRadialSpeed;
			request.orbitHeight = 0.35f;
			pendingShotRequests_.push_back(request);
		}
	}

void EnemyComponent::EmitBossConvergingTornadoPattern(const EulerTransform& transform) {
		// 発動時点のプレイヤー位置を中心として固定し、包囲した竜巻を内側へ収束させる。
		// 発動後にプレイヤーが移動しても収束中心は追従しないため、回避経路を確保できる。
		const Vector3 center = target_ ? target_->GetTransform().translate : transform.translate;
		const int tornadoCount = (std::max)(1, stats_.bossConvergingTornadoCount);
		for (int index = 0; index < tornadoCount; ++index) {
			const float angle =
			    (static_cast<float>(index) / static_cast<float>(tornadoCount)) * 2.0f * MathConstants::kPi;
			EnemyShotRequest request{};
			request.position = center + Vector3{
				std::sin(angle) * stats_.bossConvergingTornadoInitialRadius,
				0.35f,
				std::cos(angle) * stats_.bossConvergingTornadoInitialRadius
			};
			request.direction = {-std::cos(angle), 0.0f, std::sin(angle)};
			request.attack = stats_.attack * stats_.bossConvergingTornadoAttackMultiplier;
			request.size = stats_.bossConvergingTornadoSize;
			request.lifeTime = stats_.bossConvergingTornadoLifeTime;
			request.motionType = EnemyAttackEntityMotionType::ContractingOrbit;
			request.orbitCenter = center;
			request.orbitAngle = angle;
			request.orbitInitialRadius = stats_.bossConvergingTornadoInitialRadius;
			request.orbitAngularSpeed = stats_.bossConvergingTornadoAngularSpeed;
			request.orbitRadialSpeed = stats_.bossConvergingTornadoRadialSpeed;
			request.orbitHeight = 0.35f;
			pendingShotRequests_.push_back(request);
		}
	}

void EnemyComponent::EmitBossGiantTornadoPattern(const EulerTransform& transform) {
		// ボスの正面へ1つだけ生成し、弾側でプレイヤーの現在位置を継続追跡する。
		Vector3 direction = target_ ? target_->GetTransform().translate - transform.translate : Vector3{0.0f, 0.0f, 1.0f};
		direction.y = 0.0f;
		direction = Length(direction) > MathConstants::kDirectionEpsilon
		    ? Normalize(direction)
		    : Vector3{0.0f, 0.0f, 1.0f};
		EnemyShotRequest request{};
		request.position = transform.translate + stats_.bossGiantTornadoSpawnOffset * direction + Vector3{0.0f, 0.5f, 0.0f};
		request.direction = direction;
		request.speed = stats_.bossGiantTornadoSpeed;
		request.attack = stats_.attack * stats_.bossGiantTornadoAttackMultiplier;
		request.size = stats_.bossGiantTornadoSize;
		request.lifeTime = stats_.bossGiantTornadoLifeTime;
		request.motionType = EnemyAttackEntityMotionType::Homing;
		request.homingTarget = target_;
		pendingShotRequests_.push_back(request);
	}

void EnemyComponent::BeginNightSlashDash(const EulerTransform& transform) {
		// 標的への前方ベクトルから水平な右方向を作り、偶数回と奇数回で左右を切り替える。
		Vector3 toTarget = target_->GetTransform().translate - transform.translate;
		toTarget.y = 0.0f;
		const Vector3 forward = Length(toTarget) > MathConstants::kDirectionEpsilon
			? Normalize(toTarget)
			: Vector3{0.0f, 0.0f, 1.0f};
		const Vector3 right{forward.z, 0.0f, -forward.x};
		const float side = comboDashIndex_ % 2 == 0 ? 1.0f : -1.0f;
		Vector3 dashTarget = target_->GetTransform().translate + side * stats_.comboSideOffset * right;
		if (comboDashIndex_ + 1 >= (std::max)(1, stats_.comboDashCount)) {
			// 最終斬りは横へ抜けず、標的の少し奥まで貫通する軌道にする。
			dashTarget = target_->GetTransform().translate + 1.2f * forward;
		}
		Vector3 dashVector = dashTarget - transform.translate;
		dashVector.y = 0.0f;
		dashDirection_ = Length(dashVector) > MathConstants::kDirectionEpsilon ? Normalize(dashVector) : forward;
		ChangeNightSlashState(NightSlashState::Dashing);
	}

