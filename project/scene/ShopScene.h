#pragma once

#include "BaseScene.h"

#include <array>

/// <summary>
/// ゲーム全体の所持金を使い、選択中のプレイヤーへ永続的な能力強化を購入するシーンです。
/// </summary>
class ShopScene : public BaseScene {
public:
	/// <summary>ショップ画面で使用する背景、パネル、文字オブジェクトを生成します。</summary>
	void Initialize() override;
	/// <summary>商品選択、購入、タイトルへ戻る入力を処理します。</summary>
	void Update() override;
	/// <summary>所持金、商品一覧、購入結果を画面へ描画します。</summary>
	void Draw2D() override;
	/// <summary>ショップは2D UIのみのため3D描画を行いません。</summary>
	void Draw3D() override {}
	/// <summary>ショップ表示用に確保したオブジェクトを解放します。</summary>
	void Finalize() override;
	/// <summary>所持金、選択プレイヤー、シーン遷移の操作に使うSceneManagerを設定します。</summary>
	void SetSceneManager(SceneManager* manager) override { sceneManager_ = manager; }

private:
	/// <summary>プレイヤー設定一覧から開放済みキャラクターだけをショップ候補へ登録します。</summary>
	void RefreshUnlockedPlayerTargets();
	/// <summary>左右入力に応じてショップの強化対象を循環させます。</summary>
	void ChangePlayerTarget(int direction);
	/// <summary>現在ショップで強化対象にしているプレイヤー名を返します。</summary>
	std::string GetTargetPlayerTypeName() const;
	/// <summary>現在の所持金、商品レベル、価格、選択状態を表示へ反映します。</summary>
	void RefreshText();
	/// <summary>選択中商品の購入可否を判定し、プレイヤー能力と購入レベルを保存します。</summary>
	void PurchaseSelectedUpgrade();

	/// <summary>SceneManagerが所有するインスタンスへの非所有参照です。</summary>
	SceneManager* sceneManager_ = nullptr;
	/// <summary>現在選択されている商品の配列インデックスです。</summary>
	int selectedIndex_ = 0;
	/// <summary>ショップで選択可能な開放済みプレイヤー名の一覧です。</summary>
	std::vector<std::string> unlockedPlayerTypeNames_;
	/// <summary>現在強化対象にしている開放済みプレイヤーのインデックスです。</summary>
	int selectedPlayerIndex_ = 0;
	/// <summary>購入成功、所持金不足、最大レベルなどの通知文です。</summary>
	std::string message_;
	/// <summary>画面全体を塗る背景です。</summary>
	std::unique_ptr<Sprite> backgroundSprite_;
	/// <summary>商品一覧を載せる中央パネルです。</summary>
	std::unique_ptr<Sprite> panelSprite_;
	/// <summary>ショップ見出しの文字です。</summary>
	std::unique_ptr<GameObject> titleTextObject_;
	/// <summary>共有所持金と強化対象プレイヤーの表示です。</summary>
	std::unique_ptr<GameObject> moneyTextObject_;
	/// <summary>キャラクター強化4種類と全体商品3種類を表示する行です。</summary>
	std::array<std::unique_ptr<GameObject>, 7> itemTextObjects_;
	/// <summary>購入結果や購入不可理由を表示します。</summary>
	std::unique_ptr<GameObject> messageTextObject_;
	/// <summary>選択、購入、終了操作の説明です。</summary>
	std::unique_ptr<GameObject> instructionTextObject_;
};
