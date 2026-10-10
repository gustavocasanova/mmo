#include "apps/client/demo_enemies.hpp"

#include "apps/client/character_setup.hpp"

#include <cmath>

namespace mmo::app {
namespace {

constexpr float kCorpseSeconds = 5.0f;
constexpr float kRespawnSeconds = 3.0f;

}

DemoEnemies::DemoEnemies(std::shared_ptr<const assets::Model> body_model,
    game::combat::EntityRegistry& registry, glm::vec3 origin)
    : registry_(registry)
{
    const glm::vec3 homes[] = {
        origin + glm::vec3{0.0f, 0.0f, 3.0f},
        origin + glm::vec3{-6.0f, 0.0f, 9.0f},
        origin + glm::vec3{7.0f, 0.0f, 14.0f},
    };
    const char* const names[] = {"Orc", "Goblin", "Ogre"};
    for (int index = 0; index < 3; ++index) {
        Actor actor;
        actor.character = std::make_unique<character::Character>(body_model);
        bind_animations(*actor.character);
        actor.home = homes[index];
        actor.name = names[index];
        actor.level = 3 + index * 2;
        spawn(actor);
        actors_.push_back(std::move(actor));
    }
}

// Creates a fresh gameplay entity for the actor and resets its presentation.
void DemoEnemies::spawn(Actor& actor)
{
    game::combat::CombatEntity entity;
    entity.name = actor.name;
    entity.level = actor.level;
    entity.faction = game::combat::Faction::Hostile;
    entity.position = actor.home;
    entity.max_hp = 100.0f;
    entity.hp = 100.0f;
    actor.id = registry_.create(entity);
    actor.timer = 0.0f;
    actor.character->transform().position = {actor.home.x, actor.home.y, actor.home.z};
    (void)actor.character->model().animation().set_state(animation::AnimationState::Idle, 0.0f);
}

DemoEnemies::Actor* DemoEnemies::find(game::combat::EntityId id)
{
    for (Actor& actor : actors_) {
        if (actor.id == id) {
            return &actor;
        }
    }
    return nullptr;
}

void DemoEnemies::on_damage(const game::combat::DamageEvent& event)
{
    if (Actor* actor = find(event.target)) {
        auto& animation_controller = actor->character->model().animation();
        (void)animation_controller.play_animation(event.killed ? "Death01" : "Hit_Chest", false);
    }
}

void DemoEnemies::update(
    float delta_seconds, const world::Terrain& terrain, glm::vec3 player_position)
{
    for (Actor& actor : actors_) {
        if (actor.id == game::combat::kInvalidEntity) {
            actor.timer -= delta_seconds;
            if (actor.timer <= 0.0f) {
                spawn(actor);
            }
            continue;
        }
        game::combat::CombatEntity* entity = registry_.find(actor.id);
        animation::Vector3& e = actor.character->transform().position;
        e[1] = terrain.height_at({e[0], e[2]});
        entity->position = {e[0], e[1], e[2]};
        if (entity->alive()) {
            actor.yaw = std::atan2(player_position.x - e[0], player_position.z - e[2]);
        }
        actor.character->transform().rotation = {
            0.0f, std::sin(actor.yaw * 0.5f), 0.0f, std::cos(actor.yaw * 0.5f)};
        actor.character->model().update(delta_seconds);
        auto& enemy_animation = actor.character->model().animation();
        if (entity->alive()) {
            if (!enemy_animation.animator().playing()) {
                (void)enemy_animation.set_state(animation::AnimationState::Idle, 0.15f);
            }
        } else {
            // The corpse stays selectable for a while, then the entity is removed.
            actor.timer += delta_seconds;
            if (actor.timer >= kCorpseSeconds) {
                registry_.remove(actor.id);
                actor.id = game::combat::kInvalidEntity;
                actor.timer = kRespawnSeconds;
            }
        }
    }
}

void DemoEnemies::append_instances(std::vector<renderer::SkinnedInstance>& instances) const
{
    for (const Actor& actor : actors_) {
        const game::combat::CombatEntity* entity = registry_.find(actor.id);
        if (entity == nullptr) {
            continue;
        }
        const animation::Vector3& e = actor.character->transform().position;
        instances.push_back({{e[0], e[1], e[2], actor.yaw},
            entity->alive() ? glm::vec3{1.0f, 0.5f, 0.5f} : glm::vec3{0.55f},
            actor.character->model().animation().pose().skin_matrices,
            {}});
    }
}

}
