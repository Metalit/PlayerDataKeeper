#pragma once

#include <regex>

#include "paper/shared/logger.hpp"

#define BLACKLIST std::regex("tombstone_..")

static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

#define PLAYERDATAKEEPER_EXPORT __attribute__((visibility("default")))
#ifdef __cplusplus
#define PLAYERDATAKEEPER_EXPORT_FUNC extern "C" PLAYERDATAKEEPER_EXPORT
#else
#define PLAYERDATAKEEPER_EXPORT_FUNC PLAYERDATAKEEPER_EXPORT
#endif
