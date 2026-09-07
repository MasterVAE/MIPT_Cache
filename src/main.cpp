#include <stdio.h>

#include "config.h"
#include "cache.h"
#include "test.h"

static const char* CONFIG_FILENAME = "config.cfg";

int main()
{
    Cache* cache = LoadConfig(CONFIG_FILENAME);
    if(!cache) exit(EXIT_FAILURE);

    RunCache(cache);
    RunTests(cache);

    free(cache);
    return 0;
}