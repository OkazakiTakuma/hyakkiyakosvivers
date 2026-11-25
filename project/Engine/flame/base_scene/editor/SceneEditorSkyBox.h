#pragma once

#include "sky/SkyBox.h"

#include <memory>
#include <string>

/// <summary>シーンエディタ用SkyBoxのリソース、選択状態、編集UIを所有します。</summary>
class SceneEditorSkyBox final {
public:
	/// <summary>SkyBoxリソースとエディタ選択状態を初期化します。</summary>
	void Reset();
	/// <summary>有効なSkyBoxの状態を更新します。</summary>
	void Update();
	/// <summary>有効なSkyBoxを描画します。</summary>
	void Draw();
	/// <summary>テクスチャ選択と有効化を操作するエディタUIを描画します。</summary>
	void DrawControls();
	/// <summary>指定テクスチャからSkyBoxを生成して有効化します。</summary>
	void Load(const std::string& textureFilePath);
	/// <summary>シーンJSONから読み込んだ有効状態とテクスチャを復元します。</summary>
	void Restore(bool enabled, const std::string& textureFilePath);

	bool IsEnabled() const { return enabled_; }
	const std::string& GetTextureFilePath() const { return textureFilePath_; }

private:
	// 選択インデックスはエディタ表示用、パスは保存・復元用の正本として保持する。
	bool enabled_ = false;
	int selectedTextureIndex_ = 0;
	std::string textureFilePath_;
	std::unique_ptr<SkyBox> skyBox_;
};
