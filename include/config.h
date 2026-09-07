#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>

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
    size_t layers_count;
    ConfigLayer* layers;
};

Config* LoadConfig(const char* filename);

#define VALUE int

#endif //CONFIG_H