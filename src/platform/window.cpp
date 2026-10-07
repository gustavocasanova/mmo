#include "platform/window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

namespace mmo::platform {
namespace {

void glfw_error_callback(int error, const char* description)
{
    std::cerr << "[glfw] error " << error << ": "
              << (description != nullptr ? description : "(null)") << '\n';
}

int glfw_key(Key key)
{
    switch (key) {
    case Key::W: return GLFW_KEY_W;
    case Key::A: return GLFW_KEY_A;
    case Key::S: return GLFW_KEY_S;
    case Key::D: return GLFW_KEY_D;
    case Key::Q: return GLFW_KEY_Q;
    case Key::Space: return GLFW_KEY_SPACE;
    case Key::F: return GLFW_KEY_F;
    case Key::LeftShift: return GLFW_KEY_LEFT_SHIFT;
    case Key::LeftBracket: return GLFW_KEY_LEFT_BRACKET;
    case Key::RightBracket: return GLFW_KEY_RIGHT_BRACKET;
    case Key::E: return GLFW_KEY_E;
    case Key::F1: return GLFW_KEY_F1;
    case Key::F2: return GLFW_KEY_F2;
    case Key::F3: return GLFW_KEY_F3;
    case Key::Tab: return GLFW_KEY_TAB;
    case Key::L: return GLFW_KEY_L;
    case Key::LeftControl: return GLFW_KEY_LEFT_CONTROL;
    case Key::G: return GLFW_KEY_G;
    case Key::C: return GLFW_KEY_C;
    case Key::M: return GLFW_KEY_M;
    case Key::R: return GLFW_KEY_R;
    case Key::U: return GLFW_KEY_U;
    case Key::X: return GLFW_KEY_X;
    case Key::Y: return GLFW_KEY_Y;
    case Key::Z: return GLFW_KEY_Z;
    case Key::Digit0: return GLFW_KEY_0;
    case Key::Digit1: return GLFW_KEY_1;
    case Key::Digit2: return GLFW_KEY_2;
    case Key::Digit3: return GLFW_KEY_3;
    case Key::Digit4: return GLFW_KEY_4;
    case Key::Digit5: return GLFW_KEY_5;
    case Key::Digit6: return GLFW_KEY_6;
    case Key::Digit7: return GLFW_KEY_7;
    case Key::Digit8: return GLFW_KEY_8;
    case Key::Digit9: return GLFW_KEY_9;
    case Key::T: return GLFW_KEY_T;
    }
    return GLFW_KEY_UNKNOWN;
}

}

GlfwRuntime::GlfwRuntime()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("glfwInit failed");
    }
}

GlfwRuntime::~GlfwRuntime()
{
    glfwTerminate();
}

Window::Window(int width, int height, const char* title)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (handle_ == nullptr) {
        throw std::runtime_error("glfwCreateWindow failed; OpenGL 3.3 Core is required");
    }

    glfwSetWindowUserPointer(handle_, this);
    glfwSetCursorPosCallback(handle_, &Window::cursor_position_callback);
    glfwSetScrollCallback(handle_, &Window::scroll_callback);
    glfwSetWindowFocusCallback(handle_, &Window::focus_callback);
    focused_ = glfwGetWindowAttrib(handle_, GLFW_FOCUSED) == GLFW_TRUE;
    glfwMakeContextCurrent(handle_);
    glfwSwapInterval(1);
}

Window::~Window()
{
    if (handle_ != nullptr) {
        set_cursor_captured(false);
        glfwDestroyWindow(handle_);
    }
}

bool Window::should_close() const
{
    return glfwWindowShouldClose(handle_) == GLFW_TRUE;
}

bool Window::escape_pressed() const
{
    return glfwGetKey(handle_, GLFW_KEY_ESCAPE) == GLFW_PRESS;
}

bool Window::key_pressed(Key key) const
{
    return glfwGetKey(handle_, glfw_key(key)) == GLFW_PRESS;
}

bool Window::right_mouse_pressed() const
{
    return focused_ && glfwGetMouseButton(handle_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

bool Window::left_mouse_pressed() const
{
    return focused_ && glfwGetMouseButton(handle_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}

void Window::cursor_position(double& x, double& y) const
{
    glfwGetCursorPos(handle_, &x, &y);
}

void Window::window_size(int& width, int& height) const
{
    glfwGetWindowSize(handle_, &width, &height);
}

double Window::time_seconds() const
{
    return glfwGetTime();
}

void Window::set_cursor_captured(bool captured)
{
    captured = captured && focused_;
    if (cursor_captured_ == captured) {
        return;
    }

    cursor_captured_ = captured;
    cursor_position_initialized_ = false;
    cursor_delta_x_ = 0.0;
    cursor_delta_y_ = 0.0;
    glfwSetInputMode(handle_, GLFW_CURSOR,
        captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Window::consume_mouse_input(double& delta_x, double& delta_y, double& scroll_delta)
{
    delta_x = cursor_delta_x_;
    delta_y = cursor_delta_y_;
    scroll_delta = scroll_delta_;
    cursor_delta_x_ = 0.0;
    cursor_delta_y_ = 0.0;
    scroll_delta_ = 0.0;
}

void Window::cursor_position_callback(::GLFWwindow* handle, double x, double y)
{
    auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) {
        return;
    }

    if (window->focused_ && window->cursor_position_initialized_) {
        window->cursor_delta_x_ += x - window->last_cursor_x_;
        window->cursor_delta_y_ += y - window->last_cursor_y_;
    }
    window->last_cursor_x_ = x;
    window->last_cursor_y_ = y;
    window->cursor_position_initialized_ = true;
}

void Window::scroll_callback(::GLFWwindow* handle, double x_offset, double y_offset)
{
    (void)x_offset;
    auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
    if (window != nullptr && window->focused_) {
        window->scroll_delta_ += y_offset;
    }
}

void Window::focus_callback(::GLFWwindow* handle, int focused)
{
    auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
    if (window == nullptr) {
        return;
    }

    window->focused_ = focused == GLFW_TRUE;
    if (!window->focused_) {
        window->set_cursor_captured(false);
        window->cursor_delta_x_ = 0.0;
        window->cursor_delta_y_ = 0.0;
        window->scroll_delta_ = 0.0;
    }
}

void Window::request_close() const
{
    glfwSetWindowShouldClose(handle_, GLFW_TRUE);
}

void Window::poll_events() const
{
    glfwPollEvents();
}

void Window::framebuffer_size(int& width, int& height) const
{
    glfwGetFramebufferSize(handle_, &width, &height);
}

void Window::swap_buffers() const
{
    glfwSwapBuffers(handle_);
}

::GLFWwindow* Window::native_handle() const
{
    return handle_;
}

GraphicsProcAddress Window::get_proc_address(const char* name)
{
    return glfwGetProcAddress(name);
}

}