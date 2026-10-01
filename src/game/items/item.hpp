#pragma once
#include <cstdint>
#include <string>

namespace mmo::game::items {
// Content identity, independent of a mesh, texture, or GPU allocation.
class ItemDefinition {
public:
    ItemDefinition(std::string id, std::string name, std::uint32_t stack_limit = 1);
    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    std::uint32_t stack_limit() const { return stack_limit_; }
private:
    std::string id_;
    std::string name_;
    std::uint32_t stack_limit_;
};
// Owns a definition snapshot; it never holds a dangling catalog pointer.
class ItemStack {
public:
    ItemStack(ItemDefinition definition, std::uint32_t quantity);
    const ItemDefinition& definition() const { return definition_; }
    std::uint32_t quantity() const { return quantity_; }
    bool try_add(std::uint32_t quantity);
    bool try_remove(std::uint32_t quantity);
private:
    ItemDefinition definition_;
    std::uint32_t quantity_;
};
}
