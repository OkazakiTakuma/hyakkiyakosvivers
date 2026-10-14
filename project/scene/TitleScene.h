#pragma once

#include "BaseScene.h"
#include "sky/AtmosphereSky.h"
/// <summary>ゲーム開始とショップへの入口を表示するタイトルシーンです。</summary>
class TitleScene: public BaseScene {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	void Initialize() override;
	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update() override;
	/// <summary>物理モデルを簡略化した大気散乱Skyと太陽円盤を描画します。</summary>
	void DrawSkyBox() override;
	/// <summary>
	/// 2D 要素の描画処理を行います。
	/// </summary>
	void Draw2D() override;
	/// <summary>
	/// 3D 要素の描画処理を行います。
	/// </summary>
	void Draw3D() override;
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize() override;
	/// <summary>シーン遷移と共有所持金の参照に使うSceneManagerを設定します。</summary>
	void SetSceneManager(SceneManager* manager) override { sceneManager = manager; }

private:
	enum class TitlePhase {
		FadeIn,
		Slash,
		LogoReveal,
		WaitingForInput,
		Menu,
		Exit
	};
	/// <summary>経過時間から現在のタイトル演出段階を更新します。</summary>
	void UpdateTitlePhase();
	/// <summary>タイトル背景で使用する3DモデルをシーンJSONの読込前に登録します。</summary>
	void LoadTitleModels();
	/// <summary>勾玉と妖怪へタイトル画面用の待機モーションを与えます。</summary>
	void UpdateTitlePresentation();
	/// <summary>勾玉が背景の妖怪を連続撃破してから、指定シーンへのフェードを開始します。</summary>
	void UpdateExitPresentation();
	/// <summary>タイトル専用の撃破演出を開始し、完了後に遷移するシーン名を保持します。</summary>
	void BeginExitTransition(const std::string& sceneName);
	/// <summary>タイトル画面用の2D UIを生成します。</summary>
	void CreateUi();
	/// <summary>
	/// ImGui によるデバッグ用 UI の表示と編集処理を行います。
	/// </summary>
	void ImGuiUpdate();
	/// <summary>SceneManagerが所有するインスタンスへの非所有参照です。</summary>
	SceneManager* sceneManager = nullptr;
	/// <summary>球体モデルを使わず、空の散乱計算から太陽を描くタイトル専用Skyです。</summary>
	std::unique_ptr<AtmosphereSky> atmosphereSky_;
	/// <summary>タイトル背景の地面へ配置する草のインスタンシング描画です。</summary>
	std::unique_ptr<InstancingModel> grassInstancingModel_;
	/// <summary>画面全体を塗る背景です。</summary>
	std::unique_ptr<Sprite> backgroundSprite_;
	/// <summary>導入時に3Dシーンを黒から浮かび上がらせるフェードです。</summary>
	std::unique_ptr<Sprite> fadeSprite_;
	/// <summary>ロゴ出現直前の斬撃フラッシュを描く単色スプライトです。</summary>
	std::unique_ptr<Sprite> slashSprite_;
	/// <summary>画面上下のアクセントライン描画に使い回す単色スプライトです。</summary>
	std::unique_ptr<Sprite> accentSprite_;
	/// <summary>開始・初めから・ショップボタンをまとめる中央パネルです。</summary>
	std::unique_ptr<Sprite> menuPanelSprite_;
	/// <summary>ゲーム開始項目の選択状態を示す背景です。</summary>
	std::unique_ptr<Sprite> startButtonSprite_;
	/// <summary>初めから項目の選択状態を示す背景です。</summary>
	std::unique_ptr<Sprite> newGameButtonSprite_;
	/// <summary>ショップ項目の選択状態を示す背景です。</summary>
	std::unique_ptr<Sprite> shopButtonSprite_;
	/// <summary>初期化確認中に背面のメニューを暗くするスプライトです。</summary>
	std::unique_ptr<Sprite> confirmationBackdropSprite_;
	/// <summary>初期化確認メッセージを囲むパネルです。</summary>
	std::unique_ptr<Sprite> confirmationPanelSprite_;
	/// <summary>画像化したゲームタイトルロゴです。</summary>
	std::unique_ptr<Sprite> titleLogoSprite_;
	/// <summary>ゲーム開始メニューの文字です。</summary>
	std::unique_ptr<GameObject> startTextObject_;
	/// <summary>進行をリセットして始めるメニューの文字です。</summary>
	std::unique_ptr<GameObject> newGameTextObject_;
	/// <summary>ショップメニューの文字です。</summary>
	std::unique_ptr<GameObject> shopTextObject_;
	/// <summary>SceneManagerが保持する現在の所持金表示です。</summary>
	std::unique_ptr<GameObject> moneyTextObject_;
	/// <summary>キーボードとゲームパッドの操作説明です。</summary>
	std::unique_ptr<GameObject> instructionTextObject_;
	/// <summary>画面下部で決定操作を促す文字です。</summary>
	std::unique_ptr<GameObject> versionTextObject_;
	/// <summary>ゲームデータ初期化前の確認メッセージです。</summary>
	std::unique_ptr<GameObject> confirmationTextObject_;
	/// <summary>初期化の実行・キャンセル操作を案内する文字です。</summary>
	std::unique_ptr<GameObject> confirmationInstructionTextObject_;
	/// <summary>0がゲーム開始、1が初めから、2がショップを表す現在の選択位置です。</summary>
	int selectedMenuIndex_ = 0;
	/// <summary>ゲームデータ初期化の確認画面を表示中かどうかです。</summary>
	bool isResetConfirmationOpen_ = false;
	/// <summary>選択中ボタンの明滅に使う累積秒数です。</summary>
	float pulseTime_ = 0.0f;
	/// <summary>タイトル導入演出の開始からの経過秒数です。</summary>
	float presentationTime_ = 0.0f;
	/// <summary>勾玉の周回位置を速度変化で飛ばさず積算する共有角度です。</summary>
	float magatamaOrbitAngle_ = 0.0f;
	/// <summary>タイトル専用の巫女ポーズを初回だけ適用済みかどうかです。</summary>
	bool titleMikoPoseApplied_ = false;
	/// <summary>現在の導入演出またはメニュー段階です。</summary>
	TitlePhase titlePhase_ = TitlePhase::FadeIn;
	/// <summary>タイトル終了演出で移動させる勾玉への非所有参照です。</summary>
	std::vector<GameObject*> exitMagatamas_;
	/// <summary>終了演出開始時の各勾玉位置です。</summary>
	std::vector<Vector3> exitMagatamaStartPositions_;
	/// <summary>タイトル終了演出で撃破する妖怪への非所有参照です。</summary>
	std::vector<GameObject*> exitEnemies_;
	/// <summary>縮小消滅後に元の大きさを失わないための各妖怪の開始スケールです。</summary>
	std::vector<Vector3> exitEnemyStartScales_;
	/// <summary>タイトル終了演出の経過秒数です。</summary>
	float exitElapsed_ = 0.0f;
	/// <summary>撃破演出完了後にSceneManagerへ渡す遷移先です。</summary>
	std::string exitSceneName_;
	/// <summary>SceneManagerへ遷移要求を二重送信しないための状態です。</summary>
	bool exitSceneChangeRequested_ = false;
};
