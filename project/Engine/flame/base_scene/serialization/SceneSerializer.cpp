#include "base_scene/serialization/SceneSerializer.h"

#include "StringUtility.h"

#include <fstream>
#include <utility>

namespace {
// objectOverridesは名前で対象を指定するため、childrenも含めて深さ優先で検索する。
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

SceneSerializationResult LoadWithInheritance(
	const std::filesystem::path& filePath,
	nlohmann::json& root,
	int depth) {
	// 循環参照や過度に深い継承で無限再帰しないよう上限を設ける。
	if (depth > 8) {
		return {SceneSerializationError::InheritanceDepthExceeded, filePath.string()};
	}
	std::ifstream input(filePath);
	if (!input) {
		return {SceneSerializationError::FileNotFound, filePath.string()};
	}
	nlohmann::json source;
	try {
		input >> source;
	} catch (const nlohmann::json::exception& exception) {
		return {SceneSerializationError::InvalidJson, exception.what()};
	}
	if (!source.is_object() ||
		(source.contains("extends") && !source["extends"].is_string())) {
		return {SceneSerializationError::InvalidJson, filePath.string()};
	}

	const std::string extendsFile = source.value("extends", std::string());
	if (extendsFile.empty()) {
		root = std::move(source);
		return {};
	}

	// 親JSONを同じディレクトリ内へ限定し、絶対パスや親ディレクトリへの逸脱を防ぐ。
	const std::filesystem::path extendsPath = StringUtility::Utf8ToPath(extendsFile);
	if (extendsPath.filename() != extendsPath || extendsPath.extension() != ".json") {
		return {SceneSerializationError::InvalidExtendsPath, extendsFile};
	}
	SceneSerializationResult parentResult =
		LoadWithInheritance(filePath.parent_path() / extendsPath, root, depth + 1);
	if (!parentResult.Succeeded()) {
		return parentResult;
	}

	// 通常フィールドはJSON Merge Patchで親へ重ね、配列の個別操作は専用キーで後処理する。
	nlohmann::json overlay = source;
	overlay.erase("extends");
	overlay.erase("objectOverrides");
	overlay.erase("objectAdditions");
	root.merge_patch(overlay);
	if (!root.contains("objects") || !root["objects"].is_array()) {
		root["objects"] = nlohmann::json::array();
	}

	// 既存オブジェクトの差分適用後、新規オブジェクトを末尾へ追加する。
	for (const nlohmann::json& objectOverride : source.value("objectOverrides", nlohmann::json::array())) {
		ApplyObjectOverride(root["objects"], objectOverride);
	}
	for (const nlohmann::json& addition : source.value("objectAdditions", nlohmann::json::array())) {
		root["objects"].push_back(addition);
	}
	return {};
}
}

SceneSerializationResult SceneSerializer::Load(
	const std::filesystem::path& filePath,
	nlohmann::json& root) {
	return LoadWithInheritance(filePath, root, 0);
}

SceneSerializationResult SceneSerializer::Save(
	const std::filesystem::path& filePath,
	const nlohmann::json& root) {
	// シーン名ごとのサブディレクトリへも保存できるよう、親フォルダを先に作成する。
	std::error_code directoryError;
	if (!filePath.parent_path().empty()) {
		std::filesystem::create_directories(filePath.parent_path(), directoryError);
	}
	if (directoryError) {
		return {SceneSerializationError::WriteFailed, directoryError.message()};
	}

	std::ofstream output(filePath);
	if (!output) {
		return {SceneSerializationError::WriteFailed, filePath.string()};
	}
	output << root.dump(4);
	if (!output.good()) {
		return {SceneSerializationError::WriteFailed, filePath.string()};
	}
	return {};
}
