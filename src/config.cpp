#include <stdio.h>
#include <string.h>

#include "config.h"

Config* LoadConfig(const char* filename)
{
    if(!filename) return NULL;

    FILE* file = fopen(filename, "r");
    if(!file) return NULL;

    Config* config = (Config*)calloc(1, sizeof(Config));
    if(!config) return NULL;

    size_t layers_count;

    if(fscanf(file, "%lu", &layers_count) <= 0) return NULL;

    config->layers_count = layers_count;
    config->layers = (ConfigLayer*)calloc(layers_count, sizeof(ConfigLayer));

    for(size_t i = 0; i < layers_count; i++)
    {
        size_t layer_size;
        char* layer_type = (char*)calloc(101, sizeof(char));
        if(fscanf(file, "%lu %100s", &layer_size, layer_type) <= 0) return NULL;

        config->layers[i].size = layer_size;

             if(!strcmp(layer_type, "ARC"))     config->layers[i].type = LAYER_ARC;
        else if(!strcmp(layer_type, "2Q"))      config->layers[i].type = LAYER_2Q;
        else if(!strcmp(layer_type, "LFU"))     config->layers[i].type = LAYER_LFU;
        else if(!strcmp(layer_type, "LRU"))     config->layers[i].type = LAYER_LRU;
        else if(!strcmp(layer_type, "LIRS"))    config->layers[i].type = LAYER_LIRS;
        else return NULL;

        free(layer_type);
    }

    return config;
}