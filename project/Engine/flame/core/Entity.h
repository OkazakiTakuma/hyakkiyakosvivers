#pragma once
#include "core/EntityComponent.h"
#include "Matrix.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/// <summary>
/// シーン上の名前、Transform、親子関係とComponent群を所有するエンティティです。
/// Componentの初期化、更新、描画、破棄を一括して管理します。
/// </summary>
class Entity {
public:
	Entity() : runtimeId_(nextRuntimeId_.fetch_add(1, std::memory_order_relaxed)) {}
	~Entity() { Finalize(); }

	Entity(const Entity&) = delete;
	Entity& operator=(const Entity&) = delete;

	template<class T, class... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of_v<EntityComponent, T>, "T must inherit from EntityComponent.");

		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		T* rawComponent = component.get();
		rawComponent->SetOwner(this);
		components_.push_back(std::move(component));
		rawComponent->Initialize();
		NotifyIndexChanged();
		return rawComponent;
	}

	template<class T>
	T* GetComponent() {
		static_assert(std::is_base_of_v<EntityComponent, T>, "T must inherit from EntityComponent.");

		for (const auto& component : components_) {
			if (T* target = dynamic_cast<T*>(component.get())) {
				return target;
			}
		}
		return nullptr;
	}

	template<class T>
	const T* GetComponent() const {
		static_assert(std::is_base_of_v<EntityComponent, T>, "T must inherit from EntityComponent.");

		for (const auto& component : components_) {
			if (const T* target = dynamic_cast<const T*>(component.get())) {
				return target;
			}
		}
		return nullptr;
	}

	template<class T>
	/// <summary>
	/// 指定した型のコンポーネントを削除します。
	/// </summary>
	void RemoveComponent() {
		static_assert(std::is_base_of_v<EntityComponent, T>, "T must inherit from EntityComponent.");

		components_.erase(
		    std::remove_if(
		        components_.begin(),
		        components_.end(),
		        [](const std::unique_ptr<EntityComponent>& component) {
			        if (dynamic_cast<T*>(component.get())) {
				        component->Finalize();
				        return true;
			        }
			        return false;
		        }
		    ),
		    components_.end()
		);
		NotifyIndexChanged();
	}

	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update() {
		for (const auto& component : components_) {
			if (component->IsEnabled()) {
				component->Update();
			}
		}
	}

	/// <summary>
	/// 現在の状態をもとに描画処理を行います。
	/// </summary>
	void Draw() {
		for (const auto& component : components_) {
			if (component->IsEnabled()) {
				component->Draw();
			}
		}
	}

	/// <summary>
	/// 2D 要素の描画処理を行います。
	/// </summary>
	void Draw2D() {
		for (const auto& component : components_) {
			if (component->IsEnabled()) {
				component->Draw2D();
			}
		}
	}

	/// <summary>
	/// 3D 要素の描画処理を行います。
	/// </summary>
	void Draw3D() {
		for (const auto& component : components_) {
			if (component->IsEnabled()) {
				component->Draw3D();
			}
		}
	}

	/// <summary>
	/// 確保したリソースを解放し、終了処理を行います。
	/// </summary>
	void Finalize() {
		for (const auto& component : components_) {
			component->Finalize();
		}
		components_.clear();
	}

	EulerTransform& GetTransform() { return transform_; }
	const EulerTransform& GetTransform() const { return transform_; }
	void SetName(const std::string& name) {
		if (name_ != name) {
			name_ = name;
			NotifyIndexChanged();
		}
	}
	const std::string& GetName() const { return name_; }
	void SetEditorType(const std::string& editorType) { editorType_ = editorType; }
	const std::string& GetEditorType() const { return editorType_; }
	const std::string& GetParentName() const { return parentName_; }
	/// <summary>実行中のオブジェクトを一意に識別するIDです。保存データには含めません。</summary>
	uint64_t GetRuntimeId() const { return runtimeId_; }

private:
	friend class BaseScene;
	friend class SceneWorld;
	void NotifyIndexChanged() {
		if (indexChangedCallback_) indexChangedCallback_();
	}
	/// <summary>親子関係の検証後にBaseSceneだけが親名を更新します。</summary>
	void SetParentName(const std::string& parentName) { parentName_ = parentName; }
	inline static std::atomic<uint64_t> nextRuntimeId_{1};
	uint64_t runtimeId_ = 0;
	std::string name_ = "Entity";
	std::string editorType_ = "Empty";
	std::string parentName_;
	EulerTransform transform_ = {
	    {1.0f, 1.0f, 1.0f},
	    {0.0f, 0.0f, 0.0f},
	    {0.0f, 0.0f, 0.0f}
	};
	/// <summary>SceneWorld所属後の名前・Component構成変更を索引へ通知します。</summary>
	std::function<void()> indexChangedCallback_;
	std::vector<std::unique_ptr<EntityComponent>> components_;
};

