#pragma once
class Entity;

/// <summary>
/// Entityへ追加できる振る舞いの最小共通インターフェースです。
/// 有効状態と所有者だけを管理し、個別の責務は派生クラスへ委譲します。
/// </summary>
class EntityComponent {
public:
	virtual ~EntityComponent() = default;

	virtual void Initialize() {}
	virtual void Update() {}
	virtual void Draw() {}
	virtual void Draw2D() {}
	virtual void Draw3D() {}
	virtual void Finalize() {}

	Entity* GetOwner() { return owner_; }
	const Entity* GetOwner() const { return owner_; }

	void SetEnabled(bool enabled) { isEnabled_ = enabled; }
	bool IsEnabled() const { return isEnabled_; }

private:
	friend class Entity;
	void SetOwner(Entity* owner) { owner_ = owner; }

	// このコンポーネントを所有するEntityです。所有権はEntity側が持ちます。
	Entity* owner_ = nullptr;
	// falseの場合、EntityのUpdate/Draw系呼び出しからこのコンポーネントを除外します。
	bool isEnabled_ = true;
};

