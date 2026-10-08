# CG2_DirectX クラス全体関係図

`project/externals` 配下の外部ライブラリは対象外とし、プロジェクト固有の主要クラスを、継承・所有・参照・実装依存の観点で整理した図です。

```mermaid
classDiagram
direction LR

namespace Application {
  class FlameWork {
    <<abstract>>
    +Initialize()
    +Update()
    +Draw()
    +Finalize()
    +Run()
  }
  class Game {
    +Initialize()
    +Update()
    +Draw()
    +Finalize()
  }
  class WinApp
  class DirectXCommon
  class ImGuiManager
  class D3DResourceLeakChecker
  class SceneManager
  class AbstractSceneFactory {
    <<interface>>
    +CreateScene(name) unique_ptr~BaseScene~
  }
  class SceneFactory
}

namespace Scene {
  class BaseScene {
    <<abstract>>
    +Initialize()
    +Update()
    +Draw2D()
    +Draw3D()
    +Finalize()
  }
  class TitleScene
  class StageSelectScene
  class PlayerSelectScene
  class ShopScene
  class GamePlayScene
  class ResultScene
}

namespace EntityComponent {
  class GameObject
  class Component {
    <<abstract>>
    +Initialize()
    +Update()
    +Draw()
    +Finalize()
  }
  class Player
  class EnemyComponent
  class PlayerAttackComponent
  class PlayerProjectileComponent
  class EnemyProjectileComponent
  class EnemySpawnPointComponent
  class ExperienceComponent
  class ItemDropComponent
  class CameraComponent
  class Object3dComponent
  class PointLightComponent
  class OBBColliderComponent
  class SphereColliderComponent
  class ParticleEmitterComponent
  class GlowBillboardComponent
  class TrailRendererComponent
  class SpriteComponent
  class TextComponent
}

namespace Graphics {
  class Camera
  class Object3d
  class Object3dCommon
  class Model
  class ModelCommon
  class ModelManager
  class Sprite
  class SpriteCommon
  class TextureManager
  class LineDrawer
  class LineCommon
  class ParticleEmitter
  class ParticleManager
  class TrailRenderer
  class SkyBox
  class SkyBoxCommon
  class AtmosphereSky
  class InstancingModel
}

namespace Services {
  class Input
  class Audio
  class Resource
  class SrvManager
  class FontManager
  class PostEffect
  class PerformanceMonitor
  class PlayerStatusRepository
  class ParticlePresetRepository
  class EnemyStatusRepository
}

FlameWork <|-- Game
Game *-- SceneManager
Game *-- AbstractSceneFactory
Game *-- Camera
Game o-- GameObject : recording indicator
FlameWork *-- WinApp
FlameWork *-- DirectXCommon
FlameWork *-- ImGuiManager
FlameWork *-- D3DResourceLeakChecker
FlameWork o-- Input
FlameWork o-- Audio
FlameWork o-- Resource
FlameWork o-- SrvManager
FlameWork o-- TextureManager
FlameWork o-- ModelManager
FlameWork o-- ParticleManager
FlameWork o-- SpriteCommon
FlameWork o-- Object3dCommon
FlameWork o-- SkyBoxCommon

AbstractSceneFactory <|.. SceneFactory
SceneManager o-- BaseScene : current / next
SceneManager ..> AbstractSceneFactory : creates through
SceneManager o-- Sprite : transition overlay
SceneManager ..> Camera : fallback camera

BaseScene <|-- TitleScene
BaseScene <|-- StageSelectScene
BaseScene <|-- PlayerSelectScene
BaseScene <|-- ShopScene
BaseScene <|-- GamePlayScene
BaseScene <|-- ResultScene
BaseScene ..> SceneManager
BaseScene *-- GameObject : scene objects
BaseScene *-- SkyBox
BaseScene o-- Sprite : HUD / UI
BaseScene o-- Camera
BaseScene ..> Player
BaseScene ..> EnemyComponent
BaseScene ..> PlayerAttackComponent
BaseScene ..> PlayerProjectileComponent
BaseScene ..> EnemyProjectileComponent

GameObject *-- Component : unique_ptr collection
Component --> GameObject : owner
Component <|-- Player
Component <|-- EnemyComponent
Component <|-- PlayerAttackComponent
Component <|-- PlayerProjectileComponent
Component <|-- EnemyProjectileComponent
Component <|-- EnemySpawnPointComponent
Component <|-- ExperienceComponent
Component <|-- ItemDropComponent
Component <|-- CameraComponent
Component <|-- Object3dComponent
Component <|-- PointLightComponent
Component <|-- OBBColliderComponent
Component <|-- SphereColliderComponent
Component <|-- ParticleEmitterComponent
Component <|-- GlowBillboardComponent
Component <|-- TrailRendererComponent
Component <|-- SpriteComponent
Component <|-- TextComponent

Player ..> PlayerAttackComponent : owns via GameObject
Player ..> Object3d
EnemyComponent --> GameObject : target
EnemyComponent ..> EnemyProjectileComponent : shot request
EnemySpawnPointComponent ..> EnemyComponent : spawns
PlayerAttackComponent ..> PlayerProjectileComponent : shot request
PlayerAttackComponent ..> Player : reads stats
PlayerProjectileComponent --> GameObject : motion anchor
Object3dComponent ..> Object3d
Object3dComponent ..> Model
CameraComponent ..> Camera
ParticleEmitterComponent ..> ParticleEmitter
TrailRendererComponent ..> TrailRenderer
SpriteComponent ..> Sprite
TextComponent ..> Sprite
PointLightComponent ..> Object3dCommon
OBBColliderComponent ..> GameObject
SphereColliderComponent ..> GameObject

Object3d ..> Object3dCommon
Object3d ..> Model
Model ..> ModelCommon
ModelManager *-- Model
Sprite ..> SpriteCommon
Sprite ..> TextureManager
ParticleManager *-- ParticleEmitter
TrailRenderer ..> Object3d
SkyBox ..> SkyBoxCommon
AtmosphereSky ..> SkyBox
InstancingModel ..> Camera

PlayerStatusRepository ..> Player
EnemyStatusRepository ..> EnemyComponent
ParticlePresetRepository ..> ParticleEmitter
BaseScene ..> PlayerStatusRepository
BaseScene ..> EnemyStatusRepository
BaseScene ..> ParticlePresetRepository

note for GameObject "シーン上のエンティティ。\nComponentを合成して機能を付与"
note for Component "共通ライフサイクルと\n重力・衝突応答を提供"
note for SceneManager "シーンの生成・切替・遷移・\nゲーム進行データを管理"
```

## 凡例

- `継承`：白抜き三角矢印（`<|--`）
- `実装`：点線の白抜き三角矢印（`<|..`）
- `所有／合成`：黒ひし形（`*--`）。所有側の寿命に従う
- `集約`：白ひし形（`o--`）。非所有参照または共有サービス
- `依存／参照`：点線矢印（`..>`）

特に中心となる構造は、`Game → SceneManager → BaseScene → GameObject → Component` です。各シーンは `BaseScene` を継承し、個々のゲームオブジェクトへ `Player`、敵、攻撃、描画、コライダーなどのコンポーネントを組み合わせます。
