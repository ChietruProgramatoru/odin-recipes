// OpenGL rendering: chunk meshes, cows, dropped items and the block outline.
#pragma once
#include <array>
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
#include "sky.h"
#include "streamer.h"
#include "weather.h"
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
GLuint upload_texture(const Image& image, bool repeat = false);  // repeat: GL_REPEAT instead of clamping

// Vertex for the sky, sun, moon, stars and clouds: position, uv and colour.
struct SkyVertex {
    float x, y, z;
    float u, v;
    float r, g, b, a;
};

// Plain triangles for the sky and clouds (no index buffer), replaced with set().
class SkyMesh {
public:
    SkyMesh();
    ~SkyMesh();
    SkyMesh(const SkyMesh&) = delete;
    SkyMesh& operator=(const SkyMesh&) = delete;
    void set(const std::vector<SkyVertex>& triangles);
    void draw() const;

private:
    GLuint vao_ = 0, vbo_ = 0;
    GLsizei count_ = 0;
};

// Two triangles for a quad with corners a, b, c, d (in order around it).
void add_sky_quad(std::vector<SkyVertex>& out, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                  glm::vec4 color, std::array<float, 4> uv = {0, 0, 1, 1});
// Cloud boxes (fancy) or flat cloud squares (fast) for the cells within `radius` cells of
// cloud cell (cx, cz), positioned relative to that cell's corner. Shading is baked into the colour.
std::vector<SkyVertex> build_clouds(int cx, int cz, int radius, bool fancy);

class WorldRenderer {
public:
    WorldRenderer(World& world, GLuint atlas, int render_distance);

    int render_distance;
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
    // How many times blocks have been changed (rebuild() calls), so a click can tell whether it did anything.
    long edit_count() const { return edits_; }
    size_t mesh_count() const { return meshes_.size(); }
    int jobs_in_flight() const { return streamer_.jobs_in_flight(); }

    void set_instances(std::vector<ModelPart> instances) { instances_ = std::move(instances); }
    // The boxes (world coordinates, low and high corners) outlined around the block aimed at.
    void set_outline(std::vector<std::pair<glm::vec3, glm::vec3>> boxes) { outline_ = std::move(boxes); }
    // View bobbing: applied to the camera after it is pointed (identity: none).
    void set_view_bob(const glm::mat4& bob) { view_bob_ = bob; }
    // Falling rain and snow around the player, and lightning bolts (position and zigzag seed).
    // `seconds` moves the drops down.
    struct Bolt {
        glm::dvec3 pos;
        uint64_t seed;
    };
    void set_weather(std::vector<PrecipColumn> columns, std::vector<Bolt> bolts, double seconds) {
        precip_ = std::move(columns);
        bolts_ = std::move(bolts);
        weather_seconds_ = seconds;
    }
    // Clears the screen to the fog colour and draws the sky, the world and the clouds.
    void draw(int width, int height, glm::vec3 eye, glm::vec3 forward, const SkyState& sky);
    // Draw models on their own into part of the window (like the player in the inventory screen),
    // seen through `view_proj`, without fog. Leaves the viewport on that part of the window.
    void draw_models(const std::vector<ModelPart>& parts, int x, int y, int width, int height, const glm::mat4& view_proj);
    // The first-person hand over the world: parts in camera space (x right, y up, looking down -z),
    // with a depth buffer of their own, no fog, and the sky darkened by `darkness` levels like the world.
    void draw_hand(const std::vector<ModelPart>& parts, int width, int height, float darkness);
    // Draw over atlas tiles from `tile` on (its top-left corner), for the pictures that move: the
    // compass and clock icons and the map being held. The image is a whole number of tiles.
    void update_atlas(int tile, const Image& image);

private:
    void draw_sky(const glm::mat4& proj_rot, const SkyState& sky, float far_dist);
    void draw_clouds(const glm::mat4& proj_rot, glm::vec3 eye, const SkyState& sky, float far_dist);
    void draw_weather(const glm::mat4& proj_rot, glm::vec3 eye, const SkyState& sky);

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
    std::vector<std::pair<glm::vec3, glm::vec3>> outline_;
    long edits_ = 0;
    glm::mat4 view_bob_{1.0f};
    GpuMesh outline_mesh_;

    ShaderProgram sky_program_;
    GLuint sky_texture_ = 0;  // sun (left half) and moon (right half)
    SkyMesh sky_plane_, stars_, sun_, moon_, glow_, clouds_;
    struct CloudKey {
        int cx, cz, radius;
        bool fancy;
        bool operator==(const CloudKey&) const = default;
    };
    std::optional<CloudKey> cloud_key_;

    GLuint rain_texture_ = 0, snow_texture_ = 0;
    SkyMesh rain_, snow_, bolt_mesh_;
    std::vector<PrecipColumn> precip_;
    std::vector<Bolt> bolts_;
    double weather_seconds_ = 0;
};
