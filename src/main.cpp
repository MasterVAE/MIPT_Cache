#include <stdio.h>

#include "config.h"
#include "cache.h"
#include "test.h"

static const char* CONFIG_FILENAME = "config.cfg";

int main()
{

    Config* config = LoadConfig(CONFIG_FILENAME);
    if(!config) exit(EXIT_FAILURE);

    Cache* cache = CreateCache(config);
    if(!cache)
    {
        free(config);
        exit(EXIT_FAILURE);
    }

    free(config->layers);
    free(config);

    size_t count;
    if(scanf("%lu", &count) <= 0) 
    {
        DestroyCache(cache);
        exit(EXIT_FAILURE);
    }

    VALUE* numbers = (VALUE*)calloc(count, sizeof(VALUE));
    for(size_t i = 0; i < count; i++)
    {
        scanf("%d", numbers + i);
    }

    int miss = RunCache(cache, count, numbers);
    free(numbers);
    DestroyCache(cache);

    if(miss < 0) exit(EXIT_FAILURE);
    

    printf("MISS: %d\n", miss);

    return 0;
}