#pragma once

#include "base_scene/BaseScene.h"
#include <array>

/// <summary>ゲーム終了後のクリア／ゲームオーバーと最終戦績を表示します。</summary>
class ResultScene : public BaseScene {
public:
	/// <summary>確定済みの戦績を読み込み、表示用オブジェクトを生成します。</summary>
	void Initialize() override;
	/// <summary>再挑戦、ステージ選択、タイトルへの入力を処理します。</summary>
	void Update() override;
	/// <summary>終了結果、戦績、最終装備を2D画面へ描画します。</summary>
	void Draw2D() override;
	/// <summary>リザルトシーンでは3D描画を行いません。</summary>
	void Draw3D() override {}
	/// <summary>リザルト表示用に確保したオブジェクトを解放します。</summary>
	void Finalize() override;
	/// <summary>シーン遷移と共有戦績の参照に使うSceneManagerを設定します。</summary>
	void SetSceneManager(SceneManager* manager) override { sceneManager_ = manager; }

private:
	SceneManager* sceneManager_ = nullptr;
	/// <summary>見出しと配色をクリア用／死亡用に切り替えるフラグです。</summary>
	bool isStageClear_ = false;
	/// <summary>文字・札・霧を順に現す導入演出の経過時間です。</summary>
	float presentationElapsed_ = 0.0f;
	/// <summary>画面全体を塗る背景です。</summary>
	std::unique_ptr<Sprite> backgroundSprite_;
	/// <summary>戦績をまとめて表示する中央パネルです。</summary>
	std::unique_ptr<Sprite> panelSprite_;
	/// <summary>背景をゆっくり流れる夜霧です。</summary>
	std::array<std::unique_ptr<Sprite>, 3> fogSprites_;
	/// <summary>影、各札、金縁、区切り、菱形飾りに使用する個別スプライトです。</summary>
	std::array<std::unique_ptr<Sprite>, 13> decorationSprites_;
	/// <summary>STAGE CLEARまたはGAME OVERを表示します。</summary>
	std::unique_ptr<GameObject> clearTextObject_;
	/// <summary>和文見出しの下へ小さく表示する英字副題です。</summary>
	std::unique_ptr<GameObject> subtitleTextObject_;
	/// <summary>プレイヤー、ステージ、獲得金、撃破数、生存時間を表示します。</summary>
	std::unique_ptr<GameObject> resultTextObject_;
	/// <summary>終了時点の攻撃装備名とレベルを表示します。</summary>
	std::unique_ptr<GameObject> attackEquipmentTextObject_;
	/// <summary>終了時点のステータス装備名とレベルを表示します。</summary>
	std::unique_ptr<GameObject> statusEquipmentTextObject_;
	/// <summary>リザルト画面で利用できる操作を表示します。</summary>
	std::unique_ptr<GameObject> instructionTextObject_;
};
