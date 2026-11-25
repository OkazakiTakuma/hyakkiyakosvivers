#pragma once

/// <summary>Componentを各機能へ登録するときに共有する安定ID・表示名・保存キーです。</summary>
struct ComponentTypeMetadata {
	const char* id;
	const char* displayName;
	const char* jsonKey;
};

namespace ComponentTypes {
inline constexpr ComponentTypeMetadata Object3d{"object3d", "Object3d", "object3d"};
inline constexpr ComponentTypeMetadata PointLight{"point_light", "PointLight", "pointLightComponent"};
inline constexpr ComponentTypeMetadata GlowBillboard{"glow_billboard", "GlowBillboard", "glowBillboard"};
inline constexpr ComponentTypeMetadata Sprite{"sprite", "Sprite", "sprite"};
inline constexpr ComponentTypeMetadata Text{"text", "Text", "text"};
inline constexpr ComponentTypeMetadata Camera{"camera", "Camera", "camera"};
inline constexpr ComponentTypeMetadata ParticleEmitter{"particle_emitter", "ParticleEmitter", "particleEmitter"};
inline constexpr ComponentTypeMetadata Player{"player", "Player", "player"};
inline constexpr ComponentTypeMetadata PlayerAttack{"player_attack", "PlayerAttack", "playerAttack"};
inline constexpr ComponentTypeMetadata EnemySpawnPoint{"enemy_spawn_point", "EnemySpawnPoint", "enemySpawnPoint"};
inline constexpr ComponentTypeMetadata Enemy{"enemy", "Enemy", "enemy"};
inline constexpr ComponentTypeMetadata OBBCollider{"obb_collider", "OBBCollider", "obbCollider"};
inline constexpr ComponentTypeMetadata SphereCollider{"sphere_collider", "SphereCollider", "sphereCollider"};
} // namespace ComponentTypes
