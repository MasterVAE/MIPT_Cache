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

struct Layer
{
    size_t size;
    LayerType type;
    int* values;
};

struct Cache
{
    size_t layers_count;
    Layer* layers;
};

Cache* LoadConfig(const char* filename);

#endif //CONFIG_H