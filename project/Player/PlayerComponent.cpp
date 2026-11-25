#include "PlayerComponent.h"
#include "MathConstants.h"
#include "core/Entity.h"
#include "Input.h"
#include "GameTime.h"
#include "object/Object3dComponent.h"
#include "object/Object3dCommon.h"
#include "object/Object3d.h"
#include "camera/Camera.h"
#include "ImGuiManager.h"
#include "model/ModelManager.h"
#include "Matrix.h"
#include "PostEffect.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <dinput.h>

namespace {
constexpr float kHealthRegenerationInterval = 6.0f;
constexpr float kHealthRegenerationAmount = 1.0f;
constexpr int kMikoBonusRewardLevelInterval = 5;
constexpr float kCatMinimumDamageMultiplier = 0.5f;
constexpr float kCatMaximumDamageMultiplier = 2.0f;
// 初期爪の最遠配置約2.0 + 当たり判定半径約1.05 + 通常敵半径約0.45。
constexpr float kCatMaximumDamageRange = 3.5f;
constexpr float kCatMinimumDamageDistance = 15.0f;
constexpr float kTenguSpeedIncreasePerLevel = 0.01f;
constexpr float kTenguDamageIncreasePerTenSpeed = 0.01f;
// brade.obj はプレイヤーモデルより小さいため、装備表示用の倍率と柄の中心位置をまとめて管理する。
constexpr float kBladeScale = 12.0f;
constexpr float kBladeGripCenter = 0.007f * kBladeScale;
constexpr int kLeftMouseButton = 0;

Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maxDelta) {
	// 現在速度を目標速度へ一定量だけ近づけ、入力の急停止・急加速を抑える。
	const Vector3 difference = target - current;
	const float distance = Length(difference);
	if (distance <= maxDelta || distance <= MathConstants::kNormalizationEpsilon) {
		return target;
	}

	const Vector3 direction = {difference.x / distance, difference.y / distance, difference.z / distance};
	return current + maxDelta * direction;
}

bool TryGetMouseAimDirection(const Vector3& playerPosition, Vector3& outDirection) {
	Input* input = Input::GetInstance();
	Camera* camera = Object3dCommon::GetInstance()->GetDefaultCamera();
	if (!camera) {
		return false;
	}

	float viewLeft = 0.0f;
	float viewTop = 0.0f;
	float viewWidth = static_cast<float>(input->GetClientWidth());
	float viewHeight = static_cast<float>(input->GetClientHeight());
#ifdef USE_IMGUI
	const ImVec2 gameViewPosition = ImGuiManager::GetInstance()->GetGameViewContentPosition();
	const ImVec2 gameViewSize = ImGuiManager::GetInstance()->GetGameViewContentSize();
	if (gameViewSize.x > 1.0f && gameViewSize.y > 1.0f) {
		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		const float viewportOriginX = mainViewport ? mainViewport->Pos.x : 0.0f;
		const float viewportOriginY = mainViewport ? mainViewport->Pos.y : 0.0f;
		viewLeft = gameViewPosition.x - viewportOriginX;
		viewTop = gameViewPosition.y - viewportOriginY;
		viewWidth = gameViewSize.x;
		viewHeight = gameViewSize.y;
	}
#endif
	if (viewWidth <= 1.0f || viewHeight <= 1.0f) {
		return false;
	}

	const float mouseX = static_cast<float>(input->GetMouseClientX());
	const float mouseY = static_cast<float>(input->GetMouseClientY());
	if (mouseX < viewLeft || mouseX > viewLeft + viewWidth ||
	    mouseY < viewTop || mouseY > viewTop + viewHeight) {
		return false;
	}

	const float ndcX = ((mouseX - viewLeft) / viewWidth) * 2.0f - 1.0f;
	const float ndcY = 1.0f - ((mouseY - viewTop) / viewHeight) * 2.0f;
	const Matrix4x4 inverseViewProjection = Inverse(camera->GetViewProjectionMatrix());
	const Vector3 nearPoint = Transformation({ndcX, ndcY, 0.0f}, inverseViewProjection);
	const Vector3 farPoint = Transformation({ndcX, ndcY, 1.0f}, inverseViewProjection);
	const Vector3 ray = farPoint - nearPoint;
	if (std::fabs(ray.y) <= MathConstants::kNormalizationEpsilon) {
		return false;
	}

	const float distanceAlongRay = (playerPosition.y - nearPoint.y) / ray.y;
	if (distanceAlongRay < 0.0f) {
		return false;
	}

	const Vector3 aimPoint = nearPoint + distanceAlongRay * ray;
	Vector3 aimDirection = aimPoint - playerPosition;
	aimDirection.y = 0.0f;
	const float aimLength = Length(aimDirection);
	if (aimLength <= MathConstants::kNormalizationEpsilon) {
		return false;
	}

	outDirection = {aimDirection.x / aimLength, 0.0f, aimDirection.z / aimLength};
	return true;
}

}

PlayerComponent::~PlayerComponent() = default;

/// <summary>
/// 毎フレーム WASD 入力を見て、XZ 平面上でプレイヤーを移動します。
/// </summary>
void PlayerComponent::Update() {
	const float deltaTime = GameTime::GetDeltaTime();
	const float frameScale = GameTime::GetFrameScale60();
	UpdateHealthRegeneration(deltaTime);
	if (damageInvincibilityTimer_ > 0.0f) {
		damageInvincibilityTimer_ -= deltaTime;
		if (damageInvincibilityTimer_ < 0.0f) {
			damageInvincibilityTimer_ = 0.0f;
		}
	}

	Entity* owner = GetOwner();
	if (!owner) {
		return;
	}

	// プレイヤー定義に右手装備がある場合だけ、一度だけ描画オブジェクトを準備する。
	EnsureBladeEquipment();

	Input* input = Input::GetInstance();
	Vector3 keyboardMove{0.0f, 0.0f, 0.0f};
	if (input->PushKey(DIK_W)) {
		keyboardMove.z += 1.0f;
	}
	if (input->PushKey(DIK_S)) {
		keyboardMove.z -= 1.0f;
	}
	if (input->PushKey(DIK_A)) {
		keyboardMove.x -= 1.0f;
	}
	if (input->PushKey(DIK_D)) {
		keyboardMove.x += 1.0f;
	}
	const float keyboardLength = Length(keyboardMove);
	if (keyboardLength > MathConstants::kNormalizationEpsilon) {
		keyboardMove = {keyboardMove.x / keyboardLength, 0.0f, keyboardMove.z / keyboardLength};
	}

	const Vector3 leftStick = input->GetGamepadLeftStick();
	Vector3 move{keyboardMove.x + leftStick.x, 0.0f, keyboardMove.z + leftStick.z};

	const float moveLength = Length(move);
	Vector3 targetVelocity{0.0f, 0.0f, 0.0f};
	const float effectiveMoveSpeed = GetEffectiveMoveSpeed();
	if (moveLength > MathConstants::kNormalizationEpsilon) {
		// キーボード斜め入力とスティック入力を正規化し、斜め移動だけ速くならないようにする。
		const Vector3 moveDirection = {move.x / moveLength, 0.0f, move.z / moveLength};
		const float moveAmount = moveLength > 1.0f ? 1.0f : moveLength;
		targetVelocity = (effectiveMoveSpeed * moveAmount) * moveDirection;
	}

	if (demoControlled_) {
		targetVelocity = demoMoveVelocity_;
	}
	const bool hasMoveInput = Length(targetVelocity) > MathConstants::kNormalizationEpsilon;
	// 入力開始時と終了時で補間量を分け、歩き始めと停止を自然に見せる。
	const float smoothingDelta = (hasMoveInput ? effectiveMoveSpeed * 0.18f : effectiveMoveSpeed * 0.14f) * frameScale;
	currentMoveVelocity_ = demoControlled_ ? targetVelocity : MoveTowards(currentMoveVelocity_, targetVelocity, smoothingDelta);

	const float currentSpeed = Length(currentMoveVelocity_);
	if (Object3dComponent* object3d = owner->GetComponent<Object3dComponent>()) {
		if (object3d->HasAnimation()) {
			// 移動入力または補間中の速度がある間だけ歩行アニメーションを再生する。
			object3d->SetAnimationPlaying(hasMoveInput || currentSpeed > 0.0005f);
		}
	}

	// 照準入力は移動方向より優先する。両方が同時なら、明示的に押しているマウスを最優先にする。
	Vector3 facingDirection{};
	bool hasFacingDirection = false;
	if (!demoControlled_ && input->PushMouseButton(kLeftMouseButton)) {
		hasFacingDirection = TryGetMouseAimDirection(owner->GetTransform().translate, facingDirection);
	}
	if (!demoControlled_ && !hasFacingDirection) {
		const Vector3 rightStick = input->GetGamepadRightStick();
		const float rightStickLength = Length(rightStick);
		if (rightStickLength > MathConstants::kNormalizationEpsilon) {
			facingDirection = {
			    rightStick.x / rightStickLength,
			    0.0f,
			    rightStick.z / rightStickLength
			};
			hasFacingDirection = true;
		}
	}
	if (!hasFacingDirection && currentSpeed > MathConstants::kNormalizationEpsilon) {
		facingDirection = {
		    currentMoveVelocity_.x / currentSpeed,
		    0.0f,
		    currentMoveVelocity_.z / currentSpeed
		};
		hasFacingDirection = true;
	}
	if (hasFacingDirection) {
		owner->GetTransform().rotate.y = std::atan2(facingDirection.x, facingDirection.z);
	}

	if (currentSpeed <= MathConstants::kNormalizationEpsilon) {
		return;
	}

	owner->GetTransform().translate = owner->GetTransform().translate + frameScale * currentMoveVelocity_;
}

void PlayerComponent::UpdateHealthRegeneration(float deltaTime) {
	// 最大HP未満のときだけ回復タイマーを進め、満タンまたは死亡中はタイマーを初期化する。
	const float maxHealth = GetMaxHealth();
	if (currentHealth_ <= 0.0f || currentHealth_ >= maxHealth) {
		healthRegenerationTimer_ = 0.0f;
		return;
	}

	healthRegenerationTimer_ += deltaTime;
	if (healthRegenerationTimer_ < kHealthRegenerationInterval) {
		return;
	}

	const float regenerationCount = std::floor(healthRegenerationTimer_ / kHealthRegenerationInterval);
	healthRegenerationTimer_ -= regenerationCount * kHealthRegenerationInterval;
	SetCurrentHealth(currentHealth_ + regenerationCount * kHealthRegenerationAmount);
}

void PlayerComponent::Draw3D() {
	// 装備未設定または読み込み失敗時は、安全に追加描画を中止する。
	if (stats_.rightHandEquipmentModelFilePath.empty() || !bladeObject_) {
		return;
	}

	Entity* owner = GetOwner();
	Object3dComponent* playerObject3d = owner ? owner->GetComponent<Object3dComponent>() : nullptr;
	if (!playerObject3d || !playerObject3d->GetObject3d()) {
		return;
	}

	Matrix4x4 rightHandSkeletonMatrix;
	// 右手のモデル内座標を取得する。ここではプレイヤーのワールド座標をまだ含めない。
	if (!playerObject3d->GetObject3d()->GetJointSkeletonSpaceMatrix("mixamorig:RightHand", rightHandSkeletonMatrix)) {
		return;
	}

	const EulerTransform& playerTransform = owner->GetTransform();
	const Matrix4x4 playerWorldMatrix = MakeAffineMatrix(
	    playerTransform.scale,
	    playerTransform.rotate,
	    playerTransform.translate);
	// 右手のモデル内座標へプレイヤーのワールド変換を一度だけ合成し、移動量の二重適用を防ぐ。
	const Matrix4x4 rightHandWorldMatrix = Multiply(rightHandSkeletonMatrix, playerWorldMatrix);
	const float playerYaw = playerTransform.rotate.y;
	const Vector3 playerForward = {
	    std::sin(playerYaw),
	    0.0f,
	    std::cos(playerYaw)
	};
	const Vector3 rightHandPosition = {
	    rightHandWorldMatrix.m[3][0],
	    rightHandWorldMatrix.m[3][1],
	    rightHandWorldMatrix.m[3][2]
	};
	// OBJの原点ではなく柄の中央が右手に重なるよう、プレイヤーの前方向へ原点を補正する。
	const Vector3 bladePosition = rightHandPosition - kBladeGripCenter * playerForward;
	// 位置は右手に追従させ、向きは歩行中の手首回転に影響されないようプレイヤーのY回転を使う。
	const Matrix4x4 bladeWorldMatrix = MakeAffineMatrix(
	    {kBladeScale, kBladeScale, kBladeScale},
	    {MathConstants::kPi * 0.5f, playerYaw, 0.0f},
	    bladePosition);
	bladeObject_->SetWorldMatrixOverride(bladeWorldMatrix);
	// 刀はシーン管理外のObject3dなので、追従カメラへ毎フレーム明示的に同期する。
	bladeObject_->SetCamera(Object3dCommon::GetInstance()->GetDefaultCamera());
	bladeObject_->Update();
	Object3dCommon::GetInstance()->SetDraw();
	bladeObject_->Draw();
}

void PlayerComponent::Finalize() {
	// Playerが所有する装備用GPUリソースをシーン終了時に解放する。
	bladeObject_.reset();
}

void PlayerComponent::EnsureBladeEquipment() {
	// 装備指定がない場合は生成せず、生成済みなら毎フレーム再利用する。
	if (stats_.rightHandEquipmentModelFilePath.empty() || bladeObject_) {
		return;
	}

	ModelManager* modelManager = ModelManager::GetInstance();
	// シーン側で未ロードの場合にも装備単体で初期化できるよう、ここでも読み込みを保証する。
	const std::string& equipmentModel = stats_.rightHandEquipmentModelFilePath;
	if (!modelManager->FindModel(equipmentModel)) {
		modelManager->LoadModel(equipmentModel, false, stats_.rightHandEquipmentModelDirectory);
	}
	if (!modelManager->FindModel(equipmentModel)) {
		return;
	}

	bladeObject_ = std::make_unique<Object3d>();
	bladeObject_->Initialize();
	bladeObject_->SetModel(equipmentModel);
	// 白を乗算し、MTLが参照するbradeC.pngの色をそのまま使用する。
	bladeObject_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
}

void PlayerComponent::ApplyStats(const PlayerStats& stats) {
	ApplyStats(stats, stats);
}

void PlayerComponent::ApplyStats(const PlayerStats& baseStats, const PlayerStats& effectiveStats) {
	// 保存用の基礎値と、強化アイテム反映済みの実効値を分けて保持する。
	// タイプ変更で装備モデルが変わる場合は、以前の装備オブジェクトを破棄して再生成する。
	if (stats_.rightHandEquipmentModelFilePath != baseStats.rightHandEquipmentModelFilePath ||
	    stats_.rightHandEquipmentModelDirectory != baseStats.rightHandEquipmentModelDirectory) {
		bladeObject_.reset();
	}
	stats_ = baseStats;
	effectiveStats_ = effectiveStats;
	effectiveStats_.level = stats_.level;
	effectiveStats_.experience = stats_.experience;
	moveSpeed_ = effectiveStats_.baseSpeed;
	SetModelFilePath(stats_.modelFilePath, stats_.isAnimationModel);
	const float maxHealth = GetMaxHealth();
	// ゲーム開始時はショップの最大HP強化分まで回復した状態にする。
	// レベルアップなどプレイ中の再適用では、現在HPを維持して最大値超過時だけ補正する。
	if (!hasAppliedStats_ || currentHealth_ <= 0.0f || currentHealth_ > maxHealth) {
		currentHealth_ = maxHealth;
	}
	hasAppliedStats_ = true;
}

float PlayerComponent::GetEffectiveSpeedPercent() const {
	if (playerTypeName_ != "烏天狗") {
		return effectiveStats_.speed;
	}

	// Lv1を基準とし、レベルアップするたびに現在の速度倍率を1%ずつ高める。
	const int completedLevelUps = (std::max)(0, effectiveStats_.level - 1);
	return effectiveStats_.speed *
	       (1.0f + static_cast<float>(completedLevelUps) * kTenguSpeedIncreasePerLevel);
}

float PlayerComponent::GetOutgoingDamageMultiplier(float targetDistance) const {
	if (playerTypeName_ == "猫") {
		// 爪の攻撃が届く間合いまでは最大火力を維持し、その外側から15まで滑らかに減衰する。
		const float distanceRate = (std::clamp)(
			(targetDistance - kCatMaximumDamageRange) /
				(kCatMinimumDamageDistance - kCatMaximumDamageRange),
			0.0f,
			1.0f);
		return kCatMaximumDamageMultiplier +
		       (kCatMinimumDamageMultiplier - kCatMaximumDamageMultiplier) * distanceRate;
	}
	if (playerTypeName_ == "烏天狗") {
		// 特性適用後の速度10ごとに、最終与ダメージを1%増やす。
		const int speedSteps = static_cast<int>(std::floor(GetEffectiveSpeedPercent() / 10.0f));
		return 1.0f + static_cast<float>((std::max)(0, speedSteps)) * kTenguDamageIncreasePerTenSpeed;
	}
	return 1.0f;
}

int PlayerComponent::TakeDamage(float rawDamage) {
	if (demoControlled_ || rawDamage <= 0.0f || currentHealth_ <= 0.0f || damageInvincibilityTimer_ > 0.0f) {
		return 0;
	}

	// 防御力1につき被ダメージを1%下げ、最終ダメージは仕様どおり切り上げる。
	const float damageRate = 1.0f - (effectiveStats_.defense * 0.01f);
	const float clampedRate = damageRate < 0.0f ? 0.0f : damageRate;
	const int damage = static_cast<int>(std::ceil(rawDamage * clampedRate));
	if (damage <= 0) {
		return 0;
	}

	SetCurrentHealth(currentHealth_ - static_cast<float>(damage));
	damageInvincibilityTimer_ = effectiveStats_.damageInvincibilityDuration;
	// 防御計算後に実ダメージが発生した場合だけ、被弾用ポストエフェクトを再生する。
	PostEffect::GetInstance()->TriggerDamageVignette();
	return damage;
}

void PlayerComponent::AddExperience(int experience) {
	if (experience <= 0) {
		return;
	}
	// 経験値補正を割合として掛け、小数点以下は切り上げて整数経験値へ変換する。
	// 装備補正とショップの全体強化を加算してから、今回の獲得経験値へ倍率を掛ける。
	// 例: 通常100% + 全体強化30%なら、10EXP取得時にceil(10 * 1.3) = 13EXPを加算する。
	const float correctionRate = (effectiveStats_.experienceCorrection + globalExperienceBonusPercent_) / 100.0f;
	const double correctedValue = std::ceil(static_cast<double>(experience) * static_cast<double>(correctionRate));
	constexpr int maxExperience = (std::numeric_limits<int>::max)();
	const int correctedExperience = correctedValue >= static_cast<double>(maxExperience)
	                                    ? maxExperience
	                                    : static_cast<int>(correctedValue);
	if (correctedExperience <= 0) {
		return;
	}
	stats_.experience = correctedExperience > maxExperience - stats_.experience
	                        ? maxExperience
	                        : stats_.experience + correctedExperience;
	effectiveStats_.experience = stats_.experience;
	UpdateLevelFromExperience();
}

int PlayerComponent::GetRequiredExperienceForNextLevel(int level) {
	// Lv1から順に必要値を倍増させる。int上限を超える場合は最大値で打ち止めにする。
	const int safeLevel = level < 1 ? 1 : level;
	constexpr long long maxExperience = static_cast<long long>((std::numeric_limits<int>::max)());
	long long requiredExperience = 0;
	long long levelExperience = 4;
	for (int currentLevel = 1; currentLevel <= safeLevel; ++currentLevel) {
		requiredExperience += levelExperience;
		if (requiredExperience >= maxExperience) {
			return static_cast<int>(maxExperience);
		}
		if (currentLevel == safeLevel) {
			return static_cast<int>(requiredExperience);
		}
		if (levelExperience > maxExperience / 2) {
			return static_cast<int>(maxExperience);
		}
		levelExperience *= (1.8f);
	}
	return static_cast<int>(requiredExperience);
}

void PlayerComponent::UpdateLevelFromExperience() {
	// 現在経験値で到達済みのレベルまで一気に上げ、後続の報酬選択処理へ回数を渡す。
	constexpr int maxExperience = (std::numeric_limits<int>::max)();
	const int previousLevel = stats_.level;
	while (true) {
		const int requiredExperience = GetRequiredExperienceForNextLevel(stats_.level);
		if (requiredExperience == maxExperience || stats_.experience < requiredExperience) {
			break;
		}
		++stats_.level;
	}
	effectiveStats_.level = stats_.level;
	const int gainedLevels = stats_.level - previousLevel;
	pendingLevelUpCount_ += gainedLevels;
	if (playerTypeName_ == "巫女" && gainedLevels > 0) {
		// Lv5、10、15…をまたいだ回数だけ、通常分とは別の報酬選択を追加する。
		pendingLevelUpCount_ +=
			stats_.level / kMikoBonusRewardLevelInterval -
			previousLevel / kMikoBonusRewardLevelInterval;
	}
}

/// <summary>
/// 現在位置をスポーンポイントへ戻します。
/// </summary>
void PlayerComponent::ResetToSpawnPoint() {
	Entity* owner = GetOwner();
	if (!owner) {
		return;
	}

	owner->GetTransform().translate = spawnPoint_;
	currentMoveVelocity_ = {0.0f, 0.0f, 0.0f};
}
