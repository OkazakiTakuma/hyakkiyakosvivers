#include "ShopScene.h"

#include "SceneManager.h"
#include "helpers/SceneUiUtility.h"
#include "TextureManager.h"
#include "GameTime.h"
#include <model/ModelManager.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <Xinput.h>

namespace {
enum class ShopUpgradeEffect {
	BaseHealth,
	AttackPercent,
	AttackSpeedPercent,
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
	int fixedPrice;
};

constexpr int kProgressiveUpgradeBasePrice = 100;
constexpr int kProgressiveUpgradePricePerLevel = 75;
constexpr int kNoFixedPrice = -1;
constexpr const char* kUiFontName = "Yu Gothic UI";
constexpr float kPrayerEffectDuration = 1.65f;

float SmoothStep(float start, float end, float value) {
	const float rate = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
	return rate * rate * (3.0f - 2.0f * rate);
}

constexpr std::array<const char*, 8> kUpgradeDescriptions = {{
	"祈祷により最大HPを10上昇させます。\n打たれ強さを高めたい時におすすめです。",
	"祈祷により攻撃力を5%上昇させます。\nすべての攻撃で与えるダメージが増加します。",
	"祈祷により攻撃速度を5%上昇させます。\n攻撃の発動間隔が短くなり、手数が増加します。",
	"祈祷により防御力を2%上昇させます。\n受けるダメージを抑え、長く戦えるようになります。",
	"祈祷により移動速度を0.01上昇させます。\n敵との間合いを取りやすくなります。",
	"神域の加護で全員の獲得経験値を10%増やします。\nどの巫女を選んでも効果が適用されます。",
	"神域の加護で全員の獲得Gを10%増やします。\n次の祈祷に必要なお金を集めやすくなります。",
	"レベルアップ候補を1回だけ引き直せる加護です。\n一度授かれば、すべての巫女が利用できます。",
}};

/// <summary>表示、価格計算、購入効果が参照するショップ商品の単一定義です。</summary>
constexpr std::array<ShopUpgradeDefinition, 8> kUpgradeDefinitions = {{
	{"health", "最大HP", "", ShopUpgradeEffect::BaseHealth, 10.0f, 10, kNoFixedPrice},
	{"attack", "攻撃力", "%", ShopUpgradeEffect::AttackPercent, 5.0f, 10, kNoFixedPrice},
	{"attack_speed", "攻撃速度", "%", ShopUpgradeEffect::AttackSpeedPercent, 5.0f, 10, kNoFixedPrice},
	{"defense", "防御力", "%", ShopUpgradeEffect::DefensePercent, 2.0f, 10, kNoFixedPrice},
	{"speed", "移動速度", "", ShopUpgradeEffect::BaseSpeed, 0.01f, 10, kNoFixedPrice},
	{"experience_gain", "[全体] 獲得経験値", "%", ShopUpgradeEffect::ExperienceGain, 10.0f, 10, kNoFixedPrice},
	{"gold_gain", "[全体] 獲得G", "%", ShopUpgradeEffect::GoldGain, 10.0f, 10, kNoFixedPrice},
	{"level_up_reroll", "[全体] レベルアップリロール解放", "", ShopUpgradeEffect::LevelUpReroll, 0.0f, 1, 300},
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
using SceneUiUtility::DrawRotatedColorSprite;
}

void ShopScene::Initialize() {
	// ステータス強化を含むすべての商品は、選択キャラクターに依存しない共通強化として扱う。
	prayerPoseApplied_ = false;
	prayerEffectTime_ = -1.0f;
	// SceneManagerはInitialize直後にSHOP_objects.jsonを読むため、配置物が参照するモデルを先に登録する。
	ModelManager::GetInstance()->LoadModel("miko.gltf", true, "/human");
	ModelManager::GetInstance()->LoadModel("sand.obj", false, "/sand");
	ModelManager::GetInstance()->LoadModel("japanese_block.obj", false, "/stage2_japanese");
	ModelManager::GetInstance()->LoadModel("shop_shrine_hall.obj", false, "/shop_shrine");
	ModelManager::GetInstance()->LoadModel("shop_shrine_roof.obj", false, "/shop_shrine");
	ModelManager::GetInstance()->LoadModel("shop_torii_ornate.obj", false, "/shop_shrine");
	TextureManager::GetInstance()->LoadTexture("Resources/human/white.png");
	TextureManager::GetInstance()->LoadTexture("Resources/circle.png");
	backgroundSprite_ = CreateColorSprite();
	panelSprite_ = CreateColorSprite();
	dividerSprite_ = CreateColorSprite();
	shrineCaptionSprite_ = CreateColorSprite();
	detailPanelSprite_ = CreateColorSprite();
	for (auto& sprite : itemPanelSprites_) sprite = CreateColorSprite();
	for (auto& sprite : prayerTrailSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize("Resources/circle.png");
	}
	prayerOrbSprite_ = std::make_unique<Sprite>();
	prayerOrbSprite_->Initialize("Resources/circle.png");
	prayerOrbGlowSprite_ = std::make_unique<Sprite>();
	prayerOrbGlowSprite_->Initialize("Resources/circle.png");
	prayerSourceGlowSprite_ = std::make_unique<Sprite>();
	prayerSourceGlowSprite_->Initialize("Resources/circle.png");
	prayerBeamSprite_ = CreateColorSprite();
	prayerBeamCoreSprite_ = CreateColorSprite();
	for (auto& sprite : prayerPulseSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize("Resources/circle.png");
	}
	prayerArrivalGlowSprite_ = std::make_unique<Sprite>();
	prayerArrivalGlowSprite_->Initialize("Resources/circle.png");
	for (auto& sprite : prayerSparkSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize("Resources/circle.png");
	}
	for (auto& sprite : prayerLaserSprites_) sprite = CreateColorSprite();
	titleTextObject_ = SceneUiUtility::CreateHeadingText("奉納", 42.0f, {1.0f, 0.93f, 0.78f, 1.0f}, DWRITE_FONT_WEIGHT_EXTRA_BOLD);
	moneyTextObject_ = SceneUiUtility::CreateTextObject("", 22.0f, {1.0f, 0.80f, 0.25f, 1.0f}, kUiFontName);
	shrineCaptionTextObject_ = SceneUiUtility::CreateTextObject("神楽殿 － 祈祷中", 21.0f, {1.0f, 0.92f, 0.78f, 1.0f}, kUiFontName);
	categoryTextObjects_[0] = SceneUiUtility::CreateHeadingText("巫女の加護", 20.0f, {0.96f, 0.78f, 0.55f, 1.0f});
	categoryTextObjects_[1] = SceneUiUtility::CreateHeadingText("神域の加護", 20.0f, {0.96f, 0.78f, 0.55f, 1.0f});
	for (auto& item : itemTextObjects_) item = SceneUiUtility::CreateTextObject("", 19.0f, TextComponent::Anchor::Center, kUiFontName);
	detailTextObject_ = SceneUiUtility::CreateTextObject("", 19.0f, {0.90f, 0.91f, 0.87f, 1.0f}, kUiFontName);
	messageTextObject_ = SceneUiUtility::CreateTextObject("", 19.0f, {0.55f, 1.0f, 0.68f, 1.0f}, kUiFontName);
	instructionTextObject_ = SceneUiUtility::CreateTextObject(
	    "WASD・方向キー・Pad: 商品    Enter / Pad A: 授かる    Q / Pad B: 戻る", 16.0f,
	    {0.72f, 0.75f, 0.78f, 1.0f}, kUiFontName);
	RefreshText();
}

void ShopScene::RefreshText() {
	if (!sceneManager_) return;
	// 所持金と強化レベルは、どのキャラクターを選んでも同じ共通値を表示する。
	moneyTextObject_->GetComponent<TextComponent>()->SetText(
	    "所持金  " + std::to_string(sceneManager_->GetMoney()) + " G     対象  < 全キャラクター共通 >");
	for (size_t index = 0; index < itemTextObjects_.size(); ++index) {
		const ShopUpgradeDefinition& definition = kUpgradeDefinitions[index];
		const int level = sceneManager_->GetGlobalShopUpgradeLevel(definition.id);
		std::ostringstream stream;
		const int maxLevel = definition.maxLevel;
		stream << GetUpgradeDisplayName(definition) << "\n";
		if (definition.effect == ShopUpgradeEffect::LevelUpReroll) {
			stream << (level >= maxLevel ? "授与済み" : std::to_string(GetPrice(definition, level)) + " G");
		} else {
			stream << "Lv." << level << " / " << maxLevel;
			if (level < maxLevel) stream << "     " << GetPrice(definition, level) << " G";
			else stream << "     MAX";
		}
		TextComponent* text = itemTextObjects_[index]->GetComponent<TextComponent>();
		text->SetText(stream.str());
		text->SetColor(static_cast<int>(index) == selectedIndex_
		    ? Vector4{1.0f, 0.82f, 0.18f, 1.0f} : Vector4{0.88f, 0.92f, 1.0f, 1.0f});
	}
	detailTextObject_->GetComponent<TextComponent>()->SetText(
	    std::string("選択中　") + GetUpgradeDisplayName(kUpgradeDefinitions[static_cast<size_t>(selectedIndex_)]) +
	    "\n" + kUpgradeDescriptions[static_cast<size_t>(selectedIndex_)]);
	messageTextObject_->GetComponent<TextComponent>()->SetText(message_);
}

void ShopScene::UpdateShrinePresentation() {
	const float effectProgress = prayerEffectTime_ < 0.0f
	    ? 0.0f : std::clamp(prayerEffectTime_ / kPrayerEffectDuration, 0.0f, 1.0f);
	const float arrivalStrength = prayerEffectTime_ < 0.0f ? 0.0f :
	    SmoothStep(0.52f, 0.66f, effectProgress) *
	    (1.0f - SmoothStep(0.88f, 1.0f, effectProgress));
	for (Entity* object : GetMutableSceneObjects()) {
		if (!object) continue;
		Object3dComponent* object3d = object->GetComponent<Object3dComponent>();
		if (!object3d) continue;
		// 夕暮れの柔らかな光を全配置物へ統一して、朱塗りの鳥居と巫女を読み取りやすくする。
		object3d->SetDirectionalLight(
		    {1.0f, 0.78f, 0.58f, 1.0f}, Normalize(Vector3{0.38f, -0.72f, 0.58f}), 1.15f);
		const std::string& objectName = object->GetName();
		const bool isShrine = objectName.starts_with("Shrine") || objectName == "ToriiGoldPlaque";
		if (isShrine) {
			const bool isGold = objectName.find("Gold") != std::string::npos;
			const float baseIntensity = isGold ? 0.18f : 0.0f;
			object3d->SetEmission(
			    isGold ? Vector3{0.92f, 0.38f, 0.055f} : Vector3{1.0f, 0.28f, 0.035f},
			    baseIntensity + arrivalStrength * (isGold ? 1.8f : 1.15f));
		}
		if (object->GetName() != "ShopMiko" || prayerPoseApplied_) continue;

		object3d->ResetAnimationPoseToInitial();
		// 両腕を胸元へ寄せ、頭と上体を軽く伏せて祈りのシルエットを作る。
		const Quaternion leftArm = Normalize(Multiply(
		    MakeRotateAxisAngleQuaternion({0.0f, 0.0f, 1.0f}, 0.88f),
		    MakeRotateAxisAngleQuaternion({1.0f, 0.0f, 0.0f}, -0.46f)));
		const Quaternion rightArm = Normalize(Multiply(
		    MakeRotateAxisAngleQuaternion({0.0f, 0.0f, 1.0f}, -0.88f),
		    MakeRotateAxisAngleQuaternion({1.0f, 0.0f, 0.0f}, 0.46f)));
		const Quaternion spine = MakeRotateAxisAngleQuaternion({1.0f, 0.0f, 0.0f}, 0.14f);
		const Quaternion head = MakeRotateAxisAngleQuaternion({1.0f, 0.0f, 0.0f}, 0.24f);
		object3d->SetJointRotationOffset("arm.L", leftArm);
		object3d->SetJointRotationOffset("arm.R", rightArm);
		object3d->SetJointRotationOffset("spine", spine);
		object3d->SetJointRotationOffset("head", head);
		prayerPoseApplied_ = true;
	}
}

void ShopScene::DrawPrayerDeliveryEffect(float height, float panelX) {
	if (prayerEffectTime_ < 0.0f || !prayerOrbSprite_) return;
	const float progress = std::clamp(prayerEffectTime_ / kPrayerEffectDuration, 0.0f, 1.0f);
	const float travel = SmoothStep(0.04f, 0.61f, progress);
	const Vector2 start = {panelX * 0.52f, height * 0.67f};
	const Vector2 destination = {panelX * 0.40f, height * 0.28f};
	const float arc = std::sin(travel * 3.14159265f) * panelX * 0.13f;
	const Vector2 orbPosition = {
	    start.x + (destination.x - start.x) * travel - arc,
	    start.y + (destination.y - start.y) * travel};
	const float birth = SmoothStep(0.0f, 0.08f, progress) * (1.0f - SmoothStep(0.22f, 0.38f, progress));
	const float arrival = SmoothStep(0.52f, 0.66f, progress) * (1.0f - SmoothStep(0.88f, 1.0f, progress));

	// circle.png の黒地を光として加算しないことで、四角い縁を消して3D背景へ自然になじませる。
	SpriteCommon::GetInstance()->SetDraw(kBlendModeAdd);

	// 巫女の手元で一度ふくらんでから飛び出し、祈りの発生位置を明確にする。
	const float sourceSize = 42.0f + birth * 54.0f;
	DrawColorSprite(prayerSourceGlowSprite_.get(), start.x - sourceSize * 0.5f, start.y - sourceSize * 0.5f,
	    sourceSize, sourceSize, {1.0f, 0.33f, 0.035f, birth * 0.34f});

	// 軌跡より先に細い光路を敷き、後から重ねる粒の輪郭を保つ。
	if (progress > 0.22f && progress < 0.68f) {
		const float deltaX = destination.x - orbPosition.x;
		const float deltaY = destination.y - orbPosition.y;
		const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
		const float beamAlpha = std::sin(std::clamp((progress - 0.22f) / 0.46f, 0.0f, 1.0f) * 3.14159265f);
		DrawRotatedColorSprite(prayerBeamSprite_.get(), (destination.x + orbPosition.x) * 0.5f,
		    (destination.y + orbPosition.y) * 0.5f, length, 2.0f, std::atan2(deltaY, deltaX),
		    {1.0f, 0.38f, 0.025f, beamAlpha * 0.28f});
		DrawRotatedColorSprite(prayerBeamCoreSprite_.get(), (destination.x + orbPosition.x) * 0.5f,
		    (destination.y + orbPosition.y) * 0.5f, length, 0.8f, std::atan2(deltaY, deltaX),
		    {1.0f, 0.92f, 0.64f, beamAlpha * 0.72f});
	}

	// 時間差を付けた円を並べ、巫女の手元から拝殿まで伸びる光跡を作る。
	for (size_t index = 0; index < prayerTrailSprites_.size(); ++index) {
		const float delay = static_cast<float>(index + 1) * 0.055f;
		const float trailTravel = std::clamp(travel - delay, 0.0f, 1.0f);
		const float trailArc = std::sin(trailTravel * 3.14159265f) * panelX * 0.13f;
		const float alpha = SmoothStep(0.02f, 0.10f, progress) *
		    (1.0f - static_cast<float>(index) / static_cast<float>(prayerTrailSprites_.size())) *
		    (1.0f - SmoothStep(0.60f, 0.82f, progress));
		const float size = 20.0f - static_cast<float>(index) * 2.0f;
		DrawColorSprite(
		    prayerTrailSprites_[index].get(),
		    start.x + (destination.x - start.x) * trailTravel - trailArc - size * 0.5f,
		    start.y + (destination.y - start.y) * trailTravel - size * 0.5f,
		    size, size, {1.0f, 0.34f, 0.025f, alpha * 0.44f});
	}

	const float orbAlpha = SmoothStep(0.02f, 0.08f, progress) * (1.0f - SmoothStep(0.58f, 0.69f, progress));
	const float orbSize = 17.0f + std::sin(prayerEffectTime_ * 18.0f) * 2.0f;
	const float glowSize = orbSize * 3.2f;
	DrawColorSprite(prayerOrbGlowSprite_.get(), orbPosition.x - glowSize * 0.5f, orbPosition.y - glowSize * 0.5f,
	    glowSize, glowSize, {1.0f, 0.25f, 0.02f, orbAlpha * 0.22f});
	DrawColorSprite(prayerOrbSprite_.get(), orbPosition.x - orbSize * 0.5f, orbPosition.y - orbSize * 0.5f,
	    orbSize, orbSize, {1.0f, 0.74f, 0.22f, orbAlpha * 0.88f});

	// 到達点には一瞬強い白金色を置き、その後ゆっくり消える余韻を残す。
	const float arrivalGlowSize = 58.0f + arrival * 82.0f;
	DrawColorSprite(prayerArrivalGlowSprite_.get(), destination.x - arrivalGlowSize * 0.5f,
	    destination.y - arrivalGlowSize * 0.5f, arrivalGlowSize, arrivalGlowSize,
	    {1.0f, 0.48f, 0.07f, arrival * 0.32f});

	// 到達後は二重の光輪を時間差で広げ、祈りが神社へ受け取られたことを示す。
	for (size_t index = 0; index < prayerPulseSprites_.size(); ++index) {
		const float pulse = SmoothStep(0.40f + static_cast<float>(index) * 0.10f,
		    0.92f + static_cast<float>(index) * 0.06f, progress);
		const float pulseAlpha = std::sin(pulse * 3.14159265f) * 0.30f;
		const float pulseSize = 34.0f + pulse * (116.0f + static_cast<float>(index) * 34.0f);
		DrawColorSprite(prayerPulseSprites_[index].get(), destination.x - pulseSize * 0.5f,
		    destination.y - pulseSize * 0.5f, pulseSize, pulseSize,
		    {1.0f, 0.30f + static_cast<float>(index) * 0.10f, 0.025f, pulseAlpha});
	}

	// 光輪の外へ小さな火花を散らし、到達の勢いと豪華さを足す。
	const float sparkLife = SmoothStep(0.50f, 0.65f, progress) * (1.0f - SmoothStep(0.78f, 0.98f, progress));
	for (size_t index = 0; index < prayerSparkSprites_.size(); ++index) {
		const float angle = -1.40f + static_cast<float>(index) * 1.12f;
		const float distance = (20.0f + static_cast<float>(index % 2) * 9.0f) + progress * 68.0f;
		const float sparkSize = 7.0f + static_cast<float>(index % 3) * 2.0f;
		const float sparkX = destination.x + std::cos(angle) * distance;
		const float sparkY = destination.y + std::sin(angle) * distance;
		DrawColorSprite(prayerSparkSprites_[index].get(), sparkX - sparkSize * 0.5f, sparkY - sparkSize * 0.5f,
		    sparkSize, sparkSize, {1.0f, 0.58f, 0.10f, sparkLife * 0.55f});
	}

	// 到達直後に四方向のレーザーを伸ばす。太い朱金色の外光へ細い白芯を重ねて切れ味を出す。
	const float laserLife = SmoothStep(0.53f, 0.62f, progress) *
	    (1.0f - SmoothStep(0.72f, 0.90f, progress));
	const float laserLength = (38.0f + SmoothStep(0.53f, 0.72f, progress) * 142.0f) * laserLife;
	const float laserRotation = prayerEffectTime_ * 0.55f;
	for (size_t ray = 0; ray < 4; ++ray) {
		const float angle = laserRotation + 0.42f + static_cast<float>(ray) * 1.57079633f;
		const float centerX = destination.x + std::cos(angle) * laserLength * 0.5f;
		const float centerY = destination.y + std::sin(angle) * laserLength * 0.5f;
		DrawRotatedColorSprite(prayerLaserSprites_[ray * 2].get(), centerX, centerY,
		    laserLength, 7.0f, angle, {1.0f, 0.20f, 0.018f, laserLife * 0.24f});
		DrawRotatedColorSprite(prayerLaserSprites_[ray * 2 + 1].get(), centerX, centerY,
		    laserLength, 1.5f, angle, {1.0f, 0.94f, 0.72f, laserLife * 0.88f});
	}

	// 後続のショップUIは通常合成へ戻し、文字やパネルの色を変化させない。
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
}

void ShopScene::PurchaseSelectedUpgrade() {
	if (!sceneManager_) return;
	const ShopUpgradeDefinition& definition = kUpgradeDefinitions[static_cast<size_t>(selectedIndex_)];
	const char* upgradeId = definition.id;
	const int level = sceneManager_->GetGlobalShopUpgradeLevel(upgradeId);
	const int maxLevel = definition.maxLevel;
	if (level >= maxLevel) {
		message_ = "この強化は最大レベルです";
		RefreshText();
		return;
	}
	const int price = GetPrice(definition, level);
	const ShopPurchaseResult purchaseResult = sceneManager_->PurchaseGlobalShopUpgrade(upgradeId, price, maxLevel);
	if (purchaseResult != ShopPurchaseResult::Purchased) {
		message_ = purchaseResult == ShopPurchaseResult::InsufficientFunds
			? "お金が足りません"
			: purchaseResult == ShopPurchaseResult::SaveFailed
				? "保存に失敗したため購入を取り消しました"
				: "購入できませんでした";
		RefreshText();
		return;
	}
	prayerEffectTime_ = 0.0f;
	message_ = GetUpgradeDisplayName(definition) + " を購入しました";
	RefreshText();
}

void ShopScene::Update() {
	if (!sceneManager_) return;
	if (prayerEffectTime_ >= 0.0f) {
		prayerEffectTime_ += GameTime::GetDeltaTime();
		if (prayerEffectTime_ >= kPrayerEffectDuration) prayerEffectTime_ = -1.0f;
	}
	UpdateShrinePresentation();
	Input* input = Input::GetInstance();
	if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP) || input->TriggerGamepadUp()) {
		// 左右それぞれの列内で循環し、見た目どおりの上下移動にする。
		selectedIndex_ = selectedIndex_ < 5
		    ? (selectedIndex_ + 4) % 5
		    : 5 + (selectedIndex_ - 5 + 2) % 3;
		message_.clear();
		RefreshText();
	}
	if (input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN) || input->TriggerGamepadDown()) {
		selectedIndex_ = selectedIndex_ < 5
		    ? (selectedIndex_ + 1) % 5
		    : 5 + (selectedIndex_ - 5 + 1) % 3;
		message_.clear();
		RefreshText();
	}
	if (input->TriggerKey(DIK_A) || input->TriggerKey(DIK_LEFT) || input->TriggerGamepadLeft()) {
		if (selectedIndex_ >= 5) selectedIndex_ -= 5;
		message_.clear();
		RefreshText();
	}
	if (input->TriggerKey(DIK_D) || input->TriggerKey(DIK_RIGHT) || input->TriggerGamepadRight()) {
		if (selectedIndex_ < 5) selectedIndex_ = 5 + (std::min)(selectedIndex_, 2);
		message_.clear();
		RefreshText();
	}
	if (input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) ||
	    input->TriggerGamepadButton(XINPUT_GAMEPAD_A)) PurchaseSelectedUpgrade();
	if (input->TriggerKey(DIK_Q) || input->TriggerGamepadButton(XINPUT_GAMEPAD_B)) sceneManager_->ChangeScene("TITLE");
}

void ShopScene::Draw2D() {
	DirectXCommon* dxCommon = SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon || !backgroundSprite_ || !panelSprite_ || !dividerSprite_) return;
	const float width = static_cast<float>(dxCommon->GetRenderWidth());
	const float height = static_cast<float>(dxCommon->GetRenderHeight());
	const float panelX = width * 0.445f;
	const float panelY = 24.0f;
	const float panelWidth = width - panelX - 24.0f;
	const float panelHeight = height - 48.0f;
	const float contentX = panelX + 24.0f;
	const float contentWidth = panelWidth - 48.0f;
	const float columnGap = 12.0f;
	const float cardWidth = (contentWidth - columnGap) * 0.5f;
	const float cardHeight = std::clamp((panelHeight - 372.0f) * 0.2f, 42.0f, 68.0f);
	const float gridTop = panelY + 142.0f;
	const float rowGap = 9.0f;
	SpriteCommon::GetInstance()->SetDraw(kBlendModeNormal);
	// 3D情景は残しつつ、端を暗くして右側UIへ視線が自然に移るようにする。
	DrawColorSprite(backgroundSprite_.get(), 0.0f, 0.0f, width, height, {0.015f, 0.012f, 0.018f, 0.22f});
	DrawColorSprite(panelSprite_.get(), panelX, panelY, panelWidth, panelHeight, {0.055f, 0.045f, 0.050f, 0.96f});
	DrawColorSprite(dividerSprite_.get(), panelX, panelY, 4.0f, panelHeight, {0.78f, 0.12f, 0.055f, 0.95f});
	DrawColorSprite(shrineCaptionSprite_.get(), 24.0f, height - 94.0f, panelX - 48.0f, 54.0f, {0.08f, 0.035f, 0.025f, 0.78f});
	DrawPrayerDeliveryEffect(height, panelX);

	titleTextObject_->GetTransform().translate = {panelX + panelWidth * 0.5f, panelY + 38.0f, 0.0f};
	moneyTextObject_->GetTransform().translate = {panelX + panelWidth * 0.5f, panelY + 83.0f, 0.0f};
	shrineCaptionTextObject_->GetTransform().translate = {panelX * 0.5f, height - 67.0f, 0.0f};
	categoryTextObjects_[0]->GetTransform().translate = {contentX + cardWidth * 0.5f, panelY + 121.0f, 0.0f};
	categoryTextObjects_[1]->GetTransform().translate = {contentX + cardWidth + columnGap + cardWidth * 0.5f, panelY + 121.0f, 0.0f};

	for (size_t index = 0; index < itemTextObjects_.size(); ++index) {
		const bool isDivineBlessing = index >= 5;
		const int row = isDivineBlessing ? static_cast<int>(index) - 5 : static_cast<int>(index);
		const float cardX = contentX + (isDivineBlessing ? cardWidth + columnGap : 0.0f);
		const float cardY = gridTop + row * (cardHeight + rowGap);
		const bool isSelected = static_cast<int>(index) == selectedIndex_;
		DrawColorSprite(itemPanelSprites_[index].get(), cardX, cardY, cardWidth, cardHeight,
		    isSelected ? Vector4{0.32f, 0.095f, 0.045f, 0.98f} : Vector4{0.105f, 0.095f, 0.10f, 0.94f});
		itemTextObjects_[index]->GetTransform().translate = {cardX + cardWidth * 0.5f, cardY + cardHeight * 0.5f, 0.0f};
	}

	const float detailY = gridTop + 5.0f * (cardHeight + rowGap) + 5.0f;
	const float detailHeight = (std::max)(104.0f, panelY + panelHeight - detailY - 76.0f);
	DrawColorSprite(detailPanelSprite_.get(), contentX, detailY, contentWidth, detailHeight, {0.075f, 0.068f, 0.072f, 0.98f});
	detailTextObject_->GetTransform().translate = {panelX + panelWidth * 0.5f, detailY + detailHeight * 0.42f, 0.0f};
	messageTextObject_->GetTransform().translate = {panelX + panelWidth * 0.5f, detailY + detailHeight - 20.0f, 0.0f};
	instructionTextObject_->GetTransform().translate = {panelX + panelWidth * 0.5f, panelY + panelHeight - 25.0f, 0.0f};
	titleTextObject_->Draw2D();
	moneyTextObject_->Draw2D();
	shrineCaptionTextObject_->Draw2D();
	for (auto& category : categoryTextObjects_) category->Draw2D();
	for (auto& item : itemTextObjects_) item->Draw2D();
	detailTextObject_->Draw2D();
	messageTextObject_->Draw2D();
	instructionTextObject_->Draw2D();
}

void ShopScene::Finalize() {
	backgroundSprite_.reset(); panelSprite_.reset(); dividerSprite_.reset(); shrineCaptionSprite_.reset(); detailPanelSprite_.reset();
	for (auto& sprite : itemPanelSprites_) sprite.reset();
	for (auto& sprite : prayerTrailSprites_) sprite.reset();
	prayerOrbSprite_.reset();
	prayerOrbGlowSprite_.reset();
	prayerSourceGlowSprite_.reset();
	prayerBeamSprite_.reset();
	prayerBeamCoreSprite_.reset();
	for (auto& sprite : prayerPulseSprites_) sprite.reset();
	prayerArrivalGlowSprite_.reset();
	for (auto& sprite : prayerSparkSprites_) sprite.reset();
	for (auto& sprite : prayerLaserSprites_) sprite.reset();
	titleTextObject_.reset(); moneyTextObject_.reset(); shrineCaptionTextObject_.reset();
	for (auto& category : categoryTextObjects_) category.reset();
	for (auto& item : itemTextObjects_) item.reset();
	detailTextObject_.reset(); messageTextObject_.reset(); instructionTextObject_.reset();
	prayerPoseApplied_ = false;
	prayerEffectTime_ = -1.0f;
	BaseScene::Finalize();
}
