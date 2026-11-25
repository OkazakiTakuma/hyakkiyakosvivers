#include "repositories/ParticlePresetRepository.h"

#include "helpers/ParticleEmitParamJson.h"
#include "StringUtility.h"

#include <filesystem>
#include <fstream>
#include <iomanip>

namespace {
// 保存ファイルがまだ無い場合も、呼び出し側が空の一覧として扱えるようにする。
nlohmann::json LoadRoot() {
	std::ifstream input(StringUtility::Utf8ToPath(ParticlePresetRepository::kFilePath));
	if (!input) return nlohmann::json::object();
	nlohmann::json root;
	input >> root;
	return root.is_object() ? root : nlohmann::json::object();
}

// JSONに存在する項目だけを値オブジェクトへ反映し、旧形式との互換性を保つ。
ParticlePresetData JsonToPreset(const nlohmann::json& json, const ParticlePresetData& fallback) {
	ParticlePresetData preset = fallback;
	if (!json.is_object()) return preset;
	preset.textureFilePath = json.value("textureFilePath", preset.textureFilePath);
	preset.isActive = json.value("isActive", preset.isActive);
	preset.frequency = json.value("frequency", preset.frequency);
	preset.blendMode = static_cast<BlendMode>(json.value("blendMode", static_cast<int>(preset.blendMode)));
	preset.meshType = static_cast<ParticleMeshType>(json.value("meshType", static_cast<int>(preset.meshType)));

	// シーン保存と同じCodecを使い、ParticleEmitParamの項目一覧を一か所に限定する。
	preset.emitParam = ParticleEmitParamJson::FromJson(
		json.value("emitParam", nlohmann::json::object()), preset.emitParam);
	return preset;
}

// コンポーネントに依存しない値オブジェクトを、保存用JSONへ変換する。
nlohmann::json PresetToJson(const ParticlePresetData& preset) {
	nlohmann::json json;
	json["blendMode"] = static_cast<int>(preset.blendMode);
	json["count"] = preset.emitParam.count;
	json["frequency"] = preset.frequency;
	json["isActive"] = preset.isActive;
	json["meshType"] = static_cast<int>(preset.meshType);
	json["textureFilePath"] = preset.textureFilePath;
	json["emitParam"] = ParticleEmitParamJson::ToJson(preset.emitParam);
	return json;
}
}

namespace ParticlePresetRepository {

// プリセット名だけを列挙し、エディターの選択肢として利用できる形で返す。
std::vector<std::string> LoadNames() {
	std::vector<std::string> names;
	const nlohmann::json root = LoadRoot();
	for (auto it = root.begin(); it != root.end(); ++it) names.push_back(it.key());
	return names;
}

// 指定名が見つかった場合だけ、呼び出し側の既定値へJSONの値を上書きする。
bool Load(const std::string& presetName, ParticlePresetData& preset) {
	const nlohmann::json root = LoadRoot();
	if (!root.contains(presetName)) return false;
	preset = JsonToPreset(root.at(presetName), preset);
	return true;
}

// 既存プリセットを残したまま、指定された一件を追加または更新する。
bool Save(const std::string& presetName, const ParticlePresetData& preset) {
	if (presetName.empty()) return false;
	nlohmann::json root = LoadRoot();
	root[presetName] = PresetToJson(preset);
	std::filesystem::create_directories(StringUtility::Utf8ToPath(kFilePath).parent_path());
	std::ofstream output(StringUtility::Utf8ToPath(kFilePath));
	if (!output) return false;
	output << std::setw(4) << root << std::endl;
	return output.good();
}

} // namespace ParticlePresetRepository
