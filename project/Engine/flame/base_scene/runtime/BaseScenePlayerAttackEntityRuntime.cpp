#include "base_scene/BaseScene.h"
#include "helpers/BaseSceneCollisionHelpers.h"
#include "helpers/BaseSceneEditorGeometry.h"
#include "repositories/EnemyStatusRepository.h"
#include "repositories/ParticlePresetRepository.h"
#include "MathConstants.h"
#include "model/ModelManager.h"
#include "repositories/PlayerStatusRepository.h"
#include "SceneManager.h"
#include "StringUtility.h"
#include "particle/TrailRenderer.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <Xinput.h>
#ifdef USE_IMGUI
#include "../../../../../imgui/ImGuizmo.h"
#endif
#include "base_scene/details/BaseSceneRuntimeDetails.h"

namespace {
constexpr const char* kPlayerHitSparkGroup = "RuntimePlayerHitSpark";
constexpr const char* kPlayerHitSparkTexture = "Resources/circle.png";

Vector4 GetPlayerHitSparkColor(PlayerAttackEntityMotionType motionType) {
	switch (motionType) {
	case PlayerAttackEntityMotionType::Magatama:
		return {1.0f, 0.72f, 0.24f, 0.96f};
	case PlayerAttackEntityMotionType::Orbit:
		return {0.24f, 0.72f, 1.0f, 0.96f};
	case PlayerAttackEntityMotionType::PoisonCloud:
		return {0.45f, 1.0f, 0.18f, 0.90f};
	case PlayerAttackEntityMotionType::FlameField:
		return {1.0f, 0.24f, 0.03f, 0.96f};
	case PlayerAttackEntityMotionType::SkyLaser:
		return {0.58f, 0.88f, 1.0f, 1.0f};
	case PlayerAttackEntityMotionType::Boomerang:
		return {1.0f, 0.92f, 0.62f, 0.96f};
	case PlayerAttackEntityMotionType::Ricochet:
		return {0.74f, 1.0f, 0.18f, 0.96f};
	case PlayerAttackEntityMotionType::ClawSlash:
		return {1.0f, 0.30f, 0.72f, 0.96f};
	case PlayerAttackEntityMotionType::Linear:
	default:
		return {1.0f, 0.86f, 0.30f, 0.96f};
	}
}

/// <summary>命中点から短い光条を散らし、ダメージ成立を即座に伝えます。</summary>
void EmitPlayerHitSpark(Entity* projectileObject, Entity* enemyObject, bool enemyDefeated) {
	PlayerAttackEntityComponent* projectile =
		projectileObject ? projectileObject->GetComponent<PlayerAttackEntityComponent>() : nullptr;
	EnemyComponent* enemy = enemyObject ? enemyObject->GetComponent<EnemyComponent>() : nullptr;
	if (!projectile || !enemy) {
		return;
	}

	ParticleManager* particleManager = ParticleManager::GetInstance();
	if (!particleManager->HasGroup(kPlayerHitSparkGroup)) {
		particleManager->CreateParticleGroup(
			kPlayerHitSparkGroup, kPlayerHitSparkTexture, kMeshTypeQuad);
		particleManager->SetGroupBlendMode(kPlayerHitSparkGroup, kBlendModeAdd);
	}

	Vector3 attackDirection = projectile->GetDirection();
	if (Length(attackDirection) <= MathConstants::kDirectionEpsilon) {
		attackDirection = enemyObject->GetTransform().translate - projectileObject->GetTransform().translate;
	}
	attackDirection.y = 0.0f;
	if (Length(attackDirection) > MathConstants::kDirectionEpsilon) {
		attackDirection = Normalize(attackDirection);
	} else {
		attackDirection = {0.0f, 0.0f, 1.0f};
	}

	const float enemyScale = (std::max)(0.5f, enemy->GetStats().sizeScale);
	const float sparkLength = (std::clamp)(0.10f + projectile->GetSize() * 0.07f, 0.11f, 0.24f);
	const PlayerAttackEntityMotionType motionType = projectile->GetMotionType();
	const bool isSustainedArea =
		motionType == PlayerAttackEntityMotionType::PoisonCloud ||
		motionType == PlayerAttackEntityMotionType::FlameField;

	ParticleEmitParam param{};
	// 横長のQuadをランダム回転し、丸い発光ではなく放射状の火花として見せる。
	param.scale = {sparkLength, sparkLength * 0.22f, 1.0f};
	param.endScale = {sparkLength * 0.12f, sparkLength * 0.03f, 1.0f};
	param.randomScaleRange = {sparkLength * 0.32f, sparkLength * 0.05f, 0.0f};
	param.baseVelocity = {attackDirection.x * 0.010f, 0.010f, attackDirection.z * 0.010f};
	param.randomVelocityRange = {0.035f, 0.022f, 0.035f};
	param.randomPositionRange = {0.07f * enemyScale, 0.10f * enemyScale, 0.07f * enemyScale};
	param.lifeTime = isSustainedArea ? 0.16f : 0.22f;
	param.randomRotateRange = {0.0f, 0.0f, MathConstants::kPi};
	param.isRandomRotate = true;
	param.color = GetPlayerHitSparkColor(motionType);
	param.endColor = {param.color.x, param.color.y * 0.55f, param.color.z * 0.35f, 0.0f};
	// 継続ダメージと撃破パーティクルには重ねすぎず、通常命中だけ最も強く見せる。
	param.count = isSustainedArea ? 3u : enemyDefeated ? 4u : 7u;
	param.isBillboard = true;

	const Vector3 impactPosition =
		enemyObject->GetTransform().translate +
		Vector3{-attackDirection.x * 0.18f * enemyScale, 0.32f * enemyScale,
		        -attackDirection.z * 0.18f * enemyScale};
	particleManager->Emit(kPlayerHitSparkGroup, impactPosition, param.count, param);
}
} // namespace

Entity* BaseScene::FindNearestEnemy(const Vector3& position) const {
	Entity* nearest = nullptr;
	float nearestDistance = 0.0f;
	for (Entity* object : sceneObjects_.GetEntitiesWith<EnemyComponent>()) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f) {
			continue;
		}

		const float distance = Length(object->GetTransform().translate - position);
		if (!nearest || distance < nearestDistance) {
			nearest = object;
			nearestDistance = distance;
		}
	}
	return nearest;
}

void BaseScene::CreateDamageNumber(
	float damage, const Vector3& enemyPosition, float enemySizeScale) {
	if (damage <= 0.0f) {
		return;
	}

	// 同時ヒットが続いてもGPU用テキストリソースを際限なく保持しない。
	constexpr size_t kMaximumPopupCount = 96;
	if (damageNumberPopups_.size() >= kMaximumPopupCount) {
		damageNumberPopups_.erase(damageNumberPopups_.begin());
	}

	std::ostringstream damageText;
	const float roundedDamage = std::round(damage);
	if (std::fabs(damage - roundedDamage) < 0.05f) {
		damageText << static_cast<int>(roundedDamage);
	} else {
		damageText << std::fixed << std::setprecision(1) << damage;
	}

	auto textObject = std::make_unique<Entity>();
	textObject->SetName("DamageNumber");
	TextComponent* text = textObject->AddComponent<TextComponent>();
	text->SetText(damageText.str());
	text->SetFontSize(30.0f);
	text->SetAnchor(TextComponent::Anchor::BottomCenter);
	text->SetColor({1.0f, 0.82f, 0.18f, 1.0f});

	DamageNumberPopup popup;
	popup.textObject = std::move(textObject);
	// sizeScaleに合わせて通常敵・大型ボスのどちらでもモデル上端付近から表示する。
	popup.worldPosition = enemyPosition +
		Vector3{0.0f, (std::max)(0.9f, enemySizeScale * 1.15f), 0.0f};
	const int offsetStep = static_cast<int>(nextDamageNumberIndex_++ % 5) - 2;
	popup.horizontalOffset = static_cast<float>(offsetStep) * 9.0f;
	damageNumberPopups_.push_back(std::move(popup));
}

void BaseScene::UpdateDamageNumbers() {
	constexpr float kDisplayDuration = 0.9f;
	const float deltaTime = GameTime::GetDeltaTime();
	for (DamageNumberPopup& popup : damageNumberPopups_) {
		popup.elapsedSeconds += deltaTime;
	}
	damageNumberPopups_.erase(
		std::remove_if(
			damageNumberPopups_.begin(), damageNumberPopups_.end(),
			[](const DamageNumberPopup& popup) {
				return popup.elapsedSeconds >= kDisplayDuration;
			}),
		damageNumberPopups_.end());
}

void BaseScene::DrawDamageNumbers() {
	constexpr float kDisplayDuration = 0.9f;
	Camera* camera = Object3dCommon::GetInstance()
		? Object3dCommon::GetInstance()->GetDefaultCamera()
		: nullptr;
	if (!camera) {
		return;
	}

	const float screenWidth = static_cast<float>(Input::GetInstance()->GetClientWidth());
	const float screenHeight = static_cast<float>(Input::GetInstance()->GetClientHeight());
	for (DamageNumberPopup& popup : damageNumberPopups_) {
		Vector3 ndc{};
		if (!popup.textObject || !ProjectToNdc(camera, popup.worldPosition, ndc) ||
			ndc.z < 0.0f || ndc.z > 1.0f ||
			ndc.x < -1.1f || ndc.x > 1.1f || ndc.y < -1.1f || ndc.y > 1.1f) {
			continue;
		}

		const float progress = (std::clamp)(popup.elapsedSeconds / kDisplayDuration, 0.0f, 1.0f);
		EulerTransform& transform = popup.textObject->GetTransform();
		transform.translate.x = (ndc.x + 1.0f) * 0.5f * screenWidth + popup.horizontalOffset;
		transform.translate.y = (1.0f - ndc.y) * 0.5f * screenHeight - 16.0f - 54.0f * progress;
		transform.translate.z = 0.0f;

		if (TextComponent* text = popup.textObject->GetComponent<TextComponent>()) {
			// 前半は読みやすく保持し、後半だけ滑らかに透明化する。
			const float alpha = progress <= 0.35f
				? 1.0f
				: 1.0f - (progress - 0.35f) / 0.65f;
			text->SetColor({1.0f, 0.82f, 0.18f, (std::max)(0.0f, alpha)});
		}
		popup.textObject->Draw2D();
	}
}

Entity* BaseScene::CreateRuntimePlayerAttackEntity(const PlayerAttackShotRequest& request) {
	if (request.motionType == PlayerAttackEntityMotionType::PoisonCloud) {
		// 五行結界はプレイヤーへ追従する持続型フィールドなので、攻撃間隔ごとに新規生成すると
		// 生存中の五芒星が同じ場所へ重なり続ける。既存の結界が消えるまでは1個だけ維持する。
		for (const auto& sceneObject : sceneObjects_) {
			PlayerAttackEntityComponent* existingProjectile = sceneObject->GetComponent<PlayerAttackEntityComponent>();
			if (existingProjectile && existingProjectile->IsEnabled() && !existingProjectile->IsExpired() &&
			    existingProjectile->GetMotionType() == PlayerAttackEntityMotionType::PoisonCloud) {
				return sceneObject.get();
			}
		}
	}

	auto object = std::make_unique<Entity>();
	object->SetName(MakeUniqueObjectName(request.attackName.empty() ? "PlayerAttack" : request.attackName));
	object->SetEditorType("PlayerAttackEntity");
	object->GetTransform().translate = request.position;
	object->GetTransform().scale = {request.size, request.size, request.size};

	PlayerAttackEntityComponent* projectile = object->AddComponent<PlayerAttackEntityComponent>();
	projectile->SetAttackName(request.attackName);
	projectile->SetLevel(request.level);
	projectile->SetDirection(request.direction);
	projectile->SetSpeed(request.speed);
	projectile->SetAttack(request.attack);
	projectile->SetSize(request.size);
	projectile->SetLifeTime(request.lifeTime);
	projectile->SetPierceCount(request.pierceCount);
	projectile->SetKnockback(request.knockback);
	projectile->SetInfinitePierce(request.infinitePierce);
	projectile->SetRepeatHitInterval(request.repeatHitInterval);
	projectile->SetHomingEnabled(request.homing);
	projectile->SetHomingAccuracy(request.homingAccuracy);
	projectile->SetMotionType(request.motionType);
	projectile->SetMotionAnchor(request.motionAnchor);
	projectile->SetSourceOwner(request.sourceOwner);
	projectile->SetOrbitAngleRadians(request.orbitAngleRadians);
	projectile->SetOrbitRadius(request.orbitRadius);
	projectile->SetOrbitHeight(request.orbitHeight);
	projectile->SetOrbitAngularSpeed(request.orbitAngularSpeed);
	projectile->SetTravelDistance(request.travelDistance);
	projectile->SetTravelOrigin(request.position);
	projectile->SetClawSlashIndex(request.clawSlashIndex);
	projectile->SetClawSlashCount(request.clawSlashCount);
	const PlayerAttackEntityPresentation presentation = projectile->GetMotionPresentation();
	if (request.repeatHitInterval <= 0.0f && presentation.repeatHitInterval > 0.0f) {
		projectile->SetRepeatHitInterval(presentation.repeatHitInterval);
	}
	if (request.homing) {
		projectile->SetHomingTarget(FindNearestEnemy(request.position));
	}
	if (presentation.aimAtNearestEnemyOnSpawn) {
		if (Entity* target = FindNearestEnemy(request.position)) {
			Vector3 toTarget = target->GetTransform().translate - request.position;
			toTarget.y = 0.0f;
			if (Length(toTarget) > MathConstants::kDirectionEpsilon) {
				projectile->SetDirection(toTarget);
			}
		}
	}

	if (request.visualId == "straight_slash" && request.motionType == PlayerAttackEntityMotionType::Linear) {
		// 共通の弾パラメーター設定後に描画だけを差し替え、他の攻撃のモデル・軌跡には影響させない。
		// 球モデルと直線のトレイルを使わず、初フレームから完成した斬撃を表示する。
		object->AddComponent<StraightSlashVisualComponent>(request.lifeTime);
		object->Update();
		sceneObjects_.push_back(std::move(object));
		++nextObjectId_;
		return sceneObjects_.back().get();
	}

	const std::string modelFilePath = request.modelFilePath.empty() ? "sphere.obj" : request.modelFilePath;
	if (!ModelManager::GetInstance()->FindModel(modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(modelFilePath);
	}
	object->GetTransform().scale = {
		request.size * presentation.scaleMultiplier.x,
		request.size * presentation.scaleMultiplier.y,
		request.size * presentation.scaleMultiplier.z
	};
	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	if (ModelManager::GetInstance()->FindModel(modelFilePath)) {
		object3d->SetModel(modelFilePath);
	} else {
		ModelManager::GetInstance()->LoadModel("sphere.obj");
		object3d->SetModel("sphere.obj");
	}
	object3d->SetEnabled(presentation.modelVisible);
	if (!presentation.modelVisible) {
		// 専用エフェクトへ差し替えた非表示モデルは、別の影描画経路からも影を残さない。
		object3d->SetShadowEnabled(false);
	}
	if (request.motionType == PlayerAttackEntityMotionType::Boomerang) {
		// 大幣は一枚のテクスチャで無理に色分けせず、柄・紙垂・結び目を同じTransformの
		// 別モデルとして重ねる。これにより紙垂を白く保ったまま柄を木色にできる。
		object3d->SetColor({0.43f, 0.18f, 0.055f, 1.0f});

		constexpr const char* kShideModel = "oonusa_shide.obj";
		constexpr const char* kTieModel = "oonusa_tie.obj";
		for (const char* partModel : {kShideModel, kTieModel}) {
			if (!ModelManager::GetInstance()->FindModel(partModel)) {
				ModelManager::GetInstance()->LoadModel(partModel);
			}
		}

		Object3dComponent* shide = object->AddComponent<Object3dComponent>();
		shide->SetModel(kShideModel);
		shide->SetColor({1.0f, 0.98f, 0.91f, 1.0f});
		Object3dComponent* tie = object->AddComponent<Object3dComponent>();
		tie->SetModel(kTieModel);
		tie->SetColor({0.72f, 0.035f, 0.025f, 1.0f});
	}
	if (request.motionType == PlayerAttackEntityMotionType::Ricochet) {
		// 生成りのお札本体へ朱色の印面モデルを少し浮かせて重ね、反射中も球ではなく
		// 一枚の護符として読めるようにする。黄緑色は軌跡へ残して攻撃の識別色を維持する。
		object3d->SetColor({1.00f, 0.92f, 0.68f, 1.0f});

		constexpr const char* kOfudaMarkModel = "ricochet_ofuda_mark.obj";
		if (!ModelManager::GetInstance()->FindModel(kOfudaMarkModel)) {
			ModelManager::GetInstance()->LoadModel(kOfudaMarkModel);
		}
		Object3dComponent* ofudaMark = object->AddComponent<Object3dComponent>();
		ofudaMark->SetModel(kOfudaMarkModel);
		ofudaMark->SetColor({0.82f, 0.035f, 0.018f, 1.0f});
		ofudaMark->SetShadowEnabled(false);
	}
	if (request.motionType == PlayerAttackEntityMotionType::Orbit) {
		// アイコンと同じ白地と朱色の陰陽模様にし、青は水の霊気、赤は炎として軌跡へ使う。
		object3d->SetColor({1.00f, 0.96f, 0.82f, 1.0f});
		object3d->SetShadowEnabled(false);

		constexpr const char* kYinYangDarkModel = "orbit_yinyang_dark.obj";
		if (!ModelManager::GetInstance()->FindModel(kYinYangDarkModel)) {
			ModelManager::GetInstance()->LoadModel(kYinYangDarkModel);
		}
		Object3dComponent* yinYangDark = object->AddComponent<Object3dComponent>();
		yinYangDark->SetModel(kYinYangDarkModel);
		yinYangDark->SetColor({0.82f, 0.018f, 0.012f, 1.0f});
		yinYangDark->SetShadowEnabled(false);

		// 編隊全体の環は一発目だけが描画を担当し、弾数分の重複描画を避ける。
		if (request.colorIndex == 0) {
			object->AddComponent<OrbitFormationVisualComponent>(
			    request.lifeTime, request.motionAnchor, request.orbitHeight, request.orbitRadius,
			    request.level == "super");
		}
	}
	if (request.motionType == PlayerAttackEntityMotionType::PoisonCloud ||
	    request.motionType == PlayerAttackEntityMotionType::FlameField) {
		// 持続フィールドは球モデルではなく加算合成の専用演出だけを表示するため、
		// 非表示モデルがシャドウマップへ描かれて円形の黒い影を残さないようにする。
		object3d->SetShadowEnabled(false);
	}
	if (presentation.useMagatamaPalette) {
		// 白い玉部分だけを6色に染め、黒い孔の象嵌は輪郭として残す。
		object3d->SetColor(GetMagatamaProjectileColor(request.colorIndex));
	} else if (presentation.overrideModelColor) {
		object3d->SetColor(presentation.modelColor);
	}
	if (presentation.addSkyLaserVisual) {
		// 球を引き伸ばした旧表示は隠し、加算合成の専用エフェクトでレーザーを描画する。
		object->AddComponent<SkyLaserVisualComponent>(request.lifeTime, request.size);
	}
	if (request.motionType == PlayerAttackEntityMotionType::PoisonCloud) {
		object->AddComponent<PoisonBarrierVisualComponent>(request.lifeTime, request.orbitHeight);
	}
	if (request.motionType == PlayerAttackEntityMotionType::FlameField) {
		object->AddComponent<FlameFieldVisualComponent>(request.lifeTime, request.size, request.orbitHeight);
	}
	if (presentation.addTrail) {
		TrailRendererComponent* trail = object->AddComponent<TrailRendererComponent>();
		trail->SetWidth((std::max)(presentation.minimumTrailWidth, request.size * presentation.trailWidthMultiplier));
		trail->SetLifeTime(presentation.trailLifeTime);
		trail->SetMinSegmentLength((std::max)(0.025f, request.size * 0.08f));
		if (presentation.trailUsesMotionAnchor) {
			trail->SetPositionReference(request.motionAnchor);
		}
		if (presentation.useMagatamaPalette) {
			Vector4 headColor = GetMagatamaProjectileColor(request.colorIndex);
			headColor.w = 0.95f;
			trail->SetHeadColor(headColor);
			trail->SetTailColor(GetMagatamaTrailTailColor(request.colorIndex));
		} else if (presentation.overrideTrailColors) {
			if (request.motionType == PlayerAttackEntityMotionType::Orbit) {
				// 隣り合う陰陽玉で炎と水を交互にし、通常アイコンの赤青の輪郭へ合わせる。
				const bool fireTrail = (request.colorIndex % 2) == 0;
				trail->SetHeadColor(fireTrail
				    ? Vector4{1.00f, 0.20f, 0.02f, 0.92f}
				    : Vector4{0.08f, 0.62f, 1.00f, 0.92f});
				trail->SetTailColor(fireTrail
				    ? Vector4{0.72f, 0.01f, 0.00f, 0.0f}
				    : Vector4{0.01f, 0.12f, 0.82f, 0.0f});
			} else {
				trail->SetHeadColor(presentation.trailHeadColor);
				trail->SetTailColor(presentation.trailTailColor);
			}
		}
		if (presentation.addCoreTrail) {
			// 細い白熱線を外光へ重ね、一本ごとの切っ先を読み取りやすくする。
			TrailRendererComponent* coreTrail = object->AddComponent<TrailRendererComponent>();
			coreTrail->SetWidth((std::max)(0.045f, request.size * 0.085f));
			coreTrail->SetLifeTime(0.16f);
			coreTrail->SetMinSegmentLength((std::max)(0.018f, request.size * 0.055f));
			coreTrail->SetPositionReference(request.motionAnchor);
			coreTrail->SetHeadColor({1.0f, 1.0f, 0.86f, 1.0f});
			coreTrail->SetTailColor({1.0f, 0.28f, 0.02f, 0.0f});
		}
	}
	if (presentation.addGlowEmitter) {
		// 環境反射ではなく、加算合成パーティクルを弾から漏れ出す光として連続発生させる。
		constexpr const char* kGlowParticleGroup = "MagatamaGlow";
		constexpr const char* kGlowTexture = "Resources/circle.png";
		if (!ParticleManager::GetInstance()->HasGroup(kGlowParticleGroup)) {
			ParticleManager::GetInstance()->CreateParticleGroup(kGlowParticleGroup, kGlowTexture, kMeshTypeQuad);
		}

		ParticleEmitterComponent* glowEmitter = object->AddComponent<ParticleEmitterComponent>();
		glowEmitter->SetGroupName(kGlowParticleGroup);
		glowEmitter->SetTexture(kGlowTexture);
		glowEmitter->SetBlendMode(kBlendModeAdd);
		glowEmitter->SetFrequency(0.020f);

		Vector4 glowColor = GetMagatamaProjectileColor(request.colorIndex);
		glowColor.x *= 1.35f;
		glowColor.y *= 1.35f;
		glowColor.z *= 1.35f;
		glowColor.w = 0.55f;

		ParticleEmitParam glowParam;
		const float glowScale = (std::max)(0.16f, request.size * 0.82f);
		glowParam.scale = {glowScale, glowScale, glowScale};
		glowParam.endScale = {glowScale * 0.12f, glowScale * 0.12f, glowScale * 0.12f};
		glowParam.baseVelocity = {0.0f, 0.008f, 0.0f};
		glowParam.randomVelocityRange = {0.018f, 0.018f, 0.018f};
		const float leakRadius = request.size * 0.42f;
		glowParam.randomPositionRange = {leakRadius, leakRadius, leakRadius};
		glowParam.lifeTime = 0.36f;
		glowParam.color = glowColor;
		glowParam.endColor = {glowColor.x * 0.55f, glowColor.y * 0.55f, glowColor.z * 0.55f, 0.0f};
		glowParam.randomScaleRange = {glowScale * 0.22f, glowScale * 0.22f, glowScale * 0.22f};
		glowParam.count = 2;
		glowParam.isBillboard = true;
		glowEmitter->SetParam(glowParam);
		// EntityComponent::Update より前に即時発生させるため、初回分も弾の生成位置へ同期しておく。
		glowEmitter->SetTranslate(request.position);
		glowEmitter->Emit();
	}

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

/// <summary>プレイヤー弾と敵の精密判定を行い、命中時の副作用を一度だけ適用します。</summary>
PlayerProjectileHitResult CombatSystem::ResolvePlayerProjectileHit(
	Entity* projectileObject,
	Entity* enemyObject) {
	PlayerProjectileHitResult result;
	// 無効な組み合わせと同じ弾による再命中を、距離計算より前に除外する。
	PlayerAttackEntityComponent* projectile =
		projectileObject ? projectileObject->GetComponent<PlayerAttackEntityComponent>() : nullptr;
	EnemyComponent* enemy = enemyObject ? enemyObject->GetComponent<EnemyComponent>() : nullptr;
	if (!projectile || !projectile->IsEnabled() || projectile->IsExpired() ||
		!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f ||
		projectile->HasHitObject(enemyObject)) {
		return result;
	}

	++collisionDebugStats_.playerProjectileNarrowPhaseTests;
	const PlayerAttackEntityPresentation presentation = projectile->GetMotionPresentation();
	// 縦長攻撃は高さを無視したXZ平面、通常弾は3次元距離で判定する。
	float distance = Length(enemyObject->GetTransform().translate - projectileObject->GetTransform().translate);
	float hitRadius = projectile->GetSize() + 0.5f * enemy->GetStats().sizeScale;
	if (presentation.usesVerticalHitArea) {
		Vector3 horizontalDifference =
			enemyObject->GetTransform().translate - projectileObject->GetTransform().translate;
		horizontalDifference.y = 0.0f;
		distance = Length(horizontalDifference);
		hitRadius = projectile->GetSize() * 0.35f + 0.5f * enemy->GetStats().sizeScale;
	}
	if (distance > hitRadius) {
		return result;
	}

	// 命中が確定してから副作用を適用し、最後に再命中防止対象として登録する。
	float outgoingDamage = projectile->GetAttack();
	if (Entity* sourceOwner = projectile->GetSourceOwner()) {
		if (PlayerComponent* player = sourceOwner->GetComponent<PlayerComponent>()) {
			Vector3 sourceToEnemy =
				enemyObject->GetTransform().translate - sourceOwner->GetTransform().translate;
			sourceToEnemy.y = 0.0f;
			outgoingDamage *= player->GetOutgoingDamageMultiplier(Length(sourceToEnemy));
		}
	}
	const float healthBeforeHit = enemy->GetCurrentHealth();
	enemy->TakeDamage(outgoingDamage);
	result.damageDealt = healthBeforeHit - enemy->GetCurrentHealth();
	if (projectile->GetKnockback() > 0) {
		Vector3 knockbackDirection =
			enemyObject->GetTransform().translate - projectileObject->GetTransform().translate;
		knockbackDirection.y = 0.0f;
		if (Length(knockbackDirection) <= MathConstants::kDirectionEpsilon) {
			knockbackDirection = projectile->GetDirection();
			knockbackDirection.y = 0.0f;
		}
		enemy->ApplyKnockback(knockbackDirection, projectile->GetKnockback());
	}
	projectile->RegisterHitObject(enemyObject);
	result.hit = true;
	result.enemyDefeated = enemy->GetCurrentHealth() <= 0.0f;
	return result;
}

void BaseScene::UpdatePlayerAttackEntityHits() {
	CollisionSpatialGrid& collisionSpatialGrid_ = combatSystem_.GetSpatialGrid();
	CombatSystem::DebugStats& collisionDebugStats_ = combatSystem_.GetDebugStats();
	std::vector<ExperienceDropRequest> experienceDropRequests;
	std::vector<BossUpgradeDropRequest> bossUpgradeDropRequests;
	// sceneObjects_の走査中に要素を追加すると反復子が無効になるため、生成要求を一旦蓄積する。
	std::vector<ItemDropRequest> itemDropRequests;
	Camera* camera = Object3dCommon::GetInstance() ? Object3dCommon::GetInstance()->GetDefaultCamera() : nullptr;
	for (const auto& projectileObject : sceneObjects_) {
		PlayerAttackEntityComponent* projectile = projectileObject->GetComponent<PlayerAttackEntityComponent>();
		if (!projectile || projectile->IsExpired() ||
		    !projectile->GetMotionPresentation().ricochets) {
			continue;
		}

		Vector3& position = projectileObject->GetTransform().translate;
		Vector3 currentNdc{};
		Vector3 aheadNdc{};
		// ワールド座標を画面座標へ投影し、左右端・上下端に応じて画面上の進行成分を反転する。
		if (camera && ProjectToNdc(camera, position, currentNdc) &&
		    ProjectToNdc(camera, position + projectile->GetDirection(), aheadNdc)) {
			const float edgeInset = (std::clamp)(0.025f + projectile->GetSize() * 0.02f, 0.025f, 0.20f);
			const float boundary = 1.0f - edgeInset;
			const bool hitHorizontalEdge = currentNdc.x < -boundary || currentNdc.x > boundary;
			const bool hitVerticalEdge = currentNdc.y < -boundary || currentNdc.y > boundary;
			if (hitHorizontalEdge || hitVerticalEdge) {
				Vector2 screenDirection{aheadNdc.x - currentNdc.x, aheadNdc.y - currentNdc.y};
				if (hitHorizontalEdge) {
					screenDirection.x = -screenDirection.x;
				}
				if (hitVerticalEdge) {
					screenDirection.y = -screenDirection.y;
				}
				const Vector2 clampedNdc{
				    (std::clamp)(currentNdc.x, -boundary, boundary),
				    (std::clamp)(currentNdc.y, -boundary, boundary)
				};
				const Vector2 reflectedNdc{clampedNdc.x + screenDirection.x, clampedNdc.y + screenDirection.y};
				Vector3 boundaryPoint{};
				Vector3 reflectedPoint{};
				if (IntersectScreenRayToHeight(camera, clampedNdc, position.y, boundaryPoint) &&
				    IntersectScreenRayToHeight(camera, reflectedNdc, position.y, reflectedPoint)) {
					position = boundaryPoint;
					projectile->SetDirection(reflectedPoint - boundaryPoint);
				}
			}
		}

		// 障害物との反射は弾を球として扱い、衝突面の法線から反射ベクトルを求める。
		// PushBack は「そのコライダー自身を動かすか」の設定なので、静的な壁では false になる。
		// プレイヤーを押し戻す有効コライダーは PushBack の値にかかわらず反射面として扱う。
		SphereColliderShape projectileSphere{position, projectile->GetSize()};
		for (const auto& obstacleObject : sceneObjects_) {
			if (obstacleObject.get() == projectileObject.get() ||
			    obstacleObject->GetComponent<PlayerComponent>() ||
			    obstacleObject->GetComponent<EnemyComponent>() ||
			    obstacleObject->GetComponent<PlayerAttackEntityComponent>()) {
				continue;
			}

			Vector3 surfaceNormal{};
			float penetration = 0.0f;
			bool collided = false;
			if (OBBColliderComponent* obstacle = obstacleObject->GetComponent<OBBColliderComponent>();
			    obstacle && obstacle->IsEnabled()) {
				collided = BaseSceneCollisionHelpers::CalculateOBBSpherePushBack(obstacle->GetWorldOBB(), projectileSphere, surfaceNormal, penetration);
			}
			if (!collided) {
				if (SphereColliderComponent* obstacle = obstacleObject->GetComponent<SphereColliderComponent>();
				    obstacle && obstacle->IsEnabled()) {
					Vector3 projectileToObstacle{};
					collided = BaseSceneCollisionHelpers::CalculateSphereSpherePushBack(projectileSphere, obstacle->GetWorldSphere(), projectileToObstacle, penetration);
					surfaceNormal = -1.0f * projectileToObstacle;
				}
			}
			if (!collided) {
				continue;
			}

			surfaceNormal.y = 0.0f;
			if (Length(surfaceNormal) <= MathConstants::kDirectionEpsilon) {
				continue;
			}
			surfaceNormal = Normalize(surfaceNormal);
			const float incomingAmount = Dot(projectile->GetDirection(), surfaceNormal);
			if (incomingAmount >= 0.0f) {
				continue;
			}
			// めり込みを解消してから反射させ、同じ面へ連続衝突することを防ぐ。
			position = position + (penetration + 0.01f) * surfaceNormal;
			projectile->SetDirection(projectile->GetDirection() - (2.0f * incomingAmount) * surfaceNormal);
			projectileObject->GetTransform().rotate.y = std::atan2(projectile->GetDirection().x, projectile->GetDirection().z);
			break;
		}
	}
	// 生存中の敵だけを空間グリッドへ登録し、各弾が近傍の敵だけを精密判定する。
	collisionSpatialGrid_.Clear();
	collisionDebugStats_.playerProjectileCandidates = 0;
	collisionDebugStats_.playerProjectileNarrowPhaseTests = 0;
	for (const auto& enemyObject : sceneObjects_) {
		EnemyComponent* enemy = enemyObject->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f) {
			continue;
		}
		const float enemyRadius = 0.5f * enemy->GetStats().sizeScale;
		collisionSpatialGrid_.Insert(
			enemyObject.get(),
			CollisionSpatialGrid::FromCenterRadius(enemyObject->GetTransform().translate, enemyRadius));
	}

	for (const auto& projectileObject : sceneObjects_) {
		PlayerAttackEntityComponent* projectile = projectileObject->GetComponent<PlayerAttackEntityComponent>();
		if (!projectile || !projectile->IsEnabled() || projectile->IsExpired()) {
			continue;
		}

		const PlayerAttackEntityPresentation presentation = projectile->GetMotionPresentation();
		const float projectileQueryRadius = presentation.usesVerticalHitArea
			? projectile->GetSize() * 0.35f
			: projectile->GetSize();
		const std::vector<Entity*> enemyCandidates = collisionSpatialGrid_.Query(
			CollisionSpatialGrid::FromCenterRadius(
				projectileObject->GetTransform().translate, projectileQueryRadius));
		collisionDebugStats_.playerProjectileCandidates += enemyCandidates.size();

		for (Entity* enemyObject : enemyCandidates) {
			const PlayerProjectileHitResult hitResult =
				combatSystem_.ResolvePlayerProjectileHit(projectileObject.get(), enemyObject);
			if (!hitResult.hit) {
				continue;
			}
			EnemyComponent* enemy = enemyObject->GetComponent<EnemyComponent>();
			EmitPlayerHitSpark(projectileObject.get(), enemyObject, hitResult.enemyDefeated);
			CreateDamageNumber(
				hitResult.damageDealt,
				enemyObject->GetTransform().translate,
				enemy->GetStats().sizeScale);
			if (hitResult.enemyDefeated) {
				// 生存中の敵だけがこの分岐へ入るため、1体につき一度だけ加算される。
				gameplaySession_.RecordEnemyDefeat();
				if (sceneManager_ && sceneManager_->IsDemoPlayActive()) break;
				// 走査中にsceneObjects_へ追加しないよう、抽選結果を生成要求として一旦蓄積する。
				const LootDropResult loot = lootDropService_.Roll(
					*enemy, enemyObject->GetTransform().translate);
				experienceDropRequests.push_back(loot.experience);
				itemDropRequests.insert(
					itemDropRequests.end(), loot.items.begin(), loot.items.end());
				if (loot.hasBossUpgrade) {
					bossUpgradeDropRequests.push_back(loot.bossUpgrade);
				}
			}
			break;
		}
	}

	for (const ExperienceDropRequest& request : experienceDropRequests) {
		CreateRuntimeExperience(request.stats, request.position, request.target);
	}
	for (const BossUpgradeDropRequest& request : bossUpgradeDropRequests) {
		CreateRuntimeBossUpgradeDrop(request.position, request.target, request.upgradeCount);
	}
	for (const ItemDropRequest& request : itemDropRequests) {
		CreateRuntimeItemDrop(request.type, request.position, request.target, request.healAmount, request.moneyAmount);
	}
}

void BaseScene::CleanupExpiredPlayerAttackEntities() {
	sceneObjects_.erase(
	    std::remove_if(sceneObjects_.begin(), sceneObjects_.end(), [](const std::unique_ptr<Entity>& object) {
		    PlayerAttackEntityComponent* projectile = object->GetComponent<PlayerAttackEntityComponent>();
		    if (projectile) {
			    // 弾全体が画面端を抜けてから消えるよう、表示サイズを余白へ反映する。
			    const float viewMargin = 0.05f + projectile->GetSize() * 0.02f;
			    // 周回弾や反射弾は専用挙動を維持し、画面外へ飛び去る弾だけを破棄する。
			    const bool shouldExpireOutsideView = projectile->GetMotionPresentation().expiresOutsideView;
			    if (projectile->IsExpired() ||
				    (shouldExpireOutsideView && IsPointOutsideView(object->GetTransform().translate, viewMargin))) {
				    return true;
			    }
		    }
		    EnemyAttackEntityComponent* enemyProjectile = object->GetComponent<EnemyAttackEntityComponent>();
		    if (enemyProjectile && enemyProjectile->IsExpired()) {
			    return true;
		    }
		    EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		    if (enemy && enemy->GetCurrentHealth() <= 0.0f && enemy->IsDeathEffectFinished()) {
			    return true;
		    }
		    ExperienceComponent* experience = object->GetComponent<ExperienceComponent>();
		    if (experience && (experience->IsCollected() || experience->IsConsumedByCompression())) {
			    return true;
		    }
		    ItemDropComponent* item = object->GetComponent<ItemDropComponent>();
		    return item && item->IsCollected() && item->IsEffectApplied();
	    }),
	    sceneObjects_.end()
	);
	sceneEditorState_.ClampSelection(sceneObjects_.size());
}


