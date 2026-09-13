#pragma once

#include "paper2_scotland2/shared/logger.hpp"
#include "scotland2/shared/loader.hpp"

#include <regex>

#define BLACKLIST std::regex("tombstone_..|.*\\.tmp")
#define LOCAL_FILES_DIR "no_backup"

constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

static inline modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};
