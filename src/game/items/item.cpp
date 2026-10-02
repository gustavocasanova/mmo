#include "game/items/item.hpp"
#include <stdexcept>
#include <utility>

namespace mmo::game::items {
ItemDefinition::ItemDefinition(std::string id, std::string name, std::uint32_t stack_limit)
    : id_(std::move(id)), name_(std::move(name)), stack_limit_(stack_limit)
{
    if (id_.empty() || name_.empty() || stack_limit_ == 0) {
        throw std::invalid_argument("item requires an ID, a name, and a positive stack limit");
    }
}
ItemStack::ItemStack(ItemDefinition definition, std::uint32_t quantity)
    : definition_(std::move(definition)), quantity_(quantity)
{
    if (quantity > definition_.stack_limit()) throw std::invalid_argument("item stack exceeds its limit");
}
bool ItemStack::try_add(std::uint32_t quantity)
{
    if (quantity > definition_.stack_limit() - quantity_) return false;
    quantity_ += quantity;
    return true;
}
bool ItemStack::try_remove(std::uint32_t quantity)
{
    if (quantity > quantity_) return false;
    quantity_ -= quantity;
    return true;
}
}
