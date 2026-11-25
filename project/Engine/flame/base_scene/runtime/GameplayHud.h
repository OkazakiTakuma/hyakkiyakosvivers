#pragma once

#include "Sprite.h"
#include "core/Entity.h"
#include "data/PlayerStatsData.h"

#include <array>
#include <memory>
#include <string>

class PlayerComponent;

/// <summary>戦闘HUDのリソース、表示状態、レイアウト更新と描画を所有します。</summary>
class GameplayHud final {
public:
	/// <summary>生成済みHUDリソースと表示状態を初期化します。</summary>
	void Reset();
	/// <summary>プレイヤー状態と獲得Gから各HUDの表示内容を更新します。</summary>
	void Update(PlayerComponent* player, bool showChallengeMoney, int challengeMoney);
	/// <summary>現在表示対象となっている戦闘HUDを描画します。</summary>
	void Draw();
	/// <summary>攻撃スロットの画像キャッシュを破棄し、次回更新時に再読込させます。</summary>
	void InvalidateAttackTextures();
	/// <summary>ステータススロットの画像キャッシュを破棄し、次回更新時に再読込させます。</summary>
	void InvalidateStatusTextures();

private:
	void UpdatePlayerHealth(PlayerComponent* player);
	void UpdateChallengeMoney(bool visible, int challengeMoney);
	void UpdatePlayerExperience(PlayerComponent* player);
	void UpdatePlayerSlots(PlayerComponent* player);
	void DrawChallengeMoney();
	void DrawPlayerExperience();
	void DrawPlayerSlots();

	// HUD用SpriteとEntityはGameplayHudが所有し、BaseSceneから描画状態の詳細を隠す。
	std::unique_ptr<Sprite> playerHealthBarBackground_;
	std::unique_ptr<Sprite> playerHealthBarFill_;
	bool isPlayerHealthHudVisible_ = false;
	std::unique_ptr<Entity> challengeMoneyTextObject_;
	bool isChallengeMoneyHudVisible_ = false;
	std::unique_ptr<Sprite> playerExperienceBarBackground_;
	std::unique_ptr<Sprite> playerExperienceBarFill_;
	std::unique_ptr<Entity> playerExperienceTextObject_;
	float playerExperienceRate_ = 0.0f;
	bool isPlayerExperienceHudVisible_ = false;
	std::array<std::unique_ptr<Sprite>, kPlayerEquipmentSlotCount> playerAttackSlotBackgroundSprites_;
	std::array<std::unique_ptr<Sprite>, kPlayerEquipmentSlotCount> playerAttackSlotIconSprites_;
	std::array<std::unique_ptr<Sprite>, kPlayerEquipmentSlotCount> playerStatusSlotBackgroundSprites_;
	std::array<std::unique_ptr<Sprite>, kPlayerEquipmentSlotCount> playerStatusSlotIconSprites_;
	std::array<std::unique_ptr<Sprite>, kPlayerEquipmentSlotCount> playerAttackSlotLevelBackgroundSprites_;
	std::array<std::unique_ptr<Sprite>, kPlayerEquipmentSlotCount> playerStatusSlotLevelBackgroundSprites_;
	std::array<std::unique_ptr<Entity>, kPlayerEquipmentSlotCount> playerAttackSlotLevelTextObjects_;
	std::array<std::unique_ptr<Entity>, kPlayerEquipmentSlotCount> playerStatusSlotLevelTextObjects_;
	std::array<bool, kPlayerEquipmentSlotCount> playerAttackSlotIconVisible_{};
	std::array<bool, kPlayerEquipmentSlotCount> playerStatusSlotIconVisible_{};
	std::array<bool, kPlayerEquipmentSlotCount> playerAttackSlotLevelVisible_{};
	std::array<bool, kPlayerEquipmentSlotCount> playerStatusSlotLevelVisible_{};
	std::array<std::string, kPlayerEquipmentSlotCount> playerAttackSlotTextureKeys_{};
	std::array<std::string, kPlayerEquipmentSlotCount> playerAttackSlotTexturePaths_{};
	std::array<std::string, kPlayerEquipmentSlotCount> playerStatusSlotTextureKeys_{};
	std::array<std::string, kPlayerEquipmentSlotCount> playerStatusSlotTexturePaths_{};
	std::unique_ptr<Entity> playerAttackSlotLabelObject_;
	std::unique_ptr<Entity> playerStatusSlotLabelObject_;
	bool isPlayerSlotHudVisible_ = false;
};
