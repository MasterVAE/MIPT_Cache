#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstddef>

enum LayerType {
    LAYER_ARC,
    LAYER_2Q,
    LAYER_LFU,
    LAYER_LIRS
};

struct ConfigLayer {
    std::size_t size;
    LayerType type;
};

struct Config {
    std::vector<ConfigLayer> layers;
};

std::unique_ptr<Config> LoadConfig(const std::string& filename);