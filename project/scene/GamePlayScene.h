#pragma once
#include "base_scene/BaseScene.h"
#include "core/GameObject.h"
#include "object/Object3dComponent.h"
#include "particle/ParticleEmitterComponent.h"
#include "sky/AtmosphereSky.h"
#include "SpriteComponent.h"
#include "instancing/GrassInstancingModel.h"


class GamePlayScene: public BaseScene {
public:
	/// <summary>
	/// 必要なリソースを準備し、オブジェクトを初期化します。
	/// </summary>
	void Initialize() override;
	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update() override;
	/// <summary>
	/// スカイボックスの描画処理を行います。
	/// </summary>
	void DrawSkyBox() override;
	/// <summary>
	/// 2D 要素の描画処理を行います。
	/// </summary>
	void Draw2D() override;
	/// <summary>ゲームHUDより手前へポーズメニューを描画します。</summary>
	void DrawOverlay2D() override;
	/// <summary>
	/// 3D 要素の描画処理を行います。
	/// </summary>
	void Draw3D() override;
	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize() override;
	/// <summary>ゲームプレイ固有処理とBaseScene共通処理の両方へSceneManagerを設定します。</summary>
	void SetSceneManager(SceneManager* manager) override {
		BaseScene::SetSceneManager(manager);
		sceneManager = manager;
	}
	/// ゲームプレイ画面の「Show Particles」設定を共通描画処理へ反映します。
	bool IsParticleRenderingEnabled() const override { return isShowParticles_; }
	/// <summary>自動デモからプレイヤーを敗北させ、通常と同じゲームオーバー演出を開始します。</summary>
	bool BeginDemoDefeatPresentation();

private:
	bool isShowSkyBox_ = false;
	bool isShowSprite_ = true;
	bool isShowSprites_ = true;
	bool isShowObject3D_ = true;
	bool isShowInstancing_ = true;
	bool isShowSphere_ = true;
	bool isShowCylinder_ = true;
	bool isShowParticles_ = true;
	enum class PauseMenuItem { Resume, Retry, StageSelect, Count };
	int selectedPauseMenuItem_ = static_cast<int>(PauseMenuItem::Resume);
	std::unique_ptr<Sprite> pauseOverlaySprite_;
	std::unique_ptr<Sprite> pausePanelSprite_;
	std::unique_ptr<Sprite> pauseSelectionSprite_;
	std::unique_ptr<GameObject> pauseTitleTextObject_;
	std::vector<std::unique_ptr<GameObject>> pauseMenuTextObjects_;
	std::unique_ptr<GameObject> pauseInstructionTextObject_;
	std::unique_ptr<Audio> audio_ = nullptr;
	std::unique_ptr<GameObject> spriteObject_ = nullptr;
	SpriteComponent* sprite = nullptr;

	std::vector<std::unique_ptr<GameObject>> spriteObjects_;
	std::vector<SpriteComponent*> sprites;
	std::unique_ptr<GameObject> object3dObject_ = nullptr;
	Object3dComponent* object3d = nullptr;
	std::unique_ptr<GameObject> sphereGameObject_ = nullptr;
	Object3dComponent* sphereObject = nullptr;
	std::unique_ptr<GameObject> cylinderGameObject_ = nullptr;
	Object3dComponent* cylinderObject = nullptr;

	std::vector<std::unique_ptr<GameObject>> emitterObjects_;
	std::vector<ParticleEmitterComponent*> emitters_;
	int currentParticleIndex_ = 0;
	std::vector<std::string> availableTextures_;
	Vector3 cameraPosition={0.0f, 0.0f, 0.0f};
	Vector3 cameraRotate={0.0f, 0.0f, 0.0f};

	EulerTransform trsprite={};
	EulerTransform trspriteUV={};
	Vector4 spriteColor={};
	Vector2 spriteSize={};
	Vector2 anchor={0.0f, 0.0f};
	bool isFlipX = false;
	bool isFlipY = false;
	Vector2 textureLeftTop={0.0f, 0.0f};
	Vector2 textureSize={0.0f, 0.0f};
	Vector3 modelPosition={0.0f, 0.0f, 0.0f};
	Vector3 modelRotate={0.0f, 0.0f, 0.0f};
	Vector3 modelScale={1.0f, 1.0f, 1.0f};
	/// <summary>
	/// SceneModels を読み込み、内部データへ反映します。
	/// </summary>
	void LoadSceneModels();
	/// <summary>プレイヤーが力尽きて倒れるゲームオーバー演出を開始します。</summary>
	void BeginDefeatPresentation();
	/// <summary>敵や攻撃を止めたまま、プレイヤーの倒れ込みと暗転を更新します。</summary>
	void UpdateDefeatPresentation();
	/// <summary>倒れ込みの後半に暗幕と「力尽きた」表示を重ねます。</summary>
	void DrawDefeatPresentation();
	/// <summary>ポーズメニューの描画リソースを生成します。</summary>
	void InitializePauseMenu();
	/// <summary>霧と選択キャラクター固有の登場動作を使う開始演出を準備します。</summary>
	void BeginStartPresentation();
	/// <summary>ゲーム時間を止めたまま開始演出だけを更新します。</summary>
	void UpdateStartPresentation();
	/// <summary>2D霧と粒状の深度霧を同じ進行度でフェードイン・フェードアウトさせます。</summary>
	void UpdateStartFogFade(float normalizedTime);
	/// <summary>霧と開戦表示をHUDより手前へ描画します。</summary>
	void DrawStartPresentation();
	/// <summary>ポーズ中の項目選択と決定入力を処理します。</summary>
	void UpdatePauseMenu();
	/// <summary>ポーズ状態を切り替え、ゲーム時間へ反映します。</summary>
	void SetPauseMenuOpen(bool isOpen);
	/// <summary>ステージ1の太陽方向と夕方色を全3Dモデルへ反映します。</summary>
	void ApplyStageOneEveningLight();
	bool startPresentationInitialized_ = false;
	bool startPresentationActive_ = false;
	float startPresentationElapsed_ = 0.0f;
	float startFogAlpha_ = 1.0f;
	float startWhiteAlpha_ = 1.0f;
	GameObject* startPresentationPlayer_ = nullptr;
	EulerTransform startPresentationBaseTransform_{};
	std::array<std::unique_ptr<Sprite>, 3> startFogSprites_;
	std::unique_ptr<Sprite> startWhiteSprite_;
	std::unique_ptr<Sprite> startMessageSprite_;
	bool defeatPresentationActive_ = false;
	bool defeatTransitionRequested_ = false;
	float defeatPresentationElapsed_ = 0.0f;
	GameObject* defeatPresentationPlayer_ = nullptr;
	std::unique_ptr<GameObject> defeatMessageTextObject_;
	/// <summary>
	/// ImGui によるデバッグ用 UI の表示と編集処理を行います。
	/// </summary>
	void ImGuiUpdate();
	Vector4 lightColor = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 lightDirection = {0.0f, -1.0f, 0.0f};
	float lightIntensity = 1.0f;	
	bool isParticleEmit = false;
	SceneManager* sceneManager = nullptr;
	std::unique_ptr<AtmosphereSky> atmosphereSky_ = nullptr;
	std::unique_ptr<SkyBox> skyBox = nullptr;
	std::unique_ptr<GrassInstancingModel> instancingModel_ = nullptr;

};
