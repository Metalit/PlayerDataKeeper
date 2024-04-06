#pragma once

#include "scotland2/shared/modloader.h"

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#define ALLOWED_FILES { \
    "AvatarData.dat", "LocalDailyLeaderboards.dat", \
    "LocalLeaderboards.dat", "PlayerData.dat", \
    "settings.cfg" \
}

static constexpr auto Logger = Paper::ConstLoggerContext(MOD_ID);

#define PLAYERDATAKEEPER_EXPORT __attribute__((visibility("default")))
#ifdef __cplusplus
#define PLAYERDATAKEEPER_EXPORT_FUNC extern "C" PLAYERDATAKEEPER_EXPORT
#else
#define PLAYERDATAKEEPER_EXPORT_FUNC PLAYERDATAKEEPER_EXPORT
#endif
