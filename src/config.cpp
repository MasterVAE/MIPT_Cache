#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "config.h"

namespace
{

LayerType ParseLayerType(std::string_view name)
{
    static const std::unordered_map<std::string_view, LayerType> kTypes = {
        {"ARC",  LAYER_ARC},
        {"2Q",   LAYER_2Q},
        {"LFU",  LAYER_LFU},
        {"LRU",  LAYER_LRU},
        {"LIRS", LAYER_LIRS},
    };

    if (auto it = kTypes.find(name); it != kTypes.end()) {
        return it->second;
    }
    throw std::invalid_argument("Unknown layer type: " + std::string(name));
}

}  // namespace

std::unique_ptr<Config> LoadConfig(const std::string& filename)
{
    if (filename.empty()) return nullptr;

    std::ifstream file(filename);
    if (!file) return nullptr;

    auto config = std::make_unique<Config>();

    std::size_t layers_count = 0;
    if (!(file >> layers_count)) return nullptr;

    config->layers.resize(layers_count);

    for (auto& layer : config->layers) 
    {
        std::string type_name;
        if (!(file >> layer.size >> type_name)) return nullptr;

        try
        {
            layer.type = ParseLayerType(type_name);
        }
        catch (const std::invalid_argument&) 
        {
            return nullptr;
        }
    }

    return config;
}