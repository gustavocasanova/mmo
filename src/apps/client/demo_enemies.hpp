#pragma once

#include "assets/model.hpp"
#include "character/character.hpp"
#include "game/combat/combat.hpp"
#include "renderer/renderer.hpp"
#include "world/terrain.hpp"

#include <memory>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace mmo::app {

// The three training dummies of the prototype scene: gameplay entity, animated body,
// corpse timer and respawn. A real spawner/AI replaces this once the server exists.
class DemoEnemies {
public:
    DemoEnemies(std::shared_ptr<const assets::Model> body_model,
        game::combat::EntityRegistry& registry, glm::vec3 origin);

    // Presentation reaction to damage (hit/death animation).
    void on_damage(const game::combat::DamageEvent& event);
    // Advances timers and animation, keeps actors on the terrain and facing the player.
    void update(float delta_seconds, const world::Terrain& terrain, glm::vec3 player_position);
    // Spans reference actor poses and stay valid until the next update().
    void append_instances(std::vector<renderer::SkinnedInstance>& instances) const;

private:
    struct Actor {
        game::combat::EntityId id = game::combat::kInvalidEntity;
        std::unique_ptr<character::Character> character;
        glm::vec3 home{0.0f};
        std::string name;
        int level = 1;
        float yaw = 0.0f;
        float timer = 0.0f;
    };

    void spawn(Actor& actor);
    [[nodiscard]] Actor* find(game::combat::EntityId id);

    game::combat::EntityRegistry& registry_;
    std::vector<Actor> actors_;
};

}
