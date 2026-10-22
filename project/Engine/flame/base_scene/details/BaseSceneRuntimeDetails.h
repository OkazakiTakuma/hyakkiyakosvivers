#pragma once

// BaseScene の実行時処理を複数の翻訳単位へ分割するための内部実装です。
// flame 外部から直接参照しないでください。

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

namespace {
// 新規取得の除外条件を通常レベルアップとボス報酬で共有する。装備済み武器の強化には使用しない。
std::unordered_set<std::string> GetOtherPlayerInitialAttacks(const Player& player) {
	std::unordered_set<std::string> excludedAttacks;
	for (const std::string& playerTypeName : LoadPlayerTypeNames()) {
		if (playerTypeName == player.GetPlayerTypeName()) {
			continue;
		}
		const PlayerStats otherPlayerStats = LoadPlayerStats(playerTypeName);
		for (const PlayerAttackSlot& slot : otherPlayerStats.attackSlots) {
			if (slot.enabled && !slot.attackName.empty()) {
				excludedAttacks.insert(slot.attackName);
			}
		}
	}
	return excludedAttacks;
}

// Straight専用の描画。移動・当たり判定には触れず、弾の現在位置に光の円弧を重ねる。
class StraightSlashVisualComponent final : public Component {
public:
	/// <summary>弾と同じ寿命で残光を減衰させる描画専用コンポーネントを生成します。</summary>
	explicit StraightSlashVisualComponent(float lifeTime) : remainingLifeTime_(lifeTime) {}

	/// <summary>フェード用の残り時間を更新します。弾の移動・消滅判定は変更しません。</summary>
	void Update() override {
		remainingLifeTime_ -= GameTime::GetDeltaTime();
	}

	/// <summary>弾の向きに合わせた三日月状の帯を重ね、斬撃と残光を描画します。</summary>
	void Draw3D() override {
		GameObject* owner = GetOwner();
		if (!owner) {
			return;
		}
		const auto* projectile = owner->GetComponent<PlayerProjectileComponent>();
		// 命中などで弾が終了した場合は、フェード用の時間が残っていても描画しない。
		if (!projectile || !projectile->IsEnabled() || projectile->IsExpired()) {
			return;
		}

		const auto& transform = owner->GetTransform();
		const float size = projectile->GetSize();
		const float yaw = transform.rotate.y;
		// 弾のY軸回転から前方・右方向を求め、発射方向が変わっても刃先を前へ向ける。
		const Vector3 forward{std::sin(yaw), 0.0f, std::cos(yaw)};
		const Vector3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
		// 寿命の最後0.18秒だけ透明度を落とす。形状のサイズや当たり判定は縮めない。
		const float fade = (std::clamp)(remainingLifeTime_ / 0.18f, 0.0f, 1.0f);

		// lifeRateを円弧の両端で0にし、中央は太く、切っ先は鋭く絞る。
		// 帯自体はカメラを向くため、水平に飛んでも刃の厚みを視認できる。
		constexpr int kSegments = 24;
		std::vector<TrailRenderPoint> points;
		points.reserve(kSegments + 1);
		// behindは後方へのずれ、spanは横の広がり、widthは帯の太さ（いずれも弾サイズ基準）。
		// 同じ点配列を各レイヤーで再利用する。Submit側が点列をコピーして保持する。
		auto submitArc = [&](float behind, float span, float width, Vector4 color) {
			points.clear();
			for (int i = 0; i <= kSegments; ++i) {
				const float t = static_cast<float>(i) / kSegments;
				// -1.25～+1.25ラジアンの円弧を使い、中央が前へ張り出す形状にする。
				const float angle = (t - 0.5f) * 2.5f;
				const float lateral = std::sin(angle) * span;
				const float depth = (std::cos(angle) - 0.60f) * 0.85f - behind;
				// ローカルの横・奥行きをワールド座標へ変換し、左右に高低差を付けて刃を少し傾ける。
				const Vector3 position = transform.translate + (size * lateral) * right +
				    (size * depth) * forward + Vector3{0.0f, size * lateral * 0.20f, 0.0f};
				// 中央で1、両端で0となる係数を帯幅と透明度に使う。端点は誤差を避けて明示的に0にする。
				const float taper = (i == 0 || i == kSegments) ? 0.0f : std::sin(MathConstants::kPi * t);
				points.push_back({position, taper});
			}
			color.w *= fade;
			TrailRenderer::GetInstance()->Submit(points, size * width, color, {color.x, color.y, color.z, 0.0f});
		};

		// 後方の薄い残光、青い外光、白熱した刃先の順に加算合成する。
		// 残光は移動履歴ではなく後方へずらした円弧なので、発射直後から形が完成している。
		submitArc(0.42f, 0.90f, 0.22f, {0.10f, 0.48f, 1.0f, 0.20f});
		submitArc(0.22f, 1.00f, 0.32f, {0.12f, 0.72f, 1.0f, 0.32f});
		submitArc(0.00f, 1.10f, 0.48f, {0.18f, 0.80f, 1.0f, 0.52f});
		// 細い白色の帯だけ少し前へ出し、青い外光の中でも刃先を識別できるようにする。
		submitArc(-0.035f, 1.10f, 0.12f, {0.85f, 1.0f, 1.0f, 1.0f});
	}

private:
	/// <summary>透明度の減衰にだけ使用する残り秒数です。弾の寿命はPlayerProjectileComponentが管理します。</summary>
	float remainingLifeTime_ = 0.0f;
};

// 五行結界専用の描画。攻撃判定とは独立して、プレイヤーの足元に五芒星の結界を展開する。
class PoisonBarrierVisualComponent final : public Component {
public:
	PoisonBarrierVisualComponent(float lifeTime, float centerHeight)
	    : initialLifeTime_((std::max)(0.01f, lifeTime)), remainingLifeTime_(initialLifeTime_),
	      centerHeight_(centerHeight) {}

	void Update() override {
		const float deltaTime = GameTime::GetDeltaTime();
		remainingLifeTime_ -= deltaTime;
		elapsedSeconds_ += deltaTime;
	}

	void Draw3D() override {
		GameObject* owner = GetOwner();
		const auto* projectile = owner ? owner->GetComponent<PlayerProjectileComponent>() : nullptr;
		if (!owner || !projectile || !projectile->IsEnabled() || projectile->IsExpired()) {
			return;
		}

		// 判定側が毎フレーム参照している値をそのまま使い、強化や寿命演出でサイズが変化しても
		// 外周表示と実際の攻撃半径がずれないようにする。
		const float hitRadius = projectile->GetSize();
		const float appear = SmoothStep((std::clamp)(elapsedSeconds_ / 0.22f, 0.0f, 1.0f));
		const float lifeRate = (std::clamp)(remainingLifeTime_ / initialLifeTime_, 0.0f, 1.0f);
		const float fade = SmoothStep((std::clamp)(lifeRate / 0.20f, 0.0f, 1.0f));
		const float envelope = appear * fade;
		const float pulse = 0.92f + 0.08f * std::sin(elapsedSeconds_ * 4.5f);
		const float rotation = elapsedSeconds_ * 0.34f;
		const Vector3 center = owner->GetTransform().translate + Vector3{0.0f, -centerHeight_ + 0.08f, 0.0f};

		auto pointOnCircle = [&](float circleRadius, float angle, float height = 0.0f) {
			return center + Vector3{std::cos(angle) * circleRadius, height, std::sin(angle) * circleRadius};
		};
		auto submitPath = [&](const std::vector<TrailRenderPoint>& points, float width, Vector4 color, float alphaRate = 1.0f) {
			color.w *= alphaRate;
			TrailRenderer::GetInstance()->Submit(points, width, color, {color.x * 0.35f, color.y * 0.55f, color.z * 0.35f, 0.0f});
		};
		auto submitRing = [&](float circleRadius, float height, float width, Vector4 color, float angleOffset, float alphaRate = 1.0f) {
			constexpr int kSegments = 64;
			std::vector<TrailRenderPoint> ring;
			ring.reserve(kSegments + 1);
			for (int index = 0; index <= kSegments; ++index) {
				const float angle = MathConstants::kTwoPi * static_cast<float>(index) / kSegments + angleOffset;
				ring.push_back({pointOnCircle(circleRadius, angle, height), alphaRate});
			}
			submitPath(ring, width, color, alphaRate);
		};

		// 最外周は実際の攻撃半径へ置き、判定が有効な間はフェードさせず常に表示する。
		// 内側の紋様だけを出現・消滅させることで、演出中も当たり判定の境界を見失わないようにする。
		submitRing(hitRadius, 0.00f, hitRadius * 0.030f, {0.70f, 1.00f, 0.82f, 0.90f}, rotation);
		submitRing(hitRadius * 0.84f, 0.025f, hitRadius * 0.025f, {0.58f, 1.00f, 0.78f, 0.74f * pulse}, -rotation * 0.72f, envelope);
		submitRing(hitRadius * 0.54f, 0.045f, hitRadius * 0.018f, {0.18f, 0.78f, 0.54f, 0.48f}, rotation * 1.35f, envelope);

		// 円へ内接する五点を2つ飛ばしで結び、中央に五芒星を描く。
		std::array<Vector3, 5> starVertices{};
		for (int index = 0; index < 5; ++index) {
			const float angle = -MathConstants::kPi * 0.5f + MathConstants::kTwoPi * static_cast<float>(index) / 5.0f + rotation;
			starVertices[index] = pointOnCircle(hitRadius * 0.72f, angle, 0.055f);
		}
		constexpr std::array<int, 6> kStarOrder = {0, 2, 4, 1, 3, 0};
		std::vector<TrailRenderPoint> star;
		star.reserve(kStarOrder.size());
		for (int vertexIndex : kStarOrder) {
			star.push_back({starVertices[vertexIndex], envelope});
		}
		submitPath(star, hitRadius * 0.050f, {0.70f, 1.00f, 0.82f, 0.88f * pulse}, envelope);
		// 白い細線を重ねて五芒星の輪郭をくっきりさせる。
		submitPath(star, hitRadius * 0.015f, {0.90f, 1.00f, 0.94f, 0.98f}, envelope);

		// 五つの頂点を護符の核として光らせ、上部の小さな環まで光柱でつなぐ。
		for (int index = 0; index < 5; ++index) {
			const Vector3 base = starVertices[index];
			const float pillarHeight = hitRadius * (0.50f + 0.06f * std::sin(elapsedSeconds_ * 3.2f + index));
			std::vector<TrailRenderPoint> pillar = {
				{base, envelope},
				{base + Vector3{0.0f, pillarHeight, 0.0f}, 0.0f}
			};
			submitPath(pillar, hitRadius * 0.025f, {0.20f, 1.00f, 0.58f, 0.58f}, envelope);

			constexpr int kSealSegments = 16;
			std::vector<TrailRenderPoint> seal;
			seal.reserve(kSealSegments + 1);
			for (int segment = 0; segment <= kSealSegments; ++segment) {
				const float sealAngle = MathConstants::kTwoPi * static_cast<float>(segment) / kSealSegments - rotation * 2.0f;
				seal.push_back({base + Vector3{std::cos(sealAngle) * hitRadius * 0.075f, 0.035f, std::sin(sealAngle) * hitRadius * 0.075f}, envelope});
			}
			submitPath(seal, hitRadius * 0.018f, {0.68f, 1.00f, 0.76f, 0.82f * pulse}, envelope);
		}

		// 頭上の薄い環が五本の光柱を束ね、守護結界としての立体感を作る。
		submitRing(hitRadius * 0.38f, hitRadius * 0.52f, hitRadius * 0.020f, {0.42f, 1.00f, 0.68f, 0.35f}, -rotation * 1.6f, envelope);
	}

private:
	static float SmoothStep(float value) {
		return value * value * (3.0f - 2.0f * value);
	}

	float initialLifeTime_ = 0.01f;
	float remainingLifeTime_ = 0.01f;
	float elapsedSeconds_ = 0.0f;
	float centerHeight_ = 0.0f;
};

// FlameField専用の描画。円形の攻撃範囲全体へ複数の炎柱を配置し、燃えている領域を表現する。
class FlameFieldVisualComponent final : public Component {
public:
	FlameFieldVisualComponent(float lifeTime, float radius, float centerHeight)
	    : initialLifeTime_((std::max)(0.01f, lifeTime)), remainingLifeTime_(initialLifeTime_),
	      radius_((std::max)(0.1f, radius)), centerHeight_(centerHeight) {}

	void Update() override {
		const float deltaTime = GameTime::GetDeltaTime();
		remainingLifeTime_ -= deltaTime;
		elapsedSeconds_ += deltaTime;
	}

	void Draw3D() override {
		GameObject* owner = GetOwner();
		const auto* projectile = owner ? owner->GetComponent<PlayerProjectileComponent>() : nullptr;
		if (!owner || !projectile || !projectile->IsEnabled() || projectile->IsExpired()) {
			return;
		}

		const float appear = SmoothStep((std::clamp)(elapsedSeconds_ / 0.16f, 0.0f, 1.0f));
		const float lifeRate = (std::clamp)(remainingLifeTime_ / initialLifeTime_, 0.0f, 1.0f);
		const float fade = SmoothStep((std::clamp)(lifeRate / 0.22f, 0.0f, 1.0f));
		const float envelope = appear * fade;
		const Vector3 center = owner->GetTransform().translate + Vector3{0.0f, -centerHeight_ + 0.06f, 0.0f};

		auto submitFlame = [&](const std::vector<TrailRenderPoint>& points, float width, Vector4 head, Vector4 tail) {
			head.w *= envelope;
			tail.w *= envelope;
			TrailRenderer::GetInstance()->Submit(points, width, head, tail);
		};

		// 燃焼範囲の境界を赤熱した円として表示し、実際の当たり判定半径を読み取りやすくする。
		constexpr int kRingSegments = 40;
		std::vector<TrailRenderPoint> ring;
		ring.reserve(kRingSegments + 1);
		for (int segment = 0; segment <= kRingSegments; ++segment) {
			const float angle = MathConstants::kTwoPi * static_cast<float>(segment) / kRingSegments;
			const float pulseRadius = radius_ * (0.94f + 0.025f * std::sin(elapsedSeconds_ * 5.0f + angle * 3.0f));
			ring.push_back({center + Vector3{std::cos(angle) * pulseRadius, 0.0f, std::sin(angle) * pulseRadius}, envelope});
		}
		submitFlame(ring, radius_ * 0.055f, {1.00f, 0.32f, 0.02f, 0.72f}, {0.50f, 0.01f, 0.00f, 0.0f});

		// 黄金角で炎を円内へ均等に散らし、時間が経っても配置が一方向へ偏らないようにする。
		constexpr int kFlameCount = 8;
		constexpr int kFlameSegments = 4;
		constexpr float kGoldenAngle = 2.39996323f;
		for (int flameIndex = 0; flameIndex < kFlameCount; ++flameIndex) {
			const float normalizedRadius = std::sqrt((static_cast<float>(flameIndex) + 0.45f) / kFlameCount);
			const float baseAngle = flameIndex * kGoldenAngle + elapsedSeconds_ * 0.10f;
			const float radialDistance = radius_ * normalizedRadius * 0.78f;
			const Vector3 base = center + Vector3{
				std::cos(baseAngle) * radialDistance,
				0.0f,
				std::sin(baseAngle) * radialDistance};
			const float phase = flameIndex * 1.73f;
			const float flameHeight = radius_ * (0.55f + 0.16f * std::sin(elapsedSeconds_ * 4.8f + phase));
			const Vector3 swayDirection{std::cos(baseAngle + 1.1f), 0.0f, std::sin(baseAngle + 1.1f)};

			std::vector<TrailRenderPoint> flame;
			flame.reserve(kFlameSegments + 1);
			for (int segment = 0; segment <= kFlameSegments; ++segment) {
				const float t = static_cast<float>(segment) / kFlameSegments;
				const float taper = 1.0f - t;
				const float sway = std::sin(elapsedSeconds_ * 8.0f + phase + t * 4.2f) * radius_ * 0.10f * t;
				flame.push_back({base + t * flameHeight * Vector3{0.0f, 1.0f, 0.0f} + sway * swayDirection, taper * envelope});
			}

			// 赤い外炎へ細い黄白色の芯を重ね、範囲内が高温で燃えているように見せる。
			submitFlame(flame, radius_ * 0.16f, {1.00f, 0.18f, 0.01f, 0.78f}, {0.42f, 0.00f, 0.00f, 0.0f});
			submitFlame(flame, radius_ * 0.060f, {1.00f, 0.92f, 0.24f, 0.96f}, {1.00f, 0.12f, 0.00f, 0.0f});
		}
	}

private:
	static float SmoothStep(float value) {
		return value * value * (3.0f - 2.0f * value);
	}

	float initialLifeTime_ = 0.01f;
	float remainingLifeTime_ = 0.01f;
	float elapsedSeconds_ = 0.0f;
	float radius_ = 1.0f;
	float centerHeight_ = 0.0f;
};

// SkyLaser専用の描画。既存の投射物寿命と当たり判定を保ったまま、複数の加算帯で光柱を構成する。
class SkyLaserVisualComponent final : public Component {
public:
	SkyLaserVisualComponent(float lifeTime, float size)
	    : initialLifeTime_((std::max)(0.01f, lifeTime)), remainingLifeTime_(initialLifeTime_), size_((std::max)(0.05f, size)) {}

	void Update() override {
		remainingLifeTime_ -= GameTime::GetDeltaTime();
		elapsedSeconds_ += GameTime::GetDeltaTime();
	}

	void Draw3D() override {
		GameObject* owner = GetOwner();
		const auto* projectile = owner ? owner->GetComponent<PlayerProjectileComponent>() : nullptr;
		if (!owner || !projectile || projectile->IsExpired()) {
			return;
		}

		const float lifeRate = (std::clamp)(remainingLifeTime_ / initialLifeTime_, 0.0f, 1.0f);
		const float appear = SmoothStep((std::clamp)(elapsedSeconds_ / 0.075f, 0.0f, 1.0f));
		const float fade = SmoothStep((std::clamp)(lifeRate / 0.32f, 0.0f, 1.0f));
		const float envelope = appear * fade;
		const float pulse = 0.90f + 0.10f * std::sin(elapsedSeconds_ * 52.0f);
		const Vector3 center = owner->GetTransform().translate;
		// BaseSceneは標的の3m上を弾の中心としているため、着地点を元の敵の高さへ戻す。
		const Vector3 impact{center.x, center.y - 3.0f, center.z};
		const Vector3 sky{center.x, center.y + 8.0f + size_ * 2.0f, center.z};

		auto submitBeam = [&](float width, const Vector4& color) {
			std::vector<TrailRenderPoint> points = {{impact, envelope}, {sky, envelope}};
			Vector4 head = color;
			head.w *= envelope;
			TrailRenderer::GetInstance()->Submit(points, size_ * width * pulse, head, head);
		};

		// 太い低輝度の外光から細い白熱コアへ重ね、輪郭を潰さず強い発光に見せる。
		submitBeam(2.90f, {0.03f, 0.30f, 1.00f, 0.11f});
		submitBeam(1.85f, {0.05f, 0.70f, 1.00f, 0.24f});
		submitBeam(0.92f, {0.32f, 0.92f, 1.00f, 0.56f});
		submitBeam(0.30f, {0.92f, 1.00f, 1.00f, 0.96f});

		// 光柱の周囲を上昇する二本の螺旋で、静止した円柱ではなく流動するエネルギーを表現する。
		constexpr int kHelixSegments = 28;
		for (int strand = 0; strand < 2; ++strand) {
			std::vector<TrailRenderPoint> helix;
			helix.reserve(kHelixSegments + 1);
			for (int index = 0; index <= kHelixSegments; ++index) {
				const float t = static_cast<float>(index) / kHelixSegments;
				const float angle = t * MathConstants::kTwoPi * 2.5f + elapsedSeconds_ * 8.0f + strand * MathConstants::kPi;
				const float radius = size_ * (0.80f + 0.10f * std::sin(t * MathConstants::kTwoPi));
				helix.push_back({
				    impact + Vector3{std::cos(angle) * radius, (sky.y - impact.y) * t, std::sin(angle) * radius},
				    envelope * std::sin(MathConstants::kPi * t)
				});
			}
			TrailRenderer::GetInstance()->Submit(
			    helix, size_ * 0.12f, {0.60f, 0.96f, 1.00f, 0.62f * envelope}, {0.05f, 0.28f, 1.00f, 0.0f});
		}

		// 着地点ではリングを高速に広げ、レーザーの接地位置と攻撃範囲を読み取りやすくする。
		const float progress = (std::clamp)(elapsedSeconds_ / initialLifeTime_, 0.0f, 1.0f);
		const float ringRadius = size_ * (0.55f + progress * 1.35f);
		constexpr int kRingSegments = 36;
		std::vector<TrailRenderPoint> ring;
		ring.reserve(kRingSegments + 1);
		for (int index = 0; index <= kRingSegments; ++index) {
			const float angle = MathConstants::kTwoPi * static_cast<float>(index) / kRingSegments;
			ring.push_back({impact + Vector3{std::cos(angle) * ringRadius, 0.06f, std::sin(angle) * ringRadius}, envelope});
		}
		TrailRenderer::GetInstance()->Submit(
		    ring, size_ * 0.16f, {0.48f, 0.94f, 1.00f, 0.75f * envelope}, {0.02f, 0.24f, 1.00f, 0.0f});

		// 放射状の短い火花を回転させ、着弾時の瞬間的な圧力を補強する。
		constexpr int kSparkCount = 8;
		for (int index = 0; index < kSparkCount; ++index) {
			const float angle = MathConstants::kTwoPi * static_cast<float>(index) / kSparkCount + elapsedSeconds_ * 2.5f;
			const Vector3 direction{std::cos(angle), 0.10f, std::sin(angle)};
			const float inner = ringRadius * 0.45f;
			const float outer = ringRadius * (0.85f + 0.18f * std::sin(elapsedSeconds_ * 35.0f + index));
			std::vector<TrailRenderPoint> spark = {
			    {impact + inner * direction, envelope},
			    {impact + outer * direction, 0.0f}
			};
			TrailRenderer::GetInstance()->Submit(
			    spark, size_ * 0.10f, {0.82f, 1.00f, 1.00f, 0.80f * envelope}, {0.04f, 0.35f, 1.00f, 0.0f});
		}
	}

private:
	static float SmoothStep(float value) {
		return value * value * (3.0f - 2.0f * value);
	}

	float initialLifeTime_ = 0.01f;
	float remainingLifeTime_ = 0.01f;
	float elapsedSeconds_ = 0.0f;
	float size_ = 1.0f;
};

// 竜巻弾専用の描画。パーティクルの隙間を複数の螺旋帯で結び、円錐状の回転流を常に読めるようにする。
class TornadoVisualComponent final : public Component {
public:
	TornadoVisualComponent(bool isContracting, bool isGiant)
	    : rotationDirection_(isContracting ? -1.0f : 1.0f) {
		if (isGiant) {
			outerColor_ = {0.12f, 0.86f, 0.40f, 0.32f};
			coreColor_ = {0.78f, 1.00f, 0.84f, 0.86f};
		} else if (isContracting) {
			outerColor_ = {0.54f, 0.16f, 0.88f, 0.30f};
			coreColor_ = {0.94f, 0.76f, 1.00f, 0.84f};
		} else {
			outerColor_ = {0.16f, 0.66f, 0.92f, 0.30f};
			coreColor_ = {0.82f, 0.98f, 1.00f, 0.86f};
		}
	}

	void Update() override {
		elapsedSeconds_ += GameTime::GetDeltaTime();
	}

	void Draw3D() override {
		GameObject* owner = GetOwner();
		const auto* projectile = owner ? owner->GetComponent<EnemyProjectileComponent>() : nullptr;
		if (!owner || !projectile || projectile->IsExpired()) {
			return;
		}

		const float size = projectile->GetSize();
		const float height = (std::max)(1.8f, size * 3.6f);
		const float baseRadius = (std::max)(0.10f, size * 0.16f);
		const float topRadius = (std::max)(0.65f, size * 1.45f);
		const Vector3 center = owner->GetTransform().translate;
		constexpr int kHelixSegments = 30;

		// 位相の異なる三本の螺旋を下から上へ巻き上げ、点群ではなく連続した風の筋として見せる。
		for (int strand = 0; strand < 3; ++strand) {
			std::vector<TrailRenderPoint> helix;
			helix.reserve(kHelixSegments + 1);
			for (int index = 0; index <= kHelixSegments; ++index) {
				const float t = static_cast<float>(index) / kHelixSegments;
				const float angle = rotationDirection_ *
				    (elapsedSeconds_ * 8.5f + t * MathConstants::kTwoPi * 2.7f) + strand * MathConstants::kTwoPi / 3.0f;
				const float radius = baseRadius + (topRadius - baseRadius) * t;
				const float flutter = 1.0f + 0.07f * std::sin(t * MathConstants::kTwoPi * 4.0f + elapsedSeconds_ * 11.0f + strand);
				const float taper = 0.22f + 0.78f * std::sin(MathConstants::kPi * t);
				helix.push_back({
				    center + Vector3{std::cos(angle) * radius * flutter, height * t, std::sin(angle) * radius * flutter},
				    taper
				});
			}

			Vector4 outerTail = outerColor_;
			outerTail.w = 0.03f;
			TrailRenderer::GetInstance()->Submit(
			    helix, (std::max)(0.08f, size * 0.23f), outerColor_, outerTail);
			Vector4 coreTail = coreColor_;
			coreTail.w = 0.05f;
			TrailRenderer::GetInstance()->Submit(
			    helix, (std::max)(0.025f, size * 0.065f), coreColor_, coreTail);
		}

		// 高さの異なる横渦を重ね、漏斗の段差と高速回転を上方視点からも読み取れるようにする。
		constexpr std::array<float, 3> kBandHeights = {0.18f, 0.48f, 0.78f};
		constexpr int kRingSegments = 32;
		for (size_t bandIndex = 0; bandIndex < kBandHeights.size(); ++bandIndex) {
			const float t = kBandHeights[bandIndex];
			const float radius = (baseRadius + (topRadius - baseRadius) * t) *
			    (1.0f + 0.06f * std::sin(elapsedSeconds_ * 9.0f + static_cast<float>(bandIndex) * 2.1f));
			std::vector<TrailRenderPoint> ring;
			ring.reserve(kRingSegments + 1);
			for (int index = 0; index <= kRingSegments; ++index) {
				const float angle = rotationDirection_ * elapsedSeconds_ * (5.0f + bandIndex) +
				    MathConstants::kTwoPi * static_cast<float>(index) / kRingSegments;
				const float verticalRipple = size * 0.05f * std::sin(angle * 3.0f + elapsedSeconds_ * 7.0f);
				ring.push_back({
				    center + Vector3{std::cos(angle) * radius, height * t + verticalRipple, std::sin(angle) * radius},
				    1.0f
				});
			}
			Vector4 bandColor = coreColor_;
			bandColor.w *= 0.48f;
			TrailRenderer::GetInstance()->Submit(
			    ring, (std::max)(0.035f, size * 0.085f), bandColor, bandColor);
		}

		// 接地部の低い環状流で、竜巻の中心と危険範囲を地面上に固定して見せる。
		std::vector<TrailRenderPoint> baseRing;
		baseRing.reserve(kRingSegments + 1);
		const float dustRadius = (std::max)(size * 0.72f, baseRadius * 1.8f);
		for (int index = 0; index <= kRingSegments; ++index) {
			const float angle = MathConstants::kTwoPi * static_cast<float>(index) / kRingSegments - rotationDirection_ * elapsedSeconds_ * 5.5f;
			const float ripple = 1.0f + 0.10f * std::sin(angle * 4.0f + elapsedSeconds_ * 8.0f);
			baseRing.push_back({center + Vector3{std::cos(angle) * dustRadius * ripple, 0.05f, std::sin(angle) * dustRadius * ripple}, 1.0f});
		}
		Vector4 dustColor = outerColor_;
		dustColor.w = 0.42f;
		TrailRenderer::GetInstance()->Submit(
		    baseRing, (std::max)(0.06f, size * 0.14f), dustColor, dustColor);
	}

private:
	float rotationDirection_ = 1.0f;
	float elapsedSeconds_ = 0.0f;
	Vector4 outerColor_{};
	Vector4 coreColor_{};
};

// 弾の前回位置から現在位置までを球の半径分だけ太らせ、球形の対象との連続衝突を判定する。
bool IsSweptSphereToSphere(
	const Vector3& start, const Vector3& end, float movingRadius, const SphereColliderShape& target) {
	const Vector3 movement = end - start;
	const float movementLengthSquared = Dot(movement, movement);
	float t = 0.0f;
	if (movementLengthSquared > MathConstants::kDirectionEpsilon * MathConstants::kDirectionEpsilon) {
		t = (std::clamp)(Dot(target.center - start, movement) / movementLengthSquared, 0.0f, 1.0f);
	}
	const Vector3 closestPoint = start + t * movement;
	const Vector3 separation = target.center - closestPoint;
	const float combinedRadius = movingRadius + target.radius;
	return Dot(separation, separation) <= combinedRadius * combinedRadius;
}

// OBB のローカル空間へ弾の移動区間を移し、弾半径分だけ広げた AABB と判定する。
bool IsSweptSphereToOBB(
	const Vector3& start, const Vector3& end, float movingRadius, const OBBColliderShape& target) {
	auto toLocal = [&target](const Vector3& worldPosition) {
		const Vector3 offset = worldPosition - target.center;
		return Vector3{
			Dot(offset, target.orientation[0]),
			Dot(offset, target.orientation[1]),
			Dot(offset, target.orientation[2])
		};
	};

	const Vector3 localStart = toLocal(start);
	const Vector3 localEnd = toLocal(end);
	const AABBColliderShape expandedTarget{
		{-target.halfSize.x - movingRadius, -target.halfSize.y - movingRadius, -target.halfSize.z - movingRadius},
		{ target.halfSize.x + movingRadius,  target.halfSize.y + movingRadius,  target.halfSize.z + movingRadius}
	};
	return IsCollisionAABBToSegment(expandedTarget, {localStart, localEnd - localStart});
}

// 投射物の命中は専用処理で解決するため、汎用コライダー処理による押し戻しや二重判定を行わない。
bool ShouldSkipColliderPair(GameObject* objectA, GameObject* objectB) {
	if (!objectA || !objectB || objectA == objectB) {
		return true;
	}

	const EnemyComponent* enemyA = objectA->GetComponent<EnemyComponent>();
	const EnemyComponent* enemyB = objectB->GetComponent<EnemyComponent>();
	const bool isEnemyA = enemyA != nullptr;
	const bool isEnemyB = enemyB != nullptr;
	// 死亡演出中の敵は見た目だけを残し、プレイヤーや地形を押し戻さない。
	if ((enemyA && enemyA->GetCurrentHealth() <= 0.0f) ||
	    (enemyB && enemyB->GetCurrentHealth() <= 0.0f)) {
		return true;
	}
	const bool isProjectileA =
	    objectA->GetComponent<EnemyProjectileComponent>() != nullptr ||
	    objectA->GetComponent<PlayerProjectileComponent>() != nullptr;
	const bool isProjectileB =
	    objectB->GetComponent<EnemyProjectileComponent>() != nullptr ||
	    objectB->GetComponent<PlayerProjectileComponent>() != nullptr;
	return (isEnemyA && isEnemyB) || isProjectileA || isProjectileB;
}

struct EnemyPlayerContact {
	EnemyComponent* enemy = nullptr;
	Player* player = nullptr;
};

// 通常経験値の表示単位です。隣り合う値の比が、その段階を1個上へ圧縮するための必要個数になります。
constexpr std::array<int, 4> kExperienceDenominations = {1, 10, 50, 100};
constexpr const char* kEnemyDropModelFilePath = "enemy_drop.obj";
// 同じ経験値単位をまとめる際、基準オブジェクトから候補として扱う最大距離です。
constexpr float kExperienceCompressionDistance = 1.5f;

// 小さなドロップでも多面体と外周リングの陰影が読めるよう、待機中から一定速度で回転させる。
class DropItemVisualComponent final : public Component {
public:
	void Update() override {
		if (GameObject* owner = GetOwner()) {
			owner->GetTransform().rotate.y += GameTime::GetDeltaTime() * 2.4f;
		}
	}
};

// 経験値オーブ専用の弱い脈動発光。ほかのドロップには適用しない。
class ExperienceGlowVisualComponent final : public Component {
public:
	void Update() override {
		Object3dComponent* object3d = GetOwner() ? GetOwner()->GetComponent<Object3dComponent>() : nullptr;
		if (!object3d) {
			return;
		}

		if (!initialized_) {
			baseEmissionColor_ = object3d->GetEmissionColor();
			initialized_ = true;
		}

		time_ += GameTime::GetDeltaTime();
		// Bloomが途切れない範囲で、ゆっくり明滅させる。
		const float intensity = 0.54f + 0.10f * std::sin(time_ * 2.4f);
		object3d->SetEmission(baseEmissionColor_, intensity);
	}

private:
	Vector3 baseEmissionColor_ = {0.0f, 0.0f, 0.0f};
	float time_ = 0.0f;
	bool initialized_ = false;
};

// 圧縮後も経験値の段階を見た目で判別できるよう、単位ごとの表示色を返します。
Vector4 GetExperienceColor(int denomination) {
	switch (denomination) {
	case 100: return {1.0f, 0.65f, 0.08f, 1.0f};
	case 50: return {0.72f, 0.25f, 1.0f, 1.0f};
	case 10: return {0.20f, 1.0f, 0.35f, 1.0f};
	default: return {0.15f, 0.75f, 1.0f, 1.0f};
	}
}

// 発射順に赤・青・緑・黄・白・紫を循環させる。
Vector4 GetMagatamaProjectileColor(int colorIndex) {
	static const std::array<Vector4, 6> kColors = {{
	    {1.0f, 0.08f, 0.04f, 1.0f},
	    {0.08f, 0.35f, 1.0f, 1.0f},
	    {0.08f, 1.0f, 0.20f, 1.0f},
	    {1.0f, 0.90f, 0.05f, 1.0f},
	    {1.0f, 1.0f, 1.0f, 1.0f},
	    {0.72f, 0.16f, 1.0f, 1.0f},
	}};
	return kColors[static_cast<size_t>((std::max)(0, colorIndex)) % kColors.size()];
}

Vector4 GetMagatamaTrailTailColor(int colorIndex) {
	Vector4 color = GetMagatamaProjectileColor(colorIndex);
	color.x *= 0.42f;
	color.y *= 0.42f;
	color.z *= 0.42f;
	color.w = 0.0f;
	return color;
}

// 高い経験値ほど大きく表示するため、単位ごとのモデル倍率を返します。
float GetExperienceScale(int denomination) {
	switch (denomination) {
	case 100: return 0.55f;
	case 50: return 0.48f;
	case 10: return 0.41f;
	default: return 0.35f;
	}
}

bool RegisterEnemyPlayerContact(GameObject* objectA, GameObject* objectB, std::vector<EnemyPlayerContact>& contacts) {
	if (!objectA || !objectB) {
		return false;
	}

	EnemyComponent* enemy = objectA->GetComponent<EnemyComponent>();
	Player* player = objectB->GetComponent<Player>();
	if (!enemy || !player) {
		enemy = objectB->GetComponent<EnemyComponent>();
		player = objectA->GetComponent<Player>();
	}
	if (!enemy || !player) {
		return false;
	}

	// つじぎりボスなど、現在の状態によって接触攻撃が無効な敵はダメージ集計へ加えない。
	if (enemy->IsEnabled() && player->IsEnabled() && enemy->GetCurrentHealth() > 0.0f && enemy->CanDealContactDamage()) {
		const auto duplicate = std::find_if(contacts.begin(), contacts.end(), [enemy, player](const EnemyPlayerContact& contact) {
			return contact.enemy == enemy && contact.player == player;
		});
		if (duplicate == contacts.end()) {
			contacts.push_back({enemy, player});
		}
	}
	return true;
}

bool IsPointOutsideView(const Vector3& worldPosition, float margin) {
	Camera* camera = Object3dCommon::GetInstance() ? Object3dCommon::GetInstance()->GetDefaultCamera() : nullptr;
	if (!camera) {
		return false;
	}

	const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();
	const float clipX =
	    worldPosition.x * viewProjection.m[0][0] +
	    worldPosition.y * viewProjection.m[1][0] +
	    worldPosition.z * viewProjection.m[2][0] +
	    viewProjection.m[3][0];
	const float clipY =
	    worldPosition.x * viewProjection.m[0][1] +
	    worldPosition.y * viewProjection.m[1][1] +
	    worldPosition.z * viewProjection.m[2][1] +
	    viewProjection.m[3][1];
	const float clipZ =
	    worldPosition.x * viewProjection.m[0][2] +
	    worldPosition.y * viewProjection.m[1][2] +
	    worldPosition.z * viewProjection.m[2][2] +
	    viewProjection.m[3][2];
	const float clipW =
	    worldPosition.x * viewProjection.m[0][3] +
	    worldPosition.y * viewProjection.m[1][3] +
	    worldPosition.z * viewProjection.m[2][3] +
	    viewProjection.m[3][3];
	if (clipW <= MathConstants::kDirectionEpsilon) {
		return true;
	}

	const float ndcX = clipX / clipW;
	const float ndcY = clipY / clipW;
	const float ndcZ = clipZ / clipW;
	const float safeMargin = margin < 0.0f ? 0.0f : margin;
	return ndcX < -1.0f - safeMargin ||
	    ndcX > 1.0f + safeMargin ||
	    ndcY < -1.0f - safeMargin ||
	    ndcY > 1.0f + safeMargin ||
	    ndcZ < -safeMargin ||
	    ndcZ > 1.0f + safeMargin;
}

bool ProjectToNdc(Camera* camera, const Vector3& worldPosition, Vector3& outNdc) {
	if (!camera) {
		return false;
	}
	const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();
	const float clipX = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
	const float clipY = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
	const float clipZ = worldPosition.x * viewProjection.m[0][2] + worldPosition.y * viewProjection.m[1][2] + worldPosition.z * viewProjection.m[2][2] + viewProjection.m[3][2];
	const float clipW = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
	if (clipW <= MathConstants::kDirectionEpsilon) {
		return false;
	}
	outNdc = {clipX / clipW, clipY / clipW, clipZ / clipW};
	return true;
}

bool IntersectScreenRayToHeight(Camera* camera, const Vector2& ndc, float height, Vector3& outPoint) {
	if (!camera) {
		return false;
	}
	const Matrix4x4 inverseViewProjection = Inverse(camera->GetViewProjectionMatrix());
	const Vector3 nearPoint = Transformation({ndc.x, ndc.y, 0.0f}, inverseViewProjection);
	const Vector3 farPoint = Transformation({ndc.x, ndc.y, 1.0f}, inverseViewProjection);
	const Vector3 ray = farPoint - nearPoint;
	if (std::fabs(ray.y) <= MathConstants::kNormalizationEpsilon) {
		return false;
	}
	const float t = (height - nearPoint.y) / ray.y;
	if (t < 0.0f) {
		return false;
	}
	outPoint = nearPoint + t * ray;
	return true;
}
}


