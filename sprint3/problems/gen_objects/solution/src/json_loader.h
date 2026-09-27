#pragma once

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <boost/json.hpp>
#include "model.h"
#include "app.h"

namespace json_loader {

std::pair<model::Game, std::unordered_map<std::string, app::MapExtraData>> LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
