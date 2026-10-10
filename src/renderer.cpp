#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "textures.h"

namespace {

const char* VERTEX_SRC = R"(#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in float shade;
layout(location = 2) in vec2 tex;
uniform mat4 mvp;
uniform mat4 model;
uniform vec3 eye;
out vec2 v_tex;
out float v_shade;
out float v_dist;
void main() {
    vec4 world = model * vec4(position, 1.0);
    gl_Position = mvp * world;
    v_tex = tex;
    v_shade = shade;
    v_dist = length(world.xyz - eye);
}
)";

const char* FRAGMENT_SRC = R"(#version 330 core
in vec2 v_tex;
in float v_shade;
in float v_dist;
uniform sampler2D atlas;
uniform vec4 fog_color;
uniform float fog_start;
uniform float fog_end;
uniform float alpha_cut;
out vec4 out_color;
void main() {
    vec4 c = texture(atlas, v_tex);
    if (c.a < alpha_cut) discard;
    c.rgb *= v_shade;
    float t = clamp((v_dist - fog_start) / (fog_end - fog_start), 0.0, 1.0);
    c.rgb = mix(c.rgb, fog_color.rgb, t);
    out_color = c;
}
)";

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
    for (GLuint i = 0; i < 3; ++i) glEnableVertexAttribArray(i);
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

// --- World renderer ---------------------------------------------------------------------

WorldRenderer::WorldRenderer(World& world, GLuint atlas, int rd, glm::vec4 sky_)
    : render_distance(rd), sky(sky_), streamer_(world), atlas_(atlas),
      program_(VERTEX_SRC, FRAGMENT_SRC) {
    for (auto& [id, mesh] : build_models()) models_.emplace(id, GpuMesh(mesh));
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

void WorldRenderer::draw(int width, int height, glm::vec3 eye, glm::vec3 forward) {
    const float far_dist = static_cast<float>(render_distance * CHUNK);
    glm::mat4 proj = glm::perspective(glm::radians(70.0f), static_cast<float>(width) / std::max(1, height), 0.05f, far_dist + 40);
    glm::mat4 view = glm::lookAt(eye, eye + forward, glm::vec3(0, 1, 0));
    glm::mat4 mvp = proj * view;
    const glm::mat4 identity(1.0f);

    program_.use();
    glUniformMatrix4fv(program_.uniform("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(program_.uniform("eye"), eye.x, eye.y, eye.z);
    glUniform4f(program_.uniform("fog_color"), sky.r, sky.g, sky.b, sky.a);
    glUniform1f(program_.uniform("fog_start"), far_dist * 0.55f);
    glUniform1f(program_.uniform("fog_end"), far_dist - 2);
    glUniform1i(program_.uniform("atlas"), 0);
    GLint alpha_cut = program_.uniform("alpha_cut");
    GLint model = program_.uniform("model");
    glUniform1f(alpha_cut, 0.5f);
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(identity));
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
        it->second.draw();
    }
    glUniformMatrix4fv(model, 1, GL_FALSE, glm::value_ptr(identity));

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

    // Water: translucent, both sides visible, drawn last without writing depth.
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
