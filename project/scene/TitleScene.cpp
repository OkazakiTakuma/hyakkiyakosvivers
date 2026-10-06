#include "TitleScene.h"
#include "SceneManager.h"
#include "MathConstants.h"
#include <model/ModelManager.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <Xinput.h>

namespace {
constexpr float kSlashStartSeconds = 1.70f;
constexpr float kLogoStartSeconds = 2.25f;
constexpr float kPromptStartSeconds = 5.00f;
// リソースフォントを追加するまではWindows標準の和文明朝・UIゴシックを使用する。
// Resources/Fontsへ同系統のフォントを追加した場合は、このファミリー名だけを差し替えればよい。
constexpr const char* kTitleFontName = "Yu Mincho";
constexpr const char* kUiFontName = "Yu Gothic UI";
constexpr int kMenuItemCount = 3;

float SmoothStep(float start, float end, float value) {
	const float rate = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
	return rate * rate * (3.0f - 2.0f * rate);
}

std::unique_ptr<Sprite> CreateColorSprite() {
	// 1枚の白テクスチャに色を乗算し、背景・パネル・ボタンへ使い回す。
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize("Resources/human/white.png");
	return sprite;
}

std::unique_ptr<GameObject> CreateTextObject(
	const std::string& text, float fontSize, const Vector4& color, const char* fontName = kUiFontName) {
	// タイトルUIの文字はすべて中央基準で配置できるよう設定を統一する。
	auto object = std::make_unique<GameObject>();
	TextComponent* textComponent = object->AddComponent<TextComponent>();
	textComponent->SetText(text);
	textComponent->SetFontName(fontName);
	textComponent->SetFontSize(fontSize);
	textComponent->SetAnchor(TextComponent::Anchor::Center);
	textComponent->SetColor(color);
	return object;
}

void DrawColorSprite(Sprite* sprite, float x, float y, float width, float height, const Vector4& color) {
	// 単色矩形として描画するため、位置・大きさ・色を描画直前にまとめて反映する。
	EulerTransform transform = sprite->GetTransform();
	transform.translate = {x, y, 0.0f};
	sprite->SetTransform(transform);
	sprite->SetSize({width, height});
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}

void DrawRotatedColorSprite(
	Sprite* sprite, float centerX, float centerY, float width, float height, float rotation, const Vector4& color) {
	EulerTransform transform = sprite->GetTransform();
	transform.translate = {centerX, centerY, 0.0f};
	transform.rotate.z = rotation;
	sprite->SetTransform(transform);
	sprite->SetAnchorPoint({0.5f, 0.5f});
	sprite->SetSize({width, height});
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}
}

void TitleScene::Initialize() {
	// タイトルへ戻るたび、ゲーム開始が選ばれた初期状態から表示する。
	selectedMenuIndex_ = 0;
	isResetConfirmationOpen_ = false;
	pulseTime_ = 0.0f;
	presentationTime_ = 0.0f;
	titlePhase_ = TitlePhase::FadeIn;

	// 太陽方向を空・平行光源・距離霞で共有し、別々に位置を調整する必要をなくします。
	AtmosphereSettings& atmosphere = AtmosphereSystem::GetInstance()->GetSettings();
	atmosphere.enabled = true;
	atmosphere.sunDirection = NormalizeReturnVector({-0.18f, 0.03f, 0.98f});
	atmosphere.sunAngularRadius = 0.022f;
	atmosphere.sunIntensity = 3.5f;
	atmosphere.rayleighStrength = 0.72f;
	atmosphere.mieStrength = 0.075f;
	atmosphere.mieG = 0.80f;
	atmosphere.atmosphereDensity = 0.92f;
	atmosphere.horizonColor = {0.82f, 0.18f, 0.055f};
	atmosphere.distanceFogDensity = 0.012f;
	atmosphere.heightFogDensity = 0.16f;
	atmosphere.aerialPerspectiveStrength = 0.68f;

	atmosphereSky_ = std::make_unique<AtmosphereSky>();
	atmosphereSky_->Initialize();
	LoadTitleModels();
	CreateUi();
}

void TitleScene::LoadTitleModels() {
	// SceneManagerはInitializeの直後にTITLE_objects.jsonを読み込むため、先に参照モデルを登録する。
	ModelManager::GetInstance()->LoadModel("miko.gltf", true, "/human");
	ModelManager::GetInstance()->LoadModel("sand.obj", false, "/sand");
	ModelManager::GetInstance()->LoadModel("enemy_chaser.gltf");
	ModelManager::GetInstance()->LoadModel("enemy_shooter.gltf");
	ModelManager::GetInstance()->LoadModel("enemy_charger.gltf");
	ModelManager::GetInstance()->LoadModel("enemy_bomber.gltf");
	ModelManager::GetInstance()->LoadModel("magatama.gltf");
	TextureManager::GetInstance()->LoadTexture("Resources/human/white.png");
	TextureManager::GetInstance()->LoadTexture("Resources/sand/sand.png");
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");
}

void TitleScene::CreateUi() {
	// 矩形用Spriteは個別に持たせ、同一フレーム内で異なる位置と色を描画できるようにする。
	backgroundSprite_ = CreateColorSprite();
	fadeSprite_ = CreateColorSprite();
	slashSprite_ = CreateColorSprite();
	accentSprite_ = CreateColorSprite();
	menuPanelSprite_ = CreateColorSprite();
	startButtonSprite_ = CreateColorSprite();
	newGameButtonSprite_ = CreateColorSprite();
	shopButtonSprite_ = CreateColorSprite();
	confirmationBackdropSprite_ = CreateColorSprite();
	confirmationPanelSprite_ = CreateColorSprite();

	// 表示文字と基本色はここへ集約し、Draw2Dでは配置と選択状態だけを更新する。
	titleGlowTextObject_ = CreateTextObject(
		"百鬼夜行サバイバーズ", 64.0f, {0.86f, 0.08f, 0.025f, 0.0f}, kTitleFontName);
	titleTextObject_ = CreateTextObject(
		"百鬼夜行サバイバーズ", 58.0f, {1.0f, 0.94f, 0.79f, 0.0f}, kTitleFontName);
	startTextObject_ = CreateTextObject("GAME START", 30.0f, {1.0f, 1.0f, 1.0f, 1.0f});
	newGameTextObject_ = CreateTextObject("初めから", 30.0f, {0.82f, 0.87f, 0.94f, 1.0f});
	shopTextObject_ = CreateTextObject("SHOP", 30.0f, {0.82f, 0.87f, 0.94f, 1.0f});
	moneyTextObject_ = CreateTextObject("", 21.0f, {1.0f, 0.80f, 0.22f, 1.0f});
	instructionTextObject_ = CreateTextObject(
	    "W/S・上下キー・Pad上下: 選択    Enter・Pad A: 決定", 18.0f,
	    {0.68f, 0.75f, 0.86f, 1.0f});
	versionTextObject_ = CreateTextObject("PRESS ENTER / A BUTTON", 17.0f, {0.45f, 0.63f, 0.76f, 1.0f});
	confirmationTextObject_ = CreateTextObject("本当にゲームデータを初期化しますか？", 27.0f, {1.0f, 0.92f, 0.82f, 1.0f});
	confirmationInstructionTextObject_ = CreateTextObject(
	    "Enter・Pad A: 初期化する    Esc・Pad B: キャンセル", 18.0f, {0.76f, 0.84f, 0.94f, 1.0f});
}


/// <summary>
/// 毎フレームの状態更新を行います。
/// </summary>
void TitleScene::Update() {
	Input* input = Input::GetInstance();
	const float deltaTime = GameTime::GetDeltaTime();
	pulseTime_ += deltaTime;
	if (titlePhase_ != TitlePhase::Menu) {
		presentationTime_ += deltaTime;
	}
	UpdateTitlePhase();
	UpdateTitlePresentation();
	if (isResetConfirmationOpen_) {
		// 確認中は通常メニューの操作を無効にし、誤操作による別画面への遷移を防ぐ。
		if (input->TriggerKey(DIK_ESCAPE) || input->TriggerGamepadButton(XINPUT_GAMEPAD_B)) {
			isResetConfirmationOpen_ = false;
			return;
		}
		if (input->TriggerKey(DIK_RETURN) || input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) {
			sceneManager->ResetGameProgress();
			sceneManager->ChangeScene("PLAYER_SELECT");
		}
		return;
	}

	const bool isConfirmTriggered =
		input->TriggerKey(DIK_RETURN) || input->TriggerGamepadButton(XINPUT_GAMEPAD_A);
	if (titlePhase_ != TitlePhase::Menu) {
		if (isConfirmTriggered) {
			if (titlePhase_ == TitlePhase::WaitingForInput) {
				titlePhase_ = TitlePhase::Menu;
				selectedMenuIndex_ = 0;
			} else {
				// 導入中の決定入力は演出を最後まで飛ばし、次の入力でメニューを開けるようにする。
				presentationTime_ = kPromptStartSeconds;
				UpdateTitlePhase();
			}
		}
		return;
	}

	// W/S、十字キー、左スティックを同じ上下選択操作として扱う。
	if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP) || input->TriggerGamepadUp()) {
		selectedMenuIndex_ = (selectedMenuIndex_ + kMenuItemCount - 1) % kMenuItemCount;
	}
	if (input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN) || input->TriggerGamepadDown()) {
		selectedMenuIndex_ = (selectedMenuIndex_ + 1) % kMenuItemCount;
	}
	if (isConfirmTriggered) {
		// Enter / Pad A は通常の決定操作として、現在選択中の項目を実行する。
		if (selectedMenuIndex_ == 1) {
			// 「初めから」は即時消去せず、もう一度明示的な決定操作を求める。
			isResetConfirmationOpen_ = true;
		} else {
			sceneManager->ChangeScene(selectedMenuIndex_ == 0 ? "PLAYER_SELECT" : "SHOP");
		}
	}
}

void TitleScene::UpdateTitlePresentation() {
	const AtmosphereSettings& atmosphere = AtmosphereSystem::GetInstance()->GetSettings();
	const Vector3 sunDirection = NormalizeReturnVector(atmosphere.sunDirection);
	const Vector3 lightDirection = {-sunDirection.x, -sunDirection.y, -sunDirection.z};
	// 太陽が地平線へ近いほど、直接光を白から橙へ変化させます。
	const float sunsetAmount = std::clamp((0.25f - sunDirection.y) / 0.33f, 0.0f, 1.0f);
	const Vector4 daylightColor = {1.0f, 0.96f, 0.86f, 1.0f};
	const Vector4 sunsetColor = {1.0f, 0.36f, 0.12f, 1.0f};
	const Vector4 sunlightColor = {
	    daylightColor.x + (sunsetColor.x - daylightColor.x) * sunsetAmount,
	    daylightColor.y + (sunsetColor.y - daylightColor.y) * sunsetAmount,
	    daylightColor.z + (sunsetColor.z - daylightColor.z) * sunsetAmount,
	    1.0f};
	const float sunlightIntensity = 1.35f + (0.78f - 1.35f) * sunsetAmount;

	// 戦闘ロジックを持たない配置モデルだけを、名前に応じて静かに動かす。
	for (GameObject* object : GetMutableSceneObjects()) {
		if (!object) continue;
		const std::string& name = object->GetName();
		if (Object3dComponent* object3d = object->GetComponent<Object3dComponent>()) {
			// 太陽は無限遠の光源として扱い、Skyの太陽方向と全モデルの陰影を一致させます。
			object3d->SetDirectionalLight(sunlightColor, lightDirection, sunlightIntensity);
			// 旧TitleSunの局所光を廃止したため、初期値のPointLightも明示的に無効化します。
			object3d->SetPointLight({0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f, 1.0f);
		}
		if (name.starts_with("TitleMagatama")) {
			// 勾玉は個別の角度差を持つ円運動にし、4つが同じ場所へ重ならないようにする。
			const int index = static_cast<int>(name.back() - 'A');
			const float phase = static_cast<float>(index) * MathConstants::kPi * 0.5f;
			const float expansion = SmoothStep(0.75f, 2.70f, presentationTime_);
			const float slashBurst = 1.0f - SmoothStep(2.10f, 3.20f, presentationTime_);
			const float radiusX = 0.55f + expansion * (1.45f + slashBurst * 0.42f);
			const float radiusZ = 0.20f + expansion * (0.38f + slashBurst * 0.18f);
			const float angularSpeed = 0.70f + slashBurst * 2.6f;
			const float angle = phase + pulseTime_ * angularSpeed;
			EulerTransform& transform = object->GetTransform();
			transform.translate.x = std::cos(angle) * radiusX;
			transform.translate.y = 0.20f + std::sin(pulseTime_ * 1.7f + phase) * 0.16f;
			transform.translate.z = 0.40f + std::sin(angle) * radiusZ;
			// 軌道の接線方向へ向けると、回転していることが読み取りやすくなる。
			transform.rotate.y = -angle + MathConstants::kPi * 0.5f;

			// 勾玉を閾値以上の明るさへ寄せ、PostEffectのブルームで発光させる。
			static const Vector4 glowColors[] = {
				{0.62f, 0.94f, 1.0f, 1.0f},
				{1.0f, 0.52f, 0.72f, 1.0f},
				{0.62f, 1.0f, 0.68f, 1.0f},
				{1.0f, 0.78f, 0.28f, 1.0f}
			};
			const size_t glowColorIndex = static_cast<size_t>(index) % std::size(glowColors);
			if (Object3dComponent* object3d = object->GetComponent<Object3dComponent>()) {
				object3d->SetColor(glowColors[glowColorIndex]);
				// HDRの1.0を少し越える発光値を作り、太陽より弱いBloomだけを抽出させます。
				const Vector4& glowColor = glowColors[glowColorIndex];
				object3d->SetEmission({glowColor.x, glowColor.y, glowColor.z}, 0.55f);
			}

			// 軌跡はタイトルシーンで一度だけ追加し、以降は移動履歴だけを更新する。
			TrailRendererComponent* trail = object->GetComponent<TrailRendererComponent>();
			if (trail) {
				trail->SetWidth(0.12f);
				trail->SetLifeTime(titlePhase_ == TitlePhase::Menu ? 0.18f : 0.34f);
				trail->SetMinSegmentLength(0.035f);
				trail->SetHeadColor(glowColors[glowColorIndex]);
				trail->SetTailColor({glowColors[glowColorIndex].x * 0.35f, glowColors[glowColorIndex].y * 0.20f,
					glowColors[glowColorIndex].z, 0.0f});
			} else {
				trail = object->AddComponent<TrailRendererComponent>();
				trail->SetWidth(0.12f);
				trail->SetLifeTime(0.34f);
				trail->SetMinSegmentLength(0.035f);
				trail->SetHeadColor(glowColors[glowColorIndex]);
				trail->SetTailColor({glowColors[glowColorIndex].x * 0.35f, glowColors[glowColorIndex].y * 0.20f,
					glowColors[glowColorIndex].z, 0.0f});
			}
		} else if (name.starts_with("TitleEnemy")) {
			EulerTransform& transform = object->GetTransform();
			const float phase = static_cast<float>(name.back() % 4) * 0.8f;
			transform.translate.y = -1.15f + std::sin(pulseTime_ * 1.25f + phase) * 0.08f;
		}
	}
}

void TitleScene::DrawSkyBox() {
	if (atmosphereSky_) {
		atmosphereSky_->Draw();
	}
}

void TitleScene::UpdateTitlePhase() {
	if (titlePhase_ == TitlePhase::Menu) return;
	if (presentationTime_ < kSlashStartSeconds) {
		titlePhase_ = TitlePhase::FadeIn;
	} else if (presentationTime_ < 2.70f) {
		titlePhase_ = TitlePhase::Slash;
	} else if (presentationTime_ < kPromptStartSeconds) {
		titlePhase_ = TitlePhase::LogoReveal;
	} else {
		titlePhase_ = TitlePhase::WaitingForInput;
	}
}

void TitleScene::Draw2D() {
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!sceneManager || !dxCommon || !backgroundSprite_) return;
	const float width = static_cast<float>(dxCommon->GetRenderWidth());
	const float height = static_cast<float>(dxCommon->GetRenderHeight());
	const float logoAlpha = SmoothStep(kLogoStartSeconds, 3.15f, presentationTime_);
	const float fadeAlpha = 1.0f - SmoothStep(0.08f, 1.50f, presentationTime_);

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	// 紫の薄い色調を3Dへ重ね、導入時はさらに黒からフェードインする。
	DrawColorSprite(backgroundSprite_.get(), 0.0f, 0.0f, width, height, {0.018f, 0.025f, 0.070f, 0.27f});
	if (fadeAlpha > 0.0f) {
		DrawColorSprite(fadeSprite_.get(), 0.0f, 0.0f, width, height, {0.0f, 0.0f, 0.0f, fadeAlpha});
	}

	// 白い芯と橙色の外光を重ね、専用テクスチャなしで斬撃フラッシュを表現する。
	const float slashStrength =
		SmoothStep(kSlashStartSeconds, 1.88f, presentationTime_) *
		(1.0f - SmoothStep(2.18f, 2.70f, presentationTime_));
	if (slashStrength > 0.0f) {
		const float slashProgress = SmoothStep(kSlashStartSeconds, 2.35f, presentationTime_);
		const float slashLength = width * (0.30f + slashProgress * 0.48f);
		const float slashX = width * (0.38f + slashProgress * 0.22f);
		const float slashY = height * (0.49f - slashProgress * 0.08f);
		DrawRotatedColorSprite(slashSprite_.get(), slashX, slashY, slashLength, 34.0f, -0.48f,
			{1.0f, 0.18f, 0.02f, 0.22f * slashStrength});
		DrawRotatedColorSprite(slashSprite_.get(), slashX, slashY, slashLength, 13.0f, -0.48f,
			{1.0f, 0.58f, 0.12f, 0.72f * slashStrength});
		DrawRotatedColorSprite(slashSprite_.get(), slashX, slashY, slashLength, 3.0f, -0.48f,
			{1.0f, 0.96f, 0.82f, slashStrength});
	}

	if (logoAlpha > 0.0f) {
		const float glowPulse = 0.32f + 0.10f * std::sin(pulseTime_ * 2.3f);
		titleGlowTextObject_->GetTransform().translate = {width * 0.5f, height * 0.115f + 2.0f, 0.0f};
		titleGlowTextObject_->GetComponent<TextComponent>()->SetColor(
			{0.90f, 0.075f, 0.025f, logoAlpha * glowPulse});
		titleTextObject_->GetTransform().translate = {width * 0.5f, height * 0.115f, 0.0f};
		titleTextObject_->GetComponent<TextComponent>()->SetColor(
			{1.0f, 0.95f, 0.82f, logoAlpha});
		titleGlowTextObject_->Draw2D();
		titleTextObject_->Draw2D();
		DrawColorSprite(accentSprite_.get(), 0.0f, 0.0f, width, 3.0f,
			{0.52f, 0.035f, 0.025f, logoAlpha * 0.90f});
		DrawColorSprite(accentSprite_.get(), 0.0f, height - 3.0f, width, 3.0f,
			{0.92f, 0.32f, 0.055f, logoAlpha * 0.75f});
	}

	if (titlePhase_ == TitlePhase::WaitingForInput) {
		const float promptAlpha = 0.58f + 0.38f * std::sin(pulseTime_ * 2.7f);
		versionTextObject_->GetTransform().translate = {width * 0.5f, height - 58.0f, 0.0f};
		versionTextObject_->GetComponent<TextComponent>()->SetColor({0.92f, 0.87f, 0.76f, promptAlpha});
		versionTextObject_->Draw2D();
	}

	if (titlePhase_ != TitlePhase::Menu) return;

	// 実際の描画解像度を基準に、巫女を隠さない右側へメニューを配置する。
	const float panelWidth = (std::min)(380.0f, width - 80.0f);
	const float panelHeight = 180.0f;
	const float panelX = width - panelWidth - 44.0f;
	const float panelY = (std::min)(height * 0.44f, height - panelHeight - 118.0f);
	const float buttonX = panelX + 22.0f;
	const float buttonWidth = panelWidth - 44.0f;
	const float buttonHeight = 43.0f;
	const float buttonInterval = 50.0f;
	const float pulse = 0.78f + std::sin(pulseTime_ * 4.0f) * 0.12f;

	DrawColorSprite(menuPanelSprite_.get(), panelX, panelY, panelWidth, panelHeight, {0.012f, 0.020f, 0.055f, 0.70f});

	const Vector4 selectedColor = {0.42f, 0.045f, 0.025f, pulse * 0.88f};
	const Vector4 idleColor = {0.025f, 0.030f, 0.065f, 0.62f};
	// 選択項目だけ背景色を明るくして、現在のフォーカスを視覚化する。
	DrawColorSprite(startButtonSprite_.get(), buttonX, panelY + 16.0f, buttonWidth, buttonHeight,
	    selectedMenuIndex_ == 0 ? selectedColor : idleColor);
	DrawColorSprite(newGameButtonSprite_.get(), buttonX, panelY + 16.0f + buttonInterval, buttonWidth, buttonHeight,
	    selectedMenuIndex_ == 1 ? selectedColor : idleColor);
	DrawColorSprite(shopButtonSprite_.get(), buttonX, panelY + 16.0f + buttonInterval * 2.0f, buttonWidth, buttonHeight,
	    selectedMenuIndex_ == 2 ? selectedColor : idleColor);
	// 選択項目の左端へ朱色の細線を置き、塗りだけに頼らず現在位置を示す。
	DrawColorSprite(accentSprite_.get(), buttonX, panelY + 16.0f + buttonInterval * selectedMenuIndex_,
		4.0f, buttonHeight, {0.92f, 0.16f, 0.055f, 0.95f});

	const float menuCenterX = panelX + panelWidth * 0.5f;
	startTextObject_->GetTransform().translate = {menuCenterX, panelY + 37.0f, 0.0f};
	newGameTextObject_->GetTransform().translate = {menuCenterX, panelY + 37.0f + buttonInterval, 0.0f};
	shopTextObject_->GetTransform().translate = {menuCenterX, panelY + 37.0f + buttonInterval * 2.0f, 0.0f};
	// ボタン背景だけでなく文字色も変え、選択状態を読み取りやすくする。
	startTextObject_->GetComponent<TextComponent>()->SetColor(
	    selectedMenuIndex_ == 0 ? Vector4{1.0f, 0.91f, 0.72f, 1.0f} : Vector4{0.72f, 0.76f, 0.84f, 1.0f});
	newGameTextObject_->GetComponent<TextComponent>()->SetColor(
	    selectedMenuIndex_ == 1 ? Vector4{1.0f, 0.91f, 0.72f, 1.0f} : Vector4{0.72f, 0.76f, 0.84f, 1.0f});
	shopTextObject_->GetComponent<TextComponent>()->SetColor(
	    selectedMenuIndex_ == 2 ? Vector4{1.0f, 0.91f, 0.72f, 1.0f} : Vector4{0.72f, 0.76f, 0.84f, 1.0f});

	// ショップへ入る前に現在の共有所持金を確認できるよう、毎フレーム最新値を表示する。
	moneyTextObject_->GetComponent<TextComponent>()->SetText("所持金: " + std::to_string(sceneManager->GetMoney()) + " G");
	moneyTextObject_->GetTransform().translate = {menuCenterX, panelY + panelHeight + 28.0f, 0.0f};
	versionTextObject_->GetTransform().translate = {width * 0.5f, height - 74.0f, 0.0f};
	instructionTextObject_->GetTransform().translate = {width * 0.5f, height - 38.0f, 0.0f};

	startTextObject_->Draw2D();
	newGameTextObject_->Draw2D();
	shopTextObject_->Draw2D();
	moneyTextObject_->Draw2D();
	instructionTextObject_->Draw2D();

	if (isResetConfirmationOpen_) {
		// メニューを暗く覆った上に確認パネルを重ね、現在の入力対象を明確にする。
		const float confirmationWidth = (std::min)(560.0f, width - 48.0f);
		const float confirmationHeight = 160.0f;
		const float confirmationX = (width - confirmationWidth) * 0.5f;
		const float confirmationY = (height - confirmationHeight) * 0.5f;
		SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
		DrawColorSprite(confirmationBackdropSprite_.get(), 0.0f, 0.0f, width, height, {0.0f, 0.0f, 0.0f, 0.72f});
		DrawColorSprite(confirmationPanelSprite_.get(), confirmationX, confirmationY,
		    confirmationWidth, confirmationHeight, {0.018f, 0.026f, 0.065f, 0.98f});
		confirmationTextObject_->GetTransform().translate = {width * 0.5f, confirmationY + 56.0f, 0.0f};
		confirmationInstructionTextObject_->GetTransform().translate = {width * 0.5f, confirmationY + 112.0f, 0.0f};
		confirmationTextObject_->Draw2D();
		confirmationInstructionTextObject_->Draw2D();
	}
}

void TitleScene::Draw3D() {}

/// <summary>
/// 確保したリソースを解放し、終了処理を行います。
/// </summary>
void TitleScene::Finalize() {
	// シーンが生成したUIリソースをすべて解放してから基底クラスの終了処理を呼ぶ。
	if (atmosphereSky_) {
		atmosphereSky_->Finalize();
		atmosphereSky_.reset();
	}
	// タイトル専用の距離霞が次のシーンのObject3dへ残らないよう既定値へ戻します。
	AtmosphereSystem::GetInstance()->Reset();
	backgroundSprite_.reset();
	fadeSprite_.reset();
	slashSprite_.reset();
	accentSprite_.reset();
	menuPanelSprite_.reset();
	startButtonSprite_.reset();
	newGameButtonSprite_.reset();
	shopButtonSprite_.reset();
	confirmationBackdropSprite_.reset();
	confirmationPanelSprite_.reset();
	titleGlowTextObject_.reset();
	titleTextObject_.reset();
	startTextObject_.reset();
	newGameTextObject_.reset();
	shopTextObject_.reset();
	moneyTextObject_.reset();
	instructionTextObject_.reset();
	versionTextObject_.reset();
	confirmationTextObject_.reset();
	confirmationInstructionTextObject_.reset();
	BaseScene::Finalize();
}

void TitleScene::ImGuiUpdate() {}

