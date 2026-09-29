#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowTitle[] = "MMO Engine — Marco 001";

constexpr char kVertexShaderSource[] = R"(#version 330 core
layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_color;
out vec3 v_color;
void main() {
    v_color = a_color;
    gl_Position = vec4(a_position, 1.0);
}
)";

constexpr char kFragmentShaderSource[] = R"(#version 330 core
in vec3 v_color;
out vec4 out_color;
void main() {
    out_color = vec4(v_color, 1.0);
}
)";

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "[fatal] " << message << '\n';
    throw std::runtime_error(message);
}

void glfw_error_callback(int error, const char* description)
{
    std::cerr << "[glfw] error " << error << ": "
              << (description != nullptr ? description : "(null)") << '\n';
}

std::string shader_info_log(unsigned int shader)
{
    int length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::string log(static_cast<std::size_t>(length), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    if (!log.empty() && log.back() == '\0') {
        log.pop_back();
    }
    return log;
}

std::string program_info_log(unsigned int program)
{
    int length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1) {
        return {};
    }
    std::string log(static_cast<std::size_t>(length), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    if (!log.empty() && log.back() == '\0') {
        log.pop_back();
    }
    return log;
}

unsigned int compile_shader(unsigned int type, std::string_view source)
{
    const unsigned int shader = glCreateShader(type);
    if (shader == 0) {
        fail("glCreateShader returned 0");
    }

    const char* src = source.data();
    const int length = static_cast<int>(source.size());
    glShaderSource(shader, 1, &src, &length);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        fail("shader compile failed:\n" + shader_info_log(shader));
    }
    return shader;
}

unsigned int link_program(unsigned int vertex_shader, unsigned int fragment_shader)
{
    const unsigned int program = glCreateProgram();
    if (program == 0) {
        fail("glCreateProgram returned 0");
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        fail("program link failed:\n" + program_info_log(program));
    }
    return program;
}

void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
    if (width > 0 && height > 0) {
        glViewport(0, 0, width, height);
    }
}

class GlfwRuntime {
public:
    GlfwRuntime()
    {
        glfwSetErrorCallback(glfw_error_callback);
        if (glfwInit() == GLFW_FALSE) {
            fail("glfwInit failed");
        }
    }

    GlfwRuntime(const GlfwRuntime&) = delete;
    GlfwRuntime& operator=(const GlfwRuntime&) = delete;

    ~GlfwRuntime()
    {
        glfwTerminate();
    }
};

class Window {
public:
    Window(int width, int height, const char* title)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (handle_ == nullptr) {
            fail("glfwCreateWindow failed — OpenGL 3.3 Core is required");
        }

        glfwMakeContextCurrent(handle_);
        glfwSwapInterval(1);
        glfwSetFramebufferSizeCallback(handle_, framebuffer_size_callback);
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr))
    {
    }

    Window& operator=(Window&& other) noexcept
    {
        if (this != &other) {
            destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~Window()
    {
        destroy();
    }

    GLFWwindow* get() const
    {
        return handle_;
    }

    bool should_close() const
    {
        return glfwWindowShouldClose(handle_) == GLFW_TRUE;
    }

    void request_close() const
    {
        glfwSetWindowShouldClose(handle_, GLFW_TRUE);
    }

    void swap_buffers() const
    {
        glfwSwapBuffers(handle_);
    }

private:
    void destroy()
    {
        if (handle_ != nullptr) {
            glfwDestroyWindow(handle_);
            handle_ = nullptr;
        }
    }

    GLFWwindow* handle_ = nullptr;
};

class ShaderProgram {
public:
    ShaderProgram(std::string_view vertex_source, std::string_view fragment_source)
    {
        const unsigned int vs = compile_shader(GL_VERTEX_SHADER, vertex_source);
        const unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
        id_ = link_program(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ~ShaderProgram()
    {
        if (id_ != 0) {
            glDeleteProgram(id_);
        }
    }

    void bind() const
    {
        glUseProgram(id_);
    }

private:
    unsigned int id_ = 0;
};

class TriangleMesh {
public:
    TriangleMesh()
    {
        // Position (x, y, z) + color (r, g, b)
        constexpr float vertices[] = {
            -0.55f, -0.50f, 0.0f,  0.91f, 0.30f, 0.24f,
             0.55f, -0.50f, 0.0f,  0.18f, 0.80f, 0.44f,
             0.00f,  0.60f, 0.0f,  0.25f, 0.53f, 0.96f,
        };

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        if (vao_ == 0 || vbo_ == 0) {
            fail("failed to allocate VAO/VBO");
        }

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        constexpr GLsizei stride = static_cast<GLsizei>(6 * sizeof(float));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    TriangleMesh(const TriangleMesh&) = delete;
    TriangleMesh& operator=(const TriangleMesh&) = delete;

    ~TriangleMesh()
    {
        if (vbo_ != 0) {
            glDeleteBuffers(1, &vbo_);
        }
        if (vao_ != 0) {
            glDeleteVertexArrays(1, &vao_);
        }
    }

    void draw() const
    {
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
};

void log_gl_info()
{
    const auto* version = glGetString(GL_VERSION);
    const auto* vendor = glGetString(GL_VENDOR);
    const auto* renderer = glGetString(GL_RENDERER);
    const auto* glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);

    std::cout << "[gl] version  : " << (version != nullptr ? reinterpret_cast<const char*>(version) : "?") << '\n';
    std::cout << "[gl] vendor   : " << (vendor != nullptr ? reinterpret_cast<const char*>(vendor) : "?") << '\n';
    std::cout << "[gl] renderer : " << (renderer != nullptr ? reinterpret_cast<const char*>(renderer) : "?") << '\n';
    std::cout << "[gl] glsl     : " << (glsl != nullptr ? reinterpret_cast<const char*>(glsl) : "?") << '\n';
}

void run()
{
    GlfwRuntime glfw;
    Window window(kWindowWidth, kWindowHeight, kWindowTitle);

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        fail("gladLoadGL failed — could not load OpenGL 3.3 entry points");
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window.get(), &framebuffer_width, &framebuffer_height);
    glViewport(0, 0, framebuffer_width, framebuffer_height);

    log_gl_info();

    const ShaderProgram program(kVertexShaderSource, kFragmentShaderSource);
    const TriangleMesh triangle;

    std::cout << "[app] Marco 001 running. Close the window or press Escape.\n";

    while (!window.should_close()) {
        glfwPollEvents();

        if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            window.request_close();
        }

        glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        program.bind();
        triangle.draw();

        window.swap_buffers();
    }

    std::cout << "[app] shutdown complete.\n";
}

}  // namespace

int main()
{
    try {
        run();
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "[app] aborted: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
