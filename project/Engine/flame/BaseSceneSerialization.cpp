#include "BaseScene.h"
#include "repositories/EnemyStatusRepository.h"
#include "MathConstants.h"
#include "model/ModelManager.h"
#include "repositories/PlayerStatusRepository.h"
#include "helpers/SceneJsonUtility.h"
#include "SceneManager.h"
#include "StringUtility.h"

#include <array>
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

/// <summary>独立した追加Componentの保存と復元を型消去して登録するエントリです。</summary>
struct ComponentSerializationRegistration {
	const char* jsonKey;
	void (*save)(GameObject* object, nlohmann::json& objectJson);
	void (*load)(GameObject* object, const nlohmann::json& componentJson);
	bool loadExistingWithoutJson;
};

void SaveSpriteComponent(GameObject* object, nlohmann::json& objectJson) {
	SpriteComponent* sprite = object->GetComponent<SpriteComponent>();
	if (!sprite) {
		return;
	}
	nlohmann::json& componentJson = objectJson["sprite"];
	componentJson["enabled"] = sprite->IsEnabled();
	SaveComponentGravity(componentJson, sprite);
	componentJson["textureFilePath"] = sprite->GetTextureFilePath();
	componentJson["color"] = Vector4ToJson(sprite->GetColor());
	componentJson["size"] = nlohmann::json::array({sprite->GetSize().x, sprite->GetSize().y});
}

void LoadSpriteComponent(GameObject* object, const nlohmann::json& componentJson) {
	SpriteComponent* sprite = object->GetComponent<SpriteComponent>();
	if (!sprite) {
		return;
	}
	sprite->SetEnabled(componentJson.value("enabled", sprite->IsEnabled()));
	LoadComponentGravity(componentJson, sprite);
	const std::string textureFilePath = componentJson.value("textureFilePath", sprite->GetTextureFilePath());
	if (!textureFilePath.empty()) {
		sprite->SetTexture(textureFilePath);
	}
	if (componentJson.contains("color")) {
		sprite->SetColor(JsonToVector4(componentJson.value("color", nlohmann::json::array()), sprite->GetColor()));
	}
	const nlohmann::json sizeJson = componentJson.value("size", nlohmann::json::array());
	if (sizeJson.is_array() && sizeJson.size() >= 2) {
		sprite->SetSize({sizeJson.at(0).get<float>(), sizeJson.at(1).get<float>()});
	}
}

void SaveTextComponent(GameObject* object, nlohmann::json& objectJson) {
	TextComponent* text = object->GetComponent<TextComponent>();
	if (!text) {
		return;
	}
	objectJson["type"] = "Text";
	nlohmann::json& componentJson = objectJson["text"];
	componentJson["enabled"] = text->IsEnabled();
	SaveComponentGravity(componentJson, text);
	componentJson["value"] = text->GetText();
	componentJson["fontName"] = text->GetFontName();
	componentJson["fontSize"] = text->GetFontSize();
	componentJson["anchor"] = static_cast<int>(text->GetAnchor());
	componentJson["color"] = Vector4ToJson(text->GetColor());
}

void LoadTextComponent(GameObject* object, const nlohmann::json& componentJson) {
	TextComponent* text = object->GetComponent<TextComponent>();
	if (!text) {
		return;
	}
	text->SetEnabled(componentJson.value("enabled", text->IsEnabled()));
	LoadComponentGravity(componentJson, text);
	text->SetText(componentJson.value("value", text->GetText()));
	text->SetFontName(componentJson.value("fontName", text->GetFontName()));
	text->SetFontSize(componentJson.value("fontSize", text->GetFontSize()));
	text->SetAnchor(static_cast<TextComponent::Anchor>(componentJson.value("anchor", static_cast<int>(text->GetAnchor()))));
	text->SetColor(JsonToVector4(componentJson.value("color", nlohmann::json::array()), text->GetColor()));
}

void SaveCameraComponent(GameObject* object, nlohmann::json& objectJson) {
	CameraComponent* camera = object->GetComponent<CameraComponent>();
	if (!camera) {
		return;
	}
	nlohmann::json& componentJson = objectJson["camera"];
	componentJson["enabled"] = camera->IsEnabled();
	SaveComponentGravity(componentJson, camera);
	componentJson["fovY"] = camera->GetFovY();
	componentJson["nearClip"] = camera->GetNearClip();
	componentJson["farClip"] = camera->GetFarClip();
	componentJson["followTarget"] = camera->GetFollowTargetName();
	componentJson["followOffset"] = Vector3ToJson(camera->GetFollowOffset());
	componentJson["localOffset"] = Vector3ToJson(camera->GetLocalOffset());
	componentJson["overrideRotationEnabled"] = camera->GetOverrideRotationEnabled();
	componentJson["overrideRotation"] = Vector3ToJson(camera->GetOverrideRotation());
}

void LoadCameraComponent(GameObject* object, const nlohmann::json& componentJson) {
	CameraComponent* camera = object->GetComponent<CameraComponent>();
	if (!camera) {
		return;
	}
	camera->SetEnabled(componentJson.value("enabled", camera->IsEnabled()));
	LoadComponentGravity(componentJson, camera);
	camera->SetFovY(componentJson.value("fovY", camera->GetFovY()));
	camera->SetNearClip(componentJson.value("nearClip", camera->GetNearClip()));
	camera->SetFarClip(componentJson.value("farClip", camera->GetFarClip()));
	camera->SetFollowTargetName(componentJson.value("followTarget", ""));
	camera->SetFollowOffset(JsonToVector3(componentJson.value("followOffset", nlohmann::json::array()), camera->GetFollowOffset()));
	camera->SetLocalOffset(JsonToVector3(componentJson.value("localOffset", nlohmann::json::array()), camera->GetLocalOffset()));
	camera->SetOverrideRotationEnabled(componentJson.value("overrideRotationEnabled", camera->GetOverrideRotationEnabled()));
	camera->SetOverrideRotation(JsonToVector3(componentJson.value("overrideRotation", nlohmann::json::array()), camera->GetOverrideRotation()));
}

void SaveObject3dComponent(GameObject* object, nlohmann::json& objectJson) {
	Object3dComponent* object3d = object->GetComponent<Object3dComponent>();
	if (!object3d) {
		return;
	}
	nlohmann::json& componentJson = objectJson["object3d"];
	componentJson["enabled"] = object3d->IsEnabled();
	SaveComponentGravity(componentJson, object3d);
	componentJson["material"]["color"] = Vector4ToJson(object3d->GetColor());
	componentJson["material"]["emissionColor"] = Vector3ToJson(object3d->GetEmissionColor());
	componentJson["material"]["emissionIntensity"] = object3d->GetEmissionIntensity();
	componentJson["material"]["receiveLighting"] = object3d->GetLightingEnabled();
	componentJson["castShadow"] = object3d->GetShadowEnabled();
	componentJson["modelTextureFilePath"] = object3d->GetModelTextureFilePath();
	componentJson["drawSkeleton"] = object3d->GetDrawSkeleton();
	componentJson["animationPlaying"] = object3d->GetAnimationPlaying();
	// 同じモデルを使うオブジェクトごとに、選択中のアニメーションクリップ名を保存する。
	componentJson["animationName"] = object3d->GetAnimationName();
	componentJson["isPointLight"] = object3d->GetIsPointLightSet();
	componentJson["pointLight"]["color"] = Vector4ToJson(object3d->GetPointLightColor());
	componentJson["pointLight"]["position"] = Vector3ToJson(object3d->GetPointLightPosition());
	componentJson["pointLight"]["intensity"] = object3d->GetPointLightIntensity();
	componentJson["pointLight"]["radius"] = object3d->GetPointLightRadius();
	componentJson["pointLight"]["decay"] = object3d->GetPointLightDecay();
}

void LoadObject3dComponent(GameObject* object, const nlohmann::json& componentJson) {
	Object3dComponent* object3d = object->GetComponent<Object3dComponent>();
	if (!object3d) {
		return;
	}
	object3d->SetEnabled(componentJson.value("enabled", object3d->IsEnabled()));
	LoadComponentGravity(componentJson, object3d);
	const nlohmann::json materialJson = componentJson.value("material", nlohmann::json::object());
	object3d->SetColor(JsonToVector4(materialJson.value("color", nlohmann::json::array()), object3d->GetColor()));
	object3d->SetEmission(
		JsonToVector3(materialJson.value("emissionColor", nlohmann::json::array()), object3d->GetEmissionColor()),
		materialJson.value("emissionIntensity", object3d->GetEmissionIntensity()));
	object3d->SetLightingEnabled(materialJson.value("receiveLighting", object3d->GetLightingEnabled()));
	object3d->SetShadowEnabled(componentJson.value("castShadow", object3d->GetShadowEnabled()));
	const std::string textureFilePath = componentJson.value("modelTextureFilePath", std::string());
	if (!textureFilePath.empty()) {
		object3d->SetModelTexture(textureFilePath);
	}
	object3d->SetDrawSkeleton(componentJson.value("drawSkeleton", object3d->GetDrawSkeleton()));
	// 旧シーンにanimationNameがない場合は、モデル設定時に選ばれた先頭クリップを維持する。
	const std::string animationName = componentJson.value("animationName", object3d->GetAnimationName());
	if (!animationName.empty()) {
		object3d->SetAnimation(animationName, false);
	}
	object3d->SetAnimationPlaying(componentJson.value("animationPlaying", object3d->GetAnimationPlaying()));
	object3d->IsPointLightSet(componentJson.value("isPointLight", object3d->GetIsPointLightSet()));
	const nlohmann::json pointLightJson = componentJson.value("pointLight", nlohmann::json::object());
	object3d->SetPointLight(
		JsonToVector4(pointLightJson.value("color", nlohmann::json::array()), object3d->GetPointLightColor()),
		JsonToVector3(pointLightJson.value("position", nlohmann::json::array()), object3d->GetPointLightPosition()),
		pointLightJson.value("intensity", object3d->GetPointLightIntensity()),
		pointLightJson.value("radius", object3d->GetPointLightRadius()),
		pointLightJson.value("decay", object3d->GetPointLightDecay()));
}

void SaveParticleEmitterComponent(GameObject* object, nlohmann::json& objectJson) {
	ParticleEmitterComponent* emitter = object->GetComponent<ParticleEmitterComponent>();
	if (!emitter) {
		return;
	}
	const ParticleEmitParam param = emitter->GetParam();
	nlohmann::json& componentJson = objectJson["particleEmitter"];
	componentJson["enabled"] = emitter->IsEnabled();
	SaveComponentGravity(componentJson, emitter);
	componentJson["groupName"] = emitter->GetGroupName();
	componentJson["textureFilePath"] = emitter->GetTextureFilePath();
	componentJson["isActive"] = emitter->GetIsActive();
	componentJson["frequency"] = emitter->GetFrequency();
	componentJson["blendMode"] = static_cast<int>(emitter->GetBlendMode());
	componentJson["meshType"] = static_cast<int>(emitter->GetMeshType());
	componentJson["param"]["count"] = param.count;
	componentJson["param"]["lifeTime"] = param.lifeTime;
	componentJson["param"]["scale"] = Vector3ToJson(param.scale);
	componentJson["param"]["endScale"] = Vector3ToJson(param.endScale);
	componentJson["param"]["baseVelocity"] = Vector3ToJson(param.baseVelocity);
	componentJson["param"]["randomVelocityRange"] = Vector3ToJson(param.randomVelocityRange);
	componentJson["param"]["acceleration"] = Vector3ToJson(param.acceleration);
	componentJson["param"]["randomPositionRange"] = Vector3ToJson(param.randomPositionRange);
	componentJson["param"]["baseRotate"] = Vector3ToJson(param.baseRotate);
	componentJson["param"]["isRandomRotate"] = param.isRandomRotate;
	componentJson["param"]["randomRotateRange"] = Vector3ToJson(param.randomRotateRange);
	componentJson["param"]["color"] = Vector4ToJson(param.color);
	componentJson["param"]["endColor"] = Vector4ToJson(param.endColor);
	componentJson["param"]["randomScaleRange"] = Vector3ToJson(param.randomScaleRange);
	componentJson["param"]["isBillboard"] = param.isBillboard;
	componentJson["param"]["isVortex"] = param.isVortex;
	componentJson["param"]["vortexAngularSpeed"] = param.vortexAngularSpeed;
	componentJson["param"]["vortexBaseRadius"] = param.vortexBaseRadius;
	componentJson["param"]["vortexTopRadius"] = param.vortexTopRadius;
	componentJson["param"]["vortexHeight"] = param.vortexHeight;
}

void LoadParticleEmitterComponent(GameObject* object, const nlohmann::json& componentJson) {
	ParticleEmitterComponent* emitter = object->GetComponent<ParticleEmitterComponent>();
	if (!emitter) {
		return;
	}
	emitter->SetEnabled(componentJson.value("enabled", emitter->IsEnabled()));
	LoadComponentGravity(componentJson, emitter);
	const std::string groupName = componentJson.value("groupName", object->GetName());
	const std::string textureFilePath = componentJson.value("textureFilePath", std::string("Resources/circle.png"));
	const ParticleMeshType meshType =
		static_cast<ParticleMeshType>(componentJson.value("meshType", static_cast<int>(emitter->GetMeshType())));

	// ParticleManager側の描画グループを先に保証してから、Emitterの参照設定を復元する。
	if (!ParticleManager::GetInstance()->HasGroup(groupName)) {
		ParticleManager::GetInstance()->CreateParticleGroup(groupName, textureFilePath, meshType);
	}
	emitter->SetGroupName(groupName);
	emitter->SetTexture(textureFilePath);
	emitter->SetIsActive(componentJson.value("isActive", emitter->GetIsActive()));
	emitter->SetFrequency(componentJson.value("frequency", emitter->GetFrequency()));
	emitter->SetBlendMode(static_cast<BlendMode>(componentJson.value("blendMode", static_cast<int>(emitter->GetBlendMode()))));
	emitter->SetMeshType(meshType);

	const nlohmann::json paramJson = componentJson.value("param", nlohmann::json::object());
	ParticleEmitParam param = emitter->GetParam();
	param.count = paramJson.value("count", param.count);
	param.lifeTime = paramJson.value("lifeTime", param.lifeTime);
	param.scale = JsonToVector3(paramJson.value("scale", nlohmann::json::array()), param.scale);
	param.endScale = JsonToVector3(paramJson.value("endScale", nlohmann::json::array()), param.endScale);
	param.baseVelocity = JsonToVector3(paramJson.value("baseVelocity", nlohmann::json::array()), param.baseVelocity);
	param.randomVelocityRange =
		JsonToVector3(paramJson.value("randomVelocityRange", nlohmann::json::array()), param.randomVelocityRange);
	param.acceleration = JsonToVector3(paramJson.value("acceleration", nlohmann::json::array()), param.acceleration);
	param.randomPositionRange =
		JsonToVector3(paramJson.value("randomPositionRange", nlohmann::json::array()), param.randomPositionRange);
	param.baseRotate = JsonToVector3(paramJson.value("baseRotate", nlohmann::json::array()), param.baseRotate);
	param.isRandomRotate = paramJson.value("isRandomRotate", param.isRandomRotate);
	param.randomRotateRange =
		JsonToVector3(paramJson.value("randomRotateRange", nlohmann::json::array()), param.randomRotateRange);
	param.color = JsonToVector4(paramJson.value("color", nlohmann::json::array()), param.color);
	param.endColor = JsonToVector4(paramJson.value("endColor", nlohmann::json::array()), param.endColor);
	param.randomScaleRange =
		JsonToVector3(paramJson.value("randomScaleRange", nlohmann::json::array()), param.randomScaleRange);
	param.isBillboard = paramJson.value("isBillboard", param.isBillboard);
	param.isVortex = paramJson.value("isVortex", param.isVortex);
	param.vortexAngularSpeed = paramJson.value("vortexAngularSpeed", param.vortexAngularSpeed);
	param.vortexBaseRadius = paramJson.value("vortexBaseRadius", param.vortexBaseRadius);
	param.vortexTopRadius = paramJson.value("vortexTopRadius", param.vortexTopRadius);
	param.vortexHeight = paramJson.value("vortexHeight", param.vortexHeight);
	emitter->SetParam(param);
}

void SaveEnemySpawnPointComponent(GameObject* object, nlohmann::json& objectJson) {
	EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
	if (!spawnPoint) {
		return;
	}
	objectJson["type"] = "EnemySpawnPoint";
	nlohmann::json& componentJson = objectJson["enemySpawnPoint"];
	componentJson["enabled"] = spawnPoint->IsEnabled();
	SaveComponentGravity(componentJson, spawnPoint);
	componentJson["targetName"] = spawnPoint->GetTargetName();
	componentJson["cameraName"] = spawnPoint->GetCameraName();
	componentJson["enemyTypeName"] = spawnPoint->GetEnemyTypeName();
	componentJson["spawnEnabled"] = spawnPoint->GetSpawnEnabled();
	componentJson["spawnCount"] = spawnPoint->GetSpawnCount();
	componentJson["outerMargin"] = spawnPoint->GetOuterMargin();
	componentJson["minimumRadius"] = spawnPoint->GetMinimumRadius();
	componentJson["groundY"] = spawnPoint->GetGroundY();
	componentJson["pointHeight"] = spawnPoint->GetPointHeight();
	componentJson["drawDebug"] = spawnPoint->GetDrawDebug();
	componentJson["debugPointSize"] = spawnPoint->GetDebugPointSize();

	// ボス戦の時刻・敵種・双方の座標をシーン固有設定として保存する。
	const EnemySpawnPointComponent::BossEncounterSettings& bossSettings = spawnPoint->GetBossEncounterSettings();
	componentJson["bossEncounter"] = {
		{"enabled", bossSettings.enabled},
		{"triggerTime", bossSettings.triggerTimeSeconds},
		{"enemyTypeName", bossSettings.enemyTypeName},
		{"bossPosition", Vector3ToJson(bossSettings.bossPosition)},
		{"playerWarpPosition", Vector3ToJson(bossSettings.playerWarpPosition)}};

	componentJson["schedules"] = nlohmann::json::array();
	for (const EnemySpawnPointComponent::SpawnSchedule& schedule : spawnPoint->GetSpawnSchedules()) {
		// frameCounterとhasSpawnedは実行時状態なので保存せず、spawnOnceの設定値だけを永続化する。
		componentJson["schedules"].push_back({
			{"startTime", schedule.startTimeSeconds},
			{"endTime", schedule.endTimeSeconds},
			{"enemyTypeName", schedule.enemyTypeName},
			{"intervalFrames", schedule.spawnIntervalFrames},
			{"spawnAmount", schedule.spawnAmount},
			{"applyTimeScaling", schedule.applyTimeScaling},
			{"spawnOnce", schedule.spawnOnce}});
	}

	componentJson["timeScalingTiers"] = nlohmann::json::array();
	for (const EnemySpawnPointComponent::TimeScalingTier& tier : spawnPoint->GetTimeScalingTiers()) {
		componentJson["timeScalingTiers"].push_back({
			{"startTime", tier.startTimeSeconds},
			{"healthMultiplier", tier.multipliers.healthMultiplier},
			{"speedMultiplier", tier.multipliers.speedMultiplier},
			{"experienceMultiplier", tier.multipliers.experienceMultiplier}});
	}
}

void LoadEnemySpawnPointComponent(GameObject* object, const nlohmann::json& componentJson) {
	EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
	if (!spawnPoint) {
		spawnPoint = object->AddComponent<EnemySpawnPointComponent>();
	}
	spawnPoint->SetEnabled(componentJson.value("enabled", spawnPoint->IsEnabled()));
	LoadComponentGravity(componentJson, spawnPoint);
	spawnPoint->SetTargetName(componentJson.value("targetName", std::string()));
	spawnPoint->SetCameraName(componentJson.value("cameraName", std::string()));
	spawnPoint->SetEnemyTypeName(componentJson.value("enemyTypeName", spawnPoint->GetEnemyTypeName()));
	spawnPoint->SetSpawnEnabled(componentJson.value("spawnEnabled", spawnPoint->GetSpawnEnabled()));
	spawnPoint->SetSpawnCount(componentJson.value("spawnCount", spawnPoint->GetSpawnCount()));
	spawnPoint->SetOuterMargin(componentJson.value("outerMargin", spawnPoint->GetOuterMargin()));
	spawnPoint->SetMinimumRadius(componentJson.value("minimumRadius", spawnPoint->GetMinimumRadius()));
	spawnPoint->SetGroundY(componentJson.value("groundY", spawnPoint->GetGroundY()));
	spawnPoint->SetPointHeight(componentJson.value("pointHeight", spawnPoint->GetPointHeight()));
	spawnPoint->SetDrawDebug(componentJson.value("drawDebug", spawnPoint->GetDrawDebug()));
	spawnPoint->SetDebugPointSize(componentJson.value("debugPointSize", spawnPoint->GetDebugPointSize()));

	// 古いシーンJSONとの互換性を保つため、bossEncounterが存在する場合だけ復元する。
	if (componentJson.contains("bossEncounter") && componentJson["bossEncounter"].is_object()) {
		const nlohmann::json& bossJson = componentJson["bossEncounter"];
		EnemySpawnPointComponent::BossEncounterSettings bossSettings = spawnPoint->GetBossEncounterSettings();
		bossSettings.enabled = bossJson.value("enabled", bossSettings.enabled);
		bossSettings.triggerTimeSeconds = bossJson.value("triggerTime", bossSettings.triggerTimeSeconds);
		bossSettings.enemyTypeName = bossJson.value("enemyTypeName", bossSettings.enemyTypeName);
		bossSettings.bossPosition =
			JsonToVector3(bossJson.value("bossPosition", nlohmann::json::array()), bossSettings.bossPosition);
		bossSettings.playerWarpPosition =
			JsonToVector3(bossJson.value("playerWarpPosition", nlohmann::json::array()), bossSettings.playerWarpPosition);
		spawnPoint->SetBossEncounterSettings(bossSettings);
	}

	std::vector<EnemySpawnPointComponent::TimeScalingTier> timeScalingTiers;
	const nlohmann::json tiersJson = componentJson.value("timeScalingTiers", nlohmann::json::array());
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
	spawnPoint->SetTimeScalingTiers(timeScalingTiers);

	std::vector<EnemySpawnPointComponent::SpawnSchedule> schedules;
	const nlohmann::json schedulesJson = componentJson.value("schedules", nlohmann::json::array());
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
	spawnPoint->SetSpawnSchedules(schedules);
}

void SavePointLightComponent(GameObject* object, nlohmann::json& objectJson) {
	PointLightComponent* pointLight = object->GetComponent<PointLightComponent>();
	if (!pointLight) {
		return;
	}
	nlohmann::json& componentJson = objectJson["pointLightComponent"];
	componentJson["enabled"] = pointLight->IsEnabled();
	componentJson["color"] = Vector4ToJson(pointLight->GetColor());
	componentJson["intensity"] = pointLight->GetIntensity();
	componentJson["radius"] = pointLight->GetRadius();
	componentJson["decay"] = pointLight->GetDecay();
	componentJson["positionOffset"] = Vector3ToJson(pointLight->GetPositionOffset());
	componentJson["useMaterialEmissionColor"] = pointLight->GetUseMaterialEmissionColor();
}

void LoadPointLightComponent(GameObject* object, const nlohmann::json& componentJson) {
	PointLightComponent* pointLight = object->GetComponent<PointLightComponent>();
	if (!pointLight) {
		pointLight = object->AddComponent<PointLightComponent>();
	}
	pointLight->SetEnabled(componentJson.value("enabled", pointLight->IsEnabled()));
	pointLight->SetColor(JsonToVector4(componentJson.value("color", nlohmann::json::array()), pointLight->GetColor()));
	pointLight->SetIntensity(componentJson.value("intensity", pointLight->GetIntensity()));
	pointLight->SetRadius(componentJson.value("radius", pointLight->GetRadius()));
	pointLight->SetDecay(componentJson.value("decay", pointLight->GetDecay()));
	pointLight->SetPositionOffset(
		JsonToVector3(componentJson.value("positionOffset", nlohmann::json::array()), pointLight->GetPositionOffset()));
	pointLight->SetUseMaterialEmissionColor(
		componentJson.value("useMaterialEmissionColor", pointLight->GetUseMaterialEmissionColor()));
}

void SaveGlowBillboardComponent(GameObject* object, nlohmann::json& objectJson) {
	GlowBillboardComponent* glow = object->GetComponent<GlowBillboardComponent>();
	if (!glow) {
		return;
	}
	nlohmann::json& componentJson = objectJson["glowBillboard"];
	componentJson["enabled"] = glow->IsEnabled();
	componentJson["textureFilePath"] = glow->GetTexture();
	componentJson["innerColor"] = Vector4ToJson(glow->GetInnerColor());
	componentJson["outerColor"] = Vector4ToJson(glow->GetOuterColor());
	componentJson["innerSize"] = glow->GetInnerSize();
	componentJson["outerSize"] = glow->GetOuterSize();
	componentJson["innerIntensity"] = glow->GetInnerIntensity();
	componentJson["outerIntensity"] = glow->GetOuterIntensity();
	componentJson["pulseAmount"] = glow->GetPulseAmount();
	componentJson["pulseSpeed"] = glow->GetPulseSpeed();
	componentJson["positionOffset"] = Vector3ToJson(glow->GetPositionOffset());
}

void LoadGlowBillboardComponent(GameObject* object, const nlohmann::json& componentJson) {
	GlowBillboardComponent* glow = object->GetComponent<GlowBillboardComponent>();
	if (!glow) {
		glow = object->AddComponent<GlowBillboardComponent>();
	}
	glow->SetEnabled(componentJson.value("enabled", glow->IsEnabled()));
	glow->SetTexture(componentJson.value("textureFilePath", glow->GetTexture()));
	glow->SetInnerColor(JsonToVector4(componentJson.value("innerColor", nlohmann::json::array()), glow->GetInnerColor()));
	glow->SetOuterColor(JsonToVector4(componentJson.value("outerColor", nlohmann::json::array()), glow->GetOuterColor()));
	glow->SetInnerSize(componentJson.value("innerSize", glow->GetInnerSize()));
	glow->SetOuterSize(componentJson.value("outerSize", glow->GetOuterSize()));
	glow->SetInnerIntensity(componentJson.value("innerIntensity", glow->GetInnerIntensity()));
	glow->SetOuterIntensity(componentJson.value("outerIntensity", glow->GetOuterIntensity()));
	glow->SetPulseAmount(componentJson.value("pulseAmount", glow->GetPulseAmount()));
	glow->SetPulseSpeed(componentJson.value("pulseSpeed", glow->GetPulseSpeed()));
	glow->SetPositionOffset(
		JsonToVector3(componentJson.value("positionOffset", nlohmann::json::array()), glow->GetPositionOffset()));
}

void SaveOBBColliderComponent(GameObject* object, nlohmann::json& objectJson) {
	OBBColliderComponent* collider = object->GetComponent<OBBColliderComponent>();
	if (!collider) {
		return;
	}
	nlohmann::json& componentJson = objectJson["obbCollider"];
	componentJson["enabled"] = collider->IsEnabled();
	SaveComponentGravity(componentJson, collider);
	componentJson["centerOffset"] = Vector3ToJson(collider->GetCenterOffset());
	componentJson["halfSize"] = Vector3ToJson(collider->GetHalfSize());
	componentJson["drawDebug"] = collider->GetDrawDebug();
	componentJson["pushBack"] = collider->GetPushBackEnabled();
}

void LoadOBBColliderComponent(GameObject* object, const nlohmann::json& componentJson) {
	OBBColliderComponent* collider = object->GetComponent<OBBColliderComponent>();
	if (!collider) {
		collider = object->AddComponent<OBBColliderComponent>();
	}
	collider->SetEnabled(componentJson.value("enabled", collider->IsEnabled()));
	LoadComponentGravity(componentJson, collider);
	collider->SetCenterOffset(JsonToVector3(componentJson.value("centerOffset", nlohmann::json::array()), collider->GetCenterOffset()));
	collider->SetHalfSize(JsonToVector3(componentJson.value("halfSize", nlohmann::json::array()), collider->GetHalfSize()));
	collider->SetDrawDebug(componentJson.value("drawDebug", collider->GetDrawDebug()));
	collider->SetPushBackEnabled(componentJson.value("pushBack", collider->GetPushBackEnabled()));
}

void SaveSphereColliderComponent(GameObject* object, nlohmann::json& objectJson) {
	SphereColliderComponent* collider = object->GetComponent<SphereColliderComponent>();
	if (!collider) {
		return;
	}
	nlohmann::json& componentJson = objectJson["sphereCollider"];
	componentJson["enabled"] = collider->IsEnabled();
	SaveComponentGravity(componentJson, collider);
	componentJson["centerOffset"] = Vector3ToJson(collider->GetCenterOffset());
	componentJson["radius"] = collider->GetRadius();
	componentJson["drawDebug"] = collider->GetDrawDebug();
	componentJson["pushBack"] = collider->GetPushBackEnabled();
}

void LoadSphereColliderComponent(GameObject* object, const nlohmann::json& componentJson) {
	SphereColliderComponent* collider = object->GetComponent<SphereColliderComponent>();
	if (!collider) {
		collider = object->AddComponent<SphereColliderComponent>();
	}
	collider->SetEnabled(componentJson.value("enabled", collider->IsEnabled()));
	LoadComponentGravity(componentJson, collider);
	collider->SetCenterOffset(JsonToVector3(componentJson.value("centerOffset", nlohmann::json::array()), collider->GetCenterOffset()));
	collider->SetRadius(componentJson.value("radius", collider->GetRadius()));
	collider->SetDrawDebug(componentJson.value("drawDebug", collider->GetDrawDebug()));
	collider->SetPushBackEnabled(componentJson.value("pushBack", collider->GetPushBackEnabled()));
}

const std::array<ComponentSerializationRegistration, 10>& GetComponentSerializationRegistrations() {
	// 独立Componentを追加するときは、保存キーと一対のコールバックをここへ登録する。
	static const std::array<ComponentSerializationRegistration, 10> registrations = {{
		// 生成時に付与される基本Componentは、古いJSONにキーがなくても既定値の復元処理を通す。
		{"sprite", &SaveSpriteComponent, &LoadSpriteComponent, true},
		{"text", &SaveTextComponent, &LoadTextComponent, true},
		{"camera", &SaveCameraComponent, &LoadCameraComponent, true},
		{"object3d", &SaveObject3dComponent, &LoadObject3dComponent, true},
		{"particleEmitter", &SaveParticleEmitterComponent, &LoadParticleEmitterComponent, true},
		{"enemySpawnPoint", &SaveEnemySpawnPointComponent, &LoadEnemySpawnPointComponent, false},
		{"pointLightComponent", &SavePointLightComponent, &LoadPointLightComponent, false},
		{"glowBillboard", &SaveGlowBillboardComponent, &LoadGlowBillboardComponent, false},
		{"obbCollider", &SaveOBBColliderComponent, &LoadOBBColliderComponent, false},
		{"sphereCollider", &SaveSphereColliderComponent, &LoadSphereColliderComponent, false}
	}};
	return registrations;
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
		for (const ComponentSerializationRegistration& registration : GetComponentSerializationRegistrations()) {
			registration.save(object, objectJson);
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

		for (const ComponentSerializationRegistration& registration : GetComponentSerializationRegistrations()) {
			if (objectJson.contains(registration.jsonKey) || registration.loadExistingWithoutJson) {
				registration.load(object, objectJson.value(registration.jsonKey, nlohmann::json::object()));
			}
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
