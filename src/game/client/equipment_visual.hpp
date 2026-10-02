#pragma once

#include "assets/mesh_data.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace mmo::game::client {

enum class EquipmentSlot : std::size_t {
    Helmet,
    ShoulderLeft,
    ShoulderRight,
    Chest,
    GloveLeft,
    GloveRight,
    BootLeft,
    BootRight,
    Pants,
    Cape,
    BracerLeft,
    BracerRight,
    RingLeftIndex,
    RingLeftRing,
    RingRightIndex,
    RingRightRing,
    EarringLeft,
    EarringRight,
    Count,
};

enum class EquipmentStyle {
    Cloth,
    Leather,
    Plate,
    Ornate,
};

struct EquipmentItem {
    bool equipped = false;
    std::string name;
    std::array<float, 3> color{0.35f, 0.35f, 0.35f};
    EquipmentStyle style = EquipmentStyle::Leather;
};

struct CharacterEquipment {
    std::array<EquipmentItem, static_cast<std::size_t>(EquipmentSlot::Count)> items{};

    EquipmentItem& operator[](EquipmentSlot slot);
    const EquipmentItem& operator[](EquipmentSlot slot) const;
};

CharacterEquipment make_default_character_equipment();
std::vector<assets::Vertex> make_character_vertices(const CharacterEquipment& equipment);

}
