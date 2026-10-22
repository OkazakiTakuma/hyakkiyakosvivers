#include "ShopScene.h"

#include "SceneManager.h"
#include "helpers/SceneUiUtility.h"
#include "repositories/PlayerStatusRepository.h"

#include <algorithm>
#include <sstream>
#include <Xinput.h>

namespace {
enum class ShopUpgradeEffect {
	BaseHealth,
	AttackPercent,
	DefensePercent,
	BaseSpeed,
	ExperienceGain,
	GoldGain,
	LevelUpReroll,
};

struct ShopUpgradeDefinition {
	const char* id;
	const char* label;
	const char* amountSuffix;
	ShopUpgradeEffect effect;
	float amount;
	int maxLevel;
	bool isGlobal;
	int fixedPrice;
};

constexpr int kProgressiveUpgradeBasePrice = 100;
constexpr int kProgressiveUpgradePricePerLevel = 75;
constexpr int kNoFixedPrice = -1;

/// <summary>表示、価格計算、購入効果が参照するショップ商品の単一定義です。</summary>
constexpr std::array<ShopUpgradeDefinition, 7> kUpgradeDefinitions = {{
	{"health", "最大HP", "", ShopUpgradeEffect::BaseHealth, 10.0f, 10, false, kNoFixedPrice},
	{"attack", "攻撃力", "%", ShopUpgradeEffect::AttackPercent, 5.0f, 10, false, kNoFixedPrice},
	{"defense", "防御力", "%", ShopUpgradeEffect::DefensePercent, 2.0f, 10, false, kNoFixedPrice},
	{"speed", "移動速度", "", ShopUpgradeEffect::BaseSpeed, 0.01f, 10, false, kNoFixedPrice},
	{"experience_gain", "[全体] 獲得経験値", "%", ShopUpgradeEffect::ExperienceGain, 10.0f, 10, true, kNoFixedPrice},
	{"gold_gain", "[全体] 獲得G", "%", ShopUpgradeEffect::GoldGain, 10.0f, 10, true, kNoFixedPrice},
	{"level_up_reroll", "[全体] レベルアップリロール解放", "", ShopUpgradeEffect::LevelUpReroll, 0.0f, 1, true, 300},
}};

std::string GetUpgradeDisplayName(const ShopUpgradeDefinition& definition) {
	std::ostringstream stream;
	stream << definition.label;
	if (definition.amount > 0.0f) {
		stream << " +" << definition.amount << definition.amountSuffix;
	}
	return stream.str();
}

/// <summary>購入済みレベルに応じて次回購入価格を段階的に増加させます。</summary>
int GetPrice(const ShopUpgradeDefinition& definition, int level) {
	return definition.fixedPrice >= 0
		? definition.fixedPrice
		: kProgressiveUpgradeBasePrice + level * kProgressiveUpgradePricePerLevel;
}

using SceneUiUtility::CreateColorSprite;
using SceneUiUtility::DrawColorSprite;
}

void ShopScene::Initialize() {
	// ショップではゲーム開始用の選択状態と別に、開放済みキャラクターから強化対象を選ぶ。
	RefreshUnlockedPlayerTargets();
	backgroundSprite_ = CreateColorSprite();
	panelSprite_ = CreateColorSprite();
	titleTextObject_ = SceneUiUtility::CreateTextObject("SHOP", 58.0f);
	moneyTextObject_ = SceneUiUtility::CreateTextObject("", 30.0f);
	for (auto& item : itemTextObjects_) item = SceneUiUtility::CreateTextObject("", 23.0f);
	messageTextObject_ = SceneUiUtility::CreateTextObject("", 21.0f);
	instructionTextObject_ = SceneUiUtility::CreateTextObject(
	    "左右 / Pad左右: プレイヤー    上下 / Pad上下: 商品    Enter / Pad A: 購入    Q / Pad B: タイトルへ", 18.0f);
	RefreshText();
}

void ShopScene::RefreshUnlockedPlayerTargets() {
	unlockedPlayerTypeNames_.clear();
	selectedPlayerIndex_ = 0;
	if (!sceneManager_) return;

	// Defaultは設定欠損時の内部フォールバックなので、通常のショップ対象から除外する。
	for (const std::string& playerTypeName : LoadPlayerTypeNames()) {
		if (playerTypeName != "Default" && sceneManager_->IsPlayerTypeUnlocked(playerTypeName)) {
			unlockedPlayerTypeNames_.push_back(playerTypeName);
		}
	}

	// ゲーム開始時に選択中のキャラクターが開放済みなら、ショップの初期対象にも採用する。
	const auto selected = std::find(
	    unlockedPlayerTypeNames_.begin(), unlockedPlayerTypeNames_.end(), sceneManager_->GetSelectedPlayerTypeName());
	if (selected != unlockedPlayerTypeNames_.end()) {
		selectedPlayerIndex_ = static_cast<int>(std::distance(unlockedPlayerTypeNames_.begin(), selected));
	}
}

void ShopScene::ChangePlayerTarget(int direction) {
	if (unlockedPlayerTypeNames_.empty()) return;
	const int playerCount = static_cast<int>(unlockedPlayerTypeNames_.size());
	selectedPlayerIndex_ = (selectedPlayerIndex_ + direction + playerCount) % playerCount;
	message_.clear();
	RefreshText();
}

std::string ShopScene::GetTargetPlayerTypeName() const {
	if (unlockedPlayerTypeNames_.empty() || selectedPlayerIndex_ < 0 ||
	    selectedPlayerIndex_ >= static_cast<int>(unlockedPlayerTypeNames_.size())) {
		return {};
	}
	return unlockedPlayerTypeNames_[selectedPlayerIndex_];
}

void ShopScene::RefreshText() {
	if (!sceneManager_) return;
	const std::string targetPlayerType = GetTargetPlayerTypeName();
	// 所持金は全プレイヤー共通、強化レベルはショップ内で選択したプレイヤー別に表示する。
	moneyTextObject_->GetComponent<TextComponent>()->SetText(
	    "所持金: " + std::to_string(sceneManager_->GetMoney()) + " G    対象: < " +
	    (targetPlayerType.empty() ? "開放済みプレイヤーなし" : targetPlayerType) + " >");
	for (size_t index = 0; index < itemTextObjects_.size(); ++index) {
		const ShopUpgradeDefinition& definition = kUpgradeDefinitions[index];
		const int level = definition.isGlobal
		    ? sceneManager_->GetGlobalShopUpgradeLevel(definition.id)
		    : sceneManager_->GetShopUpgradeLevelForPlayer(targetPlayerType, definition.id);
		std::ostringstream stream;
		const int maxLevel = definition.maxLevel;
		stream << (static_cast<int>(index) == selectedIndex_ ? ">  " : "   ") << GetUpgradeDisplayName(definition);
		if (definition.effect == ShopUpgradeEffect::LevelUpReroll) {
			stream << (level >= maxLevel ? "    購入済み" : "    未購入    " + std::to_string(GetPrice(definition, level)) + " G");
		} else {
			stream << "    Lv." << level << " / " << maxLevel;
			if (level < maxLevel) stream << "    " << GetPrice(definition, level) << " G";
			else stream << "    MAX";
		}
		TextComponent* text = itemTextObjects_[index]->GetComponent<TextComponent>();
		text->SetText(stream.str());
		text->SetColor(static_cast<int>(index) == selectedIndex_
		    ? Vector4{1.0f, 0.82f, 0.18f, 1.0f} : Vector4{0.88f, 0.92f, 1.0f, 1.0f});
	}
	messageTextObject_->GetComponent<TextComponent>()->SetText(message_);
}

void ShopScene::PurchaseSelectedUpgrade() {
	if (!sceneManager_) return;
	const ShopUpgradeDefinition& definition = kUpgradeDefinitions[static_cast<size_t>(selectedIndex_)];
	const char* upgradeId = definition.id;
	const std::string targetPlayerType = GetTargetPlayerTypeName();
	// 表示時と同じ分類で保存先を選び、別キャラクターの購入レベルとの混在を防ぐ。
	const bool isGlobalUpgrade = definition.isGlobal;
	// 表示状態が古い場合や外部から選択名を変更された場合も、支払い前に開放状態を再検証する。
	if (!isGlobalUpgrade &&
	    (targetPlayerType.empty() || !sceneManager_->IsPlayerTypeUnlocked(targetPlayerType))) {
		message_ = "未開放のプレイヤーは強化できません";
		RefreshText();
		return;
	}
	const int level = isGlobalUpgrade
	    ? sceneManager_->GetGlobalShopUpgradeLevel(upgradeId)
	    : sceneManager_->GetShopUpgradeLevelForPlayer(targetPlayerType, upgradeId);
	const int maxLevel = definition.maxLevel;
	if (level >= maxLevel) {
		message_ = "この強化は最大レベルです";
		RefreshText();
		return;
	}
	const int price = GetPrice(definition, level);
	// SpendMoney内で残高不足を判定し、支払いに成功した場合だけ能力値を変更する。
	if (!sceneManager_->SpendMoney(price)) {
		message_ = "お金が足りません";
		RefreshText();
		return;
	}

	if (isGlobalUpgrade) {
		// 全体強化は能力JSONを変更せず、game_progress.jsonの共通レベルだけを更新する。
		sceneManager_->SetGlobalShopUpgradeLevel(upgradeId, level + 1);
	} else {
		// プレイヤー能力の保存先へ直接加算するため、次回のゲームプレイ開始時にも効果が残る。
		PlayerStats stats = LoadPlayerStats(targetPlayerType);
		switch (definition.effect) {
		case ShopUpgradeEffect::BaseHealth: stats.baseHealth += definition.amount; break;
		case ShopUpgradeEffect::AttackPercent: stats.attack += definition.amount; break;
		case ShopUpgradeEffect::DefensePercent: stats.defense += definition.amount; break;
		case ShopUpgradeEffect::BaseSpeed: stats.baseSpeed += definition.amount; break;
		default: break;
		}
		SavePlayerStats(targetPlayerType, stats);
		// 能力値とは別に購入回数をgame_progress.jsonへ保存し、価格計算と最大レベル判定に利用する。
		sceneManager_->SetShopUpgradeLevelForPlayer(targetPlayerType, upgradeId, level + 1);
	}
	// 次回起動時はこのレベルを読み戻すため、購入済み表示も終了前の状態から再開できる。
	message_ = GetUpgradeDisplayName(definition) + " を購入しました";
	RefreshText();
}

void ShopScene::Update() {
	if (!sceneManager_) return;
	Input* input = Input::GetInstance();
	if (input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_A) || input->TriggerGamepadLeft()) {
		ChangePlayerTarget(-1);
	}
	if (input->TriggerKey(DIK_RIGHT) || input->TriggerKey(DIK_D) || input->TriggerGamepadRight()) {
		ChangePlayerTarget(1);
	}
	if (input->TriggerKey(DIK_UP) || input->TriggerGamepadButton(XINPUT_GAMEPAD_DPAD_UP)) {
		selectedIndex_ = (selectedIndex_ + static_cast<int>(itemTextObjects_.size()) - 1) % static_cast<int>(itemTextObjects_.size());
		message_.clear();
		RefreshText();
	}
	if (input->TriggerKey(DIK_DOWN) || input->TriggerGamepadButton(XINPUT_GAMEPAD_DPAD_DOWN)) {
		selectedIndex_ = (selectedIndex_ + 1) % static_cast<int>(itemTextObjects_.size());
		message_.clear();
		RefreshText();
	}
	if (input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) ||
	    input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) PurchaseSelectedUpgrade();
	if (input->TriggerKey(DIK_Q) || input->TriggerGamepadButton(XINPUT_GAMEPAD_B)) sceneManager_->ChangeScene("TITLE");
}

void ShopScene::Draw2D() {
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon || !backgroundSprite_ || !panelSprite_) return;
	const float width = static_cast<float>(dxCommon->GetRenderWidth());
	const float height = static_cast<float>(dxCommon->GetRenderHeight());
	const float panelWidth = (std::min)(760.0f, width - 80.0f);
	const float panelHeight = (std::min)(680.0f, height - 30.0f);
	const float panelX = (width - panelWidth) * 0.5f;
	const float panelY = (height - panelHeight) * 0.5f;
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	DrawColorSprite(backgroundSprite_.get(), 0.0f, 0.0f, width, height, {0.025f, 0.018f, 0.05f, 1.0f});
	DrawColorSprite(panelSprite_.get(), panelX, panelY, panelWidth, panelHeight, {0.10f, 0.06f, 0.16f, 0.98f});
	titleTextObject_->GetTransform().translate = {width * 0.5f, panelY + 58.0f, 0.0f};
	titleTextObject_->GetComponent<TextComponent>()->SetColor({1.0f, 0.76f, 0.16f, 1.0f});
	moneyTextObject_->GetTransform().translate = {width * 0.5f, panelY + 125.0f, 0.0f};
	for (size_t index = 0; index < itemTextObjects_.size(); ++index)
		itemTextObjects_[index]->GetTransform().translate = {width * 0.5f, panelY + 175.0f + 48.0f * index, 0.0f};
	messageTextObject_->GetTransform().translate = {width * 0.5f, panelY + 555.0f, 0.0f};
	messageTextObject_->GetComponent<TextComponent>()->SetColor({0.45f, 1.0f, 0.62f, 1.0f});
	instructionTextObject_->GetTransform().translate = {width * 0.5f, panelY + panelHeight - 35.0f, 0.0f};
	titleTextObject_->Draw2D();
	moneyTextObject_->Draw2D();
	for (auto& item : itemTextObjects_) item->Draw2D();
	messageTextObject_->Draw2D();
	instructionTextObject_->Draw2D();
}

void ShopScene::Finalize() {
	backgroundSprite_.reset(); panelSprite_.reset(); titleTextObject_.reset(); moneyTextObject_.reset();
	for (auto& item : itemTextObjects_) item.reset();
	messageTextObject_.reset(); instructionTextObject_.reset();
	unlockedPlayerTypeNames_.clear();
	selectedPlayerIndex_ = 0;
	BaseScene::Finalize();
}
