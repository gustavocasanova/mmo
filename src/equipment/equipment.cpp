#include "equipment/equipment.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace mmo::equipment {
namespace {

bool fail(std::string* reason, const char* message)
{
    if (reason != nullptr) {
        *reason = message;
    }
    return false;
}

bool finite_transform(const AttachmentTransform& transform)
{
    for (float value : transform.position_offset) {
        if (!std::isfinite(value)) return false;
    }
    for (float value : transform.scale) {
        if (!std::isfinite(value) || value <= 0.0f) return false;
    }
    const auto& rotation = transform.rotation_offset;
    return std::isfinite(rotation.x) && std::isfinite(rotation.y) &&
        std::isfinite(rotation.z) && std::isfinite(rotation.w);
}

bool supported_model_path(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension == ".glb" || extension == ".gltf";
}

}

EquipmentManager::EquipmentManager(const animation::Skeleton& skeleton)
    : skeleton_(skeleton)
{
}

std::optional<std::size_t> EquipmentManager::slot_index(EquipmentSlot slot)
{
    const std::size_t index = static_cast<std::size_t>(slot);
    if (index >= kSlotCount) {
        return std::nullopt;
    }
    return index;
}

bool EquipmentManager::equip(const Equipment& item, std::string* reason)
{
    const std::optional<std::size_t> index = slot_index(item.slot);
    if (!index) return fail(reason, "equipment slot is invalid");
    if (item.id == 0 || item.name.empty()) return fail(reason, "equipment needs an ID and name");
    if (!supported_model_path(item.model_path)) {
        return fail(reason, "equipment model must use .glb or .gltf");
    }
    if (item.attachments.empty()) return fail(reason, "equipment needs at least one attachment");
    if (!skeleton_.is_valid()) return fail(reason, "character body skeleton is invalid");

    for (const EquipmentAttachment& attachment : item.attachments) {
        if (attachment.socket_name.empty() || attachment.bone_name.empty()) {
            return fail(reason, "attachment needs a socket and bone name");
        }
        if (!skeleton_.find_bone(attachment.bone_name)) {
            return fail(reason, "attachment bone does not exist in character body");
        }
        if (!finite_transform(attachment.local_transform)) {
            return fail(reason, "attachment transform contains invalid values");
        }
    }

    equipped_[*index] = item;
    return true;
}

std::optional<Equipment> EquipmentManager::unequip(EquipmentSlot slot)
{
    const std::optional<std::size_t> index = slot_index(slot);
    if (!index || !equipped_[*index]) {
        return std::nullopt;
    }
    std::optional<Equipment> removed = std::move(equipped_[*index]);
    equipped_[*index].reset();
    return removed;
}

const Equipment* EquipmentManager::get_equipped(EquipmentSlot slot) const
{
    const std::optional<std::size_t> index = slot_index(slot);
    return index && equipped_[*index] ? &*equipped_[*index] : nullptr;
}

bool EquipmentManager::has_equipped(EquipmentSlot slot) const
{
    return get_equipped(slot) != nullptr;
}

bool EquipmentManager::clear_slot(EquipmentSlot slot)
{
    return unequip(slot).has_value();
}

void EquipmentManager::clear()
{
    for (auto& item : equipped_) {
        item.reset();
    }
}

std::vector<ResolvedEquipmentAttachment> EquipmentManager::resolved_attachments() const
{
    std::vector<ResolvedEquipmentAttachment> result;
    for (std::size_t slot = 0; slot < equipped_.size(); ++slot) {
        if (!equipped_[slot]) continue;
        for (const EquipmentAttachment& attachment : equipped_[slot]->attachments) {
            const auto bone = skeleton_.find_bone(attachment.bone_name);
            if (bone) {
                result.push_back({static_cast<EquipmentSlot>(slot), equipped_[slot]->id,
                    *bone, attachment.socket_name, attachment.local_transform});
            }
        }
    }
    return result;
}

}