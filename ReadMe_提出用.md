## 実装内容

| 機能 | 実装内容 |
|---|---|
| Skinningモデルの表示 | Assimpからボーン、ウェイト、逆バインドポーズを読み込み、最大4ボーンの影響を使って頂点シェーダーでスキニングします。 |
| Animation補間 | 移動・拡縮は線形補間、回転はQuaternionのSlerpでキーフレーム間を補間します。Animation切り替え時のブレンドにも対応しています。 |
| 骨のデバッグ表示 | 親子Joint間をライン描画し、Editorの `Draw Bone Debug` から表示を切り替えられます。 |
| 武器の手Jointへの追従 | キャラクターの右手JointのWorld行列を取得し、武器モデルを追従させます。 |
| パーティクル描画 | パーティクル情報をStructuredBufferへ転送し、DirectX 12のインスタンシング描画でまとめて描画します。パーティクルのシミュレーションはCPU側で行います。 |

### Skinning・Skeleton・Animation関連の追加実装

- AssimpからJointの親子構造を読み込み、Skeletonを構築
- 1頂点あたり最大4つのJoint Influenceに対応
- ボーンウェイトの正規化
- 逆バインドポーズ行列とJointのSkeleton Space行列からSkinning Paletteを生成
- StructuredBufferを使用してSkinning Paletteを頂点シェーダーへ転送
- 1つのモデルに含まれる複数Animation Clipの読み込みと切り替え
- Animation Clip名の取得と指定再生
- Animationの再生・停止とループ再生
- Animation切り替え時のブレンド
- Bind Poseへのリセット
- キャラクターの移動状態に応じたAnimationの再生・停止
- シーンJSONへのAnimation名・再生状態・骨デバッグ表示設定の保存と読み込み
- Editor上でのAnimationモデル配置、Animation Clip選択、再生状態の変更
- Joint名からSkeleton Space行列およびWorld行列を取得する機能

## 主な実装箇所

| 機能 | ファイル |
|---|---|
| モデル読込・スキニングデータ生成 | `project/Engine/3d/model/Model.cpp` |
| スキニング描画・骨デバッグ表示 | `project/Engine/3d/object/Object3d.cpp` |
| アニメーション補間 | `project/Engine/3d/utilities/SkeletonAnimationUtility.cpp` |
| スキニング用頂点シェーダー | `project/Resources/Shader/Object3d.VS.hlsl` |
| GPUパーティクル描画 | `project/Engine/3d/particle/ParticleManager.cpp` |
| パーティクル用シェーダー | `project/Resources/Shader/Particle.VS.hlsl` / `Particle.PS.hlsl` |
| 武器の右手Joint追従 | `project/Player/Player.cpp` |
