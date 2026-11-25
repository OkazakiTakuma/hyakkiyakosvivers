#include "base_scene/BaseScene.h"
#include "base_scene/runtime/GameplayHud.h"
#include "data/PlayerAttackLevel.h"
#include "helpers/BaseSceneCollisionHelpers.h"
#include "helpers/BaseSceneEditorGeometry.h"
#include "repositories/EnemyStatusRepository.h"
#include "repositories/ParticlePresetRepository.h"
#include "MathConstants.h"
#include "model/ModelManager.h"
#include "repositories/PlayerStatusRepository.h"
#include "SceneManager.h"
#include "StringUtility.h"
#include "particle/TrailRenderer.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <random>
#include <unordered_set>
#include <Xinput.h>
#ifdef USE_IMGUI
#include "../../../../../imgui/ImGuizmo.h"
#endif
#include "base_scene/details/BaseSceneRuntimeDetails.h"
#include <algorithm>

PlayerComponent* BaseScene::FindActivePlayer() const {
	for (const auto& object : sceneObjects_) {
		PlayerComponent* candidate = object->GetComponent<PlayerComponent>();
		if (candidate && candidate->IsEnabled()) {
			return candidate;
		}
	}
	return nullptr;
}

void BaseScene::UpdateGameplayHud() {
	gameplayHud_.Update(
		FindActivePlayer(),
		sceneName_ == "GAMEPLAY",
		gameplaySession_.GetChallengeMoney());
}

void GameplayHud::Reset() {
	playerHealthBarBackground_.reset();
	playerHealthBarFill_.reset();
	isPlayerHealthHudVisible_ = false;
	challengeMoneyTextObject_.reset();
	isChallengeMoneyHudVisible_ = false;
	playerExperienceBarBackground_.reset();
	playerExperienceBarFill_.reset();
	playerExperienceTextObject_.reset();
	playerExperienceRate_ = 0.0f;
	isPlayerExperienceHudVisible_ = false;
	for (auto& sprite : playerAttackSlotBackgroundSprites_) sprite.reset();
	for (auto& sprite : playerAttackSlotIconSprites_) sprite.reset();
	for (auto& sprite : playerStatusSlotBackgroundSprites_) sprite.reset();
	for (auto& sprite : playerStatusSlotIconSprites_) sprite.reset();
	for (auto& sprite : playerAttackSlotLevelBackgroundSprites_) sprite.reset();
	for (auto& sprite : playerStatusSlotLevelBackgroundSprites_) sprite.reset();
	for (auto& object : playerAttackSlotLevelTextObjects_) object.reset();
	for (auto& object : playerStatusSlotLevelTextObjects_) object.reset();
	playerAttackSlotIconVisible_.fill(false);
	playerStatusSlotIconVisible_.fill(false);
	playerAttackSlotLevelVisible_.fill(false);
	playerStatusSlotLevelVisible_.fill(false);
	playerAttackSlotTextureKeys_.fill({});
	playerAttackSlotTexturePaths_.fill({});
	playerStatusSlotTextureKeys_.fill({});
	playerStatusSlotTexturePaths_.fill({});
	playerAttackSlotLabelObject_.reset();
	playerStatusSlotLabelObject_.reset();
	isPlayerSlotHudVisible_ = false;
}

void GameplayHud::Update(PlayerComponent* player, bool showChallengeMoney, int challengeMoney) {
	UpdatePlayerHealth(player);
	UpdateChallengeMoney(showChallengeMoney, challengeMoney);
	UpdatePlayerExperience(player);
	UpdatePlayerSlots(player);
}

void GameplayHud::InvalidateAttackTextures() {
	playerAttackSlotTextureKeys_.fill({});
	playerAttackSlotTexturePaths_.fill({});
}

void GameplayHud::InvalidateStatusTextures() {
	playerStatusSlotTextureKeys_.fill({});
	playerStatusSlotTexturePaths_.fill({});
}

void GameplayHud::Draw() {
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	if (isPlayerHealthHudVisible_ && playerHealthBarBackground_ && playerHealthBarFill_) {
		playerHealthBarBackground_->Draw();
		playerHealthBarFill_->Draw();
	}
	DrawChallengeMoney();
	DrawPlayerExperience();
	DrawPlayerSlots();
}

void GameplayHud::UpdatePlayerHealth(PlayerComponent* player) {
	isPlayerHealthHudVisible_ = player != nullptr;
	if (!player) {
		return;
	}

	if (!playerHealthBarBackground_) {
		playerHealthBarBackground_ = std::make_unique<Sprite>();
		playerHealthBarBackground_->Initialize("Resources/human/white.png");
		playerHealthBarBackground_->SetColor({0.08f, 0.08f, 0.08f, 1.0f});
	}
	if (!playerHealthBarFill_) {
		playerHealthBarFill_ = std::make_unique<Sprite>();
		playerHealthBarFill_->Initialize("Resources/human/white.png");
	}

	const float screenWidth = static_cast<float>(Input::GetInstance()->GetClientWidth());
	constexpr float kRightMargin = 24.0f;
	constexpr float kTopMargin = 24.0f;
	constexpr float kBarWidth = 260.0f;
	constexpr float kBarHeight = 28.0f;
	constexpr float kBorderSize = 4.0f;
	const float left = (std::max)(kRightMargin, screenWidth - kRightMargin - kBarWidth);

	EulerTransform backgroundTransform = playerHealthBarBackground_->GetTransform();
	backgroundTransform.translate = {left, kTopMargin, 0.0f};
	playerHealthBarBackground_->SetTransform(backgroundTransform);
	playerHealthBarBackground_->SetSize({kBarWidth, kBarHeight});

	const float maxHealth = player->GetMaxHealth();
	float healthRate = maxHealth > 0.0f ? player->GetCurrentHealth() / maxHealth : 0.0f;
	healthRate = (std::max)(0.0f, (std::min)(1.0f, healthRate));
	EulerTransform fillTransform = playerHealthBarFill_->GetTransform();
	fillTransform.translate = {left + kBorderSize, kTopMargin + kBorderSize, 0.0f};
	playerHealthBarFill_->SetTransform(fillTransform);
	playerHealthBarFill_->SetSize({(kBarWidth - kBorderSize * 2.0f) * healthRate, kBarHeight - kBorderSize * 2.0f});
	playerHealthBarFill_->SetColor({1.0f - healthRate, healthRate, 0.08f, 1.0f});

	playerHealthBarBackground_->Update();
	playerHealthBarFill_->Update();
}

void GameplayHud::UpdateChallengeMoney(bool visible, int challengeMoney) {
	// GAMEPLAY以外のプレイヤー編集画面などには、ステージ戦績HUDを表示しない。
	isChallengeMoneyHudVisible_ = visible;
	if (!isChallengeMoneyHudVisible_) return;

	if (!challengeMoneyTextObject_) {
		// 初回表示時だけ文字オブジェクトを生成し、以降は表示文字列だけを差し替える。
		challengeMoneyTextObject_ = std::make_unique<Entity>();
		TextComponent* text = challengeMoneyTextObject_->AddComponent<TextComponent>();
		text->SetFontSize(26.0f);
		text->SetAnchor(TextComponent::Anchor::TopLeft);
		text->SetColor({1.0f, 0.82f, 0.18f, 1.0f});
	}

	// challengeMoneyEarned_は全体G強化適用後の値なので、リザルトと同じ累計を表示できる。
	// 左上基準で固定し、右上のHP・装備HUDや画面下部の経験値バーとの重なりを避ける。
	challengeMoneyTextObject_->GetTransform().translate = {24.0f, 24.0f, 0.0f};
	challengeMoneyTextObject_->GetComponent<TextComponent>()->SetText(
	    "G: " + std::to_string(challengeMoney));
}

void GameplayHud::DrawChallengeMoney() {
	// 更新側がGAMEPLAY以外で非表示にした場合は、保持中の古い文字オブジェクトを描画しない。
	if (isChallengeMoneyHudVisible_ && challengeMoneyTextObject_) {
		challengeMoneyTextObject_->Draw2D();
	}
}

void GameplayHud::UpdatePlayerExperience(PlayerComponent* player) {
	isPlayerExperienceHudVisible_ = player != nullptr;
	if (!player) {
		playerExperienceRate_ = 0.0f;
		return;
	}

	// HUDリソースは初回表示時だけ生成し、以降のフレームでは再利用する。
	if (!playerExperienceBarBackground_) {
		playerExperienceBarBackground_ = std::make_unique<Sprite>();
		playerExperienceBarBackground_->Initialize("Resources/human/white.png");
	}
	if (!playerExperienceBarFill_) {
		playerExperienceBarFill_ = std::make_unique<Sprite>();
		playerExperienceBarFill_->Initialize("Resources/human/white.png");
	}
	if (!playerExperienceTextObject_) {
		playerExperienceTextObject_ = std::make_unique<Entity>();
		TextComponent* text = playerExperienceTextObject_->AddComponent<TextComponent>();
		text->SetFontSize(17.0f);
		text->SetAnchor(TextComponent::Anchor::BottomCenter);
		text->SetColor({0.92f, 0.96f, 1.0f, 1.0f});
	}

	const PlayerStats& stats = player->GetBaseStats();
	const int level = (std::max)(1, stats.level);
	// experienceはゲーム全体の累積値なので、現在レベルの開始値を引いてレベル内の進捗へ変換する。
	const int previousLevelExperience = level > 1 ? PlayerComponent::GetRequiredExperienceForNextLevel(level - 1) : 0;
	const int nextLevelExperience = PlayerComponent::GetRequiredExperienceForNextLevel(level);
	const long long requiredExperience = (std::max)(1LL, static_cast<long long>(nextLevelExperience) - previousLevelExperience);
	const long long currentExperience = (std::max)(0LL, static_cast<long long>(stats.experience) - previousLevelExperience);
	playerExperienceRate_ = static_cast<float>(currentExperience) / static_cast<float>(requiredExperience);
	playerExperienceRate_ = (std::max)(0.0f, (std::min)(1.0f, playerExperienceRate_));

	const float screenWidth = static_cast<float>(Input::GetInstance()->GetClientWidth());
	const float screenHeight = static_cast<float>(Input::GetInstance()->GetClientHeight());
	constexpr float kHorizontalMargin = 32.0f;
	constexpr float kBottomMargin = 18.0f;
	constexpr float kBarHeight = 18.0f;
	constexpr float kBorderSize = 3.0f;
	// 左右に一定の余白を確保し、解像度が変わっても画面最下部の中央へ配置する。
	const float barWidth = (std::max)(160.0f, screenWidth - kHorizontalMargin * 2.0f);
	const float left = (screenWidth - barWidth) * 0.5f;
	const float top = (std::max)(0.0f, screenHeight - kBottomMargin - kBarHeight);

	EulerTransform backgroundTransform = playerExperienceBarBackground_->GetTransform();
	backgroundTransform.translate = {left, top, 0.0f};
	playerExperienceBarBackground_->SetTransform(backgroundTransform);
	playerExperienceBarBackground_->SetSize({barWidth, kBarHeight});
	playerExperienceBarBackground_->SetColor({0.025f, 0.04f, 0.07f, 0.94f});

	EulerTransform fillTransform = playerExperienceBarFill_->GetTransform();
	fillTransform.translate = {left + kBorderSize, top + kBorderSize, 0.0f};
	playerExperienceBarFill_->SetTransform(fillTransform);
	playerExperienceBarFill_->SetSize({(barWidth - kBorderSize * 2.0f) * playerExperienceRate_, kBarHeight - kBorderSize * 2.0f});
	playerExperienceBarFill_->SetColor({0.16f, 0.72f, 1.0f, 1.0f});

	playerExperienceBarBackground_->Update();
	playerExperienceBarFill_->Update();
	playerExperienceTextObject_->GetTransform().translate = {screenWidth * 0.5f, top - 3.0f, 0.0f};
	playerExperienceTextObject_->GetComponent<TextComponent>()->SetText(
	    "LV " + std::to_string(level) + "    EXP " + std::to_string(currentExperience) + " / " + std::to_string(requiredExperience));
}

void GameplayHud::DrawPlayerExperience() {
	if (!isPlayerExperienceHudVisible_ || !playerExperienceBarBackground_ || !playerExperienceBarFill_) return;
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	playerExperienceBarBackground_->Draw();
	// 幅0のスプライト描画を避けるため、経験値がある場合だけ進捗部分を描画する。
	if (playerExperienceRate_ > 0.0f) playerExperienceBarFill_->Draw();
	if (playerExperienceTextObject_) playerExperienceTextObject_->Draw2D();
}

void GameplayHud::UpdatePlayerSlots(PlayerComponent* player) {
	isPlayerSlotHudVisible_ = player != nullptr;
	if (!player) {
		playerAttackSlotIconVisible_.fill(false);
		playerStatusSlotIconVisible_.fill(false);
		playerAttackSlotLevelVisible_.fill(false);
		playerStatusSlotLevelVisible_.fill(false);
		return;
	}

	// 初回だけ白画像のSpriteを用意し、背景色または装備画像へ差し替えて再利用する。
	auto createSprite = []() {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize("Resources/human/white.png");
		return sprite;
	};
	auto createLabel = [](const std::string& label) {
		auto object = std::make_unique<Entity>();
		TextComponent* text = object->AddComponent<TextComponent>();
		text->SetText(label);
		text->SetFontSize(15.0f);
		text->SetAnchor(TextComponent::Anchor::CenterRight);
		text->SetColor({1.0f, 1.0f, 1.0f, 0.9f});
		return object;
	};
	auto createLevelText = []() {
		auto object = std::make_unique<Entity>();
		TextComponent* text = object->AddComponent<TextComponent>();
		text->SetFontSize(12.0f);
		text->SetAnchor(TextComponent::Anchor::Center);
		text->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
		return object;
	};
	for (std::size_t index = 0; index < kPlayerEquipmentSlotCount; ++index) {
		if (!playerAttackSlotBackgroundSprites_[index]) playerAttackSlotBackgroundSprites_[index] = createSprite();
		if (!playerAttackSlotIconSprites_[index]) playerAttackSlotIconSprites_[index] = createSprite();
		if (!playerStatusSlotBackgroundSprites_[index]) playerStatusSlotBackgroundSprites_[index] = createSprite();
		if (!playerStatusSlotIconSprites_[index]) playerStatusSlotIconSprites_[index] = createSprite();
		if (!playerAttackSlotLevelBackgroundSprites_[index]) playerAttackSlotLevelBackgroundSprites_[index] = createSprite();
		if (!playerStatusSlotLevelBackgroundSprites_[index]) playerStatusSlotLevelBackgroundSprites_[index] = createSprite();
		if (!playerAttackSlotLevelTextObjects_[index]) playerAttackSlotLevelTextObjects_[index] = createLevelText();
		if (!playerStatusSlotLevelTextObjects_[index]) playerStatusSlotLevelTextObjects_[index] = createLevelText();
	}
	if (!playerAttackSlotLabelObject_) playerAttackSlotLabelObject_ = createLabel("武器");
	if (!playerStatusSlotLabelObject_) playerStatusSlotLabelObject_ = createLabel("加護");

	// HPバー直下を起点に、3枠全体が画面右端へ揃うよう左端座標を逆算する。
	const float screenWidth = static_cast<float>(Input::GetInstance()->GetClientWidth());
	constexpr float kRightMargin = 24.0f;
	constexpr float kTop = 68.0f;
	constexpr float kLabelWidth = 64.0f;
	constexpr float kSlotSize = 46.0f;
	constexpr float kSlotGap = 5.0f;
	constexpr float kRowGap = 7.0f;
	constexpr float kInnerMargin = 3.0f;
	constexpr float kLevelHeight = 16.0f;
	const float slotCount = static_cast<float>(kPlayerEquipmentSlotCount);
	const float slotsWidth = kSlotSize * slotCount + kSlotGap * (slotCount - 1.0f);
	const float slotsLeft = (std::max)(kRightMargin + kLabelWidth, screenWidth - kRightMargin - slotsWidth);
	const float labelRight = slotsLeft - 8.0f;
	const float attackY = kTop;
	const float statusY = attackY + kSlotSize + kRowGap;
	playerAttackSlotLabelObject_->GetTransform().translate = {labelRight, attackY + kSlotSize * 0.5f, 0.0f};
	playerStatusSlotLabelObject_->GetTransform().translate = {labelRight, statusY + kSlotSize * 0.5f, 0.0f};

	const PlayerStats& stats = player->GetBaseStats();
	for (std::size_t index = 0; index < kPlayerEquipmentSlotCount; ++index) {
		const float slotX = slotsLeft + static_cast<float>(index) * (kSlotSize + kSlotGap);
		const PlayerAttackSlot& attackSlot = stats.attackSlots[index];
		const PlayerStatusSlot& statusSlot = stats.statusSlots[index];
		const bool hasAttack = !attackSlot.attackName.empty();
		const bool hasStatus = !statusSlot.statusName.empty();

		auto updateBackground = [slotX, kSlotSize](Sprite* sprite, float y, const Vector4& color) {
			EulerTransform transform = sprite->GetTransform();
			transform.translate = {slotX, y, 0.0f};
			sprite->SetTransform(transform);
			sprite->SetSize({kSlotSize, kSlotSize});
			sprite->SetColor(color);
			sprite->Update();
		};
		// 装備中は種類別の色、無効化中は灰色、未装備は暗い空枠として表示する。
		updateBackground(playerAttackSlotBackgroundSprites_[index].get(), attackY,
		    hasAttack ? (attackSlot.enabled ? Vector4{0.12f, 0.28f, 0.52f, 0.95f} : Vector4{0.18f, 0.20f, 0.24f, 0.75f})
		              : Vector4{0.05f, 0.06f, 0.08f, 0.82f});
		updateBackground(playerStatusSlotBackgroundSprites_[index].get(), statusY,
		    hasStatus ? (statusSlot.enabled ? Vector4{0.10f, 0.42f, 0.25f, 0.95f} : Vector4{0.18f, 0.20f, 0.24f, 0.75f})
		              : Vector4{0.05f, 0.06f, 0.08f, 0.82f});

		auto updateLevel = [slotX, kSlotSize, kLevelHeight](
		    Sprite* background, Entity* textObject, float y, const std::string& levelText, bool enabled) {
			EulerTransform transform = background->GetTransform();
			transform.translate = {slotX, y + kSlotSize - kLevelHeight, 0.0f};
			background->SetTransform(transform);
			background->SetSize({kSlotSize, kLevelHeight});
			background->SetColor({0.0f, 0.0f, 0.0f, enabled ? 0.72f : 0.58f});
			background->Update();

			textObject->GetTransform().translate = {
				slotX + kSlotSize * 0.5f,
				y + kSlotSize - kLevelHeight * 0.5f,
				0.0f};
			TextComponent* text = textObject->GetComponent<TextComponent>();
			text->SetText(levelText);
			text->SetColor(enabled ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.65f, 0.65f, 0.65f, 0.9f});
		};
		playerAttackSlotLevelVisible_[index] = hasAttack;
		playerStatusSlotLevelVisible_[index] = hasStatus;
		if (hasAttack) {
			const std::string levelText = PlayerAttackLevel::IsSuper(attackSlot.attackLevel)
			    ? "SUPER"
			    : "Lv." + attackSlot.attackLevel;
			updateLevel(playerAttackSlotLevelBackgroundSprites_[index].get(),
			    playerAttackSlotLevelTextObjects_[index].get(), attackY, levelText, attackSlot.enabled);
		}
		if (hasStatus) {
			updateLevel(playerStatusSlotLevelBackgroundSprites_[index].get(),
			    playerStatusSlotLevelTextObjects_[index].get(), statusY, "Lv." + statusSlot.level, statusSlot.enabled);
		}

		// 名前とレベルの組み合わせをキーにし、毎フレームJSONを読み直さない。
		const std::string attackTextureKey = hasAttack ? attackSlot.attackName + "#" + attackSlot.attackLevel : std::string{};
		const bool attackTextureChanged = playerAttackSlotTextureKeys_[index] != attackTextureKey;
		if (attackTextureChanged) {
			playerAttackSlotTextureKeys_[index] = attackTextureKey;
			playerAttackSlotTexturePaths_[index].clear();
			if (hasAttack) {
				const PlayerAttackStats attackStats = LoadPlayerAttackStats(attackSlot.attackName);
				playerAttackSlotTexturePaths_[index] = attackStats.choiceTextureFilePath;
				// 通常Lv1～5は攻撃共通画像、Superだけはレベル専用画像を使用する。
				if (PlayerAttackLevel::IsSuper(attackSlot.attackLevel)) {
					for (const PlayerAttackLevelStats& levelStats : attackStats.levels) {
						if (PlayerAttackLevel::IsSuper(levelStats.level)) {
							playerAttackSlotTexturePaths_[index] = levelStats.choiceTextureFilePath;
							break;
						}
					}
				}
			}
		}
		// ステータス画像は現在レベルごとに異なるため、レベルもキャッシュキーへ含める。
		const std::string statusTextureKey = hasStatus ? statusSlot.statusName + "#" + statusSlot.level : std::string{};
		const bool statusTextureChanged = playerStatusSlotTextureKeys_[index] != statusTextureKey;
		if (statusTextureChanged) {
			playerStatusSlotTextureKeys_[index] = statusTextureKey;
			playerStatusSlotTexturePaths_[index].clear();
			if (hasStatus) {
				const PlayerStatusItemStats statusStats = LoadPlayerStatusItemStats(statusSlot.statusName);
				playerStatusSlotTexturePaths_[index] = statusStats.levelTextureFilePaths[PlayerStatusSlotLevelToIndex(statusSlot.level)];
			}
		}
		const std::string& attackTexture = playerAttackSlotTexturePaths_[index];
		const std::string& statusTexture = playerStatusSlotTexturePaths_[index];

		auto updateIcon = [slotX, kSlotSize, kInnerMargin](Sprite* sprite, float y, const std::string& texture, bool enabled) {
			if (sprite->GetTextureFilePath() != texture) sprite->SetTexture(texture);
			EulerTransform transform = sprite->GetTransform();
			transform.translate = {slotX + kInnerMargin, y + kInnerMargin, 0.0f};
			sprite->SetTransform(transform);
			sprite->SetSize({kSlotSize - kInnerMargin * 2.0f, kSlotSize - kInnerMargin * 2.0f});
			sprite->SetColor(enabled ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.55f, 0.55f, 0.55f, 0.55f});
			sprite->Update();
		};
		// 未設定または存在しない画像は描画せず、背景だけでスロット状態を示す。
		playerAttackSlotIconVisible_[index] = hasAttack && !attackTexture.empty() && std::filesystem::exists(StringUtility::Utf8ToPath(attackTexture));
		playerStatusSlotIconVisible_[index] = hasStatus && !statusTexture.empty() && std::filesystem::exists(StringUtility::Utf8ToPath(statusTexture));
		if (playerAttackSlotIconVisible_[index]) updateIcon(playerAttackSlotIconSprites_[index].get(), attackY, attackTexture, attackSlot.enabled);
		if (playerStatusSlotIconVisible_[index]) updateIcon(playerStatusSlotIconSprites_[index].get(), statusY, statusTexture, statusSlot.enabled);
	}
}

void GameplayHud::DrawPlayerSlots() {
	if (!isPlayerSlotHudVisible_) return;
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	// 背景、アイコン、レベル帯、レベル文字の順に重ね、最後に行ラベルを描画する。
	for (std::size_t index = 0; index < kPlayerEquipmentSlotCount; ++index) {
		playerAttackSlotBackgroundSprites_[index]->Draw();
		if (playerAttackSlotIconVisible_[index]) playerAttackSlotIconSprites_[index]->Draw();
		if (playerAttackSlotLevelVisible_[index]) {
			playerAttackSlotLevelBackgroundSprites_[index]->Draw();
			playerAttackSlotLevelTextObjects_[index]->Draw2D();
		}
		playerStatusSlotBackgroundSprites_[index]->Draw();
		if (playerStatusSlotIconVisible_[index]) playerStatusSlotIconSprites_[index]->Draw();
		if (playerStatusSlotLevelVisible_[index]) {
			playerStatusSlotLevelBackgroundSprites_[index]->Draw();
			playerStatusSlotLevelTextObjects_[index]->Draw2D();
		}
	}
	if (playerAttackSlotLabelObject_) playerAttackSlotLabelObject_->Draw2D();
	if (playerStatusSlotLabelObject_) playerStatusSlotLabelObject_->Draw2D();
}

/// <summary>
/// 有効なコライダー同士の当たり判定と押し戻しを行います。
/// </summary>

void BaseScene::UpdateLevelUpSelection() {
	Input* input = Input::GetInstance();
	if (gameplaySession_.GetMode() == GameplayMode::BossRewardPresentation) return;
	if (IsLevelUpSelectionActive()) {
		if (levelUpChoices_.empty()) {
			SetGameplayMode(GameplayMode::Playing);
			levelUpPlayer_ = nullptr;
			return;
		}
		if (input->TriggerKey(DIK_A) || input->TriggerGamepadLeft()) {
			selectedLevelUpChoiceIndex_ = (selectedLevelUpChoiceIndex_ + static_cast<int>(levelUpChoices_.size()) - 1) % static_cast<int>(levelUpChoices_.size());
		}
		if (input->TriggerKey(DIK_D) || input->TriggerGamepadRight()) {
			selectedLevelUpChoiceIndex_ = (selectedLevelUpChoiceIndex_ + 1) % static_cast<int>(levelUpChoices_.size());
		}
		if (gameplaySession_.GetMode() != GameplayMode::BossReward &&
		    (input->TriggerKey(DIK_R) || input->TriggerGamepadButton(XINPUT_GAMEPAD_Y))) {
			RerollLevelUpChoices();
		}
		if (input->TriggerKey(DIK_SPACE) || input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) {
			ApplyLevelUpChoice(selectedLevelUpChoiceIndex_);
		}
		return;
	}

	if (ShowNextBossAcquisitionOffer()) {
		return;
	}

	for (const auto& object : sceneObjects_) {
		PlayerComponent* player = object->GetComponent<PlayerComponent>();
		if (!player || !player->ConsumePendingLevelUp()) {
			continue;
		}
		levelUpPlayer_ = player;
		levelUpRerollUsed_ = false;
		if (BuildLevelUpChoices(player)) {
			SetGameplayMode(GameplayMode::LevelUpSelection);
		} else {
			levelUpPlayer_ = nullptr;
		}
		break;
	}
}

void BaseScene::UpdateBossRewardPresentation() {
	if (gameplaySession_.GetMode() != GameplayMode::BossRewardPresentation) return;
	// GameTimeは演出中停止しているため、UI専用の固定刻みで進行させる。
	bossRewardPresentationTimer_ += 1.0f / 60.0f;
	const float revealEnd = 1.35f + 0.20f * static_cast<float>(bossRewardResults_.size());
	if (bossRewardPresentationTimer_ >= revealEnd) bossRewardPresentationReady_ = true;
	if (!bossRewardPresentationReady_) return;
	Input* input = Input::GetInstance();
	if (!input->TriggerKey(DIK_SPACE) && !input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) return;

	bossRewardResults_.clear();
	SetGameplayMode(GameplayMode::Playing);
	if (!ShowNextBossAcquisitionOffer()) bossAcquisitionPlayer_ = nullptr;
}

bool BaseScene::BuildLevelUpChoices(PlayerComponent* player) {
	levelUpChoices_.clear();
	selectedLevelUpChoiceIndex_ = 0;
	if (!player) {
		return false;
	}
	const PlayerStats& stats = player->GetBaseStats();
	std::vector<LevelUpChoice> candidates;
	std::vector<std::string> equippedAttacks;
	std::vector<std::string> equippedStatuses;
	const auto otherPlayerInitialAttacks = GetOtherPlayerInitialAttacks(*player);
	int emptyAttackSlot = -1;
	int emptyStatusSlot = -1;
	// 選択後のレベルに設定された文章を優先し、空欄なら従来の自動説明へ戻す。
	auto getAttackDescription = [](const std::string& attackName, const std::string& targetLevel, const std::string& fallback) {
		const PlayerAttackStats attackStats = LoadPlayerAttackStats(attackName);
		for (const PlayerAttackLevelStats& levelStats : attackStats.levels) {
			if (levelStats.level == targetLevel && !levelStats.choiceDescription.empty()) {
				return levelStats.choiceDescription;
			}
		}
		return fallback;
	};
	auto getStatusDescription = [](const std::string& statusName, int targetLevelIndex, const std::string& fallback) {
		const PlayerStatusItemStats statusStats = LoadPlayerStatusItemStats(statusName);
		if (targetLevelIndex >= 0 && targetLevelIndex < static_cast<int>(statusStats.levelDescriptions.size()) &&
		    !statusStats.levelDescriptions[targetLevelIndex].empty()) {
			return statusStats.levelDescriptions[targetLevelIndex];
		}
		return fallback;
	};
	// 攻撃画像は通常Lv1～5で共通、Superだけレベル設定内の専用画像を参照する。
	auto getAttackTexture = [](const std::string& attackName, const std::string& targetLevel) {
		const PlayerAttackStats attackStats = LoadPlayerAttackStats(attackName);
		if (!PlayerAttackLevel::IsSuper(targetLevel)) {
			return attackStats.choiceTextureFilePath;
		}
		for (const PlayerAttackLevelStats& levelStats : attackStats.levels) {
			if (levelStats.level == targetLevel) {
				return levelStats.choiceTextureFilePath;
			}
		}
		return std::string{};
	};
	// ステータスは取得・強化後のレベル番号に対応する画像を参照する。
	auto getStatusTexture = [](const std::string& statusName, int targetLevelIndex) {
		const PlayerStatusItemStats statusStats = LoadPlayerStatusItemStats(statusName);
		if (targetLevelIndex >= 0 && targetLevelIndex < static_cast<int>(statusStats.levelTextureFilePaths.size())) {
			return statusStats.levelTextureFilePaths[targetLevelIndex];
		}
		return std::string{};
	};

	for (int index = 0; index < static_cast<int>(stats.attackSlots.size()); ++index) {
		const PlayerAttackSlot& slot = stats.attackSlots[index];
		if (slot.attackName.empty()) {
			if (emptyAttackSlot < 0) emptyAttackSlot = index;
			continue;
		}
		equippedAttacks.push_back(slot.attackName);
		const int level = std::atoi(slot.attackLevel.c_str());
		if (!PlayerAttackLevel::IsSuper(slot.attackLevel) && level >= 1 && level < 5) {
			const std::string targetLevel = std::to_string(level + 1);
			const std::string fallback = "Attack level " + slot.attackLevel + " -> " + targetLevel;
			const PlayerAttackStats attackStats = LoadPlayerAttackStats(slot.attackName);
			candidates.push_back({LevelUpChoiceType::AttackLevelUp, slot.attackName, attackStats.GetDisplayName(), getAttackDescription(slot.attackName, targetLevel, fallback), index, getAttackTexture(slot.attackName, targetLevel)});
		}
		if (slot.attackLevel == "5") {
			const PlayerAttackStats attackStats = LoadPlayerAttackStats(slot.attackName);
			bool conditionMet = !attackStats.superConditionStatusName.empty();
			const int requiredLevel = (std::max)(1, std::atoi(attackStats.superConditionStatusLevel.c_str()));
			if (conditionMet) {
				conditionMet = false;
				for (const PlayerStatusSlot& statusSlot : stats.statusSlots) {
					if (statusSlot.enabled && statusSlot.statusName == attackStats.superConditionStatusName && std::atoi(statusSlot.level.c_str()) >= requiredLevel) {
						conditionMet = true;
						break;
					}
				}
			}
			if (conditionMet) {
				candidates.push_back({LevelUpChoiceType::AttackSuper, slot.attackName, attackStats.GetDisplayName(true), getAttackDescription(slot.attackName, "super", "Promote attack level 5 -> super"), index, getAttackTexture(slot.attackName, "super")});
			}
		}
	}

	for (int index = 0; index < static_cast<int>(stats.statusSlots.size()); ++index) {
		const PlayerStatusSlot& slot = stats.statusSlots[index];
		if (slot.statusName.empty()) {
			if (emptyStatusSlot < 0) emptyStatusSlot = index;
			continue;
		}
		equippedStatuses.push_back(slot.statusName);
		const int level = std::atoi(slot.level.c_str());
		if (level >= 1 && level < 5) {
			const std::string targetLevel = std::to_string(level + 1);
			const std::string fallback = "Status level " + slot.level + " -> " + targetLevel;
			const PlayerStatusItemStats statusStats = LoadPlayerStatusItemStats(slot.statusName);
			candidates.push_back({LevelUpChoiceType::StatusLevelUp, slot.statusName, statusStats.GetDisplayName(), getStatusDescription(slot.statusName, level, fallback), index, getStatusTexture(slot.statusName, level)});
		}
	}

	if (emptyAttackSlot >= 0) {
		for (const std::string& attackName : LoadPlayerAttackNames()) {
			// 装備済み武器と他タイプ専用の初期武器を除き、空きスロットへ追加可能な武器だけを残す。
			if (std::find(equippedAttacks.begin(), equippedAttacks.end(), attackName) == equippedAttacks.end() &&
			    !otherPlayerInitialAttacks.contains(attackName)) {
				const PlayerAttackStats attackStats = LoadPlayerAttackStats(attackName);
				candidates.push_back({LevelUpChoiceType::NewAttack, attackName, attackStats.GetDisplayName(), getAttackDescription(attackName, "1", "Add a new attack at level 1"), emptyAttackSlot, getAttackTexture(attackName, "1")});
			}
		}
	}
	if (emptyStatusSlot >= 0) {
		for (const std::string& statusName : LoadPlayerStatusItemNames()) {
			if (std::find(equippedStatuses.begin(), equippedStatuses.end(), statusName) == equippedStatuses.end()) {
				const PlayerStatusItemStats statusStats = LoadPlayerStatusItemStats(statusName);
				candidates.push_back({LevelUpChoiceType::NewStatus, statusName, statusStats.GetDisplayName(), getStatusDescription(statusName, 0, "Add a new status at level 1"), emptyStatusSlot, getStatusTexture(statusName, 0)});
			}
		}
	}

	if (candidates.empty()) {
		return false;
	}
	static std::mt19937 randomEngine(std::random_device{}());
	std::shuffle(candidates.begin(), candidates.end(), randomEngine);
	auto isEquippedUpgrade = [](const LevelUpChoice& choice) {
		return choice.type == LevelUpChoiceType::AttackLevelUp ||
		       choice.type == LevelUpChoiceType::AttackSuper ||
		       choice.type == LevelUpChoiceType::StatusLevelUp;
	};
	const auto equippedUpgrade = std::find_if(candidates.begin(), candidates.end(), isEquippedUpgrade);
	if (candidates.size() > 3 && equippedUpgrade >= candidates.begin() + 3 && equippedUpgrade != candidates.end()) {
		std::uniform_int_distribution<int> selectionSlotDistribution(0, 2);
		std::iter_swap(candidates.begin() + selectionSlotDistribution(randomEngine), equippedUpgrade);
	}
	for (int index = 0; index < 3; ++index) {
		levelUpChoices_.push_back(candidates[index % candidates.size()]);
		PopulateCorrespondingOwnedIcons(levelUpChoices_.back(), stats);
	}
	return true;
}

void BaseScene::PopulateCorrespondingOwnedIcons(LevelUpChoice& choice, const PlayerStats& stats) const {
	choice.correspondingOwnedTextureFilePaths.clear();
	if (choice.type == LevelUpChoiceType::Decline) return;

	auto addTextureIfAvailable = [&choice](const std::string& textureFilePath) {
		if (textureFilePath.empty() ||
		    !std::filesystem::exists(StringUtility::Utf8ToPath(textureFilePath)) ||
		    std::find(choice.correspondingOwnedTextureFilePaths.begin(),
		              choice.correspondingOwnedTextureFilePaths.end(), textureFilePath) !=
		        choice.correspondingOwnedTextureFilePaths.end()) {
			return;
		}
		choice.correspondingOwnedTextureFilePaths.push_back(textureFilePath);
	};

	const bool attackChoice = choice.type == LevelUpChoiceType::AttackLevelUp ||
	                          choice.type == LevelUpChoiceType::AttackSuper ||
	                          choice.type == LevelUpChoiceType::NewAttack;
	if (attackChoice) {
		const PlayerAttackStats attackStats = LoadPlayerAttackStats(choice.name);
		if (attackStats.superConditionStatusName.empty()) return;
		for (const PlayerStatusSlot& slot : stats.statusSlots) {
			if (!slot.enabled || slot.statusName != attackStats.superConditionStatusName) continue;
			const PlayerStatusItemStats statusStats = LoadPlayerStatusItemStats(slot.statusName);
			const int levelIndex = std::clamp(std::atoi(slot.level.c_str()) - 1, 0,
			    static_cast<int>(statusStats.levelTextureFilePaths.size()) - 1);
			addTextureIfAvailable(statusStats.levelTextureFilePaths[levelIndex]);
			return;
		}
		return;
	}

	// アイテム候補では、そのアイテムをSuper条件にしている所持武器をすべて表示する。
	for (const PlayerAttackSlot& slot : stats.attackSlots) {
		if (!slot.enabled || slot.attackName.empty()) continue;
		const PlayerAttackStats attackStats = LoadPlayerAttackStats(slot.attackName);
		if (attackStats.superConditionStatusName != choice.name) continue;
		std::string textureFilePath = attackStats.choiceTextureFilePath;
		if (PlayerAttackLevel::IsSuper(slot.attackLevel)) {
			for (const PlayerAttackLevelStats& levelStats : attackStats.levels) {
				if (PlayerAttackLevel::IsSuper(levelStats.level)) {
					textureFilePath = levelStats.choiceTextureFilePath;
					break;
				}
			}
		}
		addTextureIfAvailable(textureFilePath);
	}
}

void BaseScene::RerollLevelUpChoices() {
	if (!sceneManager_ || !levelUpPlayer_ || gameplaySession_.GetMode() == GameplayMode::BossReward || levelUpRerollUsed_ ||
	    sceneManager_->GetGlobalShopUpgradeLevel("level_up_reroll") <= 0 || levelUpChoices_.empty()) {
		return;
	}

	const std::vector<LevelUpChoice> previousChoices = levelUpChoices_;
	auto makeSortedKeys = [](const std::vector<LevelUpChoice>& choices) {
		std::vector<std::string> keys;
		keys.reserve(choices.size());
		for (const LevelUpChoice& choice : choices) {
			keys.push_back(std::to_string(static_cast<int>(choice.type)) + ":" + choice.name + ":" +
			               std::to_string(choice.slotIndex));
		}
		std::sort(keys.begin(), keys.end());
		return keys;
	};
	const std::vector<std::string> previousKeys = makeSortedKeys(previousChoices);

	// 候補に余裕がある場合は、単なる並び替えではなく内容が変わるまで再抽選する。
	for (int attempt = 0; attempt < 12; ++attempt) {
		if (!BuildLevelUpChoices(levelUpPlayer_)) {
			levelUpChoices_ = previousChoices;
			return;
		}
		if (makeSortedKeys(levelUpChoices_) != previousKeys) {
			levelUpRerollUsed_ = true;
			selectedLevelUpChoiceIndex_ = 0;
			return;
		}
	}

	// 実質的に別候補が存在しない場合は、選択肢とリロール権をそのまま残す。
	levelUpChoices_ = previousChoices;
	selectedLevelUpChoiceIndex_ = 0;
}

void BaseScene::ApplyLevelUpChoice(int choiceIndex) {
	if (!levelUpPlayer_ || choiceIndex < 0 || choiceIndex >= static_cast<int>(levelUpChoices_.size())) return;
	const bool wasBossAcquisitionOffer = gameplaySession_.GetMode() == GameplayMode::BossReward;
	const LevelUpChoice choice = levelUpChoices_[choiceIndex];
	PlayerStats stats = levelUpPlayer_->GetBaseStats();
	switch (choice.type) {
	case LevelUpChoiceType::AttackLevelUp:
		stats.attackSlots[choice.slotIndex].enabled = true;
		stats.attackSlots[choice.slotIndex].attackLevel = std::to_string((std::min)(5, std::atoi(stats.attackSlots[choice.slotIndex].attackLevel.c_str()) + 1));
		break;
	case LevelUpChoiceType::AttackSuper:
		stats.attackSlots[choice.slotIndex].enabled = true;
		stats.attackSlots[choice.slotIndex].attackLevel = "super";
		break;
	case LevelUpChoiceType::NewAttack:
		stats.attackSlots[choice.slotIndex] = {true, choice.name, "1"};
		break;
	case LevelUpChoiceType::StatusLevelUp:
		stats.statusSlots[choice.slotIndex].enabled = true;
		stats.statusSlots[choice.slotIndex].level = std::to_string((std::min)(5, std::atoi(stats.statusSlots[choice.slotIndex].level.c_str()) + 1));
		break;
	case LevelUpChoiceType::NewStatus:
		stats.statusSlots[choice.slotIndex] = {true, choice.name, "1"};
		break;
	case LevelUpChoiceType::Decline:
		break;
	}
	if (choice.type != LevelUpChoiceType::Decline) {
		levelUpPlayer_->ApplyStats(
			stats,
			ApplyPlayerStatusItems(stats, LoadPlayerStatusItemDefinitions()));
		if (Entity* owner = levelUpPlayer_->GetOwner()) {
			ApplyPlayerAttackSlots(
				owner->GetComponent<PlayerAttackComponent>(),
				stats,
				LoadPlayerAttackDefinitions());
		}
	}
	levelUpChoices_.clear();
	if (wasBossAcquisitionOffer) {
		SetGameplayMode(GameplayMode::Playing);
		if (ShowNextBossAcquisitionOffer()) {
			return;
		}
	}
	if (levelUpPlayer_->ConsumePendingLevelUp()) {
		levelUpRerollUsed_ = false;
		if (BuildLevelUpChoices(levelUpPlayer_)) return;
	}
	SetGameplayMode(GameplayMode::Playing);
	levelUpPlayer_ = nullptr;
}

void BaseScene::EnsureLevelUpSelectionSprites() {
	if (levelUpOverlaySprite_) return;
	const std::string whiteTexture = "Resources/human/white.png";
	auto createSprite = [&whiteTexture]() {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(whiteTexture);
		return sprite;
	};
	levelUpOverlaySprite_ = createSprite();
	levelUpPanelSprite_ = createSprite();
	for (auto& sprite : levelUpPanelAccentSprites_) sprite = createSprite();
	for (int index = 0; index < 3; ++index) {
		levelUpChoiceBorderSprites_[index] = createSprite();
		levelUpChoiceSprites_[index] = std::make_unique<Sprite>();
		levelUpChoiceSprites_[index]->Initialize("Resources/UI/World/level_up_ofuda_card.png");
		levelUpChoiceAccentSprites_[index] = createSprite();
		levelUpChoiceIconSprites_[index] = createSprite();
		for (auto& sprite : levelUpChoiceCorrespondingIconSprites_[index]) {
			sprite = createSprite();
		}
	}

	auto createTextObject = [](const std::string& text, float fontSize) {
		auto object = std::make_unique<Entity>();
		TextComponent* textComponent = object->AddComponent<TextComponent>();
		textComponent->SetText(text);
		textComponent->SetFontSize(fontSize);
		textComponent->SetAnchor(TextComponent::Anchor::Center);
		return object;
	};
	levelUpTitleTextObject_ = createTextObject("神授の選択", 42.0f);
	levelUpInstructionTextObject_ = createTextObject("A / D・左右キー: 選択    Space・Pad A: 授かる", 19.0f);
	for (auto& object : levelUpChoiceTextObjects_) {
		object = createTextObject("", 23.0f);
	}
}

void BaseScene::DrawLevelUpSelection2D() {
	if (!IsLevelUpSelectionActive() || levelUpChoices_.empty()) return;
	EnsureLevelUpSelectionSprites();
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon) return;
	const float screenWidth = static_cast<float>(dxCommon->GetRenderWidth());
	const float screenHeight = static_cast<float>(dxCommon->GetRenderHeight());
	const float panelWidth = (std::min)(screenWidth - 48.0f, 1120.0f);
	// 御札テクスチャの縦横比を保ちやすい高さを確保する。
	const float panelHeight = (std::min)(screenHeight - 48.0f, 520.0f);
	const float panelX = (screenWidth - panelWidth) * 0.5f;
	const float panelY = (screenHeight - panelHeight) * 0.5f;
	const float gap = 24.0f;
	const float cardWidth = (panelWidth - 64.0f - gap * 2.0f) / 3.0f;
	const float cardHeight = panelHeight - 158.0f;
	const float cardY = panelY + 102.0f;

	auto drawSprite = [](Sprite* sprite, float x, float y, float width, float height, const Vector4& color) {
		EulerTransform transform = sprite->GetTransform();
		transform.translate = {x, y, 0.0f};
		sprite->SetTransform(transform);
		sprite->SetSize({width, height});
		sprite->SetColor(color);
		sprite->Update();
		sprite->Draw();
	};
	auto drawSpriteContained = [&drawSprite](Sprite* sprite, float x, float y, float width, float height,
	                                      const Vector4& color) {
		const Vector2 textureSize = sprite->GetTextureSize();
		if (textureSize.x <= 0.0f || textureSize.y <= 0.0f) return;
		const float scale = (std::min)(width / textureSize.x, height / textureSize.y);
		const float drawWidth = textureSize.x * scale;
		const float drawHeight = textureSize.y * scale;
		drawSprite(sprite, x + (width - drawWidth) * 0.5f, y + (height - drawHeight) * 0.5f,
		           drawWidth, drawHeight, color);
	};
	auto choiceAccentColor = [](LevelUpChoiceType type) {
		switch (type) {
		case LevelUpChoiceType::AttackSuper: return Vector4{0.78f, 0.47f, 0.08f, 1.0f};
		case LevelUpChoiceType::NewAttack: return Vector4{0.63f, 0.10f, 0.055f, 1.0f};
		case LevelUpChoiceType::StatusLevelUp: return Vector4{0.18f, 0.42f, 0.22f, 1.0f};
		case LevelUpChoiceType::NewStatus: return Vector4{0.12f, 0.38f, 0.42f, 1.0f};
		case LevelUpChoiceType::Decline: return Vector4{0.28f, 0.27f, 0.25f, 1.0f};
		case LevelUpChoiceType::AttackLevelUp:
		default: return Vector4{0.32f, 0.20f, 0.46f, 1.0f};
		}
	};

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	drawSprite(levelUpOverlaySprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight, {0.025f, 0.008f, 0.030f, 0.76f});
	// 墨色の祭壇へ朱と金の細線を重ね、戦闘中の選択を「神授の儀式」として見せる。
	drawSprite(levelUpPanelSprite_.get(), panelX, panelY, panelWidth, panelHeight, {0.055f, 0.040f, 0.050f, 0.985f});
	drawSprite(levelUpPanelAccentSprites_[0].get(), panelX, panelY, panelWidth, 5.0f, {0.67f, 0.085f, 0.045f, 1.0f});
	drawSprite(levelUpPanelAccentSprites_[1].get(), panelX + 18.0f, panelY + 9.0f, panelWidth - 36.0f, 2.0f, {0.86f, 0.65f, 0.27f, 0.92f});
	for (int index = 0; index < static_cast<int>(levelUpChoices_.size()) && index < 3; ++index) {
		const float cardX = panelX + 32.0f + static_cast<float>(index) * (cardWidth + gap);
		const bool selected = index == selectedLevelUpChoiceIndex_;
		const Vector4 accent = choiceAccentColor(levelUpChoices_[index].type);
		drawSprite(levelUpChoiceBorderSprites_[index].get(), cardX - 5.0f, cardY - 5.0f, cardWidth + 10.0f, cardHeight + 10.0f,
			selected ? Vector4{0.96f, 0.69f, 0.20f, 1.0f} : Vector4{0.30f, 0.20f, 0.16f, 1.0f});
		// 各候補は生成した和紙の御札。未選択時だけ少し落とし、紙の質感と朱模様を保つ。
		drawSprite(levelUpChoiceSprites_[index].get(), cardX, cardY, cardWidth, cardHeight,
			selected ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.78f, 0.78f, 0.75f, 0.98f});
		// 種別色は御札の下端へ細く置き、生成素材の上部意匠やアイコンを隠さない。
		drawSprite(levelUpChoiceAccentSprites_[index].get(), cardX + 22.0f, cardY + cardHeight - 8.0f,
			cardWidth - 44.0f, selected ? 5.0f : 3.0f, accent);
		const std::string& textureFilePath = levelUpChoices_[index].textureFilePath;
		const bool hasTexture = !textureFilePath.empty() && std::filesystem::exists(StringUtility::Utf8ToPath(textureFilePath));
		// 御札上部の朱模様を避け、アイコンは中央の無地領域から描く。
		const float iconMargin = 46.0f;
		const float iconHeight = (std::min)(cardHeight * 0.30f, 94.0f);
		if (hasTexture) {
			Sprite* iconSprite = levelUpChoiceIconSprites_[index].get();
			if (iconSprite->GetTextureFilePath() != textureFilePath) {
				iconSprite->SetTexture(textureFilePath);
			}
			drawSpriteContained(iconSprite, cardX + iconMargin, cardY + iconMargin, cardWidth - iconMargin * 2.0f, iconHeight,
			    selected ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.88f, 0.88f, 0.88f, 1.0f});
		}

		Entity* textObject = levelUpChoiceTextObjects_[index].get();
		const float textTop = hasTexture ? cardY + iconMargin + iconHeight + 12.0f : cardY + 18.0f;
		const auto& correspondingTextures = levelUpChoices_[index].correspondingOwnedTextureFilePaths;
		const int correspondingIconCount = (std::min)(
			static_cast<int>(correspondingTextures.size()), static_cast<int>(kPlayerEquipmentSlotCount));
		const float correspondingIconSize = 36.0f;
		const float correspondingIconGap = 8.0f;
		const float paperBottomInset = 42.0f;
		const float correspondingRowHeight = correspondingIconCount > 0 ? correspondingIconSize + 12.0f : 0.0f;
		const float textBottom = cardY + cardHeight - paperBottomInset - correspondingRowHeight;
		const float textCenterY = (textTop + textBottom) * 0.5f;
		textObject->GetTransform().translate = {cardX + cardWidth * 0.5f, textCenterY, 0.0f};
		TextComponent* text = textObject->GetComponent<TextComponent>();
		// 名前と説明をカードの内側へ文字単位で折り返し、縦に長い場合は自動縮小する。
		text->SetLayoutBounds((std::max)(1.0f, cardWidth - 36.0f), (std::max)(1.0f, textBottom - textTop));
		text->SetText(levelUpChoices_[index].title + "\n\n" + levelUpChoices_[index].description);
		text->SetColor(selected ? Vector4{0.26f, 0.055f, 0.035f, 1.0f} : Vector4{0.14f, 0.12f, 0.11f, 1.0f});

		if (correspondingIconCount > 0) {
			const float rowWidth = correspondingIconSize * correspondingIconCount +
			                       correspondingIconGap * (correspondingIconCount - 1);
			const float rowX = cardX + (cardWidth - rowWidth) * 0.5f;
			const float rowY = cardY + cardHeight - paperBottomInset - correspondingIconSize;
			for (int relatedIndex = 0; relatedIndex < correspondingIconCount; ++relatedIndex) {
				Sprite* iconSprite = levelUpChoiceCorrespondingIconSprites_[index][relatedIndex].get();
				const std::string& relatedTexture = correspondingTextures[relatedIndex];
				if (iconSprite->GetTextureFilePath() != relatedTexture) iconSprite->SetTexture(relatedTexture);
				drawSpriteContained(iconSprite,
					rowX + relatedIndex * (correspondingIconSize + correspondingIconGap), rowY,
					correspondingIconSize, correspondingIconSize,
					selected ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.88f, 0.88f, 0.88f, 1.0f});
			}
		}
	}

	if (TextComponent* titleText = levelUpTitleTextObject_->GetComponent<TextComponent>()) {
		titleText->SetText(gameplaySession_.GetMode() == GameplayMode::BossReward ? "討伐の恩賞" : "神授の選択");
		titleText->SetColor({0.98f, 0.84f, 0.48f, 1.0f});
	}
	if (TextComponent* instructionText = levelUpInstructionTextObject_->GetComponent<TextComponent>()) {
		std::string instruction = "A / D・左右キー: 選択    Space・Pad A: 授かる";
		if (gameplaySession_.GetMode() != GameplayMode::BossReward && sceneManager_ &&
		    sceneManager_->GetGlobalShopUpgradeLevel("level_up_reroll") > 0) {
			instruction += levelUpRerollUsed_ ? "    再抽選: 使用済み" : "    R・Pad Y: 再抽選";
		}
		instructionText->SetText(instruction);
		instructionText->SetColor({0.82f, 0.75f, 0.62f, 1.0f});
	}
	levelUpTitleTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + 35.0f, 0.0f};
	levelUpInstructionTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + panelHeight - 24.0f, 0.0f};
	levelUpTitleTextObject_->Draw2D();
	levelUpInstructionTextObject_->Draw2D();
	for (int index = 0; index < static_cast<int>(levelUpChoices_.size()) && index < 3; ++index) {
		levelUpChoiceTextObjects_[index]->Draw2D();
	}
}

void BaseScene::EnsureBossRewardPresentationSprites() {
	if (bossRewardOverlaySprite_) return;
	const std::string whiteTexture = "Resources/human/white.png";
	auto createSprite = [&whiteTexture]() {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(whiteTexture);
		return sprite;
	};
	bossRewardOverlaySprite_ = createSprite();
	bossRewardFlashSprite_ = createSprite();
	bossRewardChestBackSprite_ = createSprite();
	bossRewardChestFrontSprite_ = createSprite();
	bossRewardChestLidSprite_ = createSprite();
	for (auto& sprite : bossRewardBurstSprites_) sprite = createSprite();
	for (int index = 0; index < 5; ++index) {
		bossRewardIconBackSprites_[index] = createSprite();
		bossRewardIconSprites_[index] = createSprite();
	}
	auto createText = [](const std::string& value, float size) {
		auto object = std::make_unique<Entity>();
		TextComponent* text = object->AddComponent<TextComponent>();
		text->SetText(value);
		text->SetFontSize(size);
		text->SetAnchor(TextComponent::Anchor::Center);
		return object;
	};
	bossRewardTitleTextObject_ = createText("BOSS REWARD", 54.0f);
	bossRewardMoneyTextObject_ = createText("", 34.0f);
	bossRewardInstructionTextObject_ = createText("", 22.0f);
}

void BaseScene::DrawBossRewardPresentation2D() {
	if (gameplaySession_.GetMode() != GameplayMode::BossRewardPresentation) return;
	EnsureBossRewardPresentationSprites();
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon) return;
	const float screenWidth = static_cast<float>(dxCommon->GetRenderWidth());
	const float screenHeight = static_cast<float>(dxCommon->GetRenderHeight());
	const int rewardCount = static_cast<int>(bossRewardResults_.size());
	const float rarity = rewardCount >= 5 ? 1.0f : (rewardCount >= 3 ? 0.65f : 0.35f);
	const float pulse = 0.5f + 0.5f * std::sin(bossRewardPresentationTimer_ * (5.0f + rarity * 5.0f));

	auto drawSprite = [](Sprite* sprite, float x, float y, float width, float height, const Vector4& color) {
		EulerTransform transform = sprite->GetTransform();
		transform.translate = {x, y, 0.0f};
		sprite->SetTransform(transform);
		sprite->SetSize({width, height});
		sprite->SetColor(color);
		sprite->Update();
		sprite->Draw();
	};

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	drawSprite(bossRewardOverlaySprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight,
		{0.01f, 0.005f, 0.025f, 0.88f});
	const float lidProgress = (std::clamp)((bossRewardPresentationTimer_ - 0.48f) / 0.42f, 0.0f, 1.0f);
	const float openingFlash = 1.0f - (std::clamp)((bossRewardPresentationTimer_ - 0.78f) / 0.38f, 0.0f, 1.0f);
	const float flashAlpha = bossRewardPresentationReady_
		? 0.05f + 0.07f * pulse
		: openingFlash * lidProgress * (0.34f + rarity * 0.38f);
	drawSprite(bossRewardFlashSprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight,
		{1.0f, 0.72f, 0.12f, flashAlpha});

	TextComponent* title = bossRewardTitleTextObject_->GetComponent<TextComponent>();
	title->SetText(rewardCount >= 5 ? "LEGENDARY BOSS REWARD!" :
		(rewardCount >= 3 ? "GREAT BOSS REWARD!" : "BOSS REWARD!"));
	title->SetColor({1.0f, 0.82f + 0.12f * pulse, 0.22f, 1.0f});
	bossRewardTitleTextObject_->GetTransform().translate = {screenWidth * 0.5f, screenHeight * 0.20f, 0.0f};
	bossRewardTitleTextObject_->Draw2D();

	// 箱は画面中央下に弾むように現れ、ふたが後方へ回転して開く。
	const float chestCenterX = screenWidth * 0.5f;
	const float chestCenterY = screenHeight * 0.62f;
	const float chestAppear = (std::clamp)(bossRewardPresentationTimer_ / 0.38f, 0.0f, 1.0f);
	const float chestBounce = chestAppear < 1.0f
		? 1.0f + std::sin(chestAppear * MathConstants::kPi) * 0.18f
		: 1.0f + 0.025f * pulse;
	const float chestWidth = 210.0f * chestBounce;
	const float chestHeight = 118.0f * chestBounce;
	const Vector4 chestDark{0.30f, 0.10f, 0.015f, chestAppear};
	const Vector4 chestGold{0.92f, 0.50f + 0.18f * pulse, 0.045f, chestAppear};
	drawSprite(bossRewardChestBackSprite_.get(), chestCenterX - chestWidth * 0.48f,
		chestCenterY - chestHeight * 0.40f, chestWidth * 0.96f, chestHeight * 0.65f, chestDark);
	drawSprite(bossRewardChestFrontSprite_.get(), chestCenterX - chestWidth * 0.5f,
		chestCenterY - chestHeight * 0.02f, chestWidth, chestHeight * 0.58f, chestGold);
	{
		Sprite* lid = bossRewardChestLidSprite_.get();
		EulerTransform lidTransform = lid->GetTransform();
		const float lidWidth = chestWidth * (1.0f - 0.12f * lidProgress);
		const float lidHeight = chestHeight * 0.30f;
		lidTransform.translate = {
			chestCenterX - lidWidth * 0.5f - 18.0f * lidProgress,
			chestCenterY - chestHeight * (0.12f + 0.55f * lidProgress), 0.0f};
		lidTransform.rotate.z = -0.42f * lidProgress;
		lid->SetTransform(lidTransform);
		lid->SetSize({lidWidth, lidHeight});
		lid->SetColor(chestGold);
		lid->Update();
		lid->Draw();
	}

	// ふたが開いた瞬間、報酬数に応じた本数の光片を箱から放射する。
	const int burstCount = rewardCount >= 5 ? 16 : (rewardCount >= 3 ? 12 : 8);
	const float burstProgress = (std::clamp)((bossRewardPresentationTimer_ - 0.72f) / 0.72f, 0.0f, 1.0f);
	for (int index = 0; index < burstCount; ++index) {
		const float angle = -2.85f + 2.56f * static_cast<float>(index) / static_cast<float>((std::max)(1, burstCount - 1));
		const float distance = (70.0f + rarity * 105.0f + 12.0f * (index % 3)) * burstProgress;
		const float x = chestCenterX + std::cos(angle) * distance;
		const float y = chestCenterY + std::sin(angle) * distance;
		const float alpha = lidProgress * (1.0f - burstProgress);
		drawSprite(bossRewardBurstSprites_[index].get(), x - 5.0f, y - 16.0f,
			10.0f + rarity * 5.0f, 32.0f + rarity * 25.0f,
			{1.0f, 0.72f + 0.22f * pulse, 0.12f, alpha});
	}

	const float iconSize = rewardCount >= 5 ? 130.0f : 150.0f;
	const float gap = rewardCount >= 5 ? 24.0f : 34.0f;
	const float totalWidth = rewardCount > 0 ? iconSize * rewardCount + gap * (rewardCount - 1) : 0.0f;
	const float startX = (screenWidth - totalWidth) * 0.5f;
	for (int index = 0; index < rewardCount && index < 5; ++index) {
		const float revealTime = 0.88f + 0.20f * static_cast<float>(index);
		if (bossRewardPresentationTimer_ < revealTime) continue;
		const float revealProgress = (std::clamp)((bossRewardPresentationTimer_ - revealTime) / 0.52f, 0.0f, 1.0f);
		const float easedProgress = 1.0f - std::pow(1.0f - revealProgress, 3.0f);
		const float easedScale = 0.45f + 0.55f * easedProgress;
		const float size = iconSize * easedScale;
		const float destinationX = startX + iconSize * 0.5f + index * (iconSize + gap);
		const float destinationY = screenHeight * 0.43f;
		const float centerX = chestCenterX + (destinationX - chestCenterX) * easedProgress;
		const float centerY = chestCenterY + (destinationY - chestCenterY) * easedProgress
			- std::sin(revealProgress * MathConstants::kPi) * (95.0f + rarity * 55.0f);
		drawSprite(bossRewardIconBackSprites_[index].get(), centerX - size * 0.5f - 8.0f,
			centerY - size * 0.5f - 8.0f, size + 16.0f, size + 16.0f,
			{1.0f, 0.55f + 0.30f * pulse, 0.04f, (std::min)(1.0f, revealProgress * 2.0f)});
		const std::string& texture = bossRewardResults_[index].textureFilePath;
		if (!texture.empty() && std::filesystem::exists(StringUtility::Utf8ToPath(texture))) {
			Sprite* icon = bossRewardIconSprites_[index].get();
			if (icon->GetTextureFilePath() != texture) icon->SetTexture(texture);
			drawSprite(icon, centerX - size * 0.5f, centerY - size * 0.5f, size, size,
				{1.0f, 1.0f, 1.0f, (std::min)(1.0f, revealProgress * 2.0f)});
		}
	}

	TextComponent* money = bossRewardMoneyTextObject_->GetComponent<TextComponent>();
	money->SetText("+ " + std::to_string(bossRewardPresentationMoney_) + " G");
	money->SetColor({1.0f, 0.86f, 0.28f, 1.0f});
	bossRewardMoneyTextObject_->GetTransform().translate = {screenWidth * 0.5f, screenHeight * 0.72f, 0.0f};
	bossRewardMoneyTextObject_->Draw2D();

	TextComponent* instruction = bossRewardInstructionTextObject_->GetComponent<TextComponent>();
	instruction->SetText(bossRewardPresentationReady_ ? "Space / Pad A: Continue" : "Opening reward...");
	instruction->SetColor({0.92f, 0.92f, 1.0f, bossRewardPresentationReady_ ? 0.65f + 0.35f * pulse : 0.7f});
	bossRewardInstructionTextObject_->GetTransform().translate = {screenWidth * 0.5f, screenHeight * 0.84f, 0.0f};
	bossRewardInstructionTextObject_->Draw2D();
}


