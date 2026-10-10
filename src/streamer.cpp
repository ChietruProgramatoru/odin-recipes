#include "streamer.h"

#include <algorithm>
#include <cmath>

JobPool::JobPool(int threads) {
    for (int i = 0; i < threads; ++i) threads_.emplace_back([this] { run(); });
}

JobPool::~JobPool() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
        queue_.clear();
    }
    wake_.notify_all();
    for (auto& t : threads_) t.join();
}

void JobPool::submit(std::function<void()> job) {
    {
        std::lock_guard lock(mutex_);
        queue_.push_back(std::move(job));
    }
    wake_.notify_one();
}

void JobPool::run() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_) return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        job();
    }
}

namespace {
int pick_threads(int threads) {
    if (threads > 0) return threads;
    int hw = static_cast<int>(std::thread::hardware_concurrency());
    return std::clamp(hw - 1, 1, 6);
}
}  // namespace

ChunkStreamer::ChunkStreamer(World& world, int threads)
    : world_(world), max_in_flight_(pick_threads(threads) * 3), pool_(pick_threads(threads)) {}

ChunkStreamer::~ChunkStreamer() = default;

std::vector<ChunkPos> ChunkStreamer::wanted(double px, double pz, int r) {
    int pcx = chunk_of(static_cast<int>(std::floor(px))), pcz = chunk_of(static_cast<int>(std::floor(pz)));
    std::vector<ChunkPos> keys;
    for (int dx = -r; dx <= r; ++dx)
        for (int dz = -r; dz <= r; ++dz)
            if (dx * dx + dz * dz <= r * r + 1) keys.push_back({pcx + dx, pcz + dz});
    std::stable_sort(keys.begin(), keys.end(), [&](ChunkPos a, ChunkPos b) {
        auto d = [&](ChunkPos k) { return (k.cx - pcx) * (k.cx - pcx) + (k.cz - pcz) * (k.cz - pcz); };
        return d(a) < d(b);
    });
    return keys;
}

void ChunkStreamer::collect() {
    std::vector<GenResult> gens;
    std::vector<MeshResult> meshes;
    {
        std::lock_guard lock(done_mutex_);
        gens.swap(gen_done_);
        meshes.swap(mesh_done_);
    }
    in_flight_ -= static_cast<int>(gens.size() + meshes.size());
    for (GenResult& g : gens) {
        generating_.erase({g.chunk->cx, g.chunk->cz});
        world_.insert(std::move(g.chunk));  // kept out if the main thread generated it meanwhile
    }
    for (MeshResult& m : meshes) {
        auto it = meshes_.find(m.pos);
        if (it == meshes_.end() || it->second.ticket != m.ticket) continue;  // stale
        it->second.state = State::Meshed;
        ready_.push_back({m.pos, std::move(m.mesh)});
    }
}

void ChunkStreamer::update(double px, double pz, int radius) {
    collect();
    for (ChunkPos key : wanted(px, pz, radius)) {
        if (in_flight_ >= max_in_flight_) break;
        if (meshes_.count(key)) continue;
        // A chunk's mesh needs all 8 neighbours, because faces and shading look one block over.
        bool all_there = true;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dz = -1; dz <= 1; ++dz) {
                ChunkPos n{key.cx + dx, key.cz + dz};
                if (world_.find(n.cx, n.cz)) continue;
                all_there = false;
                if (generating_.count(n) || in_flight_ >= max_in_flight_) continue;
                generating_.insert(n);
                ++in_flight_;
                const World* world = &world_;
                pool_.submit([this, world, n] {
                    auto chunk = world->load_or_generate(n.cx, n.cz);
                    std::lock_guard lock(done_mutex_);
                    gen_done_.push_back({std::move(chunk)});
                    done_cv_.notify_all();
                });
            }
        if (!all_there) continue;
        uint64_t ticket = next_ticket_++;
        meshes_[key] = {State::Pending, ticket};
        ++in_flight_;
        auto padded = std::make_shared<Padded>(world_.padded(key.cx, key.cz));
        auto light = std::make_shared<PaddedLight>(world_.padded_light(key.cx, key.cz));
        auto data = std::make_shared<Padded>(world_.padded_data(key.cx, key.cz));
        const Chunk* chunk = world_.find(key.cx, key.cz);
        int max_y = chunk->max_y;
        Climate climate = chunk->climate;
        MeshOptions options = options_;
        pool_.submit([this, key, ticket, padded, light, data, max_y, options, climate] {
            ChunkMesh mesh = build_mesh(*padded, *light, *data, key.cx, key.cz, max_y, options, &climate);
            std::lock_guard lock(done_mutex_);
            mesh_done_.push_back({key, ticket, std::move(mesh)});
            done_cv_.notify_all();
        });
    }
}

std::vector<ReadyMesh> ChunkStreamer::take_ready() {
    std::vector<ReadyMesh> out;
    out.swap(ready_);
    return out;
}

ReadyMesh ChunkStreamer::rebuild_now(ChunkPos pos) {
    meshes_[pos] = {State::Meshed, next_ticket_++};
    // Drop any queued result for this chunk: the fresh mesh replaces it.
    std::erase_if(ready_, [&](const ReadyMesh& r) { return r.pos == pos; });
    return {pos, world_.build_mesh_now(pos.cx, pos.cz, options_)};
}

bool ChunkStreamer::set_mesh_options(MeshOptions options) {
    if (options == options_) return false;
    options_ = options;
    // Every mesh is out of date. Results still in flight carry old tickets and get dropped.
    meshes_.clear();
    ready_.clear();
    return true;
}

void ChunkStreamer::forget(ChunkPos pos) {
    meshes_.erase(pos);
    std::erase_if(ready_, [&](const ReadyMesh& r) { return r.pos == pos; });
}

void ChunkStreamer::finish(double px, double pz, int radius) {
    auto keys = wanted(px, pz, radius);
    auto done = [&] {
        for (ChunkPos k : keys) {
            auto it = meshes_.find(k);
            if (it == meshes_.end() || it->second.state != State::Meshed) return false;
        }
        return true;
    };
    for (;;) {
        update(px, pz, radius);
        if (done()) return;
        std::unique_lock lock(done_mutex_);
        done_cv_.wait(lock, [this] { return !gen_done_.empty() || !mesh_done_.empty(); });
    }
}

bool ChunkStreamer::has_mesh(ChunkPos pos) const {
    auto it = meshes_.find(pos);
    return it != meshes_.end() && it->second.state == State::Meshed;
}

bool ChunkStreamer::is_pending(ChunkPos pos) const {
    auto it = meshes_.find(pos);
    return it != meshes_.end() && it->second.state == State::Pending;
}
