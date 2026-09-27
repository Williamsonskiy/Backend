#pragma once

#include <filesystem>

#include "extra_data.h"
#include "model.h"

namespace json_loader {

struct GameLoadResult {
    model::Game game;
    extra_data::ExtraData extra_data;
};

GameLoadResult LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
