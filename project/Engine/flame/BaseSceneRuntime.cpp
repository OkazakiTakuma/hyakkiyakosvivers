#include "BaseScene.h"
#include "helpers/BaseSceneCollisionHelpers.h"
#include "helpers/BaseSceneEditorGeometry.h"
#include "repositories/EnemyStatusRepository.h"
#include "repositories/ParticlePresetRepository.h"
#include "MathConstants.h"
#include "model/ModelManager.h"
#include "repositories/PlayerStatusRepository.h"
#include "SceneManager.h"
#include "StringUtility.h"
#include "../3d/particle/TrailRenderer.h"
#include <cmath>
#include <filesystem>
#include <limits>
#include <random>
#include <unordered_set>
#include <Xinput.h>
#ifdef USE_IMGUI
#include "../../../imgui/ImGuizmo.h"
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

	const bool isEnemyA = objectA->GetComponent<EnemyComponent>() != nullptr;
	const bool isEnemyB = objectB->GetComponent<EnemyComponent>() != nullptr;
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

void BaseScene::ApplyCamera(Camera* camera) {
	if (!camera) {
		return;
	}

	const float clientWidth = static_cast<float>(Input::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(Input::GetInstance()->GetClientHeight());
	float aspectRatio = clientWidth > 0.0f && clientHeight > 0.0f ? clientWidth / clientHeight : 1.0f;
#ifdef USE_IMGUI
	const float gameViewAspectRatio = ImGuiManager::GetInstance()->GetGameViewAspectRatio();
	if (gameViewAspectRatio > 0.0f) {
		aspectRatio = gameViewAspectRatio;
	}
#endif
	camera->SetAspectRatio(aspectRatio);
	camera->Update();
	Object3dCommon::GetInstance()->SetDefaultCamera(camera);
	SkyBoxCommon::GetInstance()->SetDefaultCamera(camera);
	ParticleManager::GetInstance()->SetCamera(camera);
	for (const auto& object : sceneObjects_) {
		if (Object3dComponent* object3dComponent = object->GetComponent<Object3dComponent>()) {
			object3dComponent->SetCamera(camera);
			if (object3dComponent->GetObject3d()) {
				object3dComponent->GetObject3d()->Update();
			}
		}
	}
}

/// <summary>
/// 現在選択されているアクティブカメラを反映します。
/// </summary>
void BaseScene::ApplyActiveCamera() {
	if (activeCameraObjectName_.empty()) {
		ApplyCamera(fallbackCamera_);
		return;
	}

	GameObject* cameraObject = FindObjectByName(activeCameraObjectName_);
	if (!cameraObject) {
		activeCameraObjectName_.clear();
		ApplyCamera(fallbackCamera_);
		return;
	}

	CameraComponent* cameraComponent = cameraObject->GetComponent<CameraComponent>();
	if (!cameraComponent || !cameraComponent->IsEnabled() || !cameraComponent->GetCamera()) {
		activeCameraObjectName_.clear();
		ApplyCamera(fallbackCamera_);
		return;
	}

	cameraComponent->Update();
	ApplyCamera(cameraComponent->GetCamera());
}

/// <summary>
/// カメラの追従対象リンクを名前から解決します。
/// </summary>
void BaseScene::ResolveCameraLinks() {
	for (const auto& object : sceneObjects_) {
		CameraComponent* cameraComponent = object->GetComponent<CameraComponent>();
		if (!cameraComponent || !cameraComponent->IsEnabled() || cameraComponent->GetFollowTargetName().empty()) {
			continue;
		}

		GameObject* target = FindObjectByName(cameraComponent->GetFollowTargetName());
		if (target && target != object.get() && target != cameraComponent->GetFollowTarget()) {
			cameraComponent->SetFollowTarget(target);
		}
	}
}

void BaseScene::ResolveEnemySpawnPointLinks() {
	GameObject* firstPlayer = nullptr;
	for (const auto& object : sceneObjects_) {
		if (object->GetComponent<Player>()) {
			firstPlayer = object.get();
			break;
		}
	}

	for (const auto& object : sceneObjects_) {
		EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		if (!spawnPoint || !spawnPoint->IsEnabled()) {
			continue;
		}

		GameObject* target = spawnPoint->GetTargetName().empty() ? firstPlayer : FindObjectByName(spawnPoint->GetTargetName());
		if (target && target != spawnPoint->GetTarget()) {
			spawnPoint->SetTarget(target);
			if (spawnPoint->GetTargetName().empty()) {
				spawnPoint->SetTargetName(target->GetName());
			}
		}

		Camera* camera = nullptr;
		if (!spawnPoint->GetCameraName().empty()) {
			GameObject* cameraObject = FindObjectByName(spawnPoint->GetCameraName());
			CameraComponent* cameraComponent = cameraObject ? cameraObject->GetComponent<CameraComponent>() : nullptr;
			if (cameraComponent && cameraComponent->IsEnabled()) {
				camera = cameraComponent->GetCamera();
			}
		}
		if (!camera && Object3dCommon::GetInstance()) {
			camera = Object3dCommon::GetInstance()->GetDefaultCamera();
		}
		spawnPoint->SetCamera(camera);
	}
}

void BaseScene::ResolveEnemyLinks() {
	GameObject* firstPlayer = nullptr;
	for (const auto& object : sceneObjects_) {
		if (object->GetComponent<Player>()) {
			firstPlayer = object.get();
			break;
		}
	}

	for (const auto& object : sceneObjects_) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled()) {
			continue;
		}

		GameObject* target = enemy->GetTargetName().empty() ? firstPlayer : FindObjectByName(enemy->GetTargetName());
		if (target && target != enemy->GetTarget()) {
			enemy->SetTarget(target);
			if (enemy->GetTargetName().empty()) {
				enemy->SetTargetName(target->GetName());
			}
		}
	}
}

void BaseScene::UpdateEnemySpawning() {
	// ボス名を保持している間は通常スポーンを完全停止し、撃破後も報酬処理が終わるまでクリアを確定しない。
	if (!activeBossEncounterObjectName_.empty()) {
		GameObject* bossObject = FindObjectByName(activeBossEncounterObjectName_);
		EnemyComponent* bossEnemy = bossObject ? bossObject->GetComponent<EnemyComponent>() : nullptr;
		const bool bossIsAlive = bossEnemy && bossEnemy->IsEnabled() && bossEnemy->GetCurrentHealth() > 0.0f;
		for (const auto& object : sceneObjects_) {
			if (EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>()) {
				spawnPoint->SetBossEncounterActive(bossIsAlive);
			}
		}
		if (bossIsAlive) {
			return;
		}
		// 最終ボスの金色報酬を拾い、強化選択を終えるまではリザルトへ移動しない。
		for (const auto& object : sceneObjects_) {
			ExperienceComponent* reward = object->GetComponent<ExperienceComponent>();
			if (reward && reward->IsBossUpgradeReward() &&
			    (!reward->IsCollected() || !reward->IsBossUpgradeApplied())) {
				return;
			}
		}
		if (IsLevelUpSelectionActive() || !bossAcquisitionOfferQueue_.empty()) {
			return;
		}
		// Boss Encounter はステージの最後に一度だけ発生するため、撃破をクリア条件とする。
		isStageCleared_ = true;
		activeBossEncounterObjectName_.clear();
		return;
	}

	struct SpawnRequest {
		std::string enemyTypeName;
		Vector3 position;
		GameObject* target = nullptr;
		EnemySpawnPointComponent::EnemyStatMultipliers statMultipliers{};
	};
	std::vector<SpawnRequest> spawnRequests;
	EnemySpawnPointComponent* triggeredBossSpawnPoint = nullptr;
	for (const auto& object : sceneObjects_) {
		EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>();
		if (!spawnPoint || !spawnPoint->IsEnabled()) {
			continue;
		}

		if (spawnPoint->ConsumeBossEncounterRequest()) {
			triggeredBossSpawnPoint = spawnPoint;
			break;
		}

		if (!spawnPoint->GetSpawnSchedules().empty()) {
			const std::vector<EnemySpawnPointComponent::ScheduledSpawnRequest> scheduledRequests = spawnPoint->ConsumeScheduledSpawnRequests();
			for (const EnemySpawnPointComponent::ScheduledSpawnRequest& request : scheduledRequests) {
				spawnRequests.push_back({request.enemyTypeName, request.position, spawnPoint->GetTarget(), request.statMultipliers});
			}
		} else {
			const std::string enemyTypeName = spawnPoint->GetEnemyTypeName();
			const EnemyStats stats = LoadEnemyStats(enemyTypeName);
			Vector3 spawnPosition{};
			if (spawnPoint->ConsumeSpawnRequest(stats.spawnsPerMinute, spawnPosition)) {
				spawnRequests.push_back({enemyTypeName, spawnPosition, spawnPoint->GetTarget(), spawnPoint->GetCurrentTimeScaling()});
			}
		}
	}

	if (triggeredBossSpawnPoint) {
		const EnemySpawnPointComponent::BossEncounterSettings& bossSettings =
		    triggeredBossSpawnPoint->GetBossEncounterSettings();
		GameObject* player = triggeredBossSpawnPoint->GetTarget();
		if (player) {
			// ボス生成より先にプレイヤーを戦闘位置へ移し、落下速度を残さない。
			player->GetTransform().translate = bossSettings.playerWarpPosition;
			if (Player* playerComponent = player->GetComponent<Player>()) {
				playerComponent->ResetGravityVelocity();
			}
		}

		// ボス戦へ通常敵や途中の中ボスを持ち越さないよう、全EnemyComponentを即時除去する。
		sceneObjects_.erase(
		    std::remove_if(sceneObjects_.begin(), sceneObjects_.end(), [](const std::unique_ptr<GameObject>& object) {
			    return object->GetComponent<EnemyComponent>() != nullptr;
		    }),
		    sceneObjects_.end()
		);
		selectedObjectIndex_ = -1;
		for (const auto& object : sceneObjects_) {
			if (EnemySpawnPointComponent* spawnPoint = object->GetComponent<EnemySpawnPointComponent>()) {
				spawnPoint->SetBossEncounterActive(true);
			}
		}

		GameObject* bossObject = CreateRuntimeEnemy(bossSettings.enemyTypeName, bossSettings.bossPosition, player);
		if (bossObject) {
			activeBossEncounterObjectName_ = bossObject->GetName();
		}
		return;
	}

	for (const SpawnRequest& request : spawnRequests) {
		Vector3 spawnPosition = request.position;
		// ステージ2では画面外の生成候補も上下の壁より内側へ収める。
		if (sceneManager && sceneManager->GetSelectedGameplayStageId() == "stage2") {
			constexpr float kStage2SpawnLimitZ = 8.5f;
			spawnPosition.z = std::clamp(spawnPosition.z, -kStage2SpawnLimitZ, kStage2SpawnLimitZ);
		}
		CreateRuntimeEnemy(request.enemyTypeName, spawnPosition, request.target, request.statMultipliers);
	}
}

void BaseScene::UpdateStageBoundaryWrapping() {
	if (!sceneManager) {
		return;
	}

	const std::string& stageId = sceneManager->GetSelectedGameplayStageId();
	// 平原はX/Zの両方向、ステージ2は壁と平行なX方向だけをループ対象にする。
	const bool wrapsBothAxes = stageId == "default";
	const bool wrapsHorizontally = stageId == "stage2";
	if (!wrapsBothAxes && !wrapsHorizontally) {
		return;
	}

	GameObject* playerObject = nullptr;
	for (const auto& object : sceneObjects_) {
		if (object->GetComponent<Player>()) {
			playerObject = object.get();
			break;
		}
	}
	if (!playerObject) {
		return;
	}

	// 平原は中央タイルの端、ステージ2は横長フィールドの端を折り返し位置として扱う。
	// ステージ2の床判定は配置JSONでX方向±30（halfSize 24 × scale 1.25）を確保し、±20の折り返し前後を支える。
	const float halfWidth = wrapsBothAxes ? 10.0f : 20.0f;
	const float width = halfWidth * 2.0f;
	const Vector3 playerPosition = playerObject->GetTransform().translate;
	Vector3 shift{};
	// 1フレームで境界幅以上を移動しても正しい範囲へ戻せるよう、条件を満たすまで繰り返す。
	while (playerPosition.x + shift.x > halfWidth) shift.x -= width;
	while (playerPosition.x + shift.x < -halfWidth) shift.x += width;
	if (wrapsBothAxes) {
		constexpr float kHalfDepth = 10.0f;
		constexpr float kDepth = kHalfDepth * 2.0f;
		while (playerPosition.z + shift.z > kHalfDepth) shift.z -= kDepth;
		while (playerPosition.z + shift.z < -kHalfDepth) shift.z += kDepth;
	}

	if (shift.x == 0.0f && shift.z == 0.0f) {
		return;
	}

	// プレイヤーだけを移すと敵やドロップとの距離が不連続になるため、ゲーム進行に関わる動的物体をまとめて移す。
	auto movesWithStage = [](GameObject* target) {
		return target && (
		    target->GetComponent<Player>() ||
		    target->GetComponent<EnemyComponent>() ||
		    target->GetComponent<EnemyProjectileComponent>() ||
		    target->GetComponent<PlayerProjectileComponent>() ||
		    target->GetComponent<ExperienceComponent>() ||
		    target->GetComponent<ItemDropComponent>() ||
		    target->GetComponent<EnemySpawnPointComponent>());
	};

	for (const auto& object : sceneObjects_) {
		GameObject* target = object.get();
		if (movesWithStage(target)) {
			if (TrailRendererComponent* trail = target->GetComponent<TrailRendererComponent>()) {
				// 相対トレイルは参照元の移動だけで履歴全体が追従するため、保存点まで動かすと二重移動になる。
				// 参照元がないワールド座標履歴と、ループ対象外を参照する履歴だけを直接補正する。
				if (!movesWithStage(trail->GetPositionReference())) {
					trail->TranslateHistory(shift);
				}
			}
			target->GetTransform().translate = target->GetTransform().translate + shift;
			// ループ移動後の座標を同じフレームでカメラ行列へ反映する。
			// 床タイルは固定配置のままにし、カメラだけが古い座標で1フレーム
			// 描画される可能性をなくす。
			if (CameraComponent* camera = target->GetComponent<CameraComponent>()) {
				camera->Update();
			}
		}
	}
}

void BaseScene::UpdatePlayerAttacks() {
	std::vector<PlayerAttackShotRequest> shotRequests;
	for (const auto& object : sceneObjects_) {
		PlayerAttackComponent* attack = object->GetComponent<PlayerAttackComponent>();
		if (!attack || !attack->IsEnabled()) {
			continue;
		}

		std::vector<PlayerAttackShotRequest> requests = attack->ConsumeShotRequests();
		shotRequests.insert(shotRequests.end(), requests.begin(), requests.end());
	}

	// SkyLaserは画面外の敵を対象にしない。候補をシャッフルして同一発射内では別々の敵へ割り当てる。
	std::vector<GameObject*> laserTargets;
	for (const auto& object : sceneObjects_) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (enemy && enemy->IsEnabled() && enemy->GetCurrentHealth() > 0.0f &&
		    !IsPointOutsideView(object->GetTransform().translate, 0.0f)) {
			laserTargets.push_back(object.get());
		}
	}
	static std::mt19937 laserRandomEngine(std::random_device{}());
	std::shuffle(laserTargets.begin(), laserTargets.end(), laserRandomEngine);
	size_t nextLaserTargetIndex = 0;

	for (PlayerAttackShotRequest& request : shotRequests) {
		if (request.motionType == PlayerProjectileMotionType::SkyLaser) {
			// 画面内に標的が残っていない分のレーザーは生成しない。
			if (nextLaserTargetIndex >= laserTargets.size()) {
				continue;
			}
			const Vector3 targetPosition = laserTargets[nextLaserTargetIndex++]->GetTransform().translate;
			request.position = {targetPosition.x, targetPosition.y + 3.0f, targetPosition.z};
		}
		CreateRuntimePlayerProjectile(request);
	}
}

GameObject* BaseScene::CreateRuntimeEnemy(
	const std::string& enemyTypeName, const Vector3& position, GameObject* target,
	const EnemySpawnPointComponent::EnemyStatMultipliers& statMultipliers) {
	auto object = std::make_unique<GameObject>();
	const std::string resolvedTypeName = enemyTypeName.empty() ? "Default" : enemyTypeName;
	EnemyStats stats = LoadEnemyStats(resolvedTypeName);
	// 時間帯倍率は生成時に固定し、戦闘中の最大HPや移動速度が突然変化しないようにする。
	stats.health *= (std::max)(0.0f, statMultipliers.healthMultiplier);
	stats.speed *= (std::max)(0.0f, statMultipliers.speedMultiplier);
	stats.experience = (std::max)(0, static_cast<int>(std::lround(
		static_cast<float>(stats.experience) * (std::max)(0.0f, statMultipliers.experienceMultiplier))));
	object->SetName(MakeUniqueObjectName(resolvedTypeName));
	object->SetEditorType("Enemy");
	object->GetTransform().translate = position;
	// 0.75を通常敵の基準サイズとし、MidBossなどはデータ側の倍率で大型化する。
	// OBBはTransform.scaleを参照するため、見た目と接触判定が同じ倍率で拡大される。
	const float scale = 0.75f * stats.sizeScale;
	object->GetTransform().scale = {scale, scale, scale};

	EnemyComponent* enemy = object->AddComponent<EnemyComponent>();
	enemy->SetEnemyTypeName(resolvedTypeName);
	enemy->ApplyStats(stats);
	enemy->SetRuntimeSpawned(true);
	if (target) {
		enemy->SetTarget(target);
		enemy->SetTargetName(target->GetName());
	}

	// 行動に対応する標準モデルはStrategyから受け取り、ステージ固有キャラクターだけ名前で上書きする。
	const EnemyBehaviorPresentation initialPresentation = enemy->GetBehaviorPresentation();
	std::string enemyModelFilePath = initialPresentation.modelFilePath;
	if (resolvedTypeName == "Stage2Boss") {
		enemyModelFilePath = "doppelganger.gltf";
		if (!ModelManager::GetInstance()->FindModel(enemyModelFilePath)) {
			ModelManager::GetInstance()->LoadModel(enemyModelFilePath, true, "/doppelganger");
		}
	} else if (resolvedTypeName == "MidBoss") {
		enemyModelFilePath = "neko.gltf";
		if (!ModelManager::GetInstance()->FindModel(enemyModelFilePath)) {
			ModelManager::GetInstance()->LoadModel(enemyModelFilePath, true, "/cat");
		}
	} else {
		ModelManager::GetInstance()->LoadModel(enemyModelFilePath);
	}
	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel(enemyModelFilePath);

	OBBColliderComponent* collider = object->AddComponent<OBBColliderComponent>();
	collider->SetHalfSize({0.4f, 0.4f, 0.4f});
	collider->SetPushBackEnabled(true);
	if (initialPresentation.addAttackTrail) {
		// 高速移動した経路を紫色の帯として残し、切り返し方向を視覚化する。
		TrailRendererComponent* trail = object->AddComponent<TrailRendererComponent>();
		trail->SetWidth(1.15f * stats.sizeScale);
		trail->SetLifeTime(0.32f);
		trail->SetMinSegmentLength(0.08f);
		trail->SetHeadColor({0.92f, 0.20f, 1.0f, 0.92f});
		trail->SetTailColor({0.20f, 0.02f, 0.40f, 0.0f});
	}

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

GameObject* BaseScene::CreateRuntimeExperience(const EnemyStats& enemyStats, const Vector3& position, GameObject* target) {
	if (enemyStats.experience <= 0) {
		return nullptr;
	}
	if (!target) {
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<Player>()) {
				target = object.get();
				break;
			}
		}
	}

	const std::string modelFilePath = enemyStats.experienceModelFilePath.empty()
	    ? kEnemyDropModelFilePath
	    : enemyStats.experienceModelFilePath;
	if (!ModelManager::GetInstance()->FindModel(modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(modelFilePath);
	}
	if (!ModelManager::GetInstance()->FindModel(modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(kEnemyDropModelFilePath);
	}
	const std::string resolvedModelFilePath = ModelManager::GetInstance()->FindModel(modelFilePath)
	    ? modelFilePath
	    : kEnemyDropModelFilePath;
	GameObject* firstExperienceObject = nullptr;
	int remainingExperience = enemyStats.experience;
	int particleIndex = 0;
	for (auto denominationIt = kExperienceDenominations.rbegin(); denominationIt != kExperienceDenominations.rend(); ++denominationIt) {
		const int denomination = *denominationIt;
		const int particleCount = remainingExperience / denomination;
		remainingExperience %= denomination;
		for (int index = 0; index < particleCount; ++index, ++particleIndex) {
			auto object = std::make_unique<GameObject>();
			object->SetName(MakeUniqueObjectName("Experience" + std::to_string(denomination)));
			object->SetEditorType("Experience");
			const float angle = static_cast<float>(particleIndex) * 2.39996323f;
			const float radius = particleIndex == 0 ? 0.0f : 0.18f + 0.07f * std::sqrt(static_cast<float>(particleIndex));
			object->GetTransform().translate = {
				position.x + std::cos(angle) * radius,
				position.y + 0.08f * static_cast<float>(particleIndex % 3),
				position.z + std::sin(angle) * radius
			};
			const float scale = GetExperienceScale(denomination);
			object->GetTransform().scale = {scale, scale, scale};

			ExperienceComponent* experience = object->AddComponent<ExperienceComponent>();
			experience->SetExperience(denomination);
			experience->SetModelFilePath(resolvedModelFilePath);
			experience->SetTarget(target);
			object->AddComponent<DropItemVisualComponent>();
			object->AddComponent<ExperienceGlowVisualComponent>();

			Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
			object3d->SetModel(resolvedModelFilePath);
			const Vector4 experienceColor = GetExperienceColor(denomination);
			object3d->SetColor(experienceColor);
			object3d->SetEmission(
				{experienceColor.x, experienceColor.y, experienceColor.z}, 0.54f);

			object->Update();
			GameObject* createdObject = object.get();
			sceneObjects_.push_back(std::move(object));
			if (!firstExperienceObject) {
				firstExperienceObject = createdObject;
			}
			++nextObjectId_;
		}
	}
	return firstExperienceObject;
}

/// <summary>
/// 敵の撃破位置へ回復、経験値回収、または所持金用のドロップオブジェクトを生成します。
/// MoneyタイプではmoneyAmountを保持し、プレイヤーが接触した時に今回の獲得額へ加算します。
/// </summary>
GameObject* BaseScene::CreateRuntimeItemDrop(
	ItemDropType type, const Vector3& position, GameObject* target, float healAmount, int moneyAmount) {
	// 撃破した敵にターゲットが設定されていない場合も回収できるよう、シーン内のプレイヤーを補完する。
	if (!target) {
		// 全スポーンポイントへ停止状態を伝え、経過秒数とスケジュールカウンターを凍結する。
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<Player>()) {
				target = object.get();
				break;
			}
		}
	}

	ModelManager::GetInstance()->LoadModel(kEnemyDropModelFilePath);
	auto object = std::make_unique<GameObject>();
	const bool isHealthItem = type == ItemDropType::Health;
	const bool isMoneyItem = type == ItemDropType::Money;
	// 種類ごとに名前、表示サイズ、色を変え、プレイ中に効果を判別できるようにする。
	object->SetName(MakeUniqueObjectName(isHealthItem ? "HealthItem" : isMoneyItem ? "Money" : "ExperienceCollector"));
	object->SetEditorType(isHealthItem ? "HealthItem" : isMoneyItem ? "Money" : "ExperienceCollector");
	object->GetTransform().translate = position + Vector3{isHealthItem ? -0.35f : 0.35f, 0.35f, 0.0f};
	const float scale = isHealthItem ? 0.42f : isMoneyItem ? 0.30f : 0.50f;
	object->GetTransform().scale = {scale, scale, scale};

	ItemDropComponent* item = object->AddComponent<ItemDropComponent>();
	item->SetType(type);
	item->SetTarget(target);
	item->SetHealAmount(healAmount);
	item->SetMoneyAmount(moneyAmount);
	object->AddComponent<DropItemVisualComponent>();

	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel(kEnemyDropModelFilePath);
	object3d->SetColor(isHealthItem
		? Vector4{0.15f, 1.0f, 0.25f, 1.0f}
		: isMoneyItem ? Vector4{1.0f, 0.55f, 0.05f, 1.0f}
		: Vector4{1.0f, 0.85f, 0.10f, 1.0f});
	object3d->SetEmission(
		isHealthItem ? Vector3{0.08f, 0.55f, 0.12f}
		             : isMoneyItem ? Vector3{0.65f, 0.28f, 0.03f}
		                           : Vector3{0.65f, 0.48f, 0.05f},
		0.55f);

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

/// <summary>
/// ボス撃破位置へ金色の強化報酬を1個生成します。
/// upgradeCountはドロップ数ではなく、回収時に実行する強化抽選の回数です。
/// </summary>
void BaseScene::CreateRuntimeBossUpgradeDrop(const Vector3& position, GameObject* target, int upgradeCount) {
	if (!target) {
		for (const auto& object : sceneObjects_) {
			if (object->GetComponent<Player>()) {
				target = object.get();
				break;
			}
		}
	}

	ModelManager::GetInstance()->LoadModel(kEnemyDropModelFilePath);
	auto object = std::make_unique<GameObject>();
	object->SetName(MakeUniqueObjectName("BossUpgradeReward"));
	object->SetEditorType("BossUpgradeReward");
	object->GetTransform().translate = position + Vector3{0.0f, 0.45f, 0.0f};
	object->GetTransform().scale = {0.65f, 0.65f, 0.65f};

	// 経験値と同じ吸着挙動を再利用し、識別フラグによって経験値加算だけを抑止する。
	ExperienceComponent* reward = object->AddComponent<ExperienceComponent>();
	reward->SetBossUpgradeReward(true);
	reward->SetBossUpgradeCount(upgradeCount);
	reward->SetTarget(target);
	reward->SetAttractDistance(12.0f);
	reward->SetAttractSpeed(0.12f);
	object->AddComponent<DropItemVisualComponent>();

	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel(kEnemyDropModelFilePath);
	object3d->SetColor({1.0f, 0.72f, 0.08f, 1.0f});
	object3d->SetEmission({0.85f, 0.48f, 0.04f}, 0.75f);

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
}

/// <summary>
/// 装備中の強化可能な攻撃・アイテムを、指定回数だけ独立に抽選して1レベルずつ上げます。
/// 同じ装備の再選出を許可し、途中で上限へ達した装備は次の抽選から除外します。
/// </summary>
int BaseScene::ApplyRandomBossUpgrades(Player* player, int upgradeCount) {
	if (!player || upgradeCount <= 0) {
		return 0;
	}

	struct UpgradeCandidate {
		bool isAttack = false;
		int slotIndex = -1;
		bool promoteToSuper = false;
	};
	static std::mt19937 rewardRandomEngine(std::random_device{}());
	PlayerStats upgradedStats = player->GetBaseStats();
	int appliedCount = 0;
	for (; appliedCount < upgradeCount; ++appliedCount) {
		// 直前の強化結果を反映した状態から候補を作り直すことで、重複選出と上限除外を両立する。
		std::vector<UpgradeCandidate> candidates;
		for (int index = 0; index < static_cast<int>(upgradedStats.attackSlots.size()); ++index) {
			const PlayerAttackSlot& slot = upgradedStats.attackSlots[index];
			if (!slot.enabled || slot.attackName.empty() || slot.attackLevel == "super") {
				continue;
			}
			const int level = std::atoi(slot.attackLevel.c_str());
			if (level >= 1 && level < 5) {
				candidates.push_back({true, index, false});
				continue;
			}
			if (slot.attackLevel != "5") {
				continue;
			}
			// レベル5攻撃は、対応アイテムと必要レベルを満たす場合だけスーパー化候補へ加える。
			const PlayerAttackStats attackStats = LoadPlayerAttackStats(slot.attackName);
			bool conditionMet = !attackStats.superConditionStatusName.empty();
			const int requiredLevel = (std::max)(1, std::atoi(attackStats.superConditionStatusLevel.c_str()));
			for (const PlayerStatusSlot& statusSlot : upgradedStats.statusSlots) {
				if (conditionMet && statusSlot.enabled &&
				    statusSlot.statusName == attackStats.superConditionStatusName &&
				    std::atoi(statusSlot.level.c_str()) >= requiredLevel) {
					candidates.push_back({true, index, true});
					break;
				}
			}
		}
		for (int index = 0; index < static_cast<int>(upgradedStats.statusSlots.size()); ++index) {
			const PlayerStatusSlot& slot = upgradedStats.statusSlots[index];
			const int level = std::atoi(slot.level.c_str());
			if (slot.enabled && !slot.statusName.empty() && level >= 1 && level < 5) {
				candidates.push_back({false, index, false});
			}
		}
		if (candidates.empty()) {
			// 適用できなかった残り回数は、呼び出し側で未所持装備の取得確認へ変換する。
			break;
		}
		std::uniform_int_distribution<size_t> candidateDistribution(0, candidates.size() - 1);
		const UpgradeCandidate& selected = candidates[candidateDistribution(rewardRandomEngine)];
		if (selected.isAttack) {
			PlayerAttackSlot& slot = upgradedStats.attackSlots[selected.slotIndex];
			slot.attackLevel = selected.promoteToSuper
			    ? "super"
			    : std::to_string((std::min)(5, std::atoi(slot.attackLevel.c_str()) + 1));
		} else {
			PlayerStatusSlot& slot = upgradedStats.statusSlots[selected.slotIndex];
			slot.level = std::to_string((std::min)(5, std::atoi(slot.level.c_str()) + 1));
		}
	}

	if (appliedCount > 0) {
		player->ApplyStats(upgradedStats, ApplyPlayerStatusItems(upgradedStats));
		if (GameObject* owner = player->GetOwner()) {
			ApplyPlayerAttackSlots(owner->GetComponent<PlayerAttackComponent>(), upgradedStats);
		}
	}
	return appliedCount;
}

/// <summary>
/// 回収済みの金色ボス報酬へ強化効果を適用し、強化アイテム数に比例したお金を付与します。
/// </summary>
void BaseScene::UpdateBossUpgradeRewards() {
	for (const auto& object : sceneObjects_) {
		ExperienceComponent* reward = object->GetComponent<ExperienceComponent>();
		if (!reward || !reward->IsBossUpgradeReward() || !reward->IsCollected() || reward->IsBossUpgradeApplied()) {
			continue;
		}
		Player* player = reward->GetTarget() ? reward->GetTarget()->GetComponent<Player>() : nullptr;
		// 金色報酬を拾った瞬間に全体G強化を適用し、所持金と今回獲得額の両方へ反映する。
		const int baseMoney = reward->GetBossUpgradeCount() * 100;
		const int gainedMoney = sceneManager ? sceneManager->ApplyGlobalGoldBonus(baseMoney) : baseMoney;
		// challengeMoneyEarned_はリザルト表示用、AddMoneyは所持金更新とJSON即時保存を担当する。
		challengeMoneyEarned_ += gainedMoney;
		if (sceneManager) sceneManager->AddMoney(gainedMoney);
		const int appliedCount = ApplyRandomBossUpgrades(player, reward->GetBossUpgradeCount());
		QueueBossAcquisitionOffers(player, reward->GetBossUpgradeCount() - appliedCount);
		reward->MarkBossUpgradeApplied();
	}
}

/// <summary>
/// 強化対象が存在しなかった回数分、空きスロットへ追加できる未所持装備をランダムに予約します。
/// 同じ候補は1つの予約列へ重複登録せず、候補数が不足する場合は存在する分だけ提示します。
/// </summary>
void BaseScene::QueueBossAcquisitionOffers(Player* player, int offerCount) {
	if (!player || offerCount <= 0) {
		return;
	}
	const PlayerStats& stats = player->GetBaseStats();
	const bool hasEmptyAttackSlot = std::any_of(stats.attackSlots.begin(), stats.attackSlots.end(), [](const PlayerAttackSlot& slot) {
		return slot.attackName.empty();
	});
	const bool hasEmptyStatusSlot = std::any_of(stats.statusSlots.begin(), stats.statusSlots.end(), [](const PlayerStatusSlot& slot) {
		return slot.statusName.empty();
	});

	const auto otherPlayerInitialAttacks = GetOtherPlayerInitialAttacks(*player);
	std::unordered_set<std::string> ownedAttacks;
	std::unordered_set<std::string> ownedStatuses;
	for (const PlayerAttackSlot& slot : stats.attackSlots) {
		if (!slot.attackName.empty()) {
			ownedAttacks.insert(slot.attackName);
		}
	}
	for (const PlayerStatusSlot& slot : stats.statusSlots) {
		if (!slot.statusName.empty()) {
			ownedStatuses.insert(slot.statusName);
		}
	}

	// 攻撃とステータスアイテムを同じ候補配列へ集約し、種類をまたいで公平にシャッフルする。
	std::vector<LevelUpChoice> offers;
	if (hasEmptyAttackSlot) {
		for (const std::string& attackName : LoadPlayerAttackNames()) {
			// 通常レベルアップで抽選されない他タイプの初期武器は、ボス報酬でも除外する。
			if (ownedAttacks.find(attackName) != ownedAttacks.end() ||
			    otherPlayerInitialAttacks.contains(attackName)) {
				continue;
			}
			const PlayerAttackStats attackStats = LoadPlayerAttackStats(attackName);
			std::string description = "Acquire this attack at level 1?";
			std::string texture = attackStats.choiceTextureFilePath;
			for (const PlayerAttackLevelStats& levelStats : attackStats.levels) {
				if (levelStats.level == "1") {
					if (!levelStats.choiceDescription.empty()) {
						description = levelStats.choiceDescription;
					}
					if (texture.empty()) {
						texture = levelStats.choiceTextureFilePath;
					}
					break;
				}
			}
			offers.push_back({LevelUpChoiceType::NewAttack, attackName, "Acquire " + attackName, description, -1, texture});
		}
	}
	if (hasEmptyStatusSlot) {
		for (const std::string& statusName : LoadPlayerStatusItemNames()) {
			if (ownedStatuses.find(statusName) != ownedStatuses.end()) {
				continue;
			}
			const PlayerStatusItemStats statusStats = LoadPlayerStatusItemStats(statusName);
			const std::string description = statusStats.levelDescriptions[0].empty()
			    ? "Acquire this item at level 1?"
			    : statusStats.levelDescriptions[0];
			offers.push_back({
				LevelUpChoiceType::NewStatus,
				statusName,
				"Acquire " + statusName,
				description,
				-1,
				statusStats.levelTextureFilePaths[0]
			});
		}
	}
	if (offers.empty()) {
		return;
	}

	static std::mt19937 offerRandomEngine(std::random_device{}());
	std::shuffle(offers.begin(), offers.end(), offerRandomEngine);
	const int queuedCount = (std::min)(offerCount, static_cast<int>(offers.size()));
	bossAcquisitionOfferQueue_.insert(
	    bossAcquisitionOfferQueue_.end(),
	    offers.begin(),
	    offers.begin() + queuedCount
	);
	bossAcquisitionPlayer_ = player;
	if (!IsLevelUpSelectionActive()) {
		ShowNextBossAcquisitionOffer();
	}
}

/// <summary>予約候補を再検証し、「取得する／見送る」の2択としてレベルアップUIへ表示します。</summary>
bool BaseScene::ShowNextBossAcquisitionOffer() {
	// 予約後にプレイヤー設定が変更されても、表示時点の新規取得条件で再検証する。
	const auto otherPlayerInitialAttacks = bossAcquisitionPlayer_
	    ? GetOtherPlayerInitialAttacks(*bossAcquisitionPlayer_)
	    : std::unordered_set<std::string>{};
	while (bossAcquisitionPlayer_ && !bossAcquisitionOfferQueue_.empty()) {
		LevelUpChoice offer = bossAcquisitionOfferQueue_.front();
		bossAcquisitionOfferQueue_.erase(bossAcquisitionOfferQueue_.begin());
		const PlayerStats& stats = bossAcquisitionPlayer_->GetBaseStats();
		// 前の確認中にスロット状態が変わる可能性があるため、表示直前に所有状態と空き位置を再確認する。
		if (offer.type == LevelUpChoiceType::NewAttack) {
			const auto owned = std::find_if(stats.attackSlots.begin(), stats.attackSlots.end(), [&offer](const PlayerAttackSlot& slot) {
				return slot.attackName == offer.name;
			});
			const auto empty = std::find_if(stats.attackSlots.begin(), stats.attackSlots.end(), [](const PlayerAttackSlot& slot) {
				return slot.attackName.empty();
			});
			if (owned != stats.attackSlots.end() || empty == stats.attackSlots.end() ||
			    otherPlayerInitialAttacks.contains(offer.name)) {
				continue;
			}
			offer.slotIndex = static_cast<int>(std::distance(stats.attackSlots.begin(), empty));
		} else if (offer.type == LevelUpChoiceType::NewStatus) {
			const auto owned = std::find_if(stats.statusSlots.begin(), stats.statusSlots.end(), [&offer](const PlayerStatusSlot& slot) {
				return slot.statusName == offer.name;
			});
			const auto empty = std::find_if(stats.statusSlots.begin(), stats.statusSlots.end(), [](const PlayerStatusSlot& slot) {
				return slot.statusName.empty();
			});
			if (owned != stats.statusSlots.end() || empty == stats.statusSlots.end()) {
				continue;
			}
			offer.slotIndex = static_cast<int>(std::distance(stats.statusSlots.begin(), empty));
		} else {
			continue;
		}

		// 既存のレベルアップUIを再利用し、候補カードと見送りカードだけを表示する。
		levelUpPlayer_ = bossAcquisitionPlayer_;
		levelUpChoices_.clear();
		levelUpChoices_.push_back(offer);
		levelUpChoices_.push_back({
			LevelUpChoiceType::Decline,
			"",
			"Do not acquire",
			"Skip this reward.",
			-1,
			""
		});
		selectedLevelUpChoiceIndex_ = 0;
		SetGameplayMode(GameplayMode::BossReward);
		return true;
	}
	bossAcquisitionPlayer_ = nullptr;
	return false;
}

void BaseScene::UpdateEnemyAttacks() {
	std::vector<EnemyShotRequest> shotRequests;
	for (const auto& object : sceneObjects_) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f) {
			continue;
		}
		if (enemy->ConsumeSelfDestructRequest()) {
			// 爆発要求はこのフレームで一度だけ処理し、EnemyComponent側で体力0にして後続の掃除対象にする。
			GameObject* target = enemy->GetTarget();
			Player* player = target ? target->GetComponent<Player>() : nullptr;
			if (player && player->IsEnabled() && player->GetCurrentHealth() > 0.0f) {
				Vector3 toPlayer = target->GetTransform().translate - object->GetTransform().translate;
				toPlayer.y = 0.0f;
				if (Length(toPlayer) <= enemy->GetStats().selfDestructRadius) {
					player->TakeDamage(enemy->GetStats().attack);
				}
			}
			continue;
		}
		std::vector<EnemyShotRequest> requests = enemy->ConsumeShotRequests();
		shotRequests.insert(shotRequests.end(), requests.begin(), requests.end());
		const EnemyBehaviorPresentation presentation = enemy->GetBehaviorPresentation();

		if (TrailRendererComponent* trail = object->GetComponent<TrailRendererComponent>()) {
			// 待機中の不要な軌跡を防ぎ、斬撃判定が有効な期間だけ新しい点を記録する。
			trail->SetEmitting(presentation.attackTrailEmitting);
		}

		if (Object3dComponent* object3d = object->GetComponent<Object3dComponent>()) {
			if (presentation.overrideColor) {
				object3d->SetColor(presentation.color);
			} else if (enemy->GetEnemyTypeName() == "MidBoss") {
				// 行動は通常追跡のまま、紫色と大型シルエットで中ボスだと識別できるようにする。
				object3d->SetColor({0.62f, 0.16f, 0.85f, 1.0f});
			}
		}
	}
	for (const EnemyShotRequest& request : shotRequests) {
		CreateRuntimeEnemyProjectile(request);
	}
}

/// <summary>敵の発射要求から、移動・描画・当たり判定を持つ敵弾を生成します。</summary>
GameObject* BaseScene::CreateRuntimeEnemyProjectile(const EnemyShotRequest& request) {
	auto object = std::make_unique<GameObject>();
	// 軌道タイプは移動だけでなく、竜巻ごとの名前・色・軌跡表現の選択にも使用する。
	const bool isContractingTornado = request.motionType == EnemyProjectileMotionType::ContractingOrbit;
	const bool isGiantTornado = request.motionType == EnemyProjectileMotionType::Homing;
	const bool isTornado = request.motionType == EnemyProjectileMotionType::ExpandingOrbit ||
	    request.motionType == EnemyProjectileMotionType::ContractingOrbit || isGiantTornado;
	object->SetName(MakeUniqueObjectName(
	    isGiantTornado ? "BossGiantTornado"
	    : isContractingTornado ? "BossConvergingTornado" : isTornado ? "BossTornado" : "EnemyProjectile"));
	object->SetEditorType("EnemyProjectile");
	object->GetTransform().translate = request.position;
	object->GetTransform().scale = {request.size, request.size, request.size};
	object->GetTransform().rotate.y = std::atan2(request.direction.x, request.direction.z);

	EnemyProjectileComponent* projectile = object->AddComponent<EnemyProjectileComponent>();
	projectile->SetDirection(request.direction);
	if (request.motionType == EnemyProjectileMotionType::ExpandingOrbit) {
		projectile->SetExpandingOrbit(
		    request.orbitCenter,
		    request.orbitAngle,
		    request.orbitInitialRadius,
		    request.orbitAngularSpeed,
		    request.orbitRadialSpeed,
		    request.orbitHeight);
	} else if (request.motionType == EnemyProjectileMotionType::ContractingOrbit) {
		projectile->SetContractingOrbit(
		    request.orbitCenter,
		    request.orbitAngle,
		    request.orbitInitialRadius,
		    request.orbitAngularSpeed,
		    request.orbitRadialSpeed,
		    request.orbitHeight);
	} else if (isGiantTornado) {
		projectile->SetHomingTarget(request.homingTarget);
	}
	projectile->SetSpeed(request.speed);
	projectile->SetAttack(request.attack);
	projectile->SetSize(request.size);
	projectile->SetLifeTime(request.lifeTime);

	// 敵弾の見た目と同じ範囲を命中判定に使用する。
	SphereColliderComponent* collider = object->AddComponent<SphereColliderComponent>();
	// Transform の scale が弾サイズなので、ローカル半径 1.0 でワールド半径が request.size になる。
	collider->SetRadius(1.0f);
	collider->SetPushBackEnabled(false);

	ModelManager::GetInstance()->LoadModel("sphere.obj");
	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	object3d->SetModel("sphere.obj");
	object3d->SetColor(isGiantTornado
	    ? Vector4{0.20f, 1.0f, 0.48f, 1.0f}
	    : isContractingTornado
	    ? Vector4{0.78f, 0.35f, 1.0f, 1.0f}
	    : isTornado ? Vector4{0.50f, 0.92f, 1.0f, 1.0f} : Vector4{1.0f, 0.12f, 0.04f, 1.0f});
	// 竜巻弾は球モデルではなく、上方へ広がる渦パーティクルを本体表示として使う。
	object3d->SetEnabled(!isTornado);
	if (isTornado) {
		// パーティクルだけでは途切れて見える円錐輪郭を、回転する螺旋帯と接地リングで補完する。
		object->AddComponent<TornadoVisualComponent>(isContractingTornado, isGiantTornado);
		// 全竜巻で同じ描画グループを共有し、竜巻ごとのGPUバッファ生成を避ける。
		constexpr const char* kTornadoParticleGroup = "RuntimeTornadoWispParticle";
		if (!ParticleManager::GetInstance()->HasGroup(kTornadoParticleGroup)) {
			ParticleManager::GetInstance()->CreateParticleGroup(
			    kTornadoParticleGroup, "Resources/tornado_wisp.png", kMeshTypeQuad);
		}

		ParticleEmitterComponent* tornadoEmitter = object->AddComponent<ParticleEmitterComponent>();
		tornadoEmitter->SetGroupName(kTornadoParticleGroup);
		// 色や生成周期などの共通値をプリセットから読み、攻撃サイズ依存値だけ下で上書きする。
		ParticlePresetRepository::Apply("Tornado", tornadoEmitter);
		tornadoEmitter->SetFrequency(isGiantTornado ? 0.040f : 0.055f);

		ParticleEmitParam tornadoParam = tornadoEmitter->GetParam();
		// 当たり判定サイズに比例させ、通常・収束・巨大竜巻でシルエットの比率を統一する。
		tornadoParam.vortexBaseRadius = (std::max)(0.10f, request.size * 0.16f);
		tornadoParam.vortexTopRadius = (std::max)(0.65f, request.size * 1.45f);
		tornadoParam.vortexHeight = (std::max)(1.8f, request.size * 3.6f);
		// 三日月状の風テクスチャを少し横長にし、短い煙粒ではなく旋回する帯として重ねる。
		tornadoParam.scale = {request.size * 0.62f, request.size * 0.44f, request.size * 0.44f};
		tornadoParam.endScale = {request.size * 0.78f, request.size * 0.58f, request.size * 0.58f};
		tornadoParam.randomScaleRange = {
		    request.size * 0.14f, request.size * 0.10f, 0.0f};
		tornadoParam.count = isGiantTornado ? 7u : 4u;
		tornadoParam.lifeTime = isGiantTornado ? 1.65f : 1.35f;
		tornadoParam.baseVelocity.y = tornadoParam.vortexHeight / (tornadoParam.lifeTime * 60.0f) * 0.62f;
		tornadoParam.randomVelocityRange.y = tornadoParam.baseVelocity.y * 0.30f;
		tornadoParam.acceleration.y = 0.00035f;
		tornadoParam.vortexAngularSpeed = isContractingTornado ? -12.5f : isGiantTornado ? 9.5f : 12.5f;
		// 攻撃パターンを見分けられるよう、巨大は緑、収束は紫、通常は水色にする。
		tornadoParam.color = isGiantTornado
		    ? Vector4{0.38f, 1.0f, 0.58f, 0.78f}
		    : isContractingTornado
		    ? Vector4{0.82f, 0.48f, 1.0f, 0.78f}
		    : Vector4{0.58f, 0.92f, 1.0f, 0.78f};
		tornadoParam.endColor = {tornadoParam.color.x, tornadoParam.color.y, tornadoParam.color.z, 0.0f};
		tornadoEmitter->SetParam(tornadoParam);
	}
	TrailRendererComponent* trail = object->AddComponent<TrailRendererComponent>();
	trail->SetWidth((std::max)(0.10f, request.size * (isTornado ? 0.72f : 0.8f)));
	trail->SetLifeTime(isTornado ? 0.62f : 0.28f);
	trail->SetHeadColor(isGiantTornado
	    ? Vector4{0.72f, 1.0f, 0.82f, 0.98f}
	    : isContractingTornado
	    ? Vector4{0.95f, 0.75f, 1.0f, 0.95f}
	    : isTornado ? Vector4{0.80f, 1.0f, 1.0f, 0.95f} : Vector4{1.0f, 0.35f, 0.05f, 0.95f});
	trail->SetTailColor(isGiantTornado
	    ? Vector4{0.02f, 0.42f, 0.16f, 0.0f}
	    : isContractingTornado
	    ? Vector4{0.35f, 0.02f, 0.60f, 0.0f}
	    : isTornado ? Vector4{0.05f, 0.30f, 0.55f, 0.0f} : Vector4{0.70f, 0.02f, 0.01f, 0.0f});

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

/// <summary>有効な敵弾とプレイヤーのコライダーを判定し、命中時にダメージと弾の消滅を処理します。</summary>
void BaseScene::UpdateEnemyProjectileHits() {
	for (const auto& projectileObject : sceneObjects_) {
		EnemyProjectileComponent* projectile = projectileObject->GetComponent<EnemyProjectileComponent>();
		if (!projectile || !projectile->IsEnabled() || projectile->IsExpired()) {
			continue;
		}
		for (const auto& playerObject : sceneObjects_) {
			Player* player = playerObject->GetComponent<Player>();
			if (!player || !player->IsEnabled() || player->GetCurrentHealth() <= 0.0f) {
				continue;
			}

			// 通常は生成時に追加した球コライダーを使い、旧データなど未設定の場合は弾サイズから補完する。
			const SphereColliderComponent* projectileCollider =
			    projectileObject->GetComponent<SphereColliderComponent>();
			const SphereColliderShape projectileSphere = projectileCollider
			    ? projectileCollider->GetWorldSphere()
			    : SphereColliderShape{projectileObject->GetTransform().translate, projectile->GetSize()};
			const Vector3 projectileStart = projectile->GetPreviousPosition();
			const Vector3 projectileEnd = projectileSphere.center;

			bool isHit = false;
			// プレイヤーの実形状を優先し、OBB と球のどちらの構成にも対応する。
			if (const OBBColliderComponent* playerCollider = playerObject->GetComponent<OBBColliderComponent>();
			    playerCollider && playerCollider->IsEnabled()) {
				isHit = IsSweptSphereToOBB(
				    projectileStart, projectileEnd, projectileSphere.radius, playerCollider->GetWorldOBB());
			}
			if (!isHit) {
				if (const SphereColliderComponent* playerCollider = playerObject->GetComponent<SphereColliderComponent>();
				    playerCollider && playerCollider->IsEnabled()) {
					isHit = IsSweptSphereToSphere(
					    projectileStart, projectileEnd, projectileSphere.radius, playerCollider->GetWorldSphere());
				}
			}
			// コライダーを持たないプレイヤーでも従来どおり最低限の命中判定を行う。
			if (!isHit &&
			    !playerObject->GetComponent<OBBColliderComponent>() &&
			    !playerObject->GetComponent<SphereColliderComponent>()) {
				const SphereColliderShape fallbackPlayerSphere{playerObject->GetTransform().translate, 0.5f};
				isHit = IsSweptSphereToSphere(
				    projectileStart, projectileEnd, projectileSphere.radius, fallbackPlayerSphere);
			}

			if (isHit) {
				// 1発で複数回ダメージを与えないよう、命中済みにして後段の削除処理へ渡す。
				player->TakeDamage(projectile->GetAttack());
				projectile->MarkHit();
				break;
			}
		}
	}
}

/// <summary>
/// 近くにある同じ単位の通常経験値を、合計値を維持したまま次の単位へ圧縮します。
/// </summary>
void BaseScene::UpdateExperienceCompression() {
	// 小さい単位から順に処理することで、1→10→50→100の連鎖圧縮を同じフレーム内で成立させる。
	for (size_t denominationIndex = 0; denominationIndex + 1 < kExperienceDenominations.size(); ++denominationIndex) {
		const int denomination = kExperienceDenominations[denominationIndex];
		const int nextDenomination = kExperienceDenominations[denominationIndex + 1];
		// 例: 1→10は10個、10→50は5個、50→100は2個必要になる。
		const int requiredCount = nextDenomination / denomination;

		// 同じ段階で複数組を圧縮できるよう、候補がなくなるまで検索を繰り返す。
		bool compressed = true;
		while (compressed) {
			compressed = false;
			for (const auto& anchorObject : sceneObjects_) {
				ExperienceComponent* anchor = anchorObject->GetComponent<ExperienceComponent>();
				// ボス強化報酬や、すでに回収・圧縮されたオブジェクトは通常経験値としてまとめない。
				if (!anchor || anchor->IsBossUpgradeReward() || anchor->IsCollected() || anchor->IsConsumedByCompression() ||
				    anchor->GetExperience() != denomination) {
					continue;
				}

				// 基準位置の周囲から、同じ単位の経験値を必要数だけ集める。
				std::vector<GameObject*> nearbyObjects;
				nearbyObjects.reserve(requiredCount);
				nearbyObjects.push_back(anchorObject.get());
				const Vector3 anchorPosition = anchorObject->GetTransform().translate;
				for (const auto& candidateObject : sceneObjects_) {
					if (candidateObject.get() == anchorObject.get()) {
						continue;
					}
					ExperienceComponent* candidate = candidateObject->GetComponent<ExperienceComponent>();
					if (!candidate || candidate->IsBossUpgradeReward() || candidate->IsCollected() || candidate->IsConsumedByCompression() ||
					    candidate->GetExperience() != denomination) {
						continue;
					}
					if (Length(candidateObject->GetTransform().translate - anchorPosition) <= kExperienceCompressionDistance) {
						nearbyObjects.push_back(candidateObject.get());
						if (static_cast<int>(nearbyObjects.size()) == requiredCount) {
							break;
						}
					}
				}
				if (static_cast<int>(nearbyObjects.size()) < requiredCount) {
					continue;
				}

				// まとめた経験値の中心へ残存オブジェクトを移し、次の単位の見た目と値へ更新する。
				Vector3 mergedPosition{};
				for (GameObject* object : nearbyObjects) {
					mergedPosition = mergedPosition + object->GetTransform().translate;
				}
				mergedPosition = (1.0f / static_cast<float>(requiredCount)) * mergedPosition;
				anchorObject->GetTransform().translate = mergedPosition;
				const float scale = GetExperienceScale(nextDenomination);
				anchorObject->GetTransform().scale = {scale, scale, scale};
				anchor->SetExperience(nextDenomination);
				if (Object3dComponent* object3d = anchorObject->GetComponent<Object3dComponent>()) {
					object3d->SetColor(GetExperienceColor(nextDenomination));
				}
				// 基準以外は削除処理へ渡す。経験値は基準側へ集約済みなので、プレイヤーには加算しない。
				for (size_t index = 1; index < nearbyObjects.size(); ++index) {
					nearbyObjects[index]->GetComponent<ExperienceComponent>()->MarkConsumedByCompression();
				}
				compressed = true;
				break;
			}
		}
	}
}

void BaseScene::UpdateItemDrops() {
	// 接触判定はItemDropComponentが行い、シーン内の他オブジェクトが必要な効果だけをここで適用する。
	for (const auto& itemObject : sceneObjects_) {
		ItemDropComponent* item = itemObject->GetComponent<ItemDropComponent>();
		if (!item || !item->IsCollected() || item->IsEffectApplied()) {
			continue;
		}

		if (item->GetType() == ItemDropType::CollectAllExperience) {
			// ボス強化報酬や取得・圧縮済みの経験値は通常経験値ではないため対象外とする。
			for (const auto& experienceObject : sceneObjects_) {
				ExperienceComponent* experience = experienceObject->GetComponent<ExperienceComponent>();
				if (!experience || experience->IsBossUpgradeReward() || experience->IsCollected() ||
				    experience->IsConsumedByCompression()) {
					continue;
				}
				// 距離制限を実質解除し、フィールド上の経験値を一斉にプレイヤーへ向かわせる。
				experience->SetAttractDistance((std::numeric_limits<float>::max)());
				experience->SetAttractSpeed(0.22f);
			}
		} else if (item->GetType() == ItemDropType::Money) {
			// Gドロップを拾った瞬間に倍率を掛け、ゲームを途中終了しても獲得分が残るよう即時保存する。
			const int baseMoney = item->GetMoneyAmount();
			const int gainedMoney = sceneManager ? sceneManager->ApplyGlobalGoldBonus(baseMoney) : baseMoney;
			// リザルト集計と永続所持金へ同じ補正後金額を渡し、画面表示との差を作らない。
			challengeMoneyEarned_ += gainedMoney;
			if (sceneManager) sceneManager->AddMoney(gainedMoney);
		}
		item->MarkEffectApplied();
	}
}

GameObject* BaseScene::FindNearestEnemy(const Vector3& position) const {
	GameObject* nearest = nullptr;
	float nearestDistance = 0.0f;
	for (const auto& object : sceneObjects_) {
		EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		if (!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f) {
			continue;
		}

		const float distance = Length(object->GetTransform().translate - position);
		if (!nearest || distance < nearestDistance) {
			nearest = object.get();
			nearestDistance = distance;
		}
	}
	return nearest;
}

GameObject* BaseScene::CreateRuntimePlayerProjectile(const PlayerAttackShotRequest& request) {
	auto object = std::make_unique<GameObject>();
	object->SetName(MakeUniqueObjectName(request.attackName.empty() ? "PlayerAttack" : request.attackName));
	object->SetEditorType("PlayerProjectile");
	object->GetTransform().translate = request.position;
	object->GetTransform().scale = {request.size, request.size, request.size};

	PlayerProjectileComponent* projectile = object->AddComponent<PlayerProjectileComponent>();
	projectile->SetAttackName(request.attackName);
	projectile->SetLevel(request.level);
	projectile->SetDirection(request.direction);
	projectile->SetSpeed(request.speed);
	projectile->SetAttack(request.attack);
	projectile->SetSize(request.size);
	projectile->SetLifeTime(request.lifeTime);
	projectile->SetPierceCount(request.pierceCount);
	projectile->SetInfinitePierce(request.infinitePierce);
	projectile->SetHomingEnabled(request.homing);
	projectile->SetHomingAccuracy(request.homingAccuracy);
	projectile->SetMotionType(request.motionType);
	projectile->SetMotionAnchor(request.motionAnchor);
	projectile->SetOrbitAngleRadians(request.orbitAngleRadians);
	projectile->SetOrbitRadius(request.orbitRadius);
	projectile->SetOrbitHeight(request.orbitHeight);
	projectile->SetOrbitAngularSpeed(request.orbitAngularSpeed);
	projectile->SetTravelDistance(request.travelDistance);
	projectile->SetTravelOrigin(request.position);
	projectile->SetClawSlashIndex(request.clawSlashIndex);
	projectile->SetClawSlashCount(request.clawSlashCount);
	const PlayerProjectilePresentation presentation = projectile->GetMotionPresentation();
	if (presentation.repeatHitInterval > 0.0f) {
		projectile->SetRepeatHitInterval(presentation.repeatHitInterval);
	}
	if (request.homing) {
		projectile->SetHomingTarget(FindNearestEnemy(request.position));
	}
	if (presentation.aimAtNearestEnemyOnSpawn) {
		if (GameObject* target = FindNearestEnemy(request.position)) {
			Vector3 toTarget = target->GetTransform().translate - request.position;
			toTarget.y = 0.0f;
			if (Length(toTarget) > MathConstants::kDirectionEpsilon) {
				projectile->SetDirection(toTarget);
			}
		}
	}

	if (request.attackName == "Straight" && request.motionType == PlayerProjectileMotionType::Linear) {
		// 共通の弾パラメーター設定後に描画だけを差し替え、他の攻撃のモデル・軌跡には影響させない。
		// 球モデルと直線のトレイルを使わず、初フレームから完成した斬撃を表示する。
		object->AddComponent<StraightSlashVisualComponent>(request.lifeTime);
		object->Update();
		sceneObjects_.push_back(std::move(object));
		++nextObjectId_;
		return sceneObjects_.back().get();
	}

	const std::string modelFilePath = request.modelFilePath.empty() ? "sphere.obj" : request.modelFilePath;
	if (!ModelManager::GetInstance()->FindModel(modelFilePath)) {
		ModelManager::GetInstance()->LoadModel(modelFilePath);
	}
	object->GetTransform().scale = {
		request.size * presentation.scaleMultiplier.x,
		request.size * presentation.scaleMultiplier.y,
		request.size * presentation.scaleMultiplier.z
	};
	Object3dComponent* object3d = object->AddComponent<Object3dComponent>();
	if (ModelManager::GetInstance()->FindModel(modelFilePath)) {
		object3d->SetModel(modelFilePath);
	} else {
		ModelManager::GetInstance()->LoadModel("sphere.obj");
		object3d->SetModel("sphere.obj");
	}
	object3d->SetEnabled(presentation.modelVisible);
	if (presentation.useMagatamaPalette) {
		// 白い玉部分だけを6色に染め、黒い孔の象嵌は輪郭として残す。
		object3d->SetColor(GetMagatamaProjectileColor(request.colorIndex));
	} else if (presentation.overrideModelColor) {
		object3d->SetColor(presentation.modelColor);
	}
	if (presentation.addSkyLaserVisual) {
		// 球を引き伸ばした旧表示は隠し、加算合成の専用エフェクトでレーザーを描画する。
		object->AddComponent<SkyLaserVisualComponent>(request.lifeTime, request.size);
	}
	if (presentation.addTrail) {
		TrailRendererComponent* trail = object->AddComponent<TrailRendererComponent>();
		trail->SetWidth((std::max)(presentation.minimumTrailWidth, request.size * presentation.trailWidthMultiplier));
		trail->SetLifeTime(presentation.trailLifeTime);
		trail->SetMinSegmentLength((std::max)(0.025f, request.size * 0.08f));
		if (presentation.trailUsesMotionAnchor) {
			trail->SetPositionReference(request.motionAnchor);
		}
		if (presentation.useMagatamaPalette) {
			Vector4 headColor = GetMagatamaProjectileColor(request.colorIndex);
			headColor.w = 0.95f;
			trail->SetHeadColor(headColor);
			trail->SetTailColor(GetMagatamaTrailTailColor(request.colorIndex));
		} else if (presentation.overrideTrailColors) {
			trail->SetHeadColor(presentation.trailHeadColor);
			trail->SetTailColor(presentation.trailTailColor);
		}
		if (presentation.addCoreTrail) {
			// 細い白熱線を外光へ重ね、一本ごとの切っ先を読み取りやすくする。
			TrailRendererComponent* coreTrail = object->AddComponent<TrailRendererComponent>();
			coreTrail->SetWidth((std::max)(0.045f, request.size * 0.085f));
			coreTrail->SetLifeTime(0.16f);
			coreTrail->SetMinSegmentLength((std::max)(0.018f, request.size * 0.055f));
			coreTrail->SetPositionReference(request.motionAnchor);
			coreTrail->SetHeadColor({1.0f, 1.0f, 0.86f, 1.0f});
			coreTrail->SetTailColor({1.0f, 0.28f, 0.02f, 0.0f});
		}
	}
	if (presentation.addGlowEmitter) {
		// 環境反射ではなく、加算合成パーティクルを弾から漏れ出す光として連続発生させる。
		constexpr const char* kGlowParticleGroup = "MagatamaGlow";
		constexpr const char* kGlowTexture = "Resources/circle.png";
		if (!ParticleManager::GetInstance()->HasGroup(kGlowParticleGroup)) {
			ParticleManager::GetInstance()->CreateParticleGroup(kGlowParticleGroup, kGlowTexture, kMeshTypeQuad);
		}

		ParticleEmitterComponent* glowEmitter = object->AddComponent<ParticleEmitterComponent>();
		glowEmitter->SetGroupName(kGlowParticleGroup);
		glowEmitter->SetTexture(kGlowTexture);
		glowEmitter->SetBlendMode(kBlendModeAdd);
		glowEmitter->SetFrequency(0.020f);

		Vector4 glowColor = GetMagatamaProjectileColor(request.colorIndex);
		glowColor.x *= 1.35f;
		glowColor.y *= 1.35f;
		glowColor.z *= 1.35f;
		glowColor.w = 0.55f;

		ParticleEmitParam glowParam;
		const float glowScale = (std::max)(0.16f, request.size * 0.82f);
		glowParam.scale = {glowScale, glowScale, glowScale};
		glowParam.endScale = {glowScale * 0.12f, glowScale * 0.12f, glowScale * 0.12f};
		glowParam.baseVelocity = {0.0f, 0.008f, 0.0f};
		glowParam.randomVelocityRange = {0.018f, 0.018f, 0.018f};
		const float leakRadius = request.size * 0.42f;
		glowParam.randomPositionRange = {leakRadius, leakRadius, leakRadius};
		glowParam.lifeTime = 0.36f;
		glowParam.color = glowColor;
		glowParam.endColor = {glowColor.x * 0.55f, glowColor.y * 0.55f, glowColor.z * 0.55f, 0.0f};
		glowParam.randomScaleRange = {glowScale * 0.22f, glowScale * 0.22f, glowScale * 0.22f};
		glowParam.count = 2;
		glowParam.isBillboard = true;
		glowEmitter->SetParam(glowParam);
		// Component::Update より前に即時発生させるため、初回分も弾の生成位置へ同期しておく。
		glowEmitter->SetTranslate(request.position);
		glowEmitter->Emit();
	}

	object->Update();
	sceneObjects_.push_back(std::move(object));
	++nextObjectId_;
	return sceneObjects_.back().get();
}

void BaseScene::UpdatePlayerProjectileHits() {
	struct ExperienceDropRequest {
		EnemyStats stats;
		Vector3 position;
		GameObject* target = nullptr;
	};
	struct BossUpgradeDropRequest {
		// 生成する報酬は常に1個で、upgradeCountだけが70%/25%/5%の抽選結果に応じて変化する。
		Vector3 position;
		GameObject* target = nullptr;
		int upgradeCount = 1;
	};
	struct ItemDropRequest {
		ItemDropType type = ItemDropType::Health;
		Vector3 position;
		GameObject* target = nullptr;
		float healAmount = 0.0f;
		int moneyAmount = 0;
	};
	std::vector<ExperienceDropRequest> experienceDropRequests;
	std::vector<BossUpgradeDropRequest> bossUpgradeDropRequests;
	// sceneObjects_の走査中に要素を追加すると反復子が無効になるため、生成要求を一旦蓄積する。
	std::vector<ItemDropRequest> itemDropRequests;
	Camera* camera = Object3dCommon::GetInstance() ? Object3dCommon::GetInstance()->GetDefaultCamera() : nullptr;
	for (const auto& projectileObject : sceneObjects_) {
		PlayerProjectileComponent* projectile = projectileObject->GetComponent<PlayerProjectileComponent>();
		if (!projectile || projectile->IsExpired() ||
		    !projectile->GetMotionPresentation().ricochets) {
			continue;
		}

		Vector3& position = projectileObject->GetTransform().translate;
		Vector3 currentNdc{};
		Vector3 aheadNdc{};
		// ワールド座標を画面座標へ投影し、左右端・上下端に応じて画面上の進行成分を反転する。
		if (camera && ProjectToNdc(camera, position, currentNdc) &&
		    ProjectToNdc(camera, position + projectile->GetDirection(), aheadNdc)) {
			const float edgeInset = (std::clamp)(0.025f + projectile->GetSize() * 0.02f, 0.025f, 0.20f);
			const float boundary = 1.0f - edgeInset;
			const bool hitHorizontalEdge = currentNdc.x < -boundary || currentNdc.x > boundary;
			const bool hitVerticalEdge = currentNdc.y < -boundary || currentNdc.y > boundary;
			if (hitHorizontalEdge || hitVerticalEdge) {
				Vector2 screenDirection{aheadNdc.x - currentNdc.x, aheadNdc.y - currentNdc.y};
				if (hitHorizontalEdge) {
					screenDirection.x = -screenDirection.x;
				}
				if (hitVerticalEdge) {
					screenDirection.y = -screenDirection.y;
				}
				const Vector2 clampedNdc{
				    (std::clamp)(currentNdc.x, -boundary, boundary),
				    (std::clamp)(currentNdc.y, -boundary, boundary)
				};
				const Vector2 reflectedNdc{clampedNdc.x + screenDirection.x, clampedNdc.y + screenDirection.y};
				Vector3 boundaryPoint{};
				Vector3 reflectedPoint{};
				if (IntersectScreenRayToHeight(camera, clampedNdc, position.y, boundaryPoint) &&
				    IntersectScreenRayToHeight(camera, reflectedNdc, position.y, reflectedPoint)) {
					position = boundaryPoint;
					projectile->SetDirection(reflectedPoint - boundaryPoint);
				}
			}
		}

		// 障害物との反射は弾を球として扱い、衝突面の法線から反射ベクトルを求める。
		SphereColliderShape projectileSphere{position, projectile->GetSize()};
		for (const auto& obstacleObject : sceneObjects_) {
			if (obstacleObject.get() == projectileObject.get() ||
			    obstacleObject->GetComponent<Player>() ||
			    obstacleObject->GetComponent<EnemyComponent>() ||
			    obstacleObject->GetComponent<PlayerProjectileComponent>()) {
				continue;
			}

			Vector3 surfaceNormal{};
			float penetration = 0.0f;
			bool collided = false;
			if (OBBColliderComponent* obstacle = obstacleObject->GetComponent<OBBColliderComponent>();
			    obstacle && obstacle->IsEnabled() && obstacle->GetPushBackEnabled()) {
				collided = BaseSceneCollisionHelpers::CalculateOBBSpherePushBack(obstacle->GetWorldOBB(), projectileSphere, surfaceNormal, penetration);
			}
			if (!collided) {
				if (SphereColliderComponent* obstacle = obstacleObject->GetComponent<SphereColliderComponent>();
				    obstacle && obstacle->IsEnabled() && obstacle->GetPushBackEnabled()) {
					Vector3 projectileToObstacle{};
					collided = BaseSceneCollisionHelpers::CalculateSphereSpherePushBack(projectileSphere, obstacle->GetWorldSphere(), projectileToObstacle, penetration);
					surfaceNormal = -1.0f * projectileToObstacle;
				}
			}
			if (!collided) {
				continue;
			}

			surfaceNormal.y = 0.0f;
			if (Length(surfaceNormal) <= MathConstants::kDirectionEpsilon) {
				continue;
			}
			surfaceNormal = NormalizeReturnVector(surfaceNormal);
			const float incomingAmount = Dot(projectile->GetDirection(), surfaceNormal);
			if (incomingAmount >= 0.0f) {
				continue;
			}
			// めり込みを解消してから反射させ、同じ面へ連続衝突することを防ぐ。
			position = position + (penetration + 0.01f) * surfaceNormal;
			projectile->SetDirection(projectile->GetDirection() - (2.0f * incomingAmount) * surfaceNormal);
			projectileObject->GetTransform().rotate.y = std::atan2(projectile->GetDirection().x, projectile->GetDirection().z);
			break;
		}
	}
	for (const auto& projectileObject : sceneObjects_) {
		PlayerProjectileComponent* projectile = projectileObject->GetComponent<PlayerProjectileComponent>();
		if (!projectile || !projectile->IsEnabled() || projectile->IsExpired()) {
			continue;
		}

		for (const auto& enemyObject : sceneObjects_) {
			EnemyComponent* enemy = enemyObject->GetComponent<EnemyComponent>();
			if (!enemy || !enemy->IsEnabled() || enemy->GetCurrentHealth() <= 0.0f) {
				continue;
			}
			if (projectile->HasHitObject(enemyObject.get())) {
				continue;
			}

			float distance = Length(enemyObject->GetTransform().translate - projectileObject->GetTransform().translate);
			float hitRadius = projectile->GetSize() + 0.5f * enemy->GetStats().sizeScale;
			if (projectile->GetMotionPresentation().usesVerticalHitArea) {
				Vector3 horizontalDifference = enemyObject->GetTransform().translate - projectileObject->GetTransform().translate;
				horizontalDifference.y = 0.0f;
				distance = Length(horizontalDifference);
				hitRadius = projectile->GetSize() * 0.35f + 0.5f * enemy->GetStats().sizeScale;
			}
			if (distance <= hitRadius) {
				enemy->SetCurrentHealth(enemy->GetCurrentHealth() - projectile->GetAttack());
				if (enemy->GetCurrentHealth() <= 0.0f) {
					// 生存中の敵だけがこの分岐へ入るため、1体につき一度だけ加算される。
					++defeatedEnemyCount_;
					experienceDropRequests.push_back({enemy->GetStats(), enemyObject->GetTransform().translate, enemy->GetTarget()});
					static std::mt19937 itemDropRandomEngine(std::random_device{}());
					std::uniform_real_distribution<float> itemDropDistribution(0.0f, 1.0f);
					const EnemyStats& enemyStats = enemy->GetStats();
					// 各アイテムを独立して抽選するため、同じ敵から複数種類が同時に落ちる場合がある。
					if (itemDropDistribution(itemDropRandomEngine) < enemyStats.healthItemDropChance) {
						itemDropRequests.push_back({
							ItemDropType::Health, enemyObject->GetTransform().translate,
							enemy->GetTarget(), enemyStats.healthItemHealAmount
						});
					}
					if (itemDropDistribution(itemDropRandomEngine) < enemyStats.collectExperienceItemDropChance) {
						itemDropRequests.push_back({
							ItemDropType::CollectAllExperience, enemyObject->GetTransform().translate,
							enemy->GetTarget(), 0.0f
						});
					}
					// 通常敵は20%でコインを落とす。1個あたり5～15G。
					if (itemDropDistribution(itemDropRandomEngine) < 0.20f) {
						std::uniform_int_distribution<int> moneyDistribution(5, 15);
						itemDropRequests.push_back({
							ItemDropType::Money, enemyObject->GetTransform().translate,
							enemy->GetTarget(), 0.0f, moneyDistribution(itemDropRandomEngine)
						});
					}
					const std::string& enemyTypeName = enemy->GetEnemyTypeName();
					// 通常敵を除外し、途中の中ボスと各ステージの最終ボスだけを強化報酬の対象にする。
					const bool dropsBossUpgradeReward =
					    enemyTypeName == "MidBoss" ||
					    enemyTypeName == "ChaserMidBoss" ||
					    enemyTypeName == "ShooterMidBoss" ||
					    enemyTypeName == "ChargerMidBoss" ||
					    enemyTypeName == "Stage2GaleMidBoss" ||
					    enemyTypeName == "Stage2Boss";
					if (dropsBossUpgradeReward) {
						static std::mt19937 bossRewardRandomEngine(std::random_device{}());
						std::uniform_int_distribution<int> rewardRollDistribution(1, 100);
						const int rewardRoll = rewardRollDistribution(bossRewardRandomEngine);
						// 1～70=1回、71～95=3回、96～100=5回（70% / 25% / 5%）。
						const int upgradeCount = rewardRoll <= 70 ? 1 : rewardRoll <= 95 ? 3 : 5;
						bossUpgradeDropRequests.push_back({
							enemyObject->GetTransform().translate,
							enemy->GetTarget(),
							upgradeCount
						});
					}
				}
				projectile->RegisterHitObject(enemyObject.get());
				break;
			}
		}
	}

	for (const ExperienceDropRequest& request : experienceDropRequests) {
		CreateRuntimeExperience(request.stats, request.position, request.target);
	}
	for (const BossUpgradeDropRequest& request : bossUpgradeDropRequests) {
		CreateRuntimeBossUpgradeDrop(request.position, request.target, request.upgradeCount);
	}
	for (const ItemDropRequest& request : itemDropRequests) {
		CreateRuntimeItemDrop(request.type, request.position, request.target, request.healAmount, request.moneyAmount);
	}
}

void BaseScene::CleanupExpiredPlayerProjectiles() {
	sceneObjects_.erase(
	    std::remove_if(sceneObjects_.begin(), sceneObjects_.end(), [](const std::unique_ptr<GameObject>& object) {
		    PlayerProjectileComponent* projectile = object->GetComponent<PlayerProjectileComponent>();
		    if (projectile) {
			    // 弾全体が画面端を抜けてから消えるよう、表示サイズを余白へ反映する。
			    const float viewMargin = 0.05f + projectile->GetSize() * 0.02f;
			    // 周回弾や反射弾は専用挙動を維持し、画面外へ飛び去る弾だけを破棄する。
			    const bool shouldExpireOutsideView = projectile->GetMotionPresentation().expiresOutsideView;
			    if (projectile->IsExpired() ||
				    (shouldExpireOutsideView && IsPointOutsideView(object->GetTransform().translate, viewMargin))) {
				    return true;
			    }
		    }
		    EnemyProjectileComponent* enemyProjectile = object->GetComponent<EnemyProjectileComponent>();
		    if (enemyProjectile && enemyProjectile->IsExpired()) {
			    return true;
		    }
		    EnemyComponent* enemy = object->GetComponent<EnemyComponent>();
		    if (enemy && enemy->GetCurrentHealth() <= 0.0f) {
			    return true;
		    }
		    ExperienceComponent* experience = object->GetComponent<ExperienceComponent>();
		    if (experience && (experience->IsCollected() || experience->IsConsumedByCompression())) {
			    return true;
		    }
		    ItemDropComponent* item = object->GetComponent<ItemDropComponent>();
		    return item && item->IsCollected() && item->IsEffectApplied();
	    }),
	    sceneObjects_.end()
	);
	if (selectedObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		selectedObjectIndex_ = static_cast<int>(sceneObjects_.size()) - 1;
	}
}

void BaseScene::UpdatePlayerHealthHud() {
	Player* player = nullptr;
	for (const auto& object : sceneObjects_) {
		Player* candidate = object->GetComponent<Player>();
		if (candidate && candidate->IsEnabled()) {
			player = candidate;
			break;
		}
	}

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
	Player* player = nullptr;
	for (const auto& object : sceneObjects_) {
		Player* candidate = object->GetComponent<Player>();
		if (candidate && candidate->IsEnabled()) {
			player = candidate;
			break;
		}
	}

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
	Player* player = nullptr;
	for (const auto& object : sceneObjects_) {
		Player* candidate = object->GetComponent<Player>();
		if (candidate && candidate->IsEnabled()) {
			player = candidate;
			break;
		}
	}

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
void BaseScene::UpdateColliderCollisions() {
	std::vector<EnemyPlayerContact> enemyPlayerContacts;
	std::vector<OBBColliderComponent*> colliders;
	colliders.reserve(sceneObjects_.size());
	std::vector<SphereColliderComponent*> sphereColliders;
	sphereColliders.reserve(sceneObjects_.size());

	for (const auto& object : sceneObjects_) {
		OBBColliderComponent* collider = object->GetComponent<OBBColliderComponent>();
		if (collider && collider->IsEnabled()) {
			collider->SetColliding(false);
			colliders.push_back(collider);
		} else if (collider) {
			collider->SetColliding(false);
		}
		SphereColliderComponent* sphereCollider = object->GetComponent<SphereColliderComponent>();
		if (sphereCollider && sphereCollider->IsEnabled()) {
			sphereCollider->SetColliding(false);
			sphereColliders.push_back(sphereCollider);
		} else if (sphereCollider) {
			sphereCollider->SetColliding(false);
		}
	}

	for (size_t i = 0; i < colliders.size(); ++i) {
		for (size_t j = i + 1; j < colliders.size(); ++j) {
			GameObject* ownerA = colliders[i]->GetOwner();
			GameObject* ownerB = colliders[j]->GetOwner();
			if (ShouldSkipColliderPair(ownerA, ownerB)) {
				continue;
			}
			const OBBColliderShape colliderA = colliders[i]->GetWorldOBB();
			const OBBColliderShape colliderB = colliders[j]->GetWorldOBB();
			if (IsCollisionOBBToOBB(colliderA, colliderB)) {
				colliders[i]->SetColliding(true);
				colliders[j]->SetColliding(true);
				if (RegisterEnemyPlayerContact(ownerA, ownerB, enemyPlayerContacts)) {
					continue;
				}
				Vector3 direction{};
				float penetration = 0.0f;
				if (BaseSceneCollisionHelpers::CalculateOBBOBBPushBack(colliderA, colliderB, direction, penetration)) {
					BaseSceneCollisionHelpers::ApplyColliderPushBack(
					    ownerA,
					    colliders[i]->GetPushBackEnabled(),
					    ownerB,
					    colliders[j]->GetPushBackEnabled(),
					    direction,
					    penetration
					);
				}
			}
		}
	}

	for (size_t i = 0; i < sphereColliders.size(); ++i) {
		for (size_t j = i + 1; j < sphereColliders.size(); ++j) {
			GameObject* ownerA = sphereColliders[i]->GetOwner();
			GameObject* ownerB = sphereColliders[j]->GetOwner();
			if (ShouldSkipColliderPair(ownerA, ownerB)) {
				continue;
			}
			const SphereColliderShape sphereA = sphereColliders[i]->GetWorldSphere();
			const SphereColliderShape sphereB = sphereColliders[j]->GetWorldSphere();
			if (IsCollisionSphereToSphere(sphereA, sphereB)) {
				sphereColliders[i]->SetColliding(true);
				sphereColliders[j]->SetColliding(true);
				if (RegisterEnemyPlayerContact(ownerA, ownerB, enemyPlayerContacts)) {
					continue;
				}
				Vector3 direction{};
				float penetration = 0.0f;
				if (BaseSceneCollisionHelpers::CalculateSphereSpherePushBack(sphereA, sphereB, direction, penetration)) {
					BaseSceneCollisionHelpers::ApplyColliderPushBack(
					    ownerA,
					    sphereColliders[i]->GetPushBackEnabled(),
					    ownerB,
					    sphereColliders[j]->GetPushBackEnabled(),
					    direction,
					    penetration
					);
				}
			}
		}
	}

	for (OBBColliderComponent* collider : colliders) {
		for (SphereColliderComponent* sphereCollider : sphereColliders) {
			GameObject* obbOwner = collider->GetOwner();
			GameObject* sphereOwner = sphereCollider->GetOwner();
			if (ShouldSkipColliderPair(obbOwner, sphereOwner)) {
				continue;
			}

			const OBBColliderShape obb = collider->GetWorldOBB();
			const SphereColliderShape sphere = sphereCollider->GetWorldSphere();
			if (IsCollisionOBBToSphere(obb, sphere)) {
				collider->SetColliding(true);
				sphereCollider->SetColliding(true);
				if (RegisterEnemyPlayerContact(obbOwner, sphereOwner, enemyPlayerContacts)) {
					continue;
				}
				Vector3 direction{};
				float penetration = 0.0f;
				if (BaseSceneCollisionHelpers::CalculateOBBSpherePushBack(obb, sphere, direction, penetration)) {
					BaseSceneCollisionHelpers::ApplyColliderPushBack(
					    obbOwner,
					    collider->GetPushBackEnabled(),
					    sphereOwner,
					    sphereCollider->GetPushBackEnabled(),
					    direction,
					    penetration
					);
				}
			}
		}
	}

	std::vector<std::pair<Player*, float>> playerDamageTotals;
	// 同じフレームで複数の敵へ触れた場合はプレイヤー単位で合算し、TakeDamageを一度だけ呼ぶ。
	for (const EnemyPlayerContact& contact : enemyPlayerContacts) {
		auto total = std::find_if(playerDamageTotals.begin(), playerDamageTotals.end(), [&contact](const std::pair<Player*, float>& entry) {
			return entry.first == contact.player;
		});
		if (total == playerDamageTotals.end()) {
			playerDamageTotals.push_back({contact.player, contact.enemy->GetContactAttackDamage()});
		} else {
			total->second += contact.enemy->GetContactAttackDamage();
		}
	}
	for (const auto& [player, totalDamage] : playerDamageTotals) {
		player->TakeDamage(totalDamage);
	}
}

/// <summary>
/// マウスクリックによるエディタオブジェクト選択を更新します。
/// </summary>
void BaseScene::UpdateEditorObjectPicking() {
#ifdef USE_IMGUI
	constexpr int kLeftMouseButton = 0;
	Input* input = Input::GetInstance();
	if (!input->TriggerMouseButton(kLeftMouseButton)) {
		return;
	}
	if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
		return;
	}

	Camera* camera = Object3dCommon::GetInstance()->GetDefaultCamera();
	if (!camera) {
		return;
	}

	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	const ImVec2 gameViewPosition = ImGuiManager::GetInstance()->GetGameViewContentPosition();
	const ImVec2 gameViewSize = ImGuiManager::GetInstance()->GetGameViewContentSize();
	const float viewportOriginX = mainViewport ? mainViewport->Pos.x : 0.0f;
	const float viewportOriginY = mainViewport ? mainViewport->Pos.y : 0.0f;
	const float viewLeft = gameViewSize.x > 1.0f ? gameViewPosition.x - viewportOriginX : 0.0f;
	const float viewTop = gameViewSize.y > 1.0f ? gameViewPosition.y - viewportOriginY : 0.0f;
	const float width = gameViewSize.x > 1.0f ? gameViewSize.x : static_cast<float>(input->GetClientWidth());
	const float height = gameViewSize.y > 1.0f ? gameViewSize.y : static_cast<float>(input->GetClientHeight());
	const float mouseX = static_cast<float>(input->GetMouseClientX());
	const float mouseY = static_cast<float>(input->GetMouseClientY());
	if (mouseX < viewLeft || mouseX > viewLeft + width || mouseY < viewTop || mouseY > viewTop + height) {
		return;
	}

	const float ndcX = ((mouseX - viewLeft) / width) * 2.0f - 1.0f;
	const float ndcY = 1.0f - ((mouseY - viewTop) / height) * 2.0f;
	const Matrix4x4 inverseViewProjection = Inverse(camera->GetViewProjectionMatrix());
	const Vector3 nearPoint = BaseSceneEditorGeometry::TransformCoord({ndcX, ndcY, 0.0f}, inverseViewProjection);
	const Vector3 farPoint = BaseSceneEditorGeometry::TransformCoord({ndcX, ndcY, 1.0f}, inverseViewProjection);
	const Vector3 rayDirection = Normalize(farPoint - nearPoint);

	int hitIndex = -1;
	float nearestDistance = 100000.0f;
	for (int index = 0; index < static_cast<int>(sceneObjects_.size()); ++index) {
		float distance = 0.0f;
		if (BaseSceneEditorGeometry::IntersectRayToOBB(nearPoint, rayDirection, BaseSceneEditorGeometry::MakePickOBB(sceneObjects_[index].get()), distance) && distance < nearestDistance) {
			nearestDistance = distance;
			hitIndex = index;
		}
	}

	if (hitIndex >= 0) {
		selectedObjectIndex_ = hitIndex;
	}
#endif
}

/// <summary>
/// エディタ用カメラのマウス操作を更新します。
/// </summary>
void BaseScene::UpdateEditorCameraControl() {
#ifdef USE_IMGUI
	constexpr int kLeftMouseButton = 0;
	constexpr int kMiddleMouseButton = 2;
	constexpr float kRotateSpeed = 0.004f;
	constexpr float kPanSpeed = 0.0015f;

	Input* input = Input::GetInstance();
	const bool isLeftDragging = input->PushMouseButton(kLeftMouseButton);
	const bool isMiddleDragging = input->PushMouseButton(kMiddleMouseButton);
	if (ImGui::GetIO().WantCaptureMouse || (!isLeftDragging && !isMiddleDragging)) {
		return;
	}
	if (isLeftDragging && (ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
		return;
	}

	const float moveX = static_cast<float>(input->GetMouseMoveX());
	const float moveY = static_cast<float>(input->GetMouseMoveY());
	if (moveX == 0.0f && moveY == 0.0f) {
		return;
	}

	GameObject* selectedObject =
	    selectedObjectIndex_ >= 0 && selectedObjectIndex_ < static_cast<int>(sceneObjects_.size())
	        ? sceneObjects_[selectedObjectIndex_].get()
	        : nullptr;

	GameObject* activeCameraObject = FindObjectByName(activeCameraObjectName_);
	EulerTransform* cameraTransform = nullptr;
	Camera* fallbackCamera = nullptr;
	Vector3 cameraPosition{};
	Vector3 cameraRotate{};

	CameraComponent* activeCameraComponent =
	    activeCameraObject ? activeCameraObject->GetComponent<CameraComponent>() : nullptr;
	if (activeCameraComponent && activeCameraComponent->IsEnabled()) {
		cameraTransform = &activeCameraObject->GetTransform();
		cameraPosition = cameraTransform->translate;
		cameraRotate = cameraTransform->rotate;
	} else if (BaseSceneEditorGeometry::TryGetCameraTransform(fallbackCamera_, cameraPosition, cameraRotate)) {
		fallbackCamera = fallbackCamera_;
	} else {
		return;
	}

	if (isMiddleDragging) {
		const float pitchDelta = moveY * kRotateSpeed;
		const float yawDelta = moveX * kRotateSpeed;
		if (selectedObject) {
			const Vector3 target = selectedObject->GetTransform().translate;
			Vector3 offset = cameraPosition - target;
			if (Length(offset) > MathConstants::kDirectionEpsilon) {
				const Vector3 worldUp{0.0f, 1.0f, 0.0f};
				offset = BaseSceneEditorGeometry::RotateAroundAxis(offset, worldUp, yawDelta);

				Vector3 right = Cross(worldUp, Normalize(offset));
				if (Length(right) > MathConstants::kDirectionEpsilon) {
					offset = BaseSceneEditorGeometry::RotateAroundAxis(offset, right, pitchDelta);
				}
				cameraPosition = target + offset;
			}
		}

		cameraRotate.x += pitchDelta;
		cameraRotate.y += yawDelta;
	} else if (isLeftDragging) {
		const Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(cameraRotate);
		const Vector3 rightAxis{rotateMatrix.m[0][0], rotateMatrix.m[0][1], rotateMatrix.m[0][2]};
		const Vector3 upAxis{rotateMatrix.m[1][0], rotateMatrix.m[1][1], rotateMatrix.m[1][2]};
		const Vector3 right = Normalize(rightAxis);
		const Vector3 up = Normalize(upAxis);
		float distanceScale = 10.0f;
		if (selectedObject) {
			distanceScale = Length(cameraPosition - selectedObject->GetTransform().translate);
			if (distanceScale < 1.0f) {
				distanceScale = 1.0f;
			}
		}
		cameraPosition = cameraPosition + (moveX * distanceScale * kPanSpeed) * right - (moveY * distanceScale * kPanSpeed) * up;
	}

	if (cameraTransform) {
		cameraTransform->translate = cameraPosition;
		cameraTransform->rotate = cameraRotate;
	} else if (fallbackCamera) {
		fallbackCamera->SetTranslate(cameraPosition);
		fallbackCamera->SetRotate(cameraRotate);
	}
#endif
}

/// <summary>
/// 指定したオブジェクトのカメラをアクティブカメラに設定します。
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
		if (hasTexture) {
			Sprite* iconSprite = levelUpChoiceIconSprites_[index].get();
			if (iconSprite->GetTextureFilePath() != textureFilePath) {
				iconSprite->SetTexture(textureFilePath);
			}
			const float iconMargin = 18.0f;
			const float iconHeight = (std::min)(cardHeight * 0.43f, 125.0f);
			drawSprite(iconSprite, cardX + iconMargin, cardY + iconMargin, cardWidth - iconMargin * 2.0f, iconHeight,
			    selected ? Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Vector4{0.88f, 0.88f, 0.88f, 1.0f});
		}

		GameObject* textObject = levelUpChoiceTextObjects_[index].get();
		const float textCenterY = hasTexture ? cardY + cardHeight * 0.72f : cardY + cardHeight * 0.5f;
		textObject->GetTransform().translate = {cardX + cardWidth * 0.5f, textCenterY, 0.0f};
		TextComponent* text = textObject->GetComponent<TextComponent>();
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

void BaseScene::SetActiveCameraObject(GameObject* object) {
	CameraComponent* cameraComponent = object ? object->GetComponent<CameraComponent>() : nullptr;
	if (!cameraComponent || !cameraComponent->IsEnabled()) {
		return;
	}

	activeCameraObjectName_ = object->GetName();
	ApplyActiveCamera();
}

/// <summary>
/// 最初に見つかった有効なカメラオブジェクトを返します。
/// </summary>
GameObject* BaseScene::FindFirstCameraObject() {
	for (const auto& object : sceneObjects_) {
		CameraComponent* cameraComponent = object->GetComponent<CameraComponent>();
		if (cameraComponent && cameraComponent->IsEnabled()) {
			return object.get();
		}
	}
	return nullptr;
}

/// <summary>
/// 名前に一致するシーンオブジェクトを検索します。
/// </summary>
GameObject* BaseScene::FindObjectByName(const std::string& name) const {
	if (name.empty()) {
		return nullptr;
	}

	for (const auto& object : sceneObjects_) {
		if (object->GetName() == name) {
			return object.get();
		}
	}
	return nullptr;
}

