#pragma once

struct GLFWwindow;

namespace mmo::platform {

using GraphicsProcAddress = void (*)(void);

enum class Key {
    W,
    A,
    S,
    D,
    Q,
    Space,
    F,
    LeftShift,
    LeftBracket,
    RightBracket,
    E,
    F1,
    F2,
    F3,
    Tab,
    L,
    LeftControl,
    G,
    C,
    M,
    R,
    U,
    X,
    Y,
    Z,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    T,
};

class GlfwRuntime {
public:
    GlfwRuntime();
    GlfwRuntime(const GlfwRuntime&) = delete;
    GlfwRuntime& operator=(const GlfwRuntime&) = delete;
    ~GlfwRuntime();
};

class Window {
public:
    Window(int width, int height, const char* title);
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window();

    bool should_close() const;
    bool escape_pressed() const;
    bool key_pressed(Key key) const;
    bool right_mouse_pressed() const;
    bool left_mouse_pressed() const;
    void cursor_position(double& x, double& y) const;
    void window_size(int& width, int& height) const;
    double time_seconds() const;
    void set_cursor_captured(bool captured);
    void consume_mouse_input(double& delta_x, double& delta_y, double& scroll_delta);
    void request_close() const;
    void poll_events() const;
    void framebuffer_size(int& width, int& height) const;
    void swap_buffers() const;
    [[nodiscard]] ::GLFWwindow* native_handle() const;

    static GraphicsProcAddress get_proc_address(const char* name);

private:
    static void cursor_position_callback(::GLFWwindow* handle, double x, double y);
    static void scroll_callback(::GLFWwindow* handle, double x_offset, double y_offset);
    static void focus_callback(::GLFWwindow* handle, int focused);

    ::GLFWwindow* handle_ = nullptr;
    bool cursor_captured_ = false;
    bool focused_ = true;
    bool cursor_position_initialized_ = false;
    double last_cursor_x_ = 0.0;
    double last_cursor_y_ = 0.0;
    double cursor_delta_x_ = 0.0;
    double cursor_delta_y_ = 0.0;
    double scroll_delta_ = 0.0;
};

}