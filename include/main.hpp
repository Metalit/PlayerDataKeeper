#pragma once

#include "paper/shared/logger.hpp"

#define DATA_PATH "/storage/emulated/0/Android/data/com.beatgames.beatsaber/files/"
#define NOBACKUP_PATH "/storage/emulated/0/Android/data/com.beatgames.beatsaber/no_backup/"
#define ALLOWED_FILES { \
    "AvatarData.dat", "LocalDailyLeaderboards.dat", \
    "LocalLeaderboards.dat", "PlayerData.dat", \
    "settings.cfg", "MainSettings.json", \
    "GraphicsSettings.json" \
}

static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

#define PLAYERDATAKEEPER_EXPORT __attribute__((visibility("default")))
#ifdef __cplusplus
#define PLAYERDATAKEEPER_EXPORT_FUNC extern "C" PLAYERDATAKEEPER_EXPORT
#else
#define PLAYERDATAKEEPER_EXPORT_FUNC PLAYERDATAKEEPER_EXPORT
#endif
