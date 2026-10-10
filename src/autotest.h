// Scripted smoke test: `PetrutzCraft --seed 7 --autotest OUT_DIR`.
// Drives the real game for a few hundred frames, exercises mining, building, cows, eating and
// fall damage, saves screenshots to OUT_DIR and writes OUT_DIR/report.txt.
#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "blocks.h"

class Game;
class Cow;

class AutoTest {
public:
    explicit AutoTest(std::string out_dir);
    void operator()(Game& g);  // called once per frame, after the game's update
    int failures() const { return failures_; }

private:
    void log(const std::string& msg);
    void check(const std::string& name, bool ok);
    void shot(Game& g, const std::string& name);
    void face(Game& g, double x, double z, double pitch);
    void finish(Game& g);

    std::string out_;
    std::vector<std::string> lines_;
    int failures_ = 0;
    Cow* cow_ = nullptr;
    double cow_start_x_ = 0, cow_start_z_ = 0;
    std::optional<glm::ivec3> broken_;
    BlockId broken_block_ = 0;
    int hp_before_fall_ = 0;
};
