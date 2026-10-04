#include "BaseScene.h"
#include "repositories/EnemyStatusRepository.h"
#include "MathConstants.h"
#include "model/ModelManager.h"
#include "repositories/PlayerStatusRepository.h"
#include "helpers/SceneJsonUtility.h"
#include "SceneManager.h"
#include "StringUtility.h"

#include <filesystem>
#include <fstream>

using SceneJsonUtility::JsonToVector3;
using SceneJsonUtility::JsonToVector4;
using SceneJsonUtility::Vector3ToJson;
using SceneJsonUtility::Vector4ToJson;
using StringUtility::Utf8ToPath;

namespace {
bool ApplyObjectOverride(nlohmann::json& objects, const nlohmann::json& objectOverride) {
	if (!objects.is_array() || !objectOverride.is_object()) {
		return false;
	}
	const std::string targetName = objectOverride.value("name", std::string());
	if (targetName.empty()) {
		return false;
	}
	for (nlohmann::json& object : objects) {
		if (object.value("name", std::string()) == targetName) {
			object.merge_patch(objectOverride);
			return true;
		}
		if (object.contains("children") && ApplyObjectOverride(object["children"], objectOverride)) {
			return true;
		}
	}
	return false;
}

bool LoadSceneJsonWithInheritance(const std::filesystem::path& filePath, nlohmann::json& root, int depth = 0) {
	if (depth > 8) {
		return false;
	}
	std::ifstream input(filePath);
	if (!input) {
		return false;
	}
	nlohmann::json source;
	try {
		input >> source;
	} catch (const nlohmann::json::exception&) {
		return false;
	}

	const std::string extendsFile = source.value("extends", std::string());
	if (extendsFile.empty()) {
		root = std::move(source);
		return true;
	}

	const std::filesystem::path extendsPath = Utf8ToPath(extendsFile);
	if (extendsPath.filename() != extendsPath || extendsPath.extension() != ".json") {
		return false;
	}
	if (!LoadSceneJsonWithInheritance(filePath.parent_path() / extendsPath, root, depth + 1)) {
		return false;
	}

	nlohmann::json overlay = source;
	overlay.erase("extends");
	overlay.erase("objectOverrides");
	overlay.erase("objectAdditions");
	root.merge_patch(overlay);

	for (const nlohmann::json& objectOverride : source.value("objectOverrides", nlohmann::json::array())) {
		ApplyObjectOverride(root["objects"], objectOverride);
	}
	for (const nlohmann::json& addition : source.value("objectAdditions", nlohmann::json::array())) {
		root["objects"].push_back(addition);
	}
	return true;
}

void SaveComponentGravity(nlohmann::json& componentJson, Component* component) {
	if (!component) {
		return;
	}
	componentJson["gravity"]["enabled"] = component->IsGravityEnabled();
	componentJson["gravity"]["strength"] = component->GetGravityStrength();
}

void LoadComponentGravity(const nlohmann::json& componentJson, Component* component) {
	if (!component) {
		return;
	}
	const nlohmann::json gravityJson = componentJson.value("gravity", nlohmann::json::object());
	component->SetGravityEnabled(gravityJson.value("enabled", component->IsGravityEnabled()));
	component->SetGravityStrength(gravityJson.value("strength", component->GetGravityStrength()));
	component->ResetGravityVelocity();
}
}

void BaseScene::SaveEditorObjects() {
	nlohmann::json root;
	root["scene"] = sceneName_;
	root["nextObjectId"] = nextObjectId_;
	root["activeCamera"] = activeCameraObjectName_;
	root["skyBox"]["enabled"] = isEditorSkyBoxEnabled_;
	root["skyBox"]["textureFilePath"] = skyBoxTextureFilePath_;
	const AtmosphereSettings& atmosphere = AtmosphereSystem::GetInstance()->GetSettings();
	root["atmosphere"]["enabled"] = atmosphere.enabled;
	root["atmosphere"]["sunDirection"] = Vector3ToJson(atmosphere.sunDirection);
	root["atmosphere"]["sunAngularRadius"] = atmosphere.sunAngularRadius;
	root["atmosphere"]["sunIntensity"] = atmosphere.sunIntensity;
	root["atmosphere"]["rayleighColor"] = Vector3ToJson(atmosphere.rayleighColor);
	root["atmosphere"]["rayleighStrength"] = atmosphere.rayleighStrength;
	root["atmosphere"]["mieColor"] = Vector3ToJson(atmosphere.mieColor);
	root["atmosphere"]["mieStrength"] = atmosphere.mieStrength;
	root["atmosphere"]["mieG"] = atmosphere.mieG;
	root["atmosphere"]["atmosphereDensity"] = atmosphere.atmosphereDensity;
	root["atmosphere"]["horizonColor"] = Vector3ToJson(atmosphere.horizonColor);
	root["atmosphere"]["distanceFogDensity"] = atmosphere.distanceFogDensity;
	root["atmosphere"]["heightFogDensity"] = atmosphere.heightFogDensity;
	root["atmosphere"]["aerialPerspectiveStrength"] = atmosphere.aerialPerspectiveStrength;
	root["objects"] = nlohmann::json::array();

	std::function<nlohmann::json(GameObject*)> makeObjectJson = [&](GameObject* object) {
		const EulerTransform& transform = object->GetTransform();

		nlohmann::json objectJson;
		objectJson["name"] = object->GetName();
		objectJson["type"] = object->GetEditorType();
		if (object->GetEditorType().starts_with("LoadedModel:")) {
			objectJson["type"] = "LoadedModel";
			objectJson["model"] = object->GetEditorType().substr(std::string("LoadedModel:").size());
		}
		if (object->GetEditorType().starts_with("AnimatedModel:")) {
			objectJson["type"] = "AnimatedModel";
			objectJson["model"] = object->GetEditorType().substr(std::string("AnimatedModel:").size());
		}
		objectJson["parent"] = object->GetParentName();
		if (Player* player = object->GetComponent<Player>()) {
			objectJson["type"] = "Player";
			objectJson["player"]["enabled"] = player->IsEnabled();
			SaveComponentGravity(objectJson["player"], player);
			objectJson["player"]["typeName"] = player->GetPlayerTypeName();
			objectJson["player"]["spawnPoint"] = Vector3ToJson(player->GetSpawnPoint());
			objectJson["player"]["currentHealth"] = player->GetMaxHealth();
			objectJson["player"]["level"] = player->GetStats().level;
			objectJson["player"]["experience"] = player->GetStats().experience;
			objectJson["player"]["moveSpeed"] = player->GetMoveSpeed();
			objectJson["player"]["model"] = player->GetModelFilePath();
			objectJson["player"]["isAnimationModel"] = player->GetIsAnimationModel();
			if (PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>()) {
				objectJson["playerAttack"]["enabled"] = attack->IsEnabled();
				SaveComponentGravity(objectJson["playerAttack"], attack);
			}
		}
		if (EnemyComponent* enemy = object->GetComponent<EnemyComponent>()) {
			objectJson["type"] = "Enemy";
			objectJson["enemy"]["enabled"] = enemy->IsEnabled();
			SaveComponentGravity(objectJson["enemy"], enemy);
			objectJson["enemy"]["typeName"] = enemy->GetEnemyTypeName();
			objectJson["enemy"]["currentHealth"] = enemy->GetCurrentHealth();
			objectJson["enemy"]["targetName"] = enemy->GetTargetName();
		}
		if (SpriteComponent* spriteComponent = object->GetComponent<SpriteComponent>()) {
			objectJson["sprite"]["enabled"] = spriteComponent->IsEnabled();
			SaveComponentGravity(objectJson["sprite"], spriteComponent);
			objectJson["sprite"]["textureFilePath"] = spriteComponent->GetTextureFilePath();
			objectJson["sprite"]["color"] = Vector4ToJson(spriteComponent->GetColor());
			objectJson["sprite"]["size"] = nlohmann::json::array({spriteComponent->GetSize().x, spriteComponent->GetSize().y});
		}
		if (TextComponent* textComponent = object->GetComponent<TextComponent>()) {
			objectJson["type"] = "Text";
			objectJson["text"]["enabled"] = textComponent->IsEnabled();
			SaveComponentGravity(objectJson["text"], textComponent);
			objectJson["text"]["value"] = textComponent->GetText();
			objectJson["text"]["fontName"] = textComponent->GetFontName();
			objectJson["text"]["fontSize"] = textComponent->GetFontSize();
			objectJson["text"]["anchor"] = static_cast<int>(textComponent->GetAnchor());
			objectJson["text"]["color"] = Vector4ToJson(textComponent->GetColor());
		}
		if (CameraComponent* cameraComponent = object->GetComponent<CameraComponent>()) {
			objectJson["camera"]["enabled"] = cameraComponent->IsEnabled();
			SaveComponentGravity(objectJson["camera"], cameraComponent);
			objectJson["camera"]["fovY"] = cameraComponent->GetFovY();
			objectJson["camera"]["nearClip"] = cameraComponent->GetNearClip();
			objectJson["camera"]["farClip"] = cameraComponent->GetFarClip();
			objectJson["camera"]["followTarget"] = cameraComponent->GetFollowTargetName();
			objectJson["camera"]["followOffset"] = Vector3ToJson(cameraComponent->GetFollowOffset());
			objectJson["camera"]["localOffset"] = Vector3ToJson(cameraComponent->GetLocalOffset());
			objectJson["camera"]["overrideRotationEnabled"] = cameraComponent->GetOverrideRotationEnabled();
			objectJson["camera"]["overrideRotation"] = Vector3ToJson(cameraComponent->GetOverrideRotation());
		}
		if (OBBColliderComponent* collider = object->GetComponent<OBBColliderComponent>()) {
			objectJson["obbCollider"]["enabled"] = collider->IsEnabled();
			SaveComponentGravity(objectJson["obbCollider"], collider);
			objectJson["obbCollider"]["centerOffset"] = Vector3ToJson(collider->GetCenterOffset());
			objectJson["obbCollider"]["halfSize"] = Vector3ToJson(collider->GetHalfSize());
			objectJson["obbCollider"]["drawDebug"] = collider->GetDrawDebug();
			// 衝突時に位置補正を行うかをシーン単位で保存します。
			objectJson["obbCollider"]["pushBack"] = collider->GetPushBackEnabled();
		}
		if (SphereColliderComponent* collider = object->GetComponent<SphereColliderComponent>()) {
			objectJson["sphereCollider"]["enabled"] = collider->IsEnabled();
			SaveComponentGravity(objectJson["sphereCollider"], collider);
			objectJson["sphereCollider"]["centerOffset"] = Vector3ToJson(collider->GetCenterOffset());
			objectJson["sphereCollider"]["radius"] = collider->GetRadius();
			objectJson["sphereCollider"]["drawDebug"] = collider->GetDrawDebug();
			// 衝突時に位置補正を行うかをシーン単位で保存します。
			objectJson["sphereCollider"]["pushBack"] = collider->GetPushBackEnabled();
		}
		if (Object3dComponent* object3dComponent = object->GetComponent<Object3dComponent>()) {
			objectJson["object3d"]["enabled"] = object3dComponent->IsEnabled();
			SaveComponentGravity(objectJson["object3d"], object3dComponent);
			objectJson["object3d"]["material"]["color"] = Vector4ToJson(object3dComponent->GetColor());
			objectJson["object3d"]["material"]["emissionColor"] = Vector3ToJson(object3dComponent->GetEmissionColor());
			objectJson["object3d"]["material"]["emissionIntensity"] = object3dComponent->GetEmissionIntensity();
			objectJson["object3d"]["material"]["receiveLighting"] = object3dComponent->GetLightingEnabled();
			objectJson["object3d"]["castShadow"] = object3dComponent->GetShadowEnabled();
			objectJson["object3d"]["modelTextureFilePath"] = object3dComponent->GetModelTextureFilePath();
			objectJson["object3d"]["drawSkeleton"] = object3dComponent->GetDrawSkeleton();
			objectJson["object3d"]["animationPlaying"] = object3dComponent->GetAnimationPlaying();
			// 同じモデルを使うオブジェクトごとに、選択中のアニメーションクリップ名を保存する。
			objectJson["object3d"]["animationName"] = object3dComponent->GetAnimationName();
			objectJson["object3d"]["isPointLight"] = object3dComponent->GetIsPointLightSet();
			objectJson["object3d"]["pointLight"]["color"] = Vector4ToJson(object3dComponent->GetPointLightColor());
			objectJson["object3d"]["pointLight"]["position"] = Vector3ToJson(object3dComponent->GetPointLightPosition());
			objectJson["object3d"]["pointLight"]["intensity"] = object3dComponent->GetPointLightIntensity();
			objectJson["object3d"]["pointLight"]["radius"] = object3dComponent->GetPointLightRadius();
			objectJson["object3d"]["pointLight"]["decay"] = object3dComponent->GetPointLightDecay();
		}
		if (PointLightComponent* pointLight = object->GetComponent<PointLightComponent>()) {
			objectJson["pointLightComponent"]["enabled"] = pointLight->IsEnabled();
			objectJson["pointLightComponent"]["color"] = Vector4ToJson(pointLight->GetColor());
			objectJson["pointLightComponent"]["intensity"] = pointLight->GetIntensity();
			objectJson["pointLightComponent"]["radius"] = pointLight->GetRadius();
			objectJson["pointLightComponent"]["decay"] = pointLight->GetDecay();
			objectJson["pointLightComponent"]["positionOffset"] = Vector3ToJson(pointLight->GetPositionOffset());
			objectJson["pointLightComponent"]["useMaterialEmissionColor"] = pointLight->GetUseMaterialEmissionColor();
		}
		if (GlowBillboardComponent* glow = object->GetComponent<GlowBillboardComponent>()) {
			objectJson["glowBillboard"]["enabled"] = glow->IsEnabled();
			objectJson["glowBillboard"]["textureFilePath"] = glow->GetTexture();
			objectJson["glowBillboard"]["innerColor"] = Vector4ToJson(glow->GetInnerColor());
			objectJson["glowBillboard"]["outerColor"] = Vector4ToJson(glow->GetOuterColor());
			objectJson["glowBillboard"]["innerSize"] = glow->GetInnerSize();
			objectJson["glowBillboard"]["outerSize"] = glow->GetOuterSize();
			objectJson["glowBillboard"]["innerIntensity"] = glow->GetInnerIntensity();
			objectJson["glowBillboard"]["outerIntensity"] = glow->GetOuterIntensity();
			objectJson["glowBillboard"]["pulseAmount"] = glow->GetPulseAmount();
			objectJson["glowBillboard"]["pulseSpeed"] = glow->GetPulseSpeed();
			objectJson["glowBillboard"]["positionOffset"] = Vector3ToJson(glow->GetPositionOffset());
		}
		if (ParticleEmitterComponent* emitter = object->GetComponent<ParticleEmitterComponent>()) {
			const ParticleEmitParam param = emitter->GetParam();
			objectJson["particleEmitter"]["enabled"] = emitter->IsEnabled();
			SaveComponentGravity(objectJson["particleEmitter"], emitter);
			objectJson["particleEmitter"]["groupName"] = emitter->GetGroupName();
			objectJson["particleEmitter"]["textureFilePath"] = emitter->GetTextureFilePath();
			objectJson["particleEmitter"]["isActive"] = emitter->GetIsActive();
			objectJson["particleEmitter"]["frequency"] = emitter->GetFrequency();
			objectJson["particleEmitter"]["blendMode"] = static_cast<int>(emitter->GetBlendMode());
			objectJson["particleEmitter"]["meshType"] = static_cast<int>(emitter->GetMeshType());
			objectJson["particleEmitter"]["param"]["count"] = param.count;
			objectJson["particleEmitter"]["param"]["lifeTime"] = param.lifeTime;
			objectJson["particleEmitter"]["param"]["scale"] = Vector3ToJson(param.scale);
			objectJson["particleEmitter"]["param"]["endScale"] = Vector3ToJson(param.endScale);
			objectJson["particleEmitter"]["param"]["baseVelocity"] = Vector3ToJson(param.baseVelocity);
			objectJson["particleEmitter"]["param"]["randomVelocityRange"] = Vector3ToJson(param.randomVelocityRange);
			objectJson["particleEmitter"]["param"]["acceleration"] = Vector3ToJson(param.acceleration);
			objectJson["particleEmitter"]["param"]["randomPositionRange"] = Vector3ToJson(param.randomPositionRange);
			objectJson["particleEmitter"]["param"]["baseRotate"] = Vector3ToJson(param.baseRotate);
			objectJson["particleEmitter"]["param"]["isRandomRotate"] = param.isRandomRotate;
			objectJson["particleEmitter"]["param"]["randomRotateRange"] = Vector3ToJson(param.randomRotateRange);
			objectJson["particleEmitter"]["param"]["color"] = Vector4ToJson(param.color);
			objectJson["particleEmitter"]["param"]["endColor"] = Vector4ToJson(param.endColor);
			objectJson["particleEmitter"]["param"]["randomScaleRange"] = Vector3ToJson(param.randomScaleRange);
			objectJson["particleEmitter"]["param"]["isBillboard"] = param.isBillboard;
			objectJson["particleEmitter"]["param"]["isVortex"] = param.isVortex;
			objectJson["particleEmitter"]["param"]["vortexAngularSpeed"] = param.vortexAngularSpeed;
			objectJson["particleEmitter"]["param"]["vortexBaseRadius"] = param.vortexBaseRadius;
			objectJson["particleEmitter"]["param"]["vortexTopRadius"] = param.vortexTopRadius;
			objectJson["particleEmitter"]["param"]["vortexHeight"] = param.vortexHeight;
		}
		if (EnemySpawnPointComponent* enemySpawnPoint = object->GetComponent<EnemySpawnPointComponent>()) {
			objectJson["type"] = "EnemySpawnPoint";
			objectJson["enemySpawnPoint"]["enabled"] = enemySpawnPoint->IsEnabled();
			SaveComponentGravity(objectJson["enemySpawnPoint"], enemySpawnPoint);
			objectJson["enemySpawnPoint"]["targetName"] = enemySpawnPoint->GetTargetName();
			objectJson["enemySpawnPoint"]["cameraName"] = enemySpawnPoint->GetCameraName();
			objectJson["enemySpawnPoint"]["enemyTypeName"] = enemySpawnPoint->GetEnemyTypeName();
			objectJson["enemySpawnPoint"]["spawnEnabled"] = enemySpawnPoint->GetSpawnEnabled();
			objectJson["enemySpawnPoint"]["spawnCount"] = enemySpawnPoint->GetSpawnCount();
			objectJson["enemySpawnPoint"]["outerMargin"] = enemySpawnPoint->GetOuterMargin();
			objectJson["enemySpawnPoint"]["minimumRadius"] = enemySpawnPoint->GetMinimumRadius();
			objectJson["enemySpawnPoint"]["groundY"] = enemySpawnPoint->GetGroundY();
			objectJson["enemySpawnPoint"]["pointHeight"] = enemySpawnPoint->GetPointHeight();
			objectJson["enemySpawnPoint"]["drawDebug"] = enemySpawnPoint->GetDrawDebug();
			objectJson["enemySpawnPoint"]["debugPointSize"] = enemySpawnPoint->GetDebugPointSize();
			// ボス戦の時刻・敵種・双方の座標をシーン固有設定として保存する。
			const EnemySpawnPointComponent::BossEncounterSettings& bossSettings = enemySpawnPoint->GetBossEncounterSettings();
			objectJson["enemySpawnPoint"]["bossEncounter"] = {
				{"enabled", bossSettings.enabled},
				{"triggerTime", bossSettings.triggerTimeSeconds},
				{"enemyTypeName", bossSettings.enemyTypeName},
				{"bossPosition", Vector3ToJson(bossSettings.bossPosition)},
				{"playerWarpPosition", Vector3ToJson(bossSettings.playerWarpPosition)}
			};
			objectJson["enemySpawnPoint"]["schedules"] = nlohmann::json::array();
			for (const EnemySpawnPointComponent::SpawnSchedule& schedule : enemySpawnPoint->GetSpawnSchedules()) {
				// frameCounterとhasSpawnedは実行時状態なので保存せず、spawnOnceの設定値だけを永続化する。
				objectJson["enemySpawnPoint"]["schedules"].push_back({
					{"startTime", schedule.startTimeSeconds},
					{"endTime", schedule.endTimeSeconds},
					{"enemyTypeName", schedule.enemyTypeName},
					{"intervalFrames", schedule.spawnIntervalFrames},
					{"spawnAmount", schedule.spawnAmount},
					{"applyTimeScaling", schedule.applyTimeScaling},
					{"spawnOnce", schedule.spawnOnce}
				});
			}
			objectJson["enemySpawnPoint"]["timeScalingTiers"] = nlohmann::json::array();
			for (const EnemySpawnPointComponent::TimeScalingTier& tier : enemySpawnPoint->GetTimeScalingTiers()) {
				objectJson["enemySpawnPoint"]["timeScalingTiers"].push_back({
					{"startTime", tier.startTimeSeconds},
					{"healthMultiplier", tier.multipliers.healthMultiplier},
					{"speedMultiplier", tier.multipliers.speedMultiplier},
					{"experienceMultiplier", tier.multipliers.experienceMultiplier}
				});
			}
		}
		objectJson["transform"]["scale"] = Vector3ToJson(transform.scale);
		objectJson["transform"]["rotate"] = Vector3ToJson(transform.rotate);
		objectJson["transform"]["translate"] = Vector3ToJson(transform.translate);

		objectJson["children"] = nlohmann::json::array();
		for (const auto& childObject : sceneObjects_) {
			if (childObject->GetParentName() == object->GetName()) {
				objectJson["children"].push_back(makeObjectJson(childObject.get()));
			}
		}
		return objectJson;
	};

	for (const auto& object : sceneObjects_) {
		if (EnemyComponent* enemy = object->GetComponent<EnemyComponent>(); enemy && enemy->GetRuntimeSpawned()) {
			continue;
		}
		if (object->GetComponent<ExperienceComponent>()) {
			continue;
		}
		if (object->GetComponent<EnemyProjectileComponent>()) {
			continue;
		}
		if (object->GetParentName().empty() || !FindObjectByName(object->GetParentName())) {
			root["objects"].push_back(makeObjectJson(object.get()));
		}
	}

	const std::string filePath = GetSceneObjectFilePath();
	const std::filesystem::path nativeFilePath = Utf8ToPath(filePath);
	std::filesystem::create_directories(nativeFilePath.parent_path());

	std::ofstream ofs(nativeFilePath);
	if (!ofs) {
		return;
	}
	ofs << root.dump(4);
}

/// <summary>
/// JSONからエディタ配置オブジェクトを読み込みます。
/// </summary>
void BaseScene::LoadEditorObjects() {
	const std::string filePath = GetSceneObjectFilePath();
	nlohmann::json root;
	if (!LoadSceneJsonWithInheritance(Utf8ToPath(filePath), root)) {
		return;
	}

	sceneObjects_.clear();
	activeBossEncounterObjectName_.clear();
	// ステージ再読み込みを新しい挑戦として扱い、前回の戦績をすべて破棄する。
	isStageCleared_ = false;
	defeatedEnemyCount_ = 0;
	// シーン再読み込み時に前回挑戦の獲得額が引き継がれないよう初期化する。
	challengeMoneyEarned_ = 0;
	survivalTimeSeconds_ = 0.0f;
	editorSkyBox_.reset();
	selectedObjectIndex_ = -1;
	nextObjectId_ = root.value("nextObjectId", 1);
	const bool hasSavedActiveCamera = root.contains("activeCamera");
	activeCameraObjectName_ = root.value("activeCamera", "");
	const std::string requestedActiveCameraName = activeCameraObjectName_;
	const nlohmann::json skyBoxJson = root.value("skyBox", nlohmann::json::object());
	isEditorSkyBoxEnabled_ = skyBoxJson.value("enabled", false);
	skyBoxTextureFilePath_ = skyBoxJson.value("textureFilePath", std::string());
	if (!skyBoxTextureFilePath_.empty()) {
		CreateOrReloadEditorSkyBox(skyBoxTextureFilePath_);
	}

	// 大気設定がない旧シーンJSONでは、シーン側で用意した既定値をそのまま使用します。
	if (root.contains("atmosphere")) {
		const nlohmann::json& atmosphereJson = root["atmosphere"];
		AtmosphereSettings& atmosphere = AtmosphereSystem::GetInstance()->GetSettings();
		atmosphere.enabled = atmosphereJson.value("enabled", atmosphere.enabled);
		atmosphere.sunDirection = NormalizeReturnVector(JsonToVector3(
		    atmosphereJson.value("sunDirection", nlohmann::json::array()), atmosphere.sunDirection));
		atmosphere.sunAngularRadius = atmosphereJson.value("sunAngularRadius", atmosphere.sunAngularRadius);
		atmosphere.sunIntensity = atmosphereJson.value("sunIntensity", atmosphere.sunIntensity);
		atmosphere.rayleighColor = JsonToVector3(
		    atmosphereJson.value("rayleighColor", nlohmann::json::array()), atmosphere.rayleighColor);
		atmosphere.rayleighStrength = atmosphereJson.value("rayleighStrength", atmosphere.rayleighStrength);
		atmosphere.mieColor = JsonToVector3(
		    atmosphereJson.value("mieColor", nlohmann::json::array()), atmosphere.mieColor);
		atmosphere.mieStrength = atmosphereJson.value("mieStrength", atmosphere.mieStrength);
		atmosphere.mieG = atmosphereJson.value("mieG", atmosphere.mieG);
		atmosphere.atmosphereDensity = atmosphereJson.value("atmosphereDensity", atmosphere.atmosphereDensity);
		atmosphere.horizonColor = JsonToVector3(
		    atmosphereJson.value("horizonColor", nlohmann::json::array()), atmosphere.horizonColor);
		atmosphere.distanceFogDensity = atmosphereJson.value("distanceFogDensity", atmosphere.distanceFogDensity);
		atmosphere.heightFogDensity = atmosphereJson.value("heightFogDensity", atmosphere.heightFogDensity);
		atmosphere.aerialPerspectiveStrength = atmosphereJson.value(
		    "aerialPerspectiveStrength", atmosphere.aerialPerspectiveStrength);
	}

	std::vector<nlohmann::json> flatObjects;
	std::function<void(nlohmann::json, const std::string&)> collectObjects = [&](nlohmann::json objectJson, const std::string& parentName) {
		if (!parentName.empty()) {
			objectJson["parent"] = parentName;
		}
		flatObjects.push_back(objectJson);
		const std::string currentName = objectJson.value("name", "");
		const nlohmann::json children = objectJson.value("children", nlohmann::json::array());
		for (const auto& childJson : children) {
			collectObjects(childJson, currentName);
		}
	};

	const nlohmann::json objects = root.value("objects", nlohmann::json::array());
	for (const auto& objectJson : objects) {
		collectObjects(objectJson, objectJson.value("parent", ""));
	}

	for (const auto& objectJson : flatObjects) {
		const std::string typeName = objectJson.value("type", "Empty");
		std::string modelFilePath = objectJson.value("model", "");
		if (typeName == "Player") {
			const nlohmann::json playerJson = objectJson.value("player", nlohmann::json::object());
			// 選択タイプをCreateEditorObjectへ渡し、生成直後から対応するモデルとComponent設定を作る。
			modelFilePath = playerTypeOverride_.empty()
			    ? playerJson.value("typeName", playerJson.value("model", modelFilePath))
			    : playerTypeOverride_;
		}
		if (typeName == "Enemy") {
			const nlohmann::json enemyJson = objectJson.value("enemy", nlohmann::json::object());
			modelFilePath = enemyJson.value("typeName", std::string("Default"));
		}
		GameObject* object = CreateEditorObject(EditorCreateTypeFromName(typeName), modelFilePath);
		if (!object) {
			continue;
		}

		object->SetName(objectJson.value("name", object->GetName()));
		if (typeName == "LoadedModel" && !modelFilePath.empty()) {
			object->SetEditorType("LoadedModel:" + modelFilePath);
		} else if (typeName == "AnimatedModel" && !modelFilePath.empty()) {
			object->SetEditorType("AnimatedModel:" + modelFilePath);
		} else {
			object->SetEditorType(typeName);
		}
		object->SetParentName(objectJson.value("parent", ""));

		const nlohmann::json transformJson = objectJson.value("transform", nlohmann::json::object());
		EulerTransform& transform = object->GetTransform();
		transform.scale = JsonToVector3(transformJson.value("scale", nlohmann::json::array()), transform.scale);
		transform.rotate = JsonToVector3(transformJson.value("rotate", nlohmann::json::array()), transform.rotate);
		transform.translate = JsonToVector3(transformJson.value("translate", nlohmann::json::array()), transform.translate);
		if (Player* player = object->GetComponent<Player>()) {
			const nlohmann::json playerJson = objectJson.value("player", nlohmann::json::object());
			player->SetEnabled(playerJson.value("enabled", player->IsEnabled()));
			LoadComponentGravity(playerJson, player);
			// 通常のエディタ読み込みではJSONを尊重し、ゲーム開始時だけ選択タイプで差し替える。
			const std::string playerTypeName = playerTypeOverride_.empty()
			    ? playerJson.value("typeName", player->GetPlayerTypeName())
			    : playerTypeOverride_;
			player->SetPlayerTypeName(playerTypeName);
			PlayerStats playerStats = LoadPlayerStats(playerTypeName);
			// 全体経験値強化はキャラクター能力JSONへ混ぜず、実行時のPlayerへ独立して設定する。
			player->SetGlobalExperienceBonusPercent(
			    sceneManager ? sceneManager->GetGlobalExperienceBonusPercent() : 0.0f);
			player->ApplyStats(playerStats, ApplyPlayerStatusItems(playerStats));
			// ゲーム開始時に選択したプレイヤータイプ固有のサイズを、表示と当たり判定へ反映する。
			if (!playerTypeOverride_.empty()) {
				transform.scale = {playerStats.sizeScale, playerStats.sizeScale, playerStats.sizeScale};
			}
			PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>();
			if (!attack) {
				attack = object->AddComponent<PlayerAttackComponent>();
			}
			ApplyPlayerAttackSlots(attack, playerStats);
			const nlohmann::json attackJson = objectJson.value("playerAttack", nlohmann::json::object());
			attack->SetEnabled(attackJson.value("enabled", attack->IsEnabled()));
			LoadComponentGravity(attackJson, attack);
			const Vector3 spawnPoint = JsonToVector3(playerJson.value("spawnPoint", nlohmann::json::array()), transform.translate);
			player->SetSpawnPoint(spawnPoint);
			// タイプ固有モデルを使う場合は、配置時に保存された旧モデル情報を引き継がない。
			const std::string playerModelFilePath = playerTypeOverride_.empty()
			    ? playerJson.value("model", player->GetModelFilePath())
			    : playerStats.modelFilePath;
			Model* playerModel = ModelManager::GetInstance()->FindModel(playerModelFilePath);
			const bool isAnimationModel = playerTypeOverride_.empty()
			    ? playerJson.value("isAnimationModel", playerModel && playerModel->GetIsAnimation())
			    : playerStats.isAnimationModel;
			player->SetModelFilePath(playerModelFilePath, isAnimationModel);
			if (Object3dComponent* object3dComponent = object->GetComponent<Object3dComponent>()) {
				if (playerModel) {
					object3dComponent->SetModel(playerModelFilePath);
				}
				object3dComponent->SetDrawSkeleton(isAnimationModel);
			}
			player->ResetToSpawnPoint();
			player->SetCurrentHealth(player->GetMaxHealth());
			// 選択タイプで開始する新規プレイでは、保存された配置用レベル・経験値ではなく初期値へ戻す。
			player->SetLevel(playerTypeOverride_.empty() ? playerJson.value("level", player->GetStats().level) : playerStats.level);
			player->SetExperience(playerTypeOverride_.empty() ? playerJson.value("experience", player->GetStats().experience) : playerStats.experience);
			CameraComponent* playerCamera = object->GetComponent<CameraComponent>();
			if (!playerCamera) {
				playerCamera = object->AddComponent<CameraComponent>();
			}
			playerCamera->SetLocalOffset({0.0f, 15.0f, 0.0f});
			playerCamera->SetFollowOffset({0.0f, 15.0f, 0.0f});
			playerCamera->SetOverrideRotationEnabled(true);
			playerCamera->SetOverrideRotation({MathConstants::kPi * 0.5f, 0.0f, 0.0f});
			playerCamera->SetFovY(0.75f);
			playerCamera->SetFarClip(1000.0f);
			activeCameraObjectName_ = object->GetName();
		}
		if (EnemyComponent* enemy = object->GetComponent<EnemyComponent>()) {
			const nlohmann::json enemyJson = objectJson.value("enemy", nlohmann::json::object());
			enemy->SetEnabled(enemyJson.value("enabled", enemy->IsEnabled()));
			LoadComponentGravity(enemyJson, enemy);
			const std::string enemyTypeName = enemyJson.value("typeName", enemy->GetEnemyTypeName());
			enemy->SetEnemyTypeName(enemyTypeName);
			enemy->ApplyStats(LoadEnemyStats(enemyTypeName));
			enemy->SetCurrentHealth(enemyJson.value("currentHealth", enemy->GetCurrentHealth()));
			enemy->SetTargetName(enemyJson.value("targetName", std::string()));
			enemy->SetRuntimeSpawned(false);
		}

		if (SpriteComponent* spriteComponent = object->GetComponent<SpriteComponent>()) {
			const nlohmann::json spriteJson = objectJson.value("sprite", nlohmann::json::object());
			spriteComponent->SetEnabled(spriteJson.value("enabled", spriteComponent->IsEnabled()));
			LoadComponentGravity(spriteJson, spriteComponent);
			const std::string textureFilePath = spriteJson.value("textureFilePath", spriteComponent->GetTextureFilePath());
			if (!textureFilePath.empty()) {
				spriteComponent->SetTexture(textureFilePath);
			}
			if (spriteJson.contains("color")) {
				spriteComponent->SetColor(JsonToVector4(spriteJson.value("color", nlohmann::json::array()), spriteComponent->GetColor()));
			}
			const nlohmann::json sizeJson = spriteJson.value("size", nlohmann::json::array());
			if (sizeJson.is_array() && sizeJson.size() >= 2) {
				spriteComponent->SetSize({sizeJson.at(0).get<float>(), sizeJson.at(1).get<float>()});
			}
		}
		if (TextComponent* textComponent = object->GetComponent<TextComponent>()) {
			const nlohmann::json textJson = objectJson.value("text", nlohmann::json::object());
			textComponent->SetEnabled(textJson.value("enabled", textComponent->IsEnabled()));
			LoadComponentGravity(textJson, textComponent);
			textComponent->SetText(textJson.value("value", textComponent->GetText()));
			textComponent->SetFontName(textJson.value("fontName", textComponent->GetFontName()));
			textComponent->SetFontSize(textJson.value("fontSize", textComponent->GetFontSize()));
			textComponent->SetAnchor(static_cast<TextComponent::Anchor>(textJson.value("anchor", static_cast<int>(textComponent->GetAnchor()))));
			textComponent->SetColor(JsonToVector4(textJson.value("color", nlohmann::json::array()), textComponent->GetColor()));
		}
		if (Object3dComponent* object3dComponent = object->GetComponent<Object3dComponent>()) {
			const nlohmann::json object3dJson = objectJson.value("object3d", nlohmann::json::object());
			object3dComponent->SetEnabled(object3dJson.value("enabled", object3dComponent->IsEnabled()));
			LoadComponentGravity(object3dJson, object3dComponent);
			const nlohmann::json materialJson = object3dJson.value("material", nlohmann::json::object());
			object3dComponent->SetColor(
			    JsonToVector4(materialJson.value("color", nlohmann::json::array()), object3dComponent->GetColor())
			);
			object3dComponent->SetEmission(
			    JsonToVector3(materialJson.value("emissionColor", nlohmann::json::array()), object3dComponent->GetEmissionColor()),
			    materialJson.value("emissionIntensity", object3dComponent->GetEmissionIntensity())
			);
			object3dComponent->SetLightingEnabled(
			    materialJson.value("receiveLighting", object3dComponent->GetLightingEnabled())
			);
			object3dComponent->SetShadowEnabled(object3dJson.value("castShadow", object3dComponent->GetShadowEnabled()));
			const std::string textureFilePath = object3dJson.value("modelTextureFilePath", std::string());
			if (!textureFilePath.empty()) {
				object3dComponent->SetModelTexture(textureFilePath);
			}
			object3dComponent->SetDrawSkeleton(object3dJson.value("drawSkeleton", object3dComponent->GetDrawSkeleton()));
			// 旧シーンにanimationNameがない場合は、モデル設定時に選ばれた先頭クリップを維持する。
			const std::string animationName = object3dJson.value("animationName", object3dComponent->GetAnimationName());
			if (!animationName.empty()) {
				object3dComponent->SetAnimation(animationName, false);
			}
			object3dComponent->SetAnimationPlaying(object3dJson.value("animationPlaying", object3dComponent->GetAnimationPlaying()));
			object3dComponent->IsPointLightSet(object3dJson.value("isPointLight", object3dComponent->GetIsPointLightSet()));
			const nlohmann::json pointLightJson = object3dJson.value("pointLight", nlohmann::json::object());
			object3dComponent->SetPointLight(
			    JsonToVector4(pointLightJson.value("color", nlohmann::json::array()), object3dComponent->GetPointLightColor()),
			    JsonToVector3(pointLightJson.value("position", nlohmann::json::array()), object3dComponent->GetPointLightPosition()),
			    pointLightJson.value("intensity", object3dComponent->GetPointLightIntensity()),
			    pointLightJson.value("radius", object3dComponent->GetPointLightRadius()),
			    pointLightJson.value("decay", object3dComponent->GetPointLightDecay())
			);
		}
		if (objectJson.contains("pointLightComponent")) {
			const nlohmann::json pointLightJson = objectJson.value("pointLightComponent", nlohmann::json::object());
			PointLightComponent* pointLight = object->GetComponent<PointLightComponent>();
			if (!pointLight) {
				pointLight = object->AddComponent<PointLightComponent>();
			}
			pointLight->SetEnabled(pointLightJson.value("enabled", pointLight->IsEnabled()));
			pointLight->SetColor(JsonToVector4(pointLightJson.value("color", nlohmann::json::array()), pointLight->GetColor()));
			pointLight->SetIntensity(pointLightJson.value("intensity", pointLight->GetIntensity()));
			pointLight->SetRadius(pointLightJson.value("radius", pointLight->GetRadius()));
			pointLight->SetDecay(pointLightJson.value("decay", pointLight->GetDecay()));
			pointLight->SetPositionOffset(
			    JsonToVector3(pointLightJson.value("positionOffset", nlohmann::json::array()), pointLight->GetPositionOffset())
			);
			pointLight->SetUseMaterialEmissionColor(
			    pointLightJson.value("useMaterialEmissionColor", pointLight->GetUseMaterialEmissionColor())
			);
		}
		if (objectJson.contains("glowBillboard")) {
			const nlohmann::json glowJson = objectJson.value("glowBillboard", nlohmann::json::object());
			GlowBillboardComponent* glow = object->GetComponent<GlowBillboardComponent>();
			if (!glow) {
				glow = object->AddComponent<GlowBillboardComponent>();
			}
			glow->SetEnabled(glowJson.value("enabled", glow->IsEnabled()));
			glow->SetTexture(glowJson.value("textureFilePath", glow->GetTexture()));
			glow->SetInnerColor(JsonToVector4(glowJson.value("innerColor", nlohmann::json::array()), glow->GetInnerColor()));
			glow->SetOuterColor(JsonToVector4(glowJson.value("outerColor", nlohmann::json::array()), glow->GetOuterColor()));
			glow->SetInnerSize(glowJson.value("innerSize", glow->GetInnerSize()));
			glow->SetOuterSize(glowJson.value("outerSize", glow->GetOuterSize()));
			glow->SetInnerIntensity(glowJson.value("innerIntensity", glow->GetInnerIntensity()));
			glow->SetOuterIntensity(glowJson.value("outerIntensity", glow->GetOuterIntensity()));
			glow->SetPulseAmount(glowJson.value("pulseAmount", glow->GetPulseAmount()));
			glow->SetPulseSpeed(glowJson.value("pulseSpeed", glow->GetPulseSpeed()));
			glow->SetPositionOffset(
			    JsonToVector3(glowJson.value("positionOffset", nlohmann::json::array()), glow->GetPositionOffset())
			);
		}
		if (CameraComponent* cameraComponent = object->GetComponent<CameraComponent>()) {
			const nlohmann::json cameraJson = objectJson.value("camera", nlohmann::json::object());
			cameraComponent->SetEnabled(cameraJson.value("enabled", cameraComponent->IsEnabled()));
			LoadComponentGravity(cameraJson, cameraComponent);
			cameraComponent->SetFovY(cameraJson.value("fovY", cameraComponent->GetFovY()));
			cameraComponent->SetNearClip(cameraJson.value("nearClip", cameraComponent->GetNearClip()));
			cameraComponent->SetFarClip(cameraJson.value("farClip", cameraComponent->GetFarClip()));
			cameraComponent->SetFollowTargetName(cameraJson.value("followTarget", ""));
			cameraComponent->SetFollowOffset(JsonToVector3(cameraJson.value("followOffset", nlohmann::json::array()), cameraComponent->GetFollowOffset()));
			cameraComponent->SetLocalOffset(JsonToVector3(cameraJson.value("localOffset", nlohmann::json::array()), cameraComponent->GetLocalOffset()));
			cameraComponent->SetOverrideRotationEnabled(cameraJson.value("overrideRotationEnabled", cameraComponent->GetOverrideRotationEnabled()));
			cameraComponent->SetOverrideRotation(JsonToVector3(cameraJson.value("overrideRotation", nlohmann::json::array()), cameraComponent->GetOverrideRotation()));
		}
		if (objectJson.contains("obbCollider")) {
			const nlohmann::json colliderJson = objectJson.value("obbCollider", nlohmann::json::object());
			OBBColliderComponent* collider = object->GetComponent<OBBColliderComponent>();
			if (!collider) {
				collider = object->AddComponent<OBBColliderComponent>();
			}
			collider->SetEnabled(colliderJson.value("enabled", collider->IsEnabled()));
			LoadComponentGravity(colliderJson, collider);
			collider->SetCenterOffset(JsonToVector3(colliderJson.value("centerOffset", nlohmann::json::array()), collider->GetCenterOffset()));
			collider->SetHalfSize(JsonToVector3(colliderJson.value("halfSize", nlohmann::json::array()), collider->GetHalfSize()));
			collider->SetDrawDebug(colliderJson.value("drawDebug", collider->GetDrawDebug()));
			// 古い保存データに pushBack が無い場合は現在値を維持します。
			collider->SetPushBackEnabled(colliderJson.value("pushBack", collider->GetPushBackEnabled()));
		}
		if (objectJson.contains("sphereCollider")) {
			const nlohmann::json colliderJson = objectJson.value("sphereCollider", nlohmann::json::object());
			SphereColliderComponent* collider = object->GetComponent<SphereColliderComponent>();
			if (!collider) {
				collider = object->AddComponent<SphereColliderComponent>();
			}
			collider->SetEnabled(colliderJson.value("enabled", collider->IsEnabled()));
			LoadComponentGravity(colliderJson, collider);
			collider->SetCenterOffset(JsonToVector3(colliderJson.value("centerOffset", nlohmann::json::array()), collider->GetCenterOffset()));
			collider->SetRadius(colliderJson.value("radius", collider->GetRadius()));
			collider->SetDrawDebug(colliderJson.value("drawDebug", collider->GetDrawDebug()));
			// 古い保存データに pushBack が無い場合は現在値を維持します。
			collider->SetPushBackEnabled(colliderJson.value("pushBack", collider->GetPushBackEnabled()));
		}
		if (ParticleEmitterComponent* emitter = object->GetComponent<ParticleEmitterComponent>()) {
			const nlohmann::json emitterJson = objectJson.value("particleEmitter", nlohmann::json::object());
			emitter->SetEnabled(emitterJson.value("enabled", emitter->IsEnabled()));
			LoadComponentGravity(emitterJson, emitter);
			const std::string groupName = emitterJson.value("groupName", object->GetName());
			const std::string textureFilePath = emitterJson.value("textureFilePath", std::string("Resources/circle.png"));
			const ParticleMeshType meshType = static_cast<ParticleMeshType>(emitterJson.value("meshType", static_cast<int>(emitter->GetMeshType())));

			if (!ParticleManager::GetInstance()->HasGroup(groupName)) {
				ParticleManager::GetInstance()->CreateParticleGroup(groupName, textureFilePath, meshType);
			}
			emitter->SetGroupName(groupName);
			emitter->SetTexture(textureFilePath);
			emitter->SetIsActive(emitterJson.value("isActive", emitter->GetIsActive()));
			emitter->SetFrequency(emitterJson.value("frequency", emitter->GetFrequency()));
			emitter->SetBlendMode(static_cast<BlendMode>(emitterJson.value("blendMode", static_cast<int>(emitter->GetBlendMode()))));
			emitter->SetMeshType(meshType);

			const nlohmann::json paramJson = emitterJson.value("param", nlohmann::json::object());
			ParticleEmitParam param = emitter->GetParam();
			param.count = paramJson.value("count", param.count);
			param.lifeTime = paramJson.value("lifeTime", param.lifeTime);
			param.scale = JsonToVector3(paramJson.value("scale", nlohmann::json::array()), param.scale);
			param.endScale = JsonToVector3(paramJson.value("endScale", nlohmann::json::array()), param.endScale);
			param.baseVelocity = JsonToVector3(paramJson.value("baseVelocity", nlohmann::json::array()), param.baseVelocity);
			param.randomVelocityRange = JsonToVector3(paramJson.value("randomVelocityRange", nlohmann::json::array()), param.randomVelocityRange);
			param.acceleration = JsonToVector3(paramJson.value("acceleration", nlohmann::json::array()), param.acceleration);
			param.randomPositionRange = JsonToVector3(paramJson.value("randomPositionRange", nlohmann::json::array()), param.randomPositionRange);
			param.baseRotate = JsonToVector3(paramJson.value("baseRotate", nlohmann::json::array()), param.baseRotate);
			param.isRandomRotate = paramJson.value("isRandomRotate", param.isRandomRotate);
			param.randomRotateRange = JsonToVector3(paramJson.value("randomRotateRange", nlohmann::json::array()), param.randomRotateRange);
			param.color = JsonToVector4(paramJson.value("color", nlohmann::json::array()), param.color);
			param.endColor = JsonToVector4(paramJson.value("endColor", nlohmann::json::array()), param.endColor);
			param.randomScaleRange = JsonToVector3(paramJson.value("randomScaleRange", nlohmann::json::array()), param.randomScaleRange);
			param.isBillboard = paramJson.value("isBillboard", param.isBillboard);
			param.isVortex = paramJson.value("isVortex", param.isVortex);
			param.vortexAngularSpeed = paramJson.value("vortexAngularSpeed", param.vortexAngularSpeed);
			param.vortexBaseRadius = paramJson.value("vortexBaseRadius", param.vortexBaseRadius);
			param.vortexTopRadius = paramJson.value("vortexTopRadius", param.vortexTopRadius);
			param.vortexHeight = paramJson.value("vortexHeight", param.vortexHeight);
			emitter->SetParam(param);
		}
		if (objectJson.contains("enemySpawnPoint")) {
			const nlohmann::json spawnJson = objectJson.value("enemySpawnPoint", nlohmann::json::object());
			EnemySpawnPointComponent* enemySpawnPoint = object->GetComponent<EnemySpawnPointComponent>();
			if (!enemySpawnPoint) {
				enemySpawnPoint = object->AddComponent<EnemySpawnPointComponent>();
			}
			enemySpawnPoint->SetEnabled(spawnJson.value("enabled", enemySpawnPoint->IsEnabled()));
			LoadComponentGravity(spawnJson, enemySpawnPoint);
			enemySpawnPoint->SetTargetName(spawnJson.value("targetName", std::string()));
			enemySpawnPoint->SetCameraName(spawnJson.value("cameraName", std::string()));
			enemySpawnPoint->SetEnemyTypeName(spawnJson.value("enemyTypeName", enemySpawnPoint->GetEnemyTypeName()));
			enemySpawnPoint->SetSpawnEnabled(spawnJson.value("spawnEnabled", enemySpawnPoint->GetSpawnEnabled()));
			enemySpawnPoint->SetSpawnCount(spawnJson.value("spawnCount", enemySpawnPoint->GetSpawnCount()));
			enemySpawnPoint->SetOuterMargin(spawnJson.value("outerMargin", enemySpawnPoint->GetOuterMargin()));
			enemySpawnPoint->SetMinimumRadius(spawnJson.value("minimumRadius", enemySpawnPoint->GetMinimumRadius()));
			enemySpawnPoint->SetGroundY(spawnJson.value("groundY", enemySpawnPoint->GetGroundY()));
			enemySpawnPoint->SetPointHeight(spawnJson.value("pointHeight", enemySpawnPoint->GetPointHeight()));
			enemySpawnPoint->SetDrawDebug(spawnJson.value("drawDebug", enemySpawnPoint->GetDrawDebug()));
			enemySpawnPoint->SetDebugPointSize(spawnJson.value("debugPointSize", enemySpawnPoint->GetDebugPointSize()));
			// 古いシーンJSONとの互換性を保つため、bossEncounterが存在する場合だけ復元する。
			if (spawnJson.contains("bossEncounter") && spawnJson["bossEncounter"].is_object()) {
				const nlohmann::json bossJson = spawnJson["bossEncounter"];
				EnemySpawnPointComponent::BossEncounterSettings bossSettings = enemySpawnPoint->GetBossEncounterSettings();
				bossSettings.enabled = bossJson.value("enabled", bossSettings.enabled);
				bossSettings.triggerTimeSeconds = bossJson.value("triggerTime", bossSettings.triggerTimeSeconds);
				bossSettings.enemyTypeName = bossJson.value("enemyTypeName", bossSettings.enemyTypeName);
				bossSettings.bossPosition =
				    JsonToVector3(bossJson.value("bossPosition", nlohmann::json::array()), bossSettings.bossPosition);
				bossSettings.playerWarpPosition =
				    JsonToVector3(bossJson.value("playerWarpPosition", nlohmann::json::array()), bossSettings.playerWarpPosition);
				enemySpawnPoint->SetBossEncounterSettings(bossSettings);
			}
			std::vector<EnemySpawnPointComponent::TimeScalingTier> timeScalingTiers;
			const nlohmann::json tiersJson = spawnJson.value("timeScalingTiers", nlohmann::json::array());
			if (tiersJson.is_array()) {
				for (const nlohmann::json& tierJson : tiersJson) {
					if (!tierJson.is_object()) {
						continue;
					}
					EnemySpawnPointComponent::TimeScalingTier tier;
					tier.startTimeSeconds = tierJson.value("startTime", tier.startTimeSeconds);
					tier.multipliers.healthMultiplier = tierJson.value("healthMultiplier", tier.multipliers.healthMultiplier);
					tier.multipliers.speedMultiplier = tierJson.value("speedMultiplier", tier.multipliers.speedMultiplier);
					tier.multipliers.experienceMultiplier =
						tierJson.value("experienceMultiplier", tier.multipliers.experienceMultiplier);
					timeScalingTiers.push_back(tier);
				}
			}
			enemySpawnPoint->SetTimeScalingTiers(timeScalingTiers);
			std::vector<EnemySpawnPointComponent::SpawnSchedule> schedules;
			const nlohmann::json schedulesJson = spawnJson.value("schedules", nlohmann::json::array());
			if (schedulesJson.is_array()) {
				for (const nlohmann::json& scheduleJson : schedulesJson) {
					if (!scheduleJson.is_object()) {
						continue;
					}
					EnemySpawnPointComponent::SpawnSchedule schedule;
					schedule.startTimeSeconds = scheduleJson.value("startTime", schedule.startTimeSeconds);
					schedule.endTimeSeconds = scheduleJson.value("endTime", schedule.endTimeSeconds);
					schedule.enemyTypeName = scheduleJson.value("enemyTypeName", schedule.enemyTypeName);
					schedule.spawnIntervalFrames = scheduleJson.value("intervalFrames", schedule.spawnIntervalFrames);
					schedule.spawnAmount = scheduleJson.value("spawnAmount", schedule.spawnAmount);
					// 未設定の既存シーンでは倍率を適用せず、従来の敵能力を維持する。
					schedule.applyTimeScaling = scheduleJson.value("applyTimeScaling", schedule.applyTimeScaling);
					// spawnOnceがない旧データはfalseとなり、従来どおり繰り返し生成される。
					schedule.spawnOnce = scheduleJson.value("spawnOnce", schedule.spawnOnce);
					schedules.push_back(schedule);
				}
			}
			enemySpawnPoint->SetSpawnSchedules(schedules);
		}
	}

	ResolveCameraLinks();
	ResolveEnemySpawnPointLinks();
	ResolveEnemyLinks();
	if (!requestedActiveCameraName.empty()) {
		GameObject* requestedCameraObject = FindObjectByName(requestedActiveCameraName);
		CameraComponent* requestedCameraComponent =
		    requestedCameraObject ? requestedCameraObject->GetComponent<CameraComponent>() : nullptr;
		if (requestedCameraComponent && requestedCameraComponent->IsEnabled()) {
			activeCameraObjectName_ = requestedActiveCameraName;
		} else {
			activeCameraObjectName_.clear();
		}
	} else if (hasSavedActiveCamera) {
		activeCameraObjectName_.clear();
	}
	if (activeCameraObjectName_.empty() && !hasSavedActiveCamera) {
		if (GameObject* firstCamera = FindFirstCameraObject()) {
			activeCameraObjectName_ = firstCamera->GetName();
		}
	}
	ApplyActiveCamera();

	if (!sceneObjects_.empty()) {
		selectedObjectIndex_ = 0;
	}
	nextObjectId_ = root.value("nextObjectId", nextObjectId_);
}

/// <summary>
/// 現在のシーンに対応する配置JSONファイルパスを返します。
/// </summary>
std::string BaseScene::GetSceneObjectFilePath() const {
	if (!sceneObjectFilePathOverride_.empty()) {
		return sceneObjectFilePathOverride_;
	}
	return "Resources/Data/Scenes/" + sceneName_ + "_objects.json";
}

/// <summary>
/// 文字列からエディタ生成タイプへ変換します。
/// </summary>
BaseScene::EditorCreateType BaseScene::EditorCreateTypeFromName(const std::string& typeName) const {
	if (typeName == "Object3dSphere") {
		return EditorCreateType::Object3dSphere;
	}
	if (typeName == "Object3dCylinder") {
		return EditorCreateType::Object3dCylinder;
	}
	if (typeName == "Object3dCylinderOpen") {
		return EditorCreateType::Object3dCylinderOpen;
	}
	if (typeName == "Sprite") {
		return EditorCreateType::Sprite;
	}
	if (typeName == "Text") {
		return EditorCreateType::Text;
	}
	if (typeName == "LoadedModel" || typeName.starts_with("LoadedModel:")) {
		return EditorCreateType::LoadedModel;
	}
	if (typeName == "AnimatedModel" || typeName.starts_with("AnimatedModel:")) {
		return EditorCreateType::AnimatedModel;
	}
	if (typeName == "Camera") {
		return EditorCreateType::Camera;
	}
	if (typeName == "PointLight") {
		return EditorCreateType::PointLight;
	}
	if (typeName == "ParticleEmitter") {
		return EditorCreateType::ParticleEmitter;
	}
	if (typeName == "Player") {
		return EditorCreateType::Player;
	}
	if (typeName == "EnemySpawnPoint") {
		return EditorCreateType::EnemySpawnPoint;
	}
	if (typeName == "Enemy") {
		return EditorCreateType::Enemy;
	}
	return EditorCreateType::Empty;
}
