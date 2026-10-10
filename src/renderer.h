// OpenGL rendering: chunk meshes, cows, dropped items and the block outline.
#pragma once
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "gl.h"
#include "image.h"
#include "mobs.h"
#include "streamer.h"
#include "world.h"

class ShaderProgram {
public:
    ShaderProgram(const char* vertex_src, const char* fragment_src);
    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    void use() const { glUseProgram(id_); }
    GLint uniform(const char* name) const { return glGetUniformLocation(id_, name); }

private:
    GLuint id_ = 0;
};

// A mesh uploaded to the graphics card (position, shade, uv per vertex).
class GpuMesh {
public:
    GpuMesh() = default;
    explicit GpuMesh(const MeshData& mesh);
    ~GpuMesh();
    GpuMesh(GpuMesh&& other) noexcept;
    GpuMesh& operator=(GpuMesh&& other) noexcept;
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;
    void draw(GLenum mode = GL_TRIANGLES) const;
    bool empty() const { return count_ == 0; }

private:
    void release();
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei count_ = 0;
};

// Upload an image as a pixel-crisp texture (v = 1 is the top row of the image).
GLuint upload_texture(const Image& image);

class WorldRenderer {
public:
    WorldRenderer(World& world, GLuint atlas, int render_distance, glm::vec4 sky);

    int render_distance;
    glm::vec4 sky;
    // Called the first time a chunk's mesh is shown (used to spawn cows).
    std::function<void(ChunkPos)> on_chunk_shown;

    // Queue missing nearby chunks, upload finished ones and drop far ones.
    void update(double px, double pz);
    // Build everything within `radius` chunks before the first frame.
    void load_all(double px, double pz, int radius);
    // Mesh chunks differently (video settings). Every chunk is meshed again.
    void set_mesh_options(MeshOptions options);
    // Rebuild these chunks right now (after a block edit).
    void rebuild(const std::vector<ChunkPos>& chunks);
    size_t mesh_count() const { return meshes_.size(); }
    int jobs_in_flight() const { return streamer_.jobs_in_flight(); }

    void set_instances(std::vector<ModelPart> instances) { instances_ = std::move(instances); }
    void set_outline(std::optional<glm::ivec3> block) { outline_ = block; }
    void draw(int width, int height, glm::vec3 eye, glm::vec3 forward);
    // Draw models on their own into part of the window (like the player in the inventory screen),
    // seen through `view_proj`, without fog. Leaves the viewport on that part of the window.
    void draw_models(const std::vector<ModelPart>& parts, int x, int y, int width, int height, const glm::mat4& view_proj);

private:
    struct ChunkGpu {
        GpuMesh solid, water;
    };
    void upload(ReadyMesh& ready);

    ChunkStreamer streamer_;
    GLuint atlas_;
    ShaderProgram program_;
    std::unordered_map<ChunkPos, ChunkGpu, ChunkPosHash> meshes_;
    std::map<int, GpuMesh> models_;
    std::vector<ModelPart> instances_;
    std::optional<glm::ivec3> outline_;
    GpuMesh outline_mesh_;
};
