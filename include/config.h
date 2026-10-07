#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>
#include <vector>
#include <memory>

enum LayerType
{
    LAYER_ARC,
    LAYER_2Q,
    LAYER_LFU,
    LAYER_LRU,
    LAYER_LIRS
};

struct ConfigLayer
{
    size_t size;
    LayerType type;
};

struct Config
{
    std::vector<ConfigLayer> layers;
};

std::unique_ptr<Config> LoadConfig(const std::string& filename);

#define VALUE int

#endif //CONFIG_H