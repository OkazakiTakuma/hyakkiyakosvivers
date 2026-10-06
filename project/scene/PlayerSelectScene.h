#pragma once

#include "BaseScene.h"

/// <summary>左側の3Dモデルと右側の能力・初期武器情報からプレイヤーを選択します。</summary>
class PlayerSelectScene : public BaseScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw2D() override;
	void Draw3D() override;
	void Finalize() override;
	void SetSceneManager(SceneManager* manager) override { sceneManager_ = manager; }
	bool IsParticleRenderingEnabled() const override { return false; }

private:
	/// <summary>左右2カラムのパネル、見出し、操作案内を生成します。</summary>
	void CreateUi();
	/// <summary>選択変更時にモデル、能力値、初期武器表示をまとめて更新します。</summary>
	void RefreshSelectionPresentation();
	/// <summary>選択中タイプのモデルをプレビュー専用GameObjectとして生成し直します。</summary>
	void RebuildCharacterModel(const PlayerStats& stats);
	/// <summary>右上の能力パネルへ表示する文字列を生成します。</summary>
	std::string MakeStatusDescription(const PlayerStats& stats) const;
	/// <summary>右下の初期武器パネルへ表示する文字列を生成します。</summary>
	std::string MakeWeaponDescription(const PlayerStats& stats) const;

	/// <summary>決定したタイプの保存、開放状態の確認、シーン遷移に使用します。</summary>
	SceneManager* sceneManager_ = nullptr;
	/// <summary>Defaultを除いた、画面上で選択できるプレイヤータイプ一覧です。</summary>
	std::vector<std::string> playerTypeNames_;
	/// <summary>入力で現在フォーカスしているタイプの位置です。</summary>
	int selectedPlayerIndex_ = 0;
	/// <summary>モデルと詳細表示へ最後に反映した位置です。</summary>
	int presentedPlayerIndex_ = -1;
	/// <summary>プレビューモデルを緩やかに振るための経過時間です。</summary>
	float previewTime_ = 0.0f;
	/// <summary>選択画面だけで使う3Dプレビュー用カメラです。</summary>
	std::unique_ptr<Camera> previewCamera_;
	/// <summary>選択中キャラクターの3D表示を所有します。</summary>
	std::unique_ptr<GameObject> characterModelObject_;

	// 左のモデル領域と右の詳細領域を構成する単色パネルです。
	std::unique_ptr<Sprite> backgroundSprite_;
	std::unique_ptr<Sprite> modelPanelSprite_;
	std::unique_ptr<Sprite> modelPanelAccentSprite_;
	std::unique_ptr<Sprite> detailPanelSprite_;
	std::unique_ptr<Sprite> statusPanelSprite_;
	std::unique_ptr<Sprite> weaponPanelSprite_;
	std::unique_ptr<Sprite> weaponIconBackgroundSprite_;
	std::unique_ptr<Sprite> weaponIconSprite_;
	// キャラクター名、能力、武器詳細、操作案内を描画する文字オブジェクトです。
	std::unique_ptr<GameObject> titleTextObject_;
	std::unique_ptr<GameObject> characterNameTextObject_;
	std::unique_ptr<GameObject> characterStateTextObject_;
	std::unique_ptr<GameObject> statusHeadingTextObject_;
	std::unique_ptr<GameObject> statusTextObject_;
	std::unique_ptr<GameObject> weaponHeadingTextObject_;
	std::unique_ptr<GameObject> weaponNameTextObject_;
	std::unique_ptr<GameObject> weaponTextObject_;
	std::unique_ptr<GameObject> pageTextObject_;
	std::unique_ptr<GameObject> instructionTextObject_;
};
