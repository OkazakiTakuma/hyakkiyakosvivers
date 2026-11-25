#pragma once

#include "helpers/SceneJsonUtility.h"
#include "struct.h"

/// <summary>ParticleEmitParamの全フィールドを一元的にJSON変換します。</summary>
namespace ParticleEmitParamJson {

inline nlohmann::json ToJson(const ParticleEmitParam& param) {
	nlohmann::json json;
	json["count"] = param.count;
	json["lifeTime"] = param.lifeTime;
	json["scale"] = SceneJsonUtility::Vector3ToJson(param.scale);
	json["endScale"] = SceneJsonUtility::Vector3ToJson(param.endScale);
	json["baseVelocity"] = SceneJsonUtility::Vector3ToJson(param.baseVelocity);
	json["randomVelocityRange"] = SceneJsonUtility::Vector3ToJson(param.randomVelocityRange);
	json["acceleration"] = SceneJsonUtility::Vector3ToJson(param.acceleration);
	json["randomPositionRange"] = SceneJsonUtility::Vector3ToJson(param.randomPositionRange);
	json["baseRotate"] = SceneJsonUtility::Vector3ToJson(param.baseRotate);
	json["isRandomRotate"] = param.isRandomRotate;
	json["randomRotateRange"] = SceneJsonUtility::Vector3ToJson(param.randomRotateRange);
	json["color"] = SceneJsonUtility::Vector4ToJson(param.color);
	json["endColor"] = SceneJsonUtility::Vector4ToJson(param.endColor);
	json["randomScaleRange"] = SceneJsonUtility::Vector3ToJson(param.randomScaleRange);
	json["isBillboard"] = param.isBillboard;
	json["isVortex"] = param.isVortex;
	json["vortexAngularSpeed"] = param.vortexAngularSpeed;
	json["vortexBaseRadius"] = param.vortexBaseRadius;
	json["vortexTopRadius"] = param.vortexTopRadius;
	json["vortexHeight"] = param.vortexHeight;
	return json;
}

inline ParticleEmitParam FromJson(const nlohmann::json& json, const ParticleEmitParam& fallback) {
	// 欠けたフィールドは既存値を維持し、古いシーン・プリセットとの互換性を保つ。
	ParticleEmitParam param = fallback;
	if (!json.is_object()) return param;
	param.count = json.value("count", param.count);
	param.lifeTime = json.value("lifeTime", param.lifeTime);
	param.scale = SceneJsonUtility::JsonToVector3(json.value("scale", nlohmann::json::array()), param.scale);
	param.endScale = SceneJsonUtility::JsonToVector3(json.value("endScale", nlohmann::json::array()), param.endScale);
	param.baseVelocity = SceneJsonUtility::JsonToVector3(json.value("baseVelocity", nlohmann::json::array()), param.baseVelocity);
	param.randomVelocityRange = SceneJsonUtility::JsonToVector3(json.value("randomVelocityRange", nlohmann::json::array()), param.randomVelocityRange);
	param.acceleration = SceneJsonUtility::JsonToVector3(json.value("acceleration", nlohmann::json::array()), param.acceleration);
	param.randomPositionRange = SceneJsonUtility::JsonToVector3(json.value("randomPositionRange", nlohmann::json::array()), param.randomPositionRange);
	param.baseRotate = SceneJsonUtility::JsonToVector3(json.value("baseRotate", nlohmann::json::array()), param.baseRotate);
	param.isRandomRotate = json.value("isRandomRotate", param.isRandomRotate);
	param.randomRotateRange = SceneJsonUtility::JsonToVector3(json.value("randomRotateRange", nlohmann::json::array()), param.randomRotateRange);
	param.color = SceneJsonUtility::JsonToVector4(json.value("color", nlohmann::json::array()), param.color);
	param.endColor = SceneJsonUtility::JsonToVector4(json.value("endColor", nlohmann::json::array()), param.endColor);
	param.randomScaleRange = SceneJsonUtility::JsonToVector3(json.value("randomScaleRange", nlohmann::json::array()), param.randomScaleRange);
	param.isBillboard = json.value("isBillboard", param.isBillboard);
	param.isVortex = json.value("isVortex", param.isVortex);
	param.vortexAngularSpeed = json.value("vortexAngularSpeed", param.vortexAngularSpeed);
	param.vortexBaseRadius = json.value("vortexBaseRadius", param.vortexBaseRadius);
	param.vortexTopRadius = json.value("vortexTopRadius", param.vortexTopRadius);
	param.vortexHeight = json.value("vortexHeight", param.vortexHeight);
	return param;
}

} // namespace ParticleEmitParamJson
