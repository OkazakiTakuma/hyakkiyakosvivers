#pragma once

#include <filesystem>
#include <string>

#pragma warning(push)
#pragma warning(disable: 26495)
#include <json.hpp>
#pragma warning(pop)

/// <summary>シーンJSONの読み書きで呼び出し側が判定できるエラー種別です。</summary>
enum class SceneSerializationError {
	None,
	FileNotFound,
	InvalidJson,
	InheritanceDepthExceeded,
	InvalidExtendsPath,
	WriteFailed
};

/// <summary>例外を外へ送出せずに返す、シリアライズ処理の成否と詳細です。</summary>
struct SceneSerializationResult {
	SceneSerializationError error = SceneSerializationError::None;
	std::string detail;

	bool Succeeded() const { return error == SceneSerializationError::None; }
};

/// <summary>シーンJSONのファイルI/Oと継承形式の解決を担当します。</summary>
class SceneSerializer final {
public:
	/// <summary>JSONを読み込み、extends指定がある場合は親シーンとの合成まで行います。</summary>
	static SceneSerializationResult Load(
		const std::filesystem::path& filePath,
		nlohmann::json& root);
	/// <summary>保存先ディレクトリを準備し、整形済みJSONを書き込みます。</summary>
	static SceneSerializationResult Save(
		const std::filesystem::path& filePath,
		const nlohmann::json& root);
};
