#pragma once

#include "platform/window.hpp"

namespace mmo::platform {

struct InputSettings {
    Key move_forward = Key::W;
    Key move_backward = Key::S;
    Key strafe_left = Key::A;
    Key strafe_right = Key::D;
    Key jump = Key::Space;
    Key previous_animation = Key::LeftBracket;
    Key next_animation = Key::RightBracket;
};

}
