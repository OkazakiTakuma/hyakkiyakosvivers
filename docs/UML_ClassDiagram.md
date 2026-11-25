# CG2_DirectX 主要クラス設計 UML

この図は、ゲームの実行・シーン管理・Entity/Component・戦闘処理という、実装の中心になるクラスだけを示す。
DirectX の低レベル描画クラス、UI 用 Sprite、エディタ専用クラス、値オブジェクト、外部ライブラリは省略している。

```mermaid
classDiagram
direction TB

class FlameWork {
  <<abstract>>
  -bool endRequest_
  -unique_ptr~WinApp~ winApp
  -unique_ptr~DirectXCommon~ dxCommon
  -unique_ptr~ImGuiManager~ imguiManager
  +Initialize()
  +Update()
  +Draw()
  +Finalize()
  +Run()
}

class Game {
  -unique_ptr~Camera~ camera
  -unique_ptr~AbstractSceneFactory~ sceneFactory
  -unique_ptr~SceneManager~ sceneManager
  +Initialize()
  +Update()
  +Draw()
  +Finalize()
}

class AbstractSceneFactory {
  <<interface>>
  +CreateScene(sceneName) unique_ptr~BaseScene~
}

class SceneFactory {
  +CreateScene(sceneName) unique_ptr~BaseScene~
}

class SceneManager {
  -unique_ptr~BaseScene~ scene_
  -unique_ptr~BaseScene~ nextScene_
  -GameProgress gameProgress_
  -DemoDirector demoDirector_
  +SetSceneFactory(factory)
  +ChangeScene(sceneName, transitionType)
  +Update()
  +Draw2D()
  +Draw3D()
}

class BaseScene {
  <<abstract>>
  -SceneManager* sceneManager_
  -SceneWorld sceneObjects_
  -CombatSystem combatSystem_
  -EnemySpawnSystem enemySpawnSystem_
  -LootDropService lootDropService_
  -GameplaySession gameplaySession_
  -GameplayHud gameplayHud_
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

class SceneWorld {
  -vector~unique_ptr Entity~ entities_
  -componentIndex_
  -nameIndex_
  +Update()
  +Draw2D()
  +Draw3D()
  +FindByName(name) Entity*
  +GetEntitiesWith~T~() Entity[]
  +Clear()
}

class Entity {
  -uint64 runtimeId_
  -string name_
  -EulerTransform transform_
  -vector~unique_ptr EntityComponent~ components_
  +AddComponent~T~() T*
  +GetComponent~T~() T*
  +RemoveComponent~T~()
  +Update()
  +Draw2D()
  +Draw3D()
  +Finalize()
}

class EntityComponent {
  <<abstract>>
  -Entity* owner_
  -bool isEnabled_
  +Initialize()
  +Update()
  +Draw2D()
  +Draw3D()
  +Finalize()
}

class PlayerComponent {
  -PlayerStats stats_
  -float currentHealth_
  -int currentExperience_
  +AddExperience(value)
  +ApplyStats(stats)
}

class EnemyComponent {
  -EnemyStats stats_
  -Entity* target_
  -float currentHealth_
  -shotRequests_
  +TakeDamage(value)
  +ConsumeShotRequests()
}

class PlayerAttackComponent {
  -AttackSlotRuntime[] slots_
  -attackPatterns_
  -shotRequests_
  +Update()
  +AddAttackSlot(stats, level, enabled)
  +ConsumeShotRequests()
}

class IPlayerAttackPattern {
  <<interface>>
  +CreateShots(context, attackStats, levelStats, level) PlayerAttackShotRequest[]
}

class PlayerAttackEntityComponent
class EnemyAttackEntityComponent
class EnemySpawnPointComponent
class ExperienceComponent
class ItemDropComponent
class Object3dComponent
class CameraComponent
class OBBColliderComponent
class SphereColliderComponent

class CombatSystem {
  -CollisionSpatialGrid collisionSpatialGrid_
  -DebugStats collisionDebugStats_
  +UpdateColliderCollisions(world)
  +UpdateEnemyProjectileHits(world)
  +ResolvePlayerProjectileHit(projectile, enemy)
}

class EnemySpawnSystem {
  -string activeBossObjectName_
  +Reset()
  +BeginBossEncounter(bossObjectName)
  +Update(world, selectionActive, pendingOffers) EnemySpawnUpdateResult
}

class LootDropService {
  -mt19937 randomEngine_
  -BossRewardSettings bossRewardSettings_
  +Roll(enemy, position) LootDropResult
}

class GameplaySession {
  -Mode mode_
  -bool isStageCleared_
  -int defeatedEnemyCount_
  -int challengeMoneyEarned_
  -float survivalTimeSeconds_
  +Reset()
  +SetMode(mode)
  +RecordEnemyDefeat()
  +MakeResultData() StageResultData
}

class GameplayHud {
  +Reset()
  +Update(player, showMoney, money)
  +Draw()
}

class GameProgress {
  -int money_
  -shopUpgradeLevels_
  -unlockedGameplayStageIds_
  +AddMoney(amount) bool
  +SpendMoney(amount) bool
  +PurchaseGlobalUpgrade(id, price, maxLevel) ShopPurchaseResult
  +RecordGameplayStageClear(stageId, stageRules, playerRules) bool
}

class DemoDirector {
  -bool active_
  -size_t playerTypeIndex_
  -size_t stageIndex_
  +Toggle(playerTypeNames) bool
  +Update(sceneName, transitionBusy, deltaTime, fallbackType) DemoDirectorUpdate
}

FlameWork <|-- Game
Game *-- SceneManager : owns
Game *-- AbstractSceneFactory : owns
AbstractSceneFactory <|.. SceneFactory
SceneManager --> AbstractSceneFactory : creates scenes
SceneManager *-- BaseScene : current / next
SceneManager *-- GameProgress
SceneManager *-- DemoDirector

BaseScene <|-- TitleScene
BaseScene <|-- StageSelectScene
BaseScene <|-- PlayerSelectScene
BaseScene <|-- ShopScene
BaseScene <|-- GamePlayScene
BaseScene <|-- ResultScene
BaseScene --> SceneManager : requests transition

BaseScene *-- SceneWorld
BaseScene *-- CombatSystem
BaseScene *-- EnemySpawnSystem
BaseScene *-- LootDropService
BaseScene *-- GameplaySession
BaseScene *-- GameplayHud

SceneWorld *-- "0..*" Entity : owns
Entity *-- "0..*" EntityComponent : owns
EntityComponent --> "1" Entity : owner

EntityComponent <|-- PlayerComponent
EntityComponent <|-- EnemyComponent
EntityComponent <|-- PlayerAttackComponent
EntityComponent <|-- PlayerAttackEntityComponent
EntityComponent <|-- EnemyAttackEntityComponent
EntityComponent <|-- EnemySpawnPointComponent
EntityComponent <|-- ExperienceComponent
EntityComponent <|-- ItemDropComponent
EntityComponent <|-- Object3dComponent
EntityComponent <|-- CameraComponent
EntityComponent <|-- OBBColliderComponent
EntityComponent <|-- SphereColliderComponent

PlayerAttackComponent *-- IPlayerAttackPattern : strategies
PlayerAttackComponent --> PlayerComponent : reads attack modifiers
EnemyComponent --> Entity : target
ExperienceComponent --> Entity : collection target
ItemDropComponent --> Entity : collection target

CombatSystem --> SceneWorld : collision query
EnemySpawnSystem --> SceneWorld : spawn-point query
LootDropService --> EnemyComponent : reads defeated enemy
GameplayHud --> PlayerComponent : reads status
```

## クラス間で受け渡すデータ

`BaseScene` は各ComponentやSystemを直接実装するのではなく、要求・結果を値として受け取り、Entity生成や進行状態への反映を調整する。

```mermaid
classDiagram
direction LR

class SceneManager
class AbstractSceneFactory
class BaseScene
class SceneWorld
class PlayerAttackComponent
class EnemyComponent
class EnemySpawnSystem
class CombatSystem
class LootDropService
class GameplaySession
class GameplayHud

class PlayerAttackShotRequest {
  +string attackName
  +string visualId
  +Vector3 position
  +Vector3 direction
  +float attack
  +float speed
  +float size
  +float lifeTime
  +Entity* motionAnchor
  +Entity* sourceOwner
}

class EnemyShotRequest {
  +Vector3 position
  +Vector3 direction
  +float speed
  +float attack
  +float size
  +float lifeTime
  +MotionType motionType
  +Entity* homingTarget
}

class EnemySpawnUpdateResult {
  +EnemySpawnRequest[] enemyRequests
  +bool hasBossEncounterRequest
  +BossEncounterRequest bossEncounter
  +bool stageCleared
}

class EnemySpawnRequest {
  +string enemyTypeName
  +Vector3 position
  +Entity* target
  +EnemyStatMultipliers statMultipliers
}

class PlayerProjectileHitResult {
  +bool hit
  +bool enemyDefeated
  +float damageDealt
}

class LootDropResult {
  +ExperienceDropRequest experience
  +ItemDropRequest[] items
  +bool hasBossUpgrade
  +BossUpgradeDropRequest bossUpgrade
}

class StageResultData {
  +StageResultEquipment[] attacks
  +StageResultEquipment[] statuses
  +bool stageCleared
  +int defeatedEnemyCount
  +int moneyEarned
  +float survivalTimeSeconds
}

class PlayerStats
class EnemyStats
class BaseScenePointer {
  <<unique_ptr BaseScene>>
}

SceneManager --> AbstractSceneFactory : sceneName
AbstractSceneFactory --> SceneManager : BaseScenePointer

PlayerAttackComponent --> PlayerAttackShotRequest : ConsumeShotRequests()
PlayerAttackShotRequest --> BaseScene : projectile creation request

EnemyComponent --> EnemyShotRequest : ConsumeShotRequests()
EnemyShotRequest --> BaseScene : enemy projectile creation request

BaseScene --> EnemySpawnSystem : SceneWorld + selection state
EnemySpawnSystem --> EnemySpawnUpdateResult : Update() result
EnemySpawnUpdateResult *-- EnemySpawnRequest
EnemySpawnUpdateResult --> BaseScene : spawn / boss / clear requests

BaseScene --> CombatSystem : SceneWorld + projectile Entity
CombatSystem --> PlayerProjectileHitResult : hit result
PlayerProjectileHitResult --> BaseScene : damage / defeat notification

BaseScene --> LootDropService : EnemyComponent + death position
LootDropService --> LootDropResult : Roll() result
LootDropResult --> BaseScene : experience / item / boss reward requests

GameplaySession --> StageResultData : MakeResultData()
StageResultData --> BaseScene : session result
BaseScene --> SceneManager : SetStageResultData()

BaseScene --> GameplayHud : PlayerComponent + money
BaseScene --> SceneWorld : unique_ptr Entity
PlayerStats --> BaseScene : player definition
EnemyStats --> BaseScene : enemy definition
```

### 受け渡し内容一覧

| 送り元 | 受け取り先 | 渡すもの | 目的 |
|---|---|---|---|
| `SceneManager` | `AbstractSceneFactory` | シーン名 `string` | 指定された種類のシーンを生成する |
| `AbstractSceneFactory` | `SceneManager` | `unique_ptr<BaseScene>` | 生成したシーンの所有権を渡す |
| `PlayerAttackComponent` | `BaseScene` | `vector<PlayerAttackShotRequest>` | 攻撃の位置、方向、威力、速度、寿命、軌道を渡し、弾Entityを生成する |
| `EnemyComponent` | `BaseScene` | `vector<EnemyShotRequest>` | 敵弾の位置、方向、威力、速度、寿命、軌道を渡す |
| `BaseScene` | `EnemySpawnSystem` | `SceneWorld&`、選択画面状態、保留報酬状態 | スポーン地点とボスの状態を調べる |
| `EnemySpawnSystem` | `BaseScene` | `EnemySpawnUpdateResult` | 通常敵生成要求、ボス戦開始要求、ステージクリアを通知する |
| `BaseScene` | `CombatSystem` | `SceneWorld&`、弾Entity、敵Entity | 衝突候補の検索と精密判定を依頼する |
| `CombatSystem` | `BaseScene` | `PlayerProjectileHitResult` | 命中有無、撃破有無、実ダメージを返す |
| `BaseScene` | `LootDropService` | 撃破された `EnemyComponent` と座標 | 敵設定に基づくドロップ抽選を依頼する |
| `LootDropService` | `BaseScene` | `LootDropResult` | 経験値、アイテム、ボス報酬の生成要求を返す |
| `GameplaySession` | `BaseScene` | `StageResultData` | クリア状態、撃破数、獲得金、生存時間を確定する |
| `BaseScene` | `SceneManager` | 装備情報を加えた `StageResultData` | シーン切り替え後も戦績を保持し、リザルト画面へ引き継ぐ |
| `BaseScene` | `GameplayHud` | `PlayerComponent*`、獲得金、表示可否 | HP、経験値、装備、獲得金を表示する |
| `BaseScene` | `SceneWorld` | `unique_ptr<Entity>` | 生成したEntityの所有権をSceneWorldへ移す |
| Repository群 | `BaseScene` / Assembler群 | `PlayerStats`、`EnemyStats`、攻撃定義 | JSONの保存データを実行時Componentへ反映する |

## 設計の中心

1. `FlameWork` がエンジンのメインループを持ち、`Game` がゲーム固有の初期化・更新・描画を実装する。
2. `SceneManager` が現在シーンと次シーンを所有し、生成は `AbstractSceneFactory` に委譲する。
3. `BaseScene` がシーン共通の処理を提供し、各画面はこれを継承して固有処理だけを追加する。
4. `SceneWorld` がすべての `Entity` の寿命と検索索引を管理する。
5. `Entity` は `EntityComponent` を合成し、プレイヤー、敵、攻撃、描画、当たり判定などの機能を組み合わせる。
6. 戦闘判定、敵生成、ドロップ抽選、プレイ戦績、HUD は専用クラスへ分離し、`BaseScene` が処理順を調整する。

## 矢印の意味

- `<|--`：継承
- `<|..`：インターフェース実装
- `*--`：合成。接続元が接続先の寿命を所有する
- `-->`：利用・参照。接続先の所有権は持たない

このプロジェクトのクラス設計を説明する場合は、最初に
`Game → SceneManager → BaseScene → SceneWorld → Entity → EntityComponent`
の流れを示し、その後に `CombatSystem` などの役割分担を説明すると全体像が伝わりやすい。
