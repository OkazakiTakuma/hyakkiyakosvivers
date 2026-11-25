#pragma once

#include "base_scene/BaseScene.h"

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

private:
	/// <summary>現在の所持金、商品レベル、価格、選択状態を表示へ反映します。</summary>
	void RefreshText();
	/// <summary>選択中商品の購入可否を判定し、プレイヤー能力と購入レベルを保存します。</summary>
	void PurchaseSelectedUpgrade();
	/// <summary>左側の神社と祈祷中の巫女へ、ショップ専用の照明とポーズを適用します。</summary>
	void UpdateShrinePresentation();
	/// <summary>購入成功時に巫女から拝殿へ届く祈りの光を描画します。</summary>
	void DrawPrayerDeliveryEffect(float height, float panelX);

	/// <summary>現在選択されている商品の配列インデックスです。</summary>
	int selectedIndex_ = 0;
	/// <summary>購入成功、所持金不足、最大レベルなどの通知文です。</summary>
	std::string message_;
	/// <summary>画面全体を塗る背景です。</summary>
	std::unique_ptr<Sprite> backgroundSprite_;
	/// <summary>商品一覧を載せる中央パネルです。</summary>
	std::unique_ptr<Sprite> panelSprite_;
	/// <summary>3D情景と商品欄を分ける朱色の縦線です。</summary>
	std::unique_ptr<Sprite> dividerSprite_;
	/// <summary>左側の神社情景へ重ねる案内板です。</summary>
	std::unique_ptr<Sprite> shrineCaptionSprite_;
	/// <summary>各商品をカード状に区切る背景です。</summary>
	std::array<std::unique_ptr<Sprite>, 8> itemPanelSprites_;
	/// <summary>選択中商品の詳細説明を載せる下部パネルです。</summary>
	std::unique_ptr<Sprite> detailPanelSprite_;
	/// <summary>祈りが拝殿へ向かう光跡として使う円形スプライトです。</summary>
	std::array<std::unique_ptr<Sprite>, 6> prayerTrailSprites_;
	/// <summary>祈りの先端を示す発光スプライトです。</summary>
	std::unique_ptr<Sprite> prayerOrbSprite_;
	/// <summary>祈りの先端へ重ねる外側の柔らかな発光です。</summary>
	std::unique_ptr<Sprite> prayerOrbGlowSprite_;
	/// <summary>巫女の手元から祈りが生まれる瞬間の発光です。</summary>
	std::unique_ptr<Sprite> prayerSourceGlowSprite_;
	/// <summary>祈りの進行方向を示す細い光路です。</summary>
	std::unique_ptr<Sprite> prayerBeamSprite_;
	/// <summary>光路の中央を鋭く見せる白いレーザー芯です。</summary>
	std::unique_ptr<Sprite> prayerBeamCoreSprite_;
	/// <summary>拝殿へ到達した瞬間に広がる光輪です。</summary>
	std::array<std::unique_ptr<Sprite>, 2> prayerPulseSprites_;
	/// <summary>拝殿へ届いた祈りの中心に残る柔らかな残光です。</summary>
	std::unique_ptr<Sprite> prayerArrivalGlowSprite_;
	/// <summary>祈りの到達時に外へ散る小さな光です。</summary>
	std::array<std::unique_ptr<Sprite>, 6> prayerSparkSprites_;
	/// <summary>祈りの到達点から四方へ放射するレーザーの外光と芯です。</summary>
	std::array<std::unique_ptr<Sprite>, 8> prayerLaserSprites_;
	/// <summary>ショップ見出しの文字です。</summary>
	std::unique_ptr<Entity> titleTextObject_;
	/// <summary>共有所持金と強化対象プレイヤーの表示です。</summary>
	std::unique_ptr<Entity> moneyTextObject_;
	/// <summary>左側の3D情景が祈祷所であることを示す表示です。</summary>
	std::unique_ptr<Entity> shrineCaptionTextObject_;
	/// <summary>個別強化と全体強化の列見出しです。</summary>
	std::array<std::unique_ptr<Entity>, 2> categoryTextObjects_;
	/// <summary>キャラクター強化5種類と全体商品3種類を表示する行です。</summary>
	std::array<std::unique_ptr<Entity>, 8> itemTextObjects_;
	/// <summary>選択中商品の効果と購入内容を表示します。</summary>
	std::unique_ptr<Entity> detailTextObject_;
	/// <summary>購入結果や購入不可理由を表示します。</summary>
	std::unique_ptr<Entity> messageTextObject_;
	/// <summary>選択、購入、終了操作の説明です。</summary>
	std::unique_ptr<Entity> instructionTextObject_;
	/// <summary>巫女モデルの初期姿勢を祈祷ポーズへ変更済みかを示します。</summary>
	bool prayerPoseApplied_ = false;
	/// <summary>購入成功からの祈り演出経過秒です。負値は停止中を表します。</summary>
	float prayerEffectTime_ = -1.0f;
};
