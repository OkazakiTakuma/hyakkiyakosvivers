#include "base_scene/BaseScene.h"
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

Player* BaseScene::FindActivePlayer() const {
	for (const auto& object : sceneObjects_) {
		Player* candidate = object->GetComponent<Player>();
		if (candidate && candidate->IsEnabled()) {
			return candidate;
		}
	}
	return nullptr;
}

void BaseScene::UpdatePlayerHealthHud() {
	Player* player = FindActivePlayer();

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

void BaseScene::UpdateChallengeMoneyHud() {
	// GAMEPLAY以外のプレイヤー編集画面などには、ステージ戦績HUDを表示しない。
	isChallengeMoneyHudVisible_ = sceneName_ == "GAMEPLAY";
	if (!isChallengeMoneyHudVisible_) return;

	if (!challengeMoneyTextObject_) {
		// 初回表示時だけ文字オブジェクトを生成し、以降は表示文字列だけを差し替える。
		challengeMoneyTextObject_ = std::make_unique<GameObject>();
		TextComponent* text = challengeMoneyTextObject_->AddComponent<TextComponent>();
		text->SetFontSize(26.0f);
		text->SetAnchor(TextComponent::Anchor::TopLeft);
		text->SetColor({1.0f, 0.82f, 0.18f, 1.0f});
	}

	// challengeMoneyEarned_は全体G強化適用後の値なので、リザルトと同じ累計を表示できる。
	// 左上基準で固定し、右上のHP・装備HUDや画面下部の経験値バーとの重なりを避ける。
	challengeMoneyTextObject_->GetTransform().translate = {24.0f, 24.0f, 0.0f};
	challengeMoneyTextObject_->GetComponent<TextComponent>()->SetText(
	    "G: " + std::to_string(challengeMoneyEarned_));
}

void BaseScene::DrawChallengeMoneyHud() {
	// 更新側がGAMEPLAY以外で非表示にした場合は、保持中の古い文字オブジェクトを描画しない。
	if (isChallengeMoneyHudVisible_ && challengeMoneyTextObject_) {
		challengeMoneyTextObject_->Draw2D();
	}
}

void BaseScene::UpdatePlayerExperienceHud() {
	// HUDはシーン内で最初に見つかった有効なプレイヤーの経験値を表示する。
	Player* player = FindActivePlayer();

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
		playerExperienceTextObject_ = std::make_unique<GameObject>();
		TextComponent* text = playerExperienceTextObject_->AddComponent<TextComponent>();
		text->SetFontSize(17.0f);
		text->SetAnchor(TextComponent::Anchor::BottomCenter);
		text->SetColor({0.92f, 0.96f, 1.0f, 1.0f});
	}

	const PlayerStats& stats = player->GetBaseStats();
	const int level = (std::max)(1, stats.level);
	// experienceはゲーム全体の累積値なので、現在レベルの開始値を引いてレベル内の進捗へ変換する。
	const int previousLevelExperience = level > 1 ? Player::GetRequiredExperienceForNextLevel(level - 1) : 0;
	const int nextLevelExperience = Player::GetRequiredExperienceForNextLevel(level);
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

void BaseScene::DrawPlayerExperienceHud() {
	if (!isPlayerExperienceHudVisible_ || !playerExperienceBarBackground_ || !playerExperienceBarFill_) return;
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	playerExperienceBarBackground_->Draw();
	// 幅0のスプライト描画を避けるため、経験値がある場合だけ進捗部分を描画する。
	if (playerExperienceRate_ > 0.0f) playerExperienceBarFill_->Draw();
	if (playerExperienceTextObject_) playerExperienceTextObject_->Draw2D();
}

void BaseScene::UpdatePlayerSlotHud() {
	// HUDはシーン内で最初に見つかった有効なプレイヤーの基礎スロット設定を表示する。
	Player* player = FindActivePlayer();

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
		auto object = std::make_unique<GameObject>();
		TextComponent* text = object->AddComponent<TextComponent>();
		text->SetText(label);
		text->SetFontSize(15.0f);
		text->SetAnchor(TextComponent::Anchor::CenterRight);
		text->SetColor({1.0f, 1.0f, 1.0f, 0.9f});
		return object;
	};
	auto createLevelText = []() {
		auto object = std::make_unique<GameObject>();
		TextComponent* text = object->AddComponent<TextComponent>();
		text->SetFontSize(12.0f);
		text->SetAnchor(TextComponent::Anchor::Center);
		text->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
		return object;
	};
	for (int index = 0; index < 5; ++index) {
		if (!playerAttackSlotBackgroundSprites_[index]) playerAttackSlotBackgroundSprites_[index] = createSprite();
		if (!playerAttackSlotIconSprites_[index]) playerAttackSlotIconSprites_[index] = createSprite();
		if (!playerStatusSlotBackgroundSprites_[index]) playerStatusSlotBackgroundSprites_[index] = createSprite();
		if (!playerStatusSlotIconSprites_[index]) playerStatusSlotIconSprites_[index] = createSprite();
		if (!playerAttackSlotLevelBackgroundSprites_[index]) playerAttackSlotLevelBackgroundSprites_[index] = createSprite();
		if (!playerStatusSlotLevelBackgroundSprites_[index]) playerStatusSlotLevelBackgroundSprites_[index] = createSprite();
		if (!playerAttackSlotLevelTextObjects_[index]) playerAttackSlotLevelTextObjects_[index] = createLevelText();
		if (!playerStatusSlotLevelTextObjects_[index]) playerStatusSlotLevelTextObjects_[index] = createLevelText();
	}
	if (!playerAttackSlotLabelObject_) playerAttackSlotLabelObject_ = createLabel("ATTACK");
	if (!playerStatusSlotLabelObject_) playerStatusSlotLabelObject_ = createLabel("STATUS");

	// HPバー直下を起点に、5枠全体が画面右端へ揃うよう左端座標を逆算する。
	const float screenWidth = static_cast<float>(Input::GetInstance()->GetClientWidth());
	constexpr float kRightMargin = 24.0f;
	constexpr float kTop = 68.0f;
	constexpr float kLabelWidth = 64.0f;
	constexpr float kSlotSize = 46.0f;
	constexpr float kSlotGap = 5.0f;
	constexpr float kRowGap = 7.0f;
	constexpr float kInnerMargin = 3.0f;
	constexpr float kLevelHeight = 16.0f;
	const float slotsWidth = kSlotSize * 5.0f + kSlotGap * 4.0f;
	const float slotsLeft = (std::max)(kRightMargin + kLabelWidth, screenWidth - kRightMargin - slotsWidth);
	const float labelRight = slotsLeft - 8.0f;
	const float attackY = kTop;
	const float statusY = attackY + kSlotSize + kRowGap;
	playerAttackSlotLabelObject_->GetTransform().translate = {labelRight, attackY + kSlotSize * 0.5f, 0.0f};
	playerStatusSlotLabelObject_->GetTransform().translate = {labelRight, statusY + kSlotSize * 0.5f, 0.0f};

	const PlayerStats& stats = player->GetBaseStats();
	for (int index = 0; index < 5; ++index) {
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
		    Sprite* background, GameObject* textObject, float y, const std::string& levelText, bool enabled) {
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
			const std::string levelText = attackSlot.attackLevel == "super"
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
				if (attackSlot.attackLevel == "super") {
					for (const PlayerAttackLevelStats& levelStats : attackStats.levels) {
						if (levelStats.level == "super") {
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

void BaseScene::DrawPlayerSlotHud() {
	if (!isPlayerSlotHudVisible_) return;
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	// 背景、アイコン、レベル帯、レベル文字の順に重ね、最後に行ラベルを描画する。
	for (int index = 0; index < 5; ++index) {
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
		if (gameplayMode_ != GameplayMode::BossReward &&
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
		Player* player = object->GetComponent<Player>();
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

bool BaseScene::BuildLevelUpChoices(Player* player) {
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
		if (targetLevel != "super") {
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
		if (slot.attackLevel != "super" && level >= 1 && level < 5) {
			const std::string targetLevel = std::to_string(level + 1);
			const std::string fallback = "Attack level " + slot.attackLevel + " -> " + targetLevel;
			candidates.push_back({LevelUpChoiceType::AttackLevelUp, slot.attackName, slot.attackName, getAttackDescription(slot.attackName, targetLevel, fallback), index, getAttackTexture(slot.attackName, targetLevel)});
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
				candidates.push_back({LevelUpChoiceType::AttackSuper, slot.attackName, slot.attackName + " SUPER", getAttackDescription(slot.attackName, "super", "Promote attack level 5 -> super"), index, getAttackTexture(slot.attackName, "super")});
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
			candidates.push_back({LevelUpChoiceType::StatusLevelUp, slot.statusName, slot.statusName, getStatusDescription(slot.statusName, level, fallback), index, getStatusTexture(slot.statusName, level)});
		}
	}

	if (emptyAttackSlot >= 0) {
		for (const std::string& attackName : LoadPlayerAttackNames()) {
			// 装備済み武器と他タイプ専用の初期武器を除き、空きスロットへ追加可能な武器だけを残す。
			if (std::find(equippedAttacks.begin(), equippedAttacks.end(), attackName) == equippedAttacks.end() &&
			    !otherPlayerInitialAttacks.contains(attackName)) {
				candidates.push_back({LevelUpChoiceType::NewAttack, attackName, attackName, getAttackDescription(attackName, "1", "Add a new attack at level 1"), emptyAttackSlot, getAttackTexture(attackName, "1")});
			}
		}
	}
	if (emptyStatusSlot >= 0) {
		for (const std::string& statusName : LoadPlayerStatusItemNames()) {
			if (std::find(equippedStatuses.begin(), equippedStatuses.end(), statusName) == equippedStatuses.end()) {
				candidates.push_back({LevelUpChoiceType::NewStatus, statusName, statusName, getStatusDescription(statusName, 0, "Add a new status at level 1"), emptyStatusSlot, getStatusTexture(statusName, 0)});
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
	}
	return true;
}

void BaseScene::RerollLevelUpChoices() {
	if (!sceneManager || !levelUpPlayer_ || gameplayMode_ == GameplayMode::BossReward || levelUpRerollUsed_ ||
	    sceneManager->GetGlobalShopUpgradeLevel("level_up_reroll") <= 0 || levelUpChoices_.empty()) {
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
	const bool wasBossAcquisitionOffer = gameplayMode_ == GameplayMode::BossReward;
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
		levelUpPlayer_->ApplyStats(stats, ApplyPlayerStatusItems(stats));
		if (GameObject* owner = levelUpPlayer_->GetOwner()) {
			ApplyPlayerAttackSlots(owner->GetComponent<PlayerAttackComponent>(), stats);
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
	for (int index = 0; index < 3; ++index) {
		levelUpChoiceBorderSprites_[index] = createSprite();
		levelUpChoiceSprites_[index] = createSprite();
		levelUpChoiceIconSprites_[index] = createSprite();
	}

	auto createTextObject = [](const std::string& text, float fontSize) {
		auto object = std::make_unique<GameObject>();
		TextComponent* textComponent = object->AddComponent<TextComponent>();
		textComponent->SetText(text);
		textComponent->SetFontSize(fontSize);
		textComponent->SetAnchor(TextComponent::Anchor::Center);
		return object;
	};
	levelUpTitleTextObject_ = createTextObject("LEVEL UP!", 46.0f);
	levelUpInstructionTextObject_ = createTextObject("A / D or Pad: Select    Space or Pad A: Confirm", 21.0f);
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
	const float panelHeight = (std::min)(screenHeight - 48.0f, 440.0f);
	const float panelX = (screenWidth - panelWidth) * 0.5f;
	const float panelY = (screenHeight - panelHeight) * 0.5f;
	const float gap = 24.0f;
	const float cardWidth = (panelWidth - 64.0f - gap * 2.0f) / 3.0f;
	const float cardHeight = panelHeight - 145.0f;
	const float cardY = panelY + 92.0f;

	auto drawSprite = [](Sprite* sprite, float x, float y, float width, float height, const Vector4& color) {
		EulerTransform transform = sprite->GetTransform();
		transform.translate = {x, y, 0.0f};
		sprite->SetTransform(transform);
		sprite->SetSize({width, height});
		sprite->SetColor(color);
		sprite->Update();
		sprite->Draw();
	};
	auto choiceColor = [](LevelUpChoiceType type) {
		switch (type) {
		case LevelUpChoiceType::AttackSuper: return Vector4{0.72f, 0.42f, 0.05f, 0.98f};
		case LevelUpChoiceType::NewAttack: return Vector4{0.55f, 0.18f, 0.08f, 0.98f};
		case LevelUpChoiceType::StatusLevelUp: return Vector4{0.08f, 0.35f, 0.20f, 0.98f};
		case LevelUpChoiceType::NewStatus: return Vector4{0.06f, 0.32f, 0.38f, 0.98f};
		case LevelUpChoiceType::Decline: return Vector4{0.22f, 0.22f, 0.25f, 0.98f};
		case LevelUpChoiceType::AttackLevelUp:
		default: return Vector4{0.08f, 0.22f, 0.48f, 0.98f};
		}
	};

	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	drawSprite(levelUpOverlaySprite_.get(), 0.0f, 0.0f, screenWidth, screenHeight, {0.0f, 0.0f, 0.0f, 0.68f});
	drawSprite(levelUpPanelSprite_.get(), panelX, panelY, panelWidth, panelHeight, {0.035f, 0.045f, 0.075f, 0.98f});
	for (int index = 0; index < static_cast<int>(levelUpChoices_.size()) && index < 3; ++index) {
		const float cardX = panelX + 32.0f + static_cast<float>(index) * (cardWidth + gap);
		const bool selected = index == selectedLevelUpChoiceIndex_;
		drawSprite(levelUpChoiceBorderSprites_[index].get(), cardX - 5.0f, cardY - 5.0f, cardWidth + 10.0f, cardHeight + 10.0f,
			selected ? Vector4{1.0f, 0.78f, 0.12f, 1.0f} : Vector4{0.20f, 0.23f, 0.30f, 1.0f});
		Vector4 color = choiceColor(levelUpChoices_[index].type);
		if (selected) {
			color.x = (std::min)(1.0f, color.x + 0.14f);
			color.y = (std::min)(1.0f, color.y + 0.14f);
			color.z = (std::min)(1.0f, color.z + 0.14f);
		}
		drawSprite(levelUpChoiceSprites_[index].get(), cardX, cardY, cardWidth, cardHeight, color);
		const std::string& textureFilePath = levelUpChoices_[index].textureFilePath;
		const bool hasTexture = !textureFilePath.empty() && std::filesystem::exists(StringUtility::Utf8ToPath(textureFilePath));
		const float iconMargin = 18.0f;
		const float iconHeight = (std::min)(cardHeight * 0.43f, 125.0f);
		if (hasTexture) {
			Sprite* iconSprite = levelUpChoiceIconSprites_[index].get();
			if (iconSprite->GetTextureFilePath() != textureFilePath) {
				iconSprite->SetTexture(textureFilePath);
			}
			drawSprite(iconSprite, cardX + iconMargin, cardY + iconMargin, cardWidth - iconMargin * 2.0f, iconHeight,
			    selected ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.88f, 0.88f, 0.88f, 1.0f});
		}

		GameObject* textObject = levelUpChoiceTextObjects_[index].get();
		const float textTop = hasTexture ? cardY + iconMargin + iconHeight + 12.0f : cardY + 18.0f;
		const float textBottom = cardY + cardHeight - 18.0f;
		const float textCenterY = (textTop + textBottom) * 0.5f;
		textObject->GetTransform().translate = {cardX + cardWidth * 0.5f, textCenterY, 0.0f};
		TextComponent* text = textObject->GetComponent<TextComponent>();
		// 名前と説明をカードの内側へ文字単位で折り返し、縦に長い場合は自動縮小する。
		text->SetLayoutBounds((std::max)(1.0f, cardWidth - 36.0f), (std::max)(1.0f, textBottom - textTop));
		text->SetText(levelUpChoices_[index].title + "\n\n" + levelUpChoices_[index].description);
		text->SetColor(selected ? Vector4{1.0f, 0.92f, 0.55f, 1.0f} : Vector4{1.0f, 1.0f, 1.0f, 1.0f});
	}

	if (TextComponent* titleText = levelUpTitleTextObject_->GetComponent<TextComponent>()) {
		titleText->SetText(gameplayMode_ == GameplayMode::BossReward ? "BOSS REWARD!" : "LEVEL UP!");
	}
	if (TextComponent* instructionText = levelUpInstructionTextObject_->GetComponent<TextComponent>()) {
		std::string instruction = "A / D or Pad: Select    Space or Pad A: Confirm";
		if (gameplayMode_ != GameplayMode::BossReward && sceneManager &&
		    sceneManager->GetGlobalShopUpgradeLevel("level_up_reroll") > 0) {
			instruction += levelUpRerollUsed_ ? "    Reroll: Used" : "    R or Pad Y: Reroll";
		}
		instructionText->SetText(instruction);
	}
	levelUpTitleTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + 35.0f, 0.0f};
	levelUpInstructionTextObject_->GetTransform().translate = {screenWidth * 0.5f, panelY + panelHeight - 24.0f, 0.0f};
	levelUpTitleTextObject_->Draw2D();
	levelUpInstructionTextObject_->Draw2D();
	for (int index = 0; index < static_cast<int>(levelUpChoices_.size()) && index < 3; ++index) {
		levelUpChoiceTextObjects_[index]->Draw2D();
	}
}


