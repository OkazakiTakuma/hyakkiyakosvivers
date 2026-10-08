#pragma once

#include "TrailRenderer.h"
#include "../../base/GameTime.h"
#include "../../flame/Component.h"
#include "../../flame/GameObject.h"
#include <algorithm>
#include <deque>
#include <vector>

/// <summary>GameObjectの移動履歴を収集し、時間とともに消える軌跡を生成します。</summary>
class TrailRendererComponent : public Component {
public:
	void Initialize() override {
		// 再初期化時に古い軌跡を残さず、現在位置を軌跡の始点にする。
		Clear();
		if (GetOwner()) {
			points_.push_back({GetOwner()->GetTransform().translate - GetReferenceOrigin(), 0.0f});
		}
	}

	void Update() override {
		const float deltaTime = GameTime::GetDeltaTime();
		// 各点の経過時間を進め、寿命を超えた古い点を先頭から破棄する。
		for (Point& point : points_) {
			point.age += deltaTime;
		}
		while (!points_.empty() && points_.front().age >= lifeTime_) {
			points_.pop_front();
		}

		if (!emitting_ || !GetOwner()) {
			return;
		}
		// 基準オブジェクトからの相対座標で保存し、親の移動へ追従できるようにする。
		const Vector3 position = GetOwner()->GetTransform().translate + offset_ - GetReferenceOrigin();
		if (points_.empty() || Length(position - points_.back().position) >= minSegmentLength_) {
			points_.push_back({position, 0.0f});
			while (points_.size() > maxPointCount_) {
				points_.pop_front();
			}
		}
	}

	void Draw3D() override {
		if (points_.empty() || !GetOwner() || lifeTime_ <= 0.0f) {
			return;
		}
		// 点の寿命を0～1の残存率へ変換してレンダラーへ渡す。
		std::vector<TrailRenderPoint> renderPoints;
		renderPoints.reserve(points_.size() + 1);
		const Vector3 referenceOrigin = GetReferenceOrigin();
		for (const Point& point : points_) {
			renderPoints.push_back({point.position + referenceOrigin, 1.0f - (std::clamp)(point.age / lifeTime_, 0.0f, 1.0f)});
		}
		// 履歴点は一定距離ごとに間引く一方、描画時だけ現在位置を終端へ足す。
		// これにより低速弾でも2点未満になりにくく、軌跡の先端が弾本体から離れない。
		const Vector3 currentPosition = GetOwner()->GetTransform().translate + offset_;
		if (Length(currentPosition - renderPoints.back().position) > kPositionEpsilon) {
			renderPoints.push_back({currentPosition, 1.0f});
		}
		if (renderPoints.size() < 2) {
			return;
		}
		TrailRenderer::GetInstance()->Submit(renderPoints, width_, headColor_, tailColor_);
	}

	void Clear() { points_.clear(); }
	/// <summary>座標系の折り返し時に、保存済みの軌跡点を同じ移動量だけ平行移動します。</summary>
	void TranslateHistory(const Vector3& translation) {
		// 点を消去せず平行移動することで、ループ前後でも軌跡の長さと残り寿命を維持する。
		for (Point& point : points_) {
			point.position = point.position + translation;
		}
	}
	/// <summary>falseの場合、既存の履歴は減衰させながら新しい点の追加だけを停止します。</summary>
	void SetEmitting(bool emitting) { emitting_ = emitting; }
	bool IsEmitting() const { return emitting_; }
	void SetWidth(float width) { width_ = (std::max)(0.0f, width); }
	float GetWidth() const { return width_; }
	void SetLifeTime(float lifeTime) { lifeTime_ = (std::max)(0.01f, lifeTime); }
	float GetLifeTime() const { return lifeTime_; }
	void SetMinSegmentLength(float length) { minSegmentLength_ = (std::max)(0.001f, length); }
	float GetMinSegmentLength() const { return minSegmentLength_; }
	void SetMaxPointCount(size_t count) { maxPointCount_ = (std::max)(size_t{2}, count); }
	void SetHeadColor(const Vector4& color) { headColor_ = color; }
	void SetTailColor(const Vector4& color) { tailColor_ = color; }
	/// <summary>所有オブジェクトの中心から履歴を記録する位置をずらします。</summary>
	void SetOffset(const Vector3& offset) { offset_ = offset; }
	/// <summary>履歴を指定オブジェクト基準の相対座標で保持し、基準の移動へ追従させます。</summary>
	void SetPositionReference(GameObject* referenceObject) {
		// 基準を切り替えてもワールド上の軌跡位置が飛ばないよう、保存座標を補正する。
		const Vector3 oldOrigin = GetReferenceOrigin();
		const Vector3 newOrigin = referenceObject ? referenceObject->GetTransform().translate : Vector3{};
		for (Point& point : points_) {
			point.position = point.position + oldOrigin - newOrigin;
		}
		referenceObject_ = referenceObject;
	}
	GameObject* GetPositionReference() const { return referenceObject_; }

private:
	static constexpr float kPositionEpsilon = 0.00001f;

	/// <summary>軌跡を構成する点の相対位置と生成後の経過時間です。</summary>
	struct Point {
		Vector3 position{};
		float age = 0.0f;
	};

	Vector3 GetReferenceOrigin() const {
		// 基準なしの場合は原点を返し、通常のワールド座標履歴として扱う。
		return referenceObject_ ? referenceObject_->GetTransform().translate : Vector3{};
	}

	/// <summary>古い点を先頭から効率良く破棄するための時系列キューです。</summary>
	std::deque<Point> points_;
	/// <summary>軌跡の帯幅です。</summary>
	float width_ = 0.75f;
	/// <summary>各点を保持する秒数です。</summary>
	float lifeTime_ = 0.35f;
	/// <summary>新しい点を追加するために必要な最小移動距離です。</summary>
	float minSegmentLength_ = 0.05f;
	/// <summary>メモリと描画負荷を制限する最大点数です。</summary>
	size_t maxPointCount_ = 64;
	Vector4 headColor_{0.25f, 0.85f, 1.0f, 0.9f};
	Vector4 tailColor_{0.05f, 0.20f, 1.0f, 0.0f};
	Vector3 offset_{};
	/// <summary>軌跡座標の原点として使用する、所有権を持たない参照です。</summary>
	GameObject* referenceObject_ = nullptr;
	/// <summary>新しい軌跡点を追加するかを表します。</summary>
	bool emitting_ = true;
};
