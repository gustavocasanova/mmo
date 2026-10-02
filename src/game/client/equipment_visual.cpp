#include "game/client/equipment_visual.hpp"

#include <algorithm>
#include <cmath>

namespace mmo::game::client {
namespace {

using Point = std::array<float, 3>;
using Color = std::array<float, 3>;

constexpr float kPi = 3.14159265f;
constexpr Color kSkin{0.72f, 0.48f, 0.32f};
constexpr Color kLeather{0.23f, 0.12f, 0.07f};
constexpr Color kIron{0.34f, 0.39f, 0.41f};
constexpr Color kCloth{0.13f, 0.27f, 0.23f};
constexpr Color kGold{0.82f, 0.59f, 0.19f};

Point add(Point left, Point right)
{
    return {left[0] + right[0], left[1] + right[1], left[2] + right[2]};
}

Point subtract(Point left, Point right)
{
    return {left[0] - right[0], left[1] - right[1], left[2] - right[2]};
}

Point cross(Point left, Point right)
{
    return {
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    };
}

Point normalize(Point value)
{
    const float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    if (length <= 0.000001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {value[0] / length, value[1] / length, value[2] / length};
}

void append_triangle(std::vector<assets::Vertex>& vertices, Point first, Point second,
    Point third, Color color)
{
    const Point normal = normalize(cross(subtract(second, first), subtract(third, first)));
    vertices.push_back({{first[0], first[1], first[2]}, {normal[0], normal[1], normal[2]},
        {color[0], color[1], color[2]}});
    vertices.push_back({{second[0], second[1], second[2]}, {normal[0], normal[1], normal[2]},
        {color[0], color[1], color[2]}});
    vertices.push_back({{third[0], third[1], third[2]}, {normal[0], normal[1], normal[2]},
        {color[0], color[1], color[2]}});
}

void append_quad(std::vector<assets::Vertex>& vertices, Point first, Point second,
    Point third, Point fourth, Color color)
{
    append_triangle(vertices, first, second, third, color);
    append_triangle(vertices, first, third, fourth, color);
}

void append_ellipsoid(std::vector<assets::Vertex>& vertices, Point center, Point radii,
    Color color, int slices = 10, int stacks = 6)
{
    for (int stack = 0; stack < stacks; ++stack) {
        const float first_theta = kPi * static_cast<float>(stack) / stacks;
        const float second_theta = kPi * static_cast<float>(stack + 1) / stacks;
        for (int slice = 0; slice < slices; ++slice) {
            const float first_phi = 2.0f * kPi * static_cast<float>(slice) / slices;
            const float second_phi = 2.0f * kPi * static_cast<float>(slice + 1) / slices;
            const auto point = [&](float theta, float phi) {
                return add(center, {
                    radii[0] * std::sin(theta) * std::cos(phi),
                    radii[1] * std::cos(theta),
                    radii[2] * std::sin(theta) * std::sin(phi),
                });
            };
            const Point top_left = point(first_theta, first_phi);
            const Point bottom_left = point(second_theta, first_phi);
            const Point bottom_right = point(second_theta, second_phi);
            const Point top_right = point(first_theta, second_phi);
            append_triangle(vertices, top_left, bottom_right, bottom_left, color);
            append_triangle(vertices, top_left, top_right, bottom_right, color);
        }
    }
}

void append_box(std::vector<assets::Vertex>& vertices, Point center, Point half_size, Color color)
{
    const float x0 = center[0] - half_size[0];
    const float x1 = center[0] + half_size[0];
    const float y0 = center[1] - half_size[1];
    const float y1 = center[1] + half_size[1];
    const float z0 = center[2] - half_size[2];
    const float z1 = center[2] + half_size[2];
    append_quad(vertices, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, color);
    append_quad(vertices, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, color);
    append_quad(vertices, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, color);
    append_quad(vertices, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, color);
    append_quad(vertices, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}, color);
    append_quad(vertices, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, color);
}

const EquipmentItem& item(const CharacterEquipment& equipment, EquipmentSlot slot)
{
    return equipment[slot];
}

bool equipped(const CharacterEquipment& equipment, EquipmentSlot slot)
{
    return item(equipment, slot).equipped;
}

Color color_of(const EquipmentItem& item)
{
    return item.color;
}

Point armor_scale(const EquipmentItem& item, Point scale)
{
    const float multiplier = item.style == EquipmentStyle::Plate ? 1.12f :
        item.style == EquipmentStyle::Cloth ? 0.94f : 1.0f;
    return {scale[0] * multiplier, scale[1] * multiplier, scale[2] * multiplier};
}

void append_ring(std::vector<assets::Vertex>& vertices, Point center, float major_radius,
    float minor_radius, Color color, bool facing_side)
{
    constexpr int kSegments = 8;
    constexpr int kTubeSegments = 5;
    for (int segment = 0; segment < kSegments; ++segment) {
        const float first = 2.0f * kPi * static_cast<float>(segment) / kSegments;
        const float second = 2.0f * kPi * static_cast<float>(segment + 1) / kSegments;
        for (int tube = 0; tube < kTubeSegments; ++tube) {
            const float first_tube = 2.0f * kPi * static_cast<float>(tube) / kTubeSegments;
            const float second_tube = 2.0f * kPi * static_cast<float>(tube + 1) / kTubeSegments;
            const auto point = [&](float angle, float around) {
                const float radius = major_radius + minor_radius * std::cos(around);
                if (facing_side) {
                    return Point{center[0] + radius * std::cos(angle),
                        center[1] + radius * std::sin(angle),
                        center[2] + minor_radius * std::sin(around)};
                }
                return Point{center[0] + minor_radius * std::sin(around),
                    center[1] + radius * std::cos(angle),
                    center[2] + radius * std::sin(angle)};
            };
            const Point a = point(first, first_tube);
            const Point b = point(second, first_tube);
            const Point c = point(second, second_tube);
            const Point d = point(first, second_tube);
            append_triangle(vertices, a, b, c, color);
            append_triangle(vertices, a, c, d, color);
        }
    }
}

void add_item(CharacterEquipment& equipment, EquipmentSlot slot, const char* name,
    Color color, EquipmentStyle style)
{
    equipment[slot] = {true, name, color, style};
}

}

EquipmentItem& CharacterEquipment::operator[](EquipmentSlot slot)
{
    return items[static_cast<std::size_t>(slot)];
}

const EquipmentItem& CharacterEquipment::operator[](EquipmentSlot slot) const
{
    return items[static_cast<std::size_t>(slot)];
}

CharacterEquipment make_default_character_equipment()
{
    CharacterEquipment equipment;
    add_item(equipment, EquipmentSlot::Helmet, "Ironbound helm", kIron, EquipmentStyle::Plate);
    add_item(equipment, EquipmentSlot::ShoulderLeft, "Ironbound pauldron", kIron, EquipmentStyle::Plate);
    add_item(equipment, EquipmentSlot::ShoulderRight, "Ironbound pauldron", kIron, EquipmentStyle::Plate);
    add_item(equipment, EquipmentSlot::Chest, "Greenwarden cuirass", kCloth, EquipmentStyle::Leather);
    add_item(equipment, EquipmentSlot::GloveLeft, "Trail gloves", kLeather, EquipmentStyle::Leather);
    add_item(equipment, EquipmentSlot::GloveRight, "Trail gloves", kLeather, EquipmentStyle::Leather);
    add_item(equipment, EquipmentSlot::BootLeft, "Trail boots", kLeather, EquipmentStyle::Leather);
    add_item(equipment, EquipmentSlot::BootRight, "Trail boots", kLeather, EquipmentStyle::Leather);
    add_item(equipment, EquipmentSlot::Pants, "Wool trousers", kCloth, EquipmentStyle::Cloth);
    add_item(equipment, EquipmentSlot::Cape, "Mossweave cape", {0.28f, 0.18f, 0.11f}, EquipmentStyle::Cloth);
    add_item(equipment, EquipmentSlot::BracerLeft, "Iron wristguard", kIron, EquipmentStyle::Plate);
    add_item(equipment, EquipmentSlot::BracerRight, "Iron wristguard", kIron, EquipmentStyle::Plate);
    add_item(equipment, EquipmentSlot::RingLeftIndex, "Amber signet", kGold, EquipmentStyle::Ornate);
    add_item(equipment, EquipmentSlot::RingLeftRing, "Amber band", kGold, EquipmentStyle::Ornate);
    add_item(equipment, EquipmentSlot::RingRightIndex, "Amber band", kGold, EquipmentStyle::Ornate);
    add_item(equipment, EquipmentSlot::RingRightRing, "Amber signet", kGold, EquipmentStyle::Ornate);
    add_item(equipment, EquipmentSlot::EarringLeft, "Copper hoop", kGold, EquipmentStyle::Ornate);
    add_item(equipment, EquipmentSlot::EarringRight, "Copper hoop", kGold, EquipmentStyle::Ornate);
    return equipment;
}

std::vector<assets::Vertex> make_character_vertices(const CharacterEquipment& equipment)
{
    std::vector<assets::Vertex> vertices;
    vertices.reserve(5000);

    append_ellipsoid(vertices, {0.0f, 1.12f, 0.0f}, {0.235f, 0.32f, 0.145f}, kSkin);
    append_ellipsoid(vertices, {0.0f, 0.78f, 0.0f}, {0.18f, 0.13f, 0.12f}, kSkin);
    append_ellipsoid(vertices, {0.0f, 1.47f, 0.0f}, {0.075f, 0.12f, 0.075f}, kSkin);
    append_ellipsoid(vertices, {0.0f, 1.67f, 0.015f}, {0.135f, 0.18f, 0.125f}, kSkin, 12, 8);
    for (float side : {-1.0f, 1.0f}) {
        append_ellipsoid(vertices, {side * 0.31f, 1.22f, 0.0f}, {0.12f, 0.25f, 0.115f}, kSkin);
        append_ellipsoid(vertices, {side * 0.44f, 0.91f, 0.0f}, {0.085f, 0.19f, 0.085f}, kSkin);
        append_ellipsoid(vertices, {side * 0.49f, 0.69f, 0.015f}, {0.085f, 0.095f, 0.075f}, kSkin);
        append_ellipsoid(vertices, {side * 0.13f, 0.48f, 0.0f}, {0.105f, 0.34f, 0.105f}, kSkin);
        append_ellipsoid(vertices, {side * 0.13f, 0.105f, 0.055f}, {0.115f, 0.095f, 0.19f}, kSkin);

        const EquipmentSlot shoulder = side < 0.0f ? EquipmentSlot::ShoulderLeft : EquipmentSlot::ShoulderRight;
        const EquipmentSlot glove = side < 0.0f ? EquipmentSlot::GloveLeft : EquipmentSlot::GloveRight;
        const EquipmentSlot boot = side < 0.0f ? EquipmentSlot::BootLeft : EquipmentSlot::BootRight;
        const EquipmentSlot bracer = side < 0.0f ? EquipmentSlot::BracerLeft : EquipmentSlot::BracerRight;
        if (equipped(equipment, shoulder)) {
            const EquipmentItem& armor = item(equipment, shoulder);
            append_ellipsoid(vertices, {side * 0.31f, 1.39f, 0.0f},
                armor_scale(armor, {0.17f, 0.135f, 0.16f}), color_of(armor));
            if (armor.style == EquipmentStyle::Ornate) {
                append_ellipsoid(vertices, {side * 0.31f, 1.51f, 0.0f},
                    {0.055f, 0.025f, 0.055f}, kGold);
            }
        }
        if (equipped(equipment, glove)) {
            const EquipmentItem& armor = item(equipment, glove);
            append_ellipsoid(vertices, {side * 0.49f, 0.69f, 0.015f},
                armor_scale(armor, {0.095f, 0.105f, 0.085f}), color_of(armor));
        }
        if (equipped(equipment, boot)) {
            const EquipmentItem& armor = item(equipment, boot);
            append_ellipsoid(vertices, {side * 0.13f, 0.12f, 0.055f},
                armor_scale(armor, {0.13f, 0.12f, 0.205f}), color_of(armor));
        }
        if (equipped(equipment, bracer)) {
            const EquipmentItem& armor = item(equipment, bracer);
            append_ellipsoid(vertices, {side * 0.44f, 0.94f, 0.0f},
                armor_scale(armor, {0.105f, 0.14f, 0.105f}), color_of(armor));
        }
    }

    if (equipped(equipment, EquipmentSlot::Pants)) {
        const EquipmentItem& pants = item(equipment, EquipmentSlot::Pants);
        for (float side : {-1.0f, 1.0f}) {
            append_ellipsoid(vertices, {side * 0.13f, 0.52f, 0.0f},
                armor_scale(pants, {0.112f, 0.285f, 0.115f}), color_of(pants));
        }
    }
    if (equipped(equipment, EquipmentSlot::Chest)) {
        const EquipmentItem& armor = item(equipment, EquipmentSlot::Chest);
        append_ellipsoid(vertices, {0.0f, 1.13f, 0.0f},
            armor_scale(armor, {0.25f, 0.32f, 0.16f}), color_of(armor));
        if (armor.style == EquipmentStyle::Ornate || armor.style == EquipmentStyle::Plate) {
            append_box(vertices, {0.0f, 1.16f, 0.157f}, {0.075f, 0.18f, 0.014f}, kGold);
        }
    }
    if (equipped(equipment, EquipmentSlot::Helmet)) {
        const EquipmentItem& helmet = item(equipment, EquipmentSlot::Helmet);
        append_ellipsoid(vertices, {0.0f, 1.8f, 0.015f},
            armor_scale(helmet, {0.15f, 0.105f, 0.14f}), color_of(helmet), 12, 6);
        if (helmet.style == EquipmentStyle::Ornate) {
            append_box(vertices, {0.0f, 1.84f, 0.0f}, {0.018f, 0.13f, 0.025f}, kGold);
        }
    }
    if (equipped(equipment, EquipmentSlot::Cape)) {
        const EquipmentItem& cape = item(equipment, EquipmentSlot::Cape);
        const Color color = color_of(cape);
        append_quad(vertices, {-0.18f, 1.43f, -0.145f}, {0.18f, 1.43f, -0.145f},
            {0.38f, 0.52f, -0.19f}, {-0.38f, 0.52f, -0.19f}, color);
        append_quad(vertices, {0.18f, 1.43f, -0.145f}, {-0.18f, 1.43f, -0.145f},
            {-0.38f, 0.52f, -0.19f}, {0.38f, 0.52f, -0.19f}, color);
    }

    constexpr std::array<EquipmentSlot, 4> ring_slots{
        EquipmentSlot::RingLeftIndex, EquipmentSlot::RingLeftRing,
        EquipmentSlot::RingRightIndex, EquipmentSlot::RingRightRing,
    };
    for (std::size_t ring = 0; ring < ring_slots.size(); ++ring) {
        if (!equipped(equipment, ring_slots[ring])) {
            continue;
        }
        const float side = ring < 2 ? -1.0f : 1.0f;
        const float depth = ring % 2 == 0 ? -0.018f : 0.028f;
        append_ring(vertices, {side * 0.52f, 0.695f, depth}, 0.026f, 0.009f,
            color_of(item(equipment, ring_slots[ring])), false);
    }
    for (EquipmentSlot slot : {EquipmentSlot::EarringLeft, EquipmentSlot::EarringRight}) {
        if (!equipped(equipment, slot)) {
            continue;
        }
        const float side = slot == EquipmentSlot::EarringLeft ? -1.0f : 1.0f;
        append_ring(vertices, {side * 0.135f, 1.65f, 0.07f}, 0.035f, 0.009f,
            color_of(item(equipment, slot)), true);
    }

    return vertices;
}

}
