#pragma once
#include "Component.h"
#include "../base/struct.h"
#include "../base/GameTime.h"
#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/// <summary>
/// シーン上の名前、Transform、親子関係とComponent群を所有するエンティティです。
/// Componentの初期化、更新、描画、破棄を一括して管理します。
/// </summary>
class GameObject {
public:
	GameObject() = default;
	~GameObject() { Finalize(); }

	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;

	template<class T, class... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component.");

		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		T* rawComponent = component.get();
		rawComponent->SetOwner(this);
		components_.push_back(std::move(component));
		rawComponent->Initialize();
		return rawComponent;
	}

	template<class T>
	T* GetComponent() {
		static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component.");

		for (const auto& component : components_) {
			if (T* target = dynamic_cast<T*>(component.get())) {
				return target;
			}
		}
		return nullptr;
	}

	template<class T>
	const T* GetComponent() const {
		static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component.");

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
		static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component.");

		components_.erase(
		    std::remove_if(
		        components_.begin(),
		        components_.end(),
		        [](const std::unique_ptr<Component>& component) {
			        if (dynamic_cast<T*>(component.get())) {
				        component->Finalize();
				        return true;
			        }
			        return false;
		        }
		    ),
		    components_.end()
		);
	}

	/// <summary>
	/// 毎フレームの状態更新を行います。
	/// </summary>
	void Update() {
		for (const auto& component : components_) {
			if (component->IsEnabled()) {
				component->ApplyGravity(transform_, GameTime::GetDeltaTime());
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

	void ApplyCollisionResponse(const Vector3& collisionNormal) {
		for (const auto& component : components_) {
			if (component->IsEnabled()) {
				component->ApplyCollisionResponse(collisionNormal);
			}
		}
	}

	EulerTransform& GetTransform() { return transform_; }
	const EulerTransform& GetTransform() const { return transform_; }
	void SetName(const std::string& name) { name_ = name; }
	const std::string& GetName() const { return name_; }
	void SetEditorType(const std::string& editorType) { editorType_ = editorType; }
	const std::string& GetEditorType() const { return editorType_; }
	void SetParentName(const std::string& parentName) { parentName_ = parentName; }
	const std::string& GetParentName() const { return parentName_; }

private:
	std::string name_ = "GameObject";
	std::string editorType_ = "Empty";
	std::string parentName_;
	EulerTransform transform_ = {
	    {1.0f, 1.0f, 1.0f},
	    {0.0f, 0.0f, 0.0f},
	    {0.0f, 0.0f, 0.0f}
	};
	std::vector<std::unique_ptr<Component>> components_;
};
