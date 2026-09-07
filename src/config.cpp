#include <stdio.h>
#include <string.h>

#include "config.h"

Cache* LoadConfig(const char* filename)
{
    if(!filename) return NULL;

    FILE* file = fopen(filename, "r");
    if(!file) return NULL;

    Cache* cache = (Cache*)calloc(1, sizeof(Cache));
    if(!cache) return NULL;

    size_t layers_count;

    if(fscanf(file, "%lu", &layers_count) <= 0) return NULL;

    cache->layers_count = layers_count;
    cache->layers = (Layer*)calloc(layers_count, sizeof(Layer));

    for(size_t i = 0; i < layers_count; i++)
    {
        size_t layer_size;
        char* layer_type = (char*)calloc(101, sizeof(char));
        if(fscanf(file, "%lu %100s", &layer_size, layer_type) <= 0) return NULL;

        cache->layers[i].size = layer_size;
        cache->layers[i].values = (int*)calloc(layer_size, sizeof(int));

             if(!strcmp(layer_type, "ARC"))     cache->layers[i].type = LAYER_ARC;
        else if(!strcmp(layer_type, "2Q"))      cache->layers[i].type = LAYER_2Q;
        else if(!strcmp(layer_type, "LFU"))     cache->layers[i].type = LAYER_LFU;
        else if(!strcmp(layer_type, "LRU"))     cache->layers[i].type = LAYER_LRU;
        else if(!strcmp(layer_type, "LIRS"))    cache->layers[i].type = LAYER_LIRS;
        else return NULL;

        free(layer_type);
    }

    return cache;
}