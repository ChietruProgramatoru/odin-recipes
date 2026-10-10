// Background chunk streaming: worker threads generate terrain and build meshes, the main
// thread hands them the data and collects the results. No OpenGL in here.
#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "world.h"

class JobPool {
public:
    explicit JobPool(int threads);
    ~JobPool();
    void submit(std::function<void()> job);

private:
    void run();
    std::vector<std::thread> threads_;
    std::deque<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_ = false;
};

struct ReadyMesh {
    ChunkPos pos;
    ChunkMesh mesh;
};

class ChunkStreamer {
public:
    // threads = 0 picks a count from the CPU (leaving one core for the game).
    ChunkStreamer(World& world, int threads = 0);
    ~ChunkStreamer();

    // Main thread, once per frame: collect finished work, then queue the nearest missing
    // chunks within `radius` chunks of block position (px, pz).
    void update(double px, double pz, int radius);
    // Meshes finished since the last call, nearest first as they were queued.
    std::vector<ReadyMesh> take_ready();
    // Mesh a chunk right now (after a block edit). Any older background result is dropped.
    ReadyMesh rebuild_now(ChunkPos pos);
    // Stop tracking a chunk's mesh (it was unloaded). It is queued again if it comes back in range.
    void forget(ChunkPos pos);
    // Change how chunks are meshed. Forgets every mesh, so all chunks get meshed again.
    void set_mesh_options(MeshOptions options);
    // Block until every chunk within `radius` has a mesh waiting in take_ready().
    void finish(double px, double pz, int radius);

    bool has_mesh(ChunkPos pos) const;   // meshed (taken or waiting in take_ready)
    bool is_pending(ChunkPos pos) const; // meshing in the background
    int jobs_in_flight() const { return in_flight_; }

    // Chunk positions within `radius` of (px, pz), nearest first (same order the Python game used).
    static std::vector<ChunkPos> wanted(double px, double pz, int radius);

private:
    enum class State { Pending, Meshed };
    struct Entry {
        State state;
        uint64_t ticket;  // results with another ticket are stale
    };
    void collect();

    World& world_;
    MeshOptions options_;
    int max_in_flight_;
    int in_flight_ = 0;
    uint64_t next_ticket_ = 1;
    std::unordered_map<ChunkPos, Entry, ChunkPosHash> meshes_;
    std::unordered_set<ChunkPos, ChunkPosHash> generating_;
    std::vector<ReadyMesh> ready_;

    struct GenResult {
        std::unique_ptr<Chunk> chunk;
    };
    struct MeshResult {
        ChunkPos pos;
        uint64_t ticket;
        ChunkMesh mesh;
    };
    std::mutex done_mutex_;
    std::condition_variable done_cv_;
    std::vector<GenResult> gen_done_;
    std::vector<MeshResult> mesh_done_;

    JobPool pool_;  // last member: its threads stop before the queues they write to are destroyed
};
