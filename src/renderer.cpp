#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "rng.h"
#include "textures.h"

namespace {

const char* VERTEX_SRC = R"(#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in float shade;
layout(location = 2) in vec2 tex;
layout(location = 3) in vec2 light;
uniform mat4 mvp;
uniform mat4 model;
uniform vec3 eye;
uniform vec2 model_light;  // models: the light where they stand; chunks: (1, 0)
out vec2 v_tex;
out float v_shade;
out float v_dist;
out vec2 v_light;
void main() {
    vec4 world = model * vec4(position, 1.0);
    gl_Position = mvp * world;
    v_tex = tex;
    v_shade = shade;
    v_dist = length(world.xyz - eye);
    v_light = vec2(light.x * model_light.x, max(light.y, model_light.y));
}
)";

// Light levels become brightness with Beta's curve: level 15 is full, each level below it a
// little darker, never quite black. Sky light loses `darkness` levels at night.
const char* FRAGMENT_SRC = R"(#version 330 core
in vec2 v_tex;
in float v_shade;
in float v_dist;
in vec2 v_light;
uniform sampler2D atlas;
uniform vec4 fog_color;
uniform float fog_start;
uniform float fog_end;
uniform float alpha_cut;
uniform float darkness;
out vec4 out_color;
void main() {
    vec4 c = texture(atlas, v_tex);
    if (c.a < alpha_cut) discard;
    float level = clamp(max(v_light.x * 15.0 - darkness, v_light.y * 15.0), 0.0, 15.0);
    float f = 1.0 - level / 15.0;
    float bright = (1.0 - f) / (f * 3.0 + 1.0) * 0.95 + 0.05;
    c.rgb *= v_shade * bright;
    float t = clamp((v_dist - fog_start) / (fog_end - fog_start), 0.0, 1.0);
    c.rgb = mix(c.rgb, fog_color.rgb, t);
    out_color = c;
}
)";

const char* SKY_VERTEX_SRC = R"(#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec4 color;
uniform mat4 mvp;    // projection times the camera's rotation (the camera is at the origin)
uniform mat4 model;
out vec2 v_tex;
out vec4 v_color;
out float v_dist;
void main() {
    vec4 p = model * vec4(position, 1.0);
    gl_Position = mvp * p;
    v_tex = tex;
    v_color = color;
    v_dist = length(p.xyz);
}
)";

const char* SKY_FRAGMENT_SRC = R"(#version 330 core
in vec2 v_tex;
in vec4 v_color;
in float v_dist;
uniform sampler2D image;
uniform int textured;
uniform vec4 tint;
uniform vec3 fog_color;
uniform float fog_start;
uniform float fog_end;  // 0: no fog
out vec4 out_color;
void main() {
    vec4 c = v_color * tint;
    if (textured != 0) c *= texture(image, v_tex);
    if (fog_end > 0.0) c.rgb = mix(c.rgb, fog_color, clamp((v_dist - fog_start) / (fog_end - fog_start), 0.0, 1.0));
    out_color = c;
}
)";

// The sun (left) and the moon (right), 16 x 16 pixels each, on black: they are drawn by adding
// their colour to the sky, so black shows nothing.
Image make_sky_image() {
    Image img(32, 16, {0, 0, 0, 255});
    for (int y = 3; y < 13; ++y)
        for (int x = 3; x < 13; ++x) {
            bool edge = x == 3 || x == 12 || y == 3 || y == 12;
            bool core = x >= 5 && x <= 10 && y >= 5 && y <= 10;
            img.at(x, y) = edge ? Rgba{200, 150, 40, 255} : core ? Rgba{255, 255, 225, 255} : Rgba{255, 230, 120, 255};
        }
    const char* moon[10] = {
        "..oooooo..", ".oooooooo.", "oo#oooo#oo", "o###oooooo", "oo#ooo##oo",
        "oooooo##oo", "oo##oooooo", "o###ooo#oo", ".o#ooooo#.", "..oooooo..",
    };
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            char ch = moon[y][x];
            if (ch == 'o') img.at(19 + x, 3 + y) = {225, 228, 235, 255};
            if (ch == '#') img.at(19 + x, 3 + y) = {150, 155, 170, 255};
        }
    return img;
}

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        throw std::runtime_error(std::string("shader compile failed: ") + log);
    }
    return s;
}

}  // namespace

// --- Shader programs --------------------------------------------------------------------

ShaderProgram::ShaderProgram(const char* vertex_src, const char* fragment_src) {
    GLuint vs = compile(GL_VERTEX_SHADER, vertex_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_src);
    id_ = glCreateProgram();
    glAttachShader(id_, vs);
    glAttachShader(id_, fs);
    glLinkProgram(id_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(id_, sizeof log, nullptr, log);
        throw std::runtime_error(std::string("shader link failed: ") + log);
    }
}

ShaderProgram::~ShaderProgram() {
    if (id_) glDeleteProgram(id_);
}

// --- Meshes on the graphics card -------------------------------------------------------

GpuMesh::GpuMesh(const MeshData& mesh) : count_(static_cast<GLsizei>(mesh.indices.size())) {
    if (!count_) return;
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);
    const GLsizei stride = sizeof(Vertex);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, x)));
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, shade)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, u)));
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, sky)));
    for (GLuint i = 0; i < 4; ++i) glEnableVertexAttribArray(i);
    glBindVertexArray(0);
}

GpuMesh::~GpuMesh() { release(); }

GpuMesh::GpuMesh(GpuMesh&& o) noexcept : vao_(o.vao_), vbo_(o.vbo_), ebo_(o.ebo_), count_(o.count_) {
    o.vao_ = o.vbo_ = o.ebo_ = 0;
    o.count_ = 0;
}

GpuMesh& GpuMesh::operator=(GpuMesh&& o) noexcept {
    if (this != &o) {
        release();
        vao_ = o.vao_;
        vbo_ = o.vbo_;
        ebo_ = o.ebo_;
        count_ = o.count_;
        o.vao_ = o.vbo_ = o.ebo_ = 0;
        o.count_ = 0;
    }
    return *this;
}

void GpuMesh::release() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    vao_ = vbo_ = ebo_ = 0;
    count_ = 0;
}

void GpuMesh::draw(GLenum mode) const {
    if (!count_) return;
    glBindVertexArray(vao_);
    glDrawElements(mode, count_, GL_UNSIGNED_INT, nullptr);
}

GLuint upload_texture(const Image& image) {
    // OpenGL's first row is the bottom one, so upload the image upside down.
    std::vector<Rgba> flipped(image.pixels.size());
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) flipped[static_cast<size_t>(image.height - 1 - y) * image.width + x] = image.at(x, y);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// --- Sky meshes -------------------------------------------------------------------------

SkyMesh::SkyMesh() {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = sizeof(SkyVertex);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkyVertex, x)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkyVertex, u)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkyVertex, r)));
    for (GLuint i = 0; i < 3; ++i) glEnableVertexAttribArray(i);
    glBindVertexArray(0);
}

SkyMesh::~SkyMesh() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
}

void SkyMesh::set(const std::vector<SkyVertex>& triangles) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(triangles.size() * sizeof(SkyVertex)), triangles.data(),
                 GL_DYNAMIC_DRAW);
    count_ = static_cast<GLsizei>(triangles.size());
}

void SkyMesh::draw() const {
    if (!count_) return;
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, count_);
}

void add_sky_quad(std::vector<SkyVertex>& out, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec4 col,
                  std::array<float, 4> uv) {
    auto v = [&](glm::vec3 p, float u, float vv) { out.push_back({p.x, p.y, p.z, u, vv, col.r, col.g, col.b, col.a}); };
    v(a, uv[0], uv[1]);
    v(b, uv[2], uv[1]);
    v(c, uv[2], uv[3]);
    v(a, uv[0], uv[1]);
    v(c, uv[2], uv[3]);
    v(d, uv[0], uv[3]);
}

std::vector<SkyVertex> build_clouds(int cx, int cz, int radius, bool fancy) {
    std::vector<SkyVertex> out;
    const float S = CLOUD_CELL, T = fancy ? CLOUD_THICKNESS : 0;
    for (int dz = -radius; dz <= radius; ++dz)
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dz * dz > radius * radius || !cloud_at(cx + dx, cz + dz)) continue;
            float x0 = dx * S, x1 = x0 + S, z0 = dz * S, z1 = z0 + S;
            add_sky_quad(out, {x0, T, z0}, {x1, T, z0}, {x1, T, z1}, {x0, T, z1}, {1, 1, 1, 1});
            if (!fancy) continue;
            // Sides and bottom are shaded like block faces; faces between two clouds are left out.
            add_sky_quad(out, {x0, 0, z0}, {x0, 0, z1}, {x1, 0, z1}, {x1, 0, z0}, {0.7f, 0.7f, 0.7f, 1});
            if (!cloud_at(cx + dx - 1, cz + dz)) add_sky_quad(out, {x0, 0, z0}, {x0, T, z0}, {x0, T, z1}, {x0, 0, z1}, {0.9f, 0.9f, 0.9f, 1});
            if (!cloud_at(cx + dx + 1, cz + dz)) add_sky_quad(out, {x1, 0, z0}, {x1, 0, z1}, {x1, T, z1}, {x1, T, z0}, {0.9f, 0.9f, 0.9f, 1});
            if (!cloud_at(cx + dx, cz + dz - 1)) add_sky_quad(out, {x0, 0, z0}, {x1, 0, z0}, {x1, T, z0}, {x0, T, z0}, {0.8f, 0.8f, 0.8f, 1});
            if (!cloud_at(cx + dx, cz + dz + 1)) add_sky_quad(out, {x0, 0, z1}, {x0, T, z1}, {x1, T, z1}, {x1, 0, z1}, {0.8f, 0.8f, 0.8f, 1});
        }
    return out;
}

// --- World renderer ---------------------------------------------------------------------

WorldRenderer::WorldRenderer(World& world, GLuint atlas, int rd)
    : render_distance(rd), streamer_(world), atlas_(atlas), program_(VERTEX_SRC, FRAGMENT_SRC),
      sky_program_(SKY_VERTEX_SRC, SKY_FRAGMENT_SRC) {
    for (auto& [id, mesh] : build_models()) models_.emplace(id, GpuMesh(mesh));
    sky_texture_ = upload_texture(make_sky_image());

    // A flat ceiling 16 blocks above the camera, coloured with the sky colour and fogged towards
    // the horizon, like Beta's sky.
    std::vector<SkyVertex> plane;
    for (int x = -384; x < 384; x += 64)
        for (int z = -384; z < 384; z += 64) {
            float x0 = static_cast<float>(x), x1 = x0 + 64, z0 = static_cast<float>(z), z1 = z0 + 64;
            add_sky_quad(plane, {x0, 16, z0}, {x1, 16, z0}, {x1, 16, z1}, {x0, 16, z1}, {1, 1, 1, 1});
        }
    sky_plane_.set(plane);

    // 1500 stars at fixed random spots on a sphere (seen from inside), like Beta.
    std::vector<SkyVertex> stars;
    Rng rng(10842);
    for (int i = 0; i < 1500; ++i) {
        glm::vec3 d(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1));
        float size = static_cast<float>(rng.uniform(0.15, 0.25));
        float len2 = glm::dot(d, d);
        if (len2 >= 1 || len2 <= 0.01f) continue;
        d = glm::normalize(d) * 100.0f;
        glm::vec3 side = glm::normalize(glm::cross(d, std::abs(d.y) < 90 ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0))) * size;
        glm::vec3 up = glm::normalize(glm::cross(side, d)) * size;
        add_sky_quad(stars, d - side - up, d + side - up, d + side + up, d - side + up, {1, 1, 1, 1});
    }
    stars_.set(stars);

    std::vector<SkyVertex> quad;
    add_sky_quad(quad, {-15, 100, -15}, {15, 100, -15}, {15, 100, 15}, {-15, 100, 15}, {1, 1, 1, 1}, {0, 0, 0.5f, 1});
    sun_.set(quad);
    quad.clear();
    add_sky_quad(quad, {-10, -100, 10}, {10, -100, 10}, {10, -100, -10}, {-10, -100, -10}, {1, 1, 1, 1}, {0.5f, 0, 1, 1});
    moon_.set(quad);
}

void WorldRenderer::upload(ReadyMesh& ready) {
    auto it = meshes_.find(ready.pos);
    bool first = it == meshes_.end();
    ChunkGpu gpu{GpuMesh(ready.mesh.solid), GpuMesh(ready.mesh.water)};
    if (first)
        meshes_.emplace(ready.pos, std::move(gpu));
    else
        it->second = std::move(gpu);
    if (first && on_chunk_shown) on_chunk_shown(ready.pos);
}

void WorldRenderer::update(double px, double pz) {
    streamer_.update(px, pz, render_distance);
    for (ReadyMesh& r : streamer_.take_ready()) upload(r);
    int pcx = chunk_of(static_cast<int>(std::floor(px))), pcz = chunk_of(static_cast<int>(std::floor(pz)));
    int limit = (render_distance + 2) * (render_distance + 2);
    for (auto it = meshes_.begin(); it != meshes_.end();) {
        int dx = it->first.cx - pcx, dz = it->first.cz - pcz;
        if (dx * dx + dz * dz > limit) {
            streamer_.forget(it->first);
            it = meshes_.erase(it);
        } else {
            ++it;
        }
    }
}

void WorldRenderer::load_all(double px, double pz, int radius) {
    streamer_.finish(px, pz, radius);
    for (ReadyMesh& r : streamer_.take_ready()) upload(r);
}

void WorldRenderer::set_mesh_options(MeshOptions options) {
    if (streamer_.set_mesh_options(options)) meshes_.clear();
}

void WorldRenderer::rebuild(const std::vector<ChunkPos>& chunks) {
    for (ChunkPos pos : chunks) {
        // Only chunks that are shown or about to be; others get meshed when they come into range.
        if (!meshes_.count(pos) && !streamer_.is_pending(pos)) continue;
        ReadyMesh r = streamer_.rebuild_now(pos);
        upload(r);
    }
}

void WorldRenderer::draw_sky(const glm::mat4& proj_rot, const SkyState& sky, float far_dist) {
    sky_program_.use();
    const glm::mat4 identity(1.0f);
    glUniformMatrix4fv(sky_program_.uniform("mvp"), 1, GL_FALSE, glm::value_ptr(proj_rot));
    glUniformMatrix4fv(sky_program_.uniform("model"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniform3f(sky_program_.uniform("fog_color"), sky.fog.r, sky.fog.g, sky.fog.b);
    glUniform1i(sky_program_.uniform("image"), 0);
    GLint textured = sky_program_.uniform("textured"), tint = sky_program_.uniform("tint");
    GLint fog_start = sky_program_.uniform("fog_start"), fog_end = sky_program_.uniform("fog_end");
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glDisable(GL_BLEND);
    glUniform1i(textured, 0);
    glUniform4f(tint, sky.sky.r, sky.sky.g, sky.sky.b, 1);
    glUniform1f(fog_start, 0);
    glUniform1f(fog_end, std::max(far_dist, 48.0f));
    sky_plane_.draw();
    glUniform1f(fog_end, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (sky.sunset) {
        // A fan of colour around the horizon, strongest on the side of the rising or setting sun.
        glm::vec4 c = *sky.sunset;
        float side = sky.angle > 0.5f ? 1.0f : -1.0f;  // sunrise in the east (+x), sunset in the west
        std::vector<SkyVertex> fan;
        glm::vec3 centre(side * 100, 0, 0);
        const int N = 16;
        for (int j = 0; j < N; ++j) {
            auto ring = [&](int k) {
                float a = k * 6.2831853f / N;
                return glm::vec3(side * 120 * std::cos(a), 40 * c.a * std::cos(a), 120 * std::sin(a));
            };
            glm::vec3 p = ring(j), q = ring(j + 1);
            fan.push_back({centre.x, centre.y, centre.z, 0, 0, c.r, c.g, c.b, c.a});
            fan.push_back({p.x, p.y, p.z, 0, 0, c.r, c.g, c.b, 0});
            fan.push_back({q.x, q.y, q.z, 0, 0, c.r, c.g, c.b, 0});
        }
        glow_.set(fan);
        glUniform4f(tint, 1, 1, 1, 1);
        glow_.draw();
    }

    // Sun, moon and stars turn with the time of day around the north-south axis, so the sun
    // rises in the east (+x) and sets in the west.
    glm::mat4 turn = glm::rotate(glm::mat4(1.0f), sky.angle * 6.2831853f, glm::vec3(0, 0, 1));
    glUniformMatrix4fv(sky_program_.uniform("model"), 1, GL_FALSE, glm::value_ptr(turn));
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    if (sky.stars > 0) {
        glUniform4f(tint, sky.stars, sky.stars, sky.stars, 1);
        stars_.draw();
    }
    glUniform1i(textured, 1);
    glUniform4f(tint, 1, 1, 1, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sky_texture_);
    sun_.draw();
    moon_.draw();
    glUniform1i(textured, 0);
    glDepthMask(GL_TRUE);
}

void WorldRenderer::draw_clouds(const glm::mat4& proj_rot, glm::vec3 eye, const SkyState& sky, float far_dist) {
    if (!sky.show_clouds) return;
    // The cloud map drifts towards +x; find the map cell above the camera.
    double map_x = (eye.x - sky.cloud_offset) / CLOUD_CELL, map_z = static_cast<double>(eye.z) / CLOUD_CELL;
    int cx = static_cast<int>(std::floor(map_x)), cz = static_cast<int>(std::floor(map_z));
    int radius = std::max(10, static_cast<int>(far_dist * 1.5f) / CLOUD_CELL + 2);
    CloudKey key{cx, cz, radius, sky.fancy_clouds};
    if (cloud_key_ != key) {
        clouds_.set(build_clouds(cx, cz, radius, sky.fancy_clouds));
        cloud_key_ = key;
    }
    glm::vec3 corner(static_cast<float>(cx * CLOUD_CELL + sky.cloud_offset - eye.x), CLOUD_HEIGHT - eye.y,
                     static_cast<float>(cz * CLOUD_CELL) - eye.z);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), corner);

    sky_program_.use();
    glUniformMatrix4fv(sky_program_.uniform("mvp"), 1, GL_FALSE, glm::value_ptr(proj_rot));
    glUniformMatrix4fv(sky_program_.uniform("model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(sky_program_.uniform("textured"), 0);
    glUniform4f(sky_program_.uniform("tint"), sky.clouds.r, sky.clouds.g, sky.clouds.b, 0.8f);
    glUniform3f(sky_program_.uniform("fog_color"), sky.fog.r, sky.fog.g, sky.fog.b);
    glUniform1f(sky_program_.uniform("fog_start"), radius * CLOUD_CELL * 0.4f);
    glUniform1f(sky_program_.uniform("fog_end"), radius * CLOUD_CELL * 0.95f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (sky.fancy_clouds) {
        // Depth first, then colour only where the nearest cloud face is, so overlapping faces
        // inside the see-through clouds don't add up.
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        clouds_.draw();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        clouds_.draw();
        glDepthFunc(GL_LESS);
    } else {
        clouds_.draw();
    }
}

void WorldRenderer::draw(int width, int height, glm::vec3 eye, glm::vec3 forward, const SkyState& sky) {
    glClearColor(sky.fog.r, sky.fog.g, sky.fog.b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const float far_dist = static_cast<float>(render_distance * CHUNK);
    const float aspect = static_cast<float>(width) / std::max(1, height);
    glm::mat4 proj = glm::perspective(glm::radians(70.0f), aspect, 0.05f, far_dist + 40);
    glm::mat4 view = glm::lookAt(eye, eye + forward, glm::vec3(0, 1, 0));
    glm::mat4 mvp = proj * view;
    const glm::mat4 identity(1.0f);
    // The sky and clouds are drawn around a camera at the origin, with a far plane that reaches them.
    glm::mat4 sky_proj = glm::perspective(glm::radians(70.0f), aspect, 0.05f, std::max(far_dist * 4, 600.0f));
    glm::mat4 proj_rot = sky_proj * glm::lookAt(glm::vec3(0), forward, glm::vec3(0, 1, 0));
    draw_sky(proj_rot, sky, far_dist);

    program_.use();
    glUniformMatrix4fv(program_.uniform("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(program_.uniform("eye"), eye.x, eye.y, eye.z);
    glUniform4f(program_.uniform("fog_color"), sky.fog.r, sky.fog.g, sky.fog.b, 1);
    glUniform1f(program_.uniform("fog_start"), far_dist * 0.55f);
    glUniform1f(program_.uniform("fog_end"), far_dist - 2);
    glUniform1f(program_.uniform("darkness"), sky.darkness);
    glUniform1i(program_.uniform("atlas"), 0);
    GLint alpha_cut = program_.uniform("alpha_cut");
    GLint model = program_.uniform("model");
    GLint model_light = program_.uniform("model_light");
    glUniform1f(alpha_cut, 0.5f);
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(identity));
    glUniform2f(model_light, 1, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);  // face corners are listed clockwise when seen from outside
    glDisable(GL_BLEND);
    for (const auto& [pos, gpu] : meshes_) gpu.solid.draw();
    for (const ModelPart& part : instances_) {
        auto it = models_.find(part.model);
        if (it == models_.end()) continue;
        glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(part.transform));
        glUniform2f(model_light, part.sky, part.block);
        it->second.draw();
    }
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(identity));
    glUniform2f(model_light, 1, 0);
    if (outline_) {
        // Rebuilt every frame: 12 edges, slightly bigger than the block.
        const float e = 0.003f;
        glm::vec3 b(*outline_);
        glm::vec3 corners[8];
        for (int i = 0; i < 8; ++i)
            corners[i] = {(i & 1) ? b.x + 1 + e : b.x - e, (i & 2) ? b.y + 1 + e : b.y - e, (i & 4) ? b.z + 1 + e : b.z - e};
        const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        auto uv = tile_uv(T_BLACK);
        float u = (uv[0] + uv[2]) / 2, v = (uv[1] + uv[3]) / 2;
        MeshData lines;
        for (const auto& edge : edges)
            for (int k : edge) {
                lines.indices.push_back(static_cast<uint32_t>(lines.vertices.size()));
                lines.vertices.push_back({corners[k].x, corners[k].y, corners[k].z, 1.0f, u, v});
            }
        outline_mesh_ = GpuMesh(lines);
        outline_mesh_.draw(GL_LINES);
    }

    draw_clouds(proj_rot, eye, sky, far_dist);

    // Water: translucent, both sides visible, drawn last without writing depth.
    program_.use();
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glUniform1f(alpha_cut, 0.0f);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (const auto& [pos, gpu] : meshes_) gpu.water.draw();
    glDepthMask(GL_TRUE);

    // Leave the state the 2D HUD expects.
    glDisable(GL_DEPTH_TEST);
    glFrontFace(GL_CCW);
    glBindVertexArray(0);
}
