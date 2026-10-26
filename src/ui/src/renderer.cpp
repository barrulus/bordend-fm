#include "nordend/ui/renderer.hpp"

#include <GLES3/gl3.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>

#include "nordend/core/log.hpp"

namespace nordend::ui {
namespace {

// One instanced quad per primitive. The fragment shader evaluates the signed distance to a
// rounded box (per-corner radii) in physical pixels: coverage gives 1-pixel antialiasing, an
// inner offset of the same distance gives the border, and a Gaussian falloff of the distance
// gives the shadow (an approximation of a blurred rounded box, exact on straight edges).
constexpr const char* kVertexShader = R"glsl(#version 300 es
layout(location = 0) in vec2 a_corner;        // unit quad, 0..1
layout(location = 1) in vec4 a_rect;          // physical x, y, width, height
layout(location = 2) in vec4 a_radii;         // top-left, top-right, bottom-right, bottom-left
layout(location = 3) in vec4 a_fill;          // premultiplied
layout(location = 4) in vec4 a_border_color;  // premultiplied
layout(location = 5) in vec2 a_params;        // border width, shadow sigma (0 = box)

uniform vec2 u_viewport;

out vec2 v_pos;
flat out vec4 v_rect;
flat out vec4 v_radii;
flat out vec4 v_fill;
flat out vec4 v_border_color;
flat out vec2 v_params;

void main() {
    float pad = a_params.y > 0.0 ? 3.0 * a_params.y : 1.0;
    vec2 pos = a_rect.xy - pad + a_corner * (a_rect.zw + 2.0 * pad);
    v_pos = pos;
    v_rect = a_rect;
    v_radii = a_radii;
    v_fill = a_fill;
    v_border_color = a_border_color;
    v_params = a_params;
    vec2 ndc = pos / u_viewport * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)glsl";

constexpr const char* kFragmentShader = R"glsl(#version 300 es
precision highp float;

in vec2 v_pos;
flat in vec4 v_rect;
flat in vec4 v_radii;
flat in vec4 v_fill;
flat in vec4 v_border_color;
flat in vec2 v_params;

out vec4 o_color;

float rounded_box_distance(vec2 p, vec2 half_size, vec4 radii) {
    float r = p.x < 0.0 ? (p.y < 0.0 ? radii.x : radii.w) : (p.y < 0.0 ? radii.y : radii.z);
    r = min(r, min(half_size.x, half_size.y));
    vec2 q = abs(p) - half_size + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

// Abramowitz-Stegun style approximation of erf, max error ~5e-4.
float erf_approx(float x) {
    float s = sign(x);
    float a = abs(x);
    float t = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
    t *= t;
    return s - s / (t * t);
}

void main() {
    vec2 half_size = v_rect.zw * 0.5;
    vec2 p = v_pos - (v_rect.xy + half_size);
    float d = rounded_box_distance(p, half_size, v_radii);

    float sigma = v_params.y;
    if (sigma > 0.0) {
        float alpha = 0.5 - 0.5 * erf_approx(d / (sigma * 1.41421356));
        o_color = v_fill * alpha;
        return;
    }

    float coverage = clamp(0.5 - d, 0.0, 1.0);
    float inner = clamp(0.5 - (d + v_params.x), 0.0, 1.0);
    o_color = v_fill * inner + v_border_color * (coverage - inner);
}
)glsl";

constexpr std::size_t kMaxInstancesPerDraw = 4096;

GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        std::array<char, 1024> info{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(info.size()), nullptr, info.data());
        glDeleteShader(shader);
        throw std::runtime_error{std::format("shader compile failed: {}", info.data())};
    }
    return shader;
}

GLuint link_program(const char* vertex_source, const char* fragment_source) {
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment = 0;
    try {
        fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    } catch (...) {
        glDeleteShader(vertex);
        throw;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);  // flagged; freed with the program
    glDeleteShader(fragment);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        std::array<char, 1024> info{};
        glGetProgramInfoLog(program, static_cast<GLsizei>(info.size()), nullptr, info.data());
        glDeleteProgram(program);
        throw std::runtime_error{std::format("shader link failed: {}", info.data())};
    }
    return program;
}

// Logical -> physical, rounded to the nearest pixel edge.
float snap(float logical, float scale) { return std::round(logical * scale); }

}  // namespace

// Mirrors the per-instance vertex attributes (locations 1..5).
struct Renderer::Instance {
    std::array<float, 4> rect;
    std::array<float, 4> radii;
    std::array<float, 4> fill;
    std::array<float, 4> border_color;
    std::array<float, 2> params;
};

struct Renderer::Gl {
    GLuint program{0};
    GLint viewport_uniform{-1};
    GLuint vao{0};
    GLuint quad_vbo{0};
    GLuint instance_vbo{0};

    Gl()
        : program{link_program(kVertexShader, kFragmentShader)},
          viewport_uniform{glGetUniformLocation(program, "u_viewport")} {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        static constexpr std::array<float, 8> kQuad{0, 0, 1, 0, 0, 1, 1, 1};
        glGenBuffers(1, &quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        glGenBuffers(1, &instance_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo);

        struct Attribute {
            GLuint location;
            GLint components;
            std::size_t offset;
        };

        static constexpr std::array<Attribute, 5> kAttributes{{
            {1, 4, offsetof(Instance, rect)},
            {2, 4, offsetof(Instance, radii)},
            {3, 4, offsetof(Instance, fill)},
            {4, 4, offsetof(Instance, border_color)},
            {5, 2, offsetof(Instance, params)},
        }};
        for (const auto& attribute : kAttributes) {
            glEnableVertexAttribArray(attribute.location);
            // GL takes buffer offsets as pointers.
            glVertexAttribPointer(attribute.location, attribute.components, GL_FLOAT, GL_FALSE,
                                  sizeof(Instance),
                                  reinterpret_cast<const void*>(attribute.offset));  // NOLINT
            glVertexAttribDivisor(attribute.location, 1);
        }
        glBindVertexArray(0);
    }

    ~Gl() {
        glDeleteBuffers(1, &instance_vbo);
        glDeleteBuffers(1, &quad_vbo);
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(program);
    }

    Gl(const Gl&) = delete;
    Gl& operator=(const Gl&) = delete;
    Gl(Gl&&) = delete;
    Gl& operator=(Gl&&) = delete;
};

Renderer::Renderer() : gl_{std::make_unique<Gl>()} { instances_.reserve(kMaxInstancesPerDraw); }

Renderer::~Renderer() = default;

void Renderer::begin_frame(std::int32_t buffer_width, std::int32_t buffer_height, float scale,
                           Color clear) {
    buffer_width_ = buffer_width;
    buffer_height_ = buffer_height;
    scale_ = scale;
    instances_.clear();
    clips_.clear();

    glViewport(0, 0, buffer_width_, buffer_height_);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(clear.r, clear.g, clear.b, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // premultiplied alpha
    glUseProgram(gl_->program);
    glUniform2f(gl_->viewport_uniform, static_cast<float>(buffer_width_),
                static_cast<float>(buffer_height_));
    glBindVertexArray(gl_->vao);
}

void Renderer::end_frame() {
    flush();
    if (!clips_.empty()) {
        log::warn("renderer: {} clip(s) still pushed at end of frame", clips_.size());
    }
    glBindVertexArray(0);
}

void Renderer::draw_box(const Rect& rect, const BoxStyle& style) {
    if (rect.empty()) {
        return;
    }
    const float x0 = snap(rect.x, scale_);
    const float y0 = snap(rect.y, scale_);
    const float x1 = snap(rect.right(), scale_);
    const float y1 = snap(rect.bottom(), scale_);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    // Borders are whole physical pixels, at least one, so a 1 px border is a crisp hairline.
    const float border =
        style.border_width > 0.0F ? std::max(1.0F, snap(style.border_width, scale_)) : 0.0F;
    const Color fill = style.fill.premultiplied();
    const Color border_color = style.border.premultiplied();
    instances_.push_back({
        .rect = {x0, y0, x1 - x0, y1 - y0},
        .radii = {style.radius.top_left * scale_, style.radius.top_right * scale_,
                  style.radius.bottom_right * scale_, style.radius.bottom_left * scale_},
        .fill = {fill.r, fill.g, fill.b, fill.a},
        .border_color = {border_color.r, border_color.g, border_color.b, border_color.a},
        .params = {border, 0.0F},
    });
    if (instances_.size() == kMaxInstancesPerDraw) {
        flush();
    }
}

void Renderer::draw_shadow(const Rect& rect, const Corners& radius, float blur, Color color) {
    if (rect.empty() || blur <= 0.0F) {
        return;
    }
    const Color premultiplied = color.premultiplied();
    instances_.push_back({
        .rect = {rect.x * scale_, rect.y * scale_, rect.width * scale_, rect.height * scale_},
        .radii = {radius.top_left * scale_, radius.top_right * scale_, radius.bottom_right * scale_,
                  radius.bottom_left * scale_},
        .fill = {premultiplied.r, premultiplied.g, premultiplied.b, premultiplied.a},
        .border_color = {0.0F, 0.0F, 0.0F, 0.0F},
        .params = {0.0F, blur * scale_},
    });
    if (instances_.size() == kMaxInstancesPerDraw) {
        flush();
    }
}

void Renderer::push_clip(const Rect& rect) {
    flush();
    clips_.push_back(clips_.empty() ? rect : clips_.back().intersect(rect));
    apply_clip();
}

void Renderer::pop_clip() {
    if (clips_.empty()) {
        log::warn("renderer: pop_clip without push_clip");
        return;
    }
    flush();
    clips_.pop_back();
    apply_clip();
}

void Renderer::apply_clip() {
    if (clips_.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Rect& clip = clips_.back();
    const auto x0 = static_cast<GLint>(snap(clip.x, scale_));
    const auto y0 = static_cast<GLint>(snap(clip.y, scale_));
    const auto x1 = static_cast<GLint>(snap(clip.right(), scale_));
    const auto y1 = static_cast<GLint>(snap(clip.bottom(), scale_));
    glEnable(GL_SCISSOR_TEST);
    // GL's scissor origin is the bottom-left corner.
    glScissor(x0, buffer_height_ - y1, std::max(0, x1 - x0), std::max(0, y1 - y0));
}

void Renderer::flush() {
    if (instances_.empty()) {
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, gl_->instance_vbo);
    // Orphan the previous storage so the driver never waits for the last draw to finish.
    const auto bytes = static_cast<GLsizeiptr>(instances_.size() * sizeof(Instance));
    glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, instances_.data());
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(instances_.size()));
    instances_.clear();
}

}  // namespace nordend::ui
