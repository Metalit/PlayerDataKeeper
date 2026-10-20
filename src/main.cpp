#include "main.hpp"
#include "atomic-copy.hpp"

#include <mutex>

#include "config.hpp"
#include "hooks.hpp"
#include "scotland2/shared/modloader.h"

#include "GlobalNamespace/BeatmapCharacteristic.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/ColorSchemesSettings.hpp"
#include "GlobalNamespace/EnvironmentInfoSO.hpp"
#include "GlobalNamespace/EnvironmentType.hpp"
#include "GlobalNamespace/EnvironmentsListModel.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/MultiplayerModeSettings.hpp"
#include "GlobalNamespace/OverrideEnvironmentSettings.hpp"
#include "GlobalNamespace/PlayerAgreements.hpp"
#include "GlobalNamespace/PlayerAllOverallStatsData.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/PlayerDataFileModel.hpp"
#include "GlobalNamespace/PlayerLevelStatsData.hpp"
#include "GlobalNamespace/PlayerMissionStatsData.hpp"
#include "GlobalNamespace/PlayerSaveData.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "System/Collections/Generic/List_1.hpp"
#include "System/IO/File.hpp"
#include "System/ValueTuple_2.hpp"

using namespace GlobalNamespace;
using namespace System::IO;
using namespace System::Collections::Generic;

static bool lightsSet = false;
static std::string filesPath;
static std::string localFilesPath;

static std::mutex& BackupMutex() {
    static std::mutex mutex;
    return mutex;
}

static bool CopyContent(const std::filesystem::path& source, const std::filesystem::path& destination) {
    auto result = DataKeeper::AtomicCopy(source.string(), destination.string(), source.filename() == "PlayerData.dat");
    if (!result) {
        logger.error("DataKeeper copy failed at {} (errno {}): {} -> {}; existing destination retained",
                     result.operation, result.error, source.string(), destination.string());
        return false;
    }
    logger.info("DataKeeper content replacement committed: {} -> {}", source.string(), destination.string());
    return true;
}

static inline std::string GetBackupPath() {
    return getConfig().backupPath.GetValue();
}

static inline std::string GetLocalsBackupPath() {
    return std::filesystem::path(getConfig().backupPath.GetValue()) / LOCAL_FILES_DIR;
}

static void HandleSave(std::filesystem::path fullPath, std::string checkPath, std::string backupPath) {
    if (fullPath.parent_path() == checkPath && !std::filesystem::is_directory(fullPath) && !std::regex_search(fullPath.string(), BLACKLIST)) {
        logger.info("Copying for backup: {} -> {}", fullPath.string(), backupPath);
        CopyContent(fullPath, backupPath / fullPath.filename());
    }
}

static void HandleSave(std::string filePath) {
    std::lock_guard guard(BackupMutex());
    try {
        auto fullPath = std::filesystem::canonical(filePath);
        logger.info("File saved, path: {} ({})", fullPath.string(), filePath);
        HandleSave(fullPath, filesPath, GetBackupPath());
        HandleSave(fullPath, localFilesPath, GetLocalsBackupPath());
    } catch (const std::exception& error) {
        // The game save already succeeded. Report a backup failure without
        // throwing across the managed File hook or damaging the previous copy.
        logger.error("DataKeeper could not back up {}: {}", filePath, error.what());
    }
}

MAKE_AUTO_HOOK_MATCH(File_WriteAllText, &File::WriteAllText, void, StringW path, StringW contents) {
    File_WriteAllText(path, contents);
    HandleSave(path);
}

MAKE_AUTO_HOOK_MATCH(
    File_Replace,
    &File::Replace,
    void,
    StringW sourceFileName,
    StringW destinationFileName,
    StringW destinationBackupFileName,
    bool ignoreMetadataErrors
) {
    File_Replace(sourceFileName, destinationFileName, destinationBackupFileName, ignoreMetadataErrors);
    HandleSave(destinationFileName);
}

MAKE_AUTO_HOOK_MATCH(
    PlayerData_ctor,
    &PlayerData::_ctor,
    void,
    PlayerData* self,
    StringW playerId,
    StringW playerName,
    bool shouldShowTutorialPrompt,
    bool shouldShow360Warning,
    bool agreedToEula,
    bool didSelectLanguage,
    bool agreedToMultiplayerDisclaimer,
    int didSelectRegionVersion,
    StringW selectedAvatarSystemTypeId,
    PlayerAgreements* playerAgreements,
    BeatmapDifficulty lastSelectedBeatmapDifficulty,
    BeatmapCharacteristic lastSelectedBeatmapCharacteristic,
    GameplayModifiers* gameplayModifiers,
    PlayerSpecificSettings* playerSpecificSettings,
    PracticeSettings* practiceSettings,
    PlayerAllOverallStatsData* playerAllOverallStatsData,
    List_1<PlayerLevelStatsData*>* levelsStatsData,
    List_1<PlayerMissionStatsData*>* missionsStatsData,
    List_1<StringW>* showedMissionHelpIds,
    List_1<StringW>* guestPlayerNames,
    ColorSchemesSettings* colorSchemesSettings,
    OverrideEnvironmentSettings* overrideEnvironmentSettings,
    List_1<StringW>* favoritesLevelIds,
    MultiplayerModeSettings* multiplayerModeSettings,
    int currentDlcPromoDisplayCount,
    StringW currentDlcPromoId,
    OculusStudios::Platform::Core::UserAgeCategory userAgeCategory,
    PlayerSensitivityFlag desiredSensitivityFlag,
    List_1<System::ValueTuple_2<StringW, int>>* promoCounters
) {
    if (!lightsSet) {
        playerSpecificSettings->____environmentEffectsFilterDefaultPreset = EnvironmentEffectsFilterPreset::AllEffects;
        playerSpecificSettings->____environmentEffectsFilterExpertPlusPreset = EnvironmentEffectsFilterPreset::AllEffects;
        lightsSet = true;
    }

    PlayerData_ctor(
        self,
        playerId,
        playerName,
        shouldShowTutorialPrompt,
        shouldShow360Warning,
        agreedToEula,
        didSelectLanguage,
        agreedToMultiplayerDisclaimer,
        didSelectRegionVersion,
        selectedAvatarSystemTypeId,
        playerAgreements,
        lastSelectedBeatmapDifficulty,
        lastSelectedBeatmapCharacteristic,
        gameplayModifiers,
        playerSpecificSettings,
        practiceSettings,
        playerAllOverallStatsData,
        levelsStatsData,
        missionsStatsData,
        showedMissionHelpIds,
        guestPlayerNames,
        colorSchemesSettings,
        overrideEnvironmentSettings,
        favoritesLevelIds,
        multiplayerModeSettings,
        currentDlcPromoDisplayCount,
        currentDlcPromoId,
        userAgeCategory,
        desiredSensitivityFlag,
        promoCounters
    );
}

MAKE_AUTO_HOOK_MATCH(
    PlayerDataFileModel_LoadFromCurrentVersion,
    &PlayerDataFileModel::LoadFromCurrentVersion,
    PlayerData*,
    PlayerDataFileModel* self,
    PlayerSaveData* playerSaveData
) {
    auto loadedData = PlayerDataFileModel_LoadFromCurrentVersion(self, playerSaveData);
    if (!loadedData)
        return loadedData;

    auto player = playerSaveData->localPlayers->get_Item(0);

    // fix override environment settings
    bool override = player->overrideEnvironmentSettings->overrideEnvironments;
    auto override360Env = self->GetEnvironmentInfoBySerializedName(player->overrideEnvironmentSettings->override360EnvironmentName);
    auto overrideNormalEnv = self->GetEnvironmentInfoBySerializedName(player->overrideEnvironmentSettings->overrideNormalEnvironmentName);

    if (!override360Env || override360Env->_environmentType != EnvironmentType::Circle)
        override360Env = self->____environmentsListModel->GetFirstEnvironmentInfoWithType(EnvironmentType::Circle);
    if (!overrideNormalEnv || overrideNormalEnv->_environmentType != EnvironmentType::Normal)
        overrideNormalEnv = self->____environmentsListModel->GetFirstEnvironmentInfoWithType(EnvironmentType::Normal);

    loadedData->overrideEnvironmentSettings->overrideEnvironments = override;
    loadedData->overrideEnvironmentSettings->SetEnvironmentInfoForType(EnvironmentType::Circle, override360Env);
    loadedData->overrideEnvironmentSettings->SetEnvironmentInfoForType(EnvironmentType::Normal, overrideNormalEnv);

    return loadedData;
}

static void CopyFolder(std::string backupFolder, std::string destFolder) {
    if (std::filesystem::exists(backupFolder)) {
        for (auto const& file : std::filesystem::directory_iterator(backupFolder)) {
            if (!std::filesystem::is_directory(file)) {
                auto filename = file.path().filename().string();
                // A process interrupted before rename may leave our private
                // staging file. It must never become restored game data.
                if (filename.find(".datakeeper-") != std::string::npos && filename.ends_with(".tmp"))
                    continue;
                logger.info("Loading backup: {}", file.path().string());
                CopyContent(file.path(), destFolder / file.path().filename());
            }
        }
    } else
        std::filesystem::create_directories(backupFolder);
}

extern "C" __attribute__((visibility("default"))) void setup(CModInfo* info) {
    *info = modInfo.to_c();
    getConfig().Init(modInfo);

    try {
        std::filesystem::create_directories(modloader::get_external_dir());
        filesPath = std::filesystem::canonical(modloader::get_external_dir());
        localFilesPath = std::filesystem::path(filesPath).parent_path() / LOCAL_FILES_DIR;
        std::filesystem::create_directories(localFilesPath);
        CopyFolder(GetBackupPath(), filesPath);
        CopyFolder(GetLocalsBackupPath(), localFilesPath);
    } catch (const std::exception& error) {
        logger.error("DataKeeper could not restore backups: {}; existing game data retained", error.what());
    }

    lightsSet = getConfig().lightsSet.GetValue();
    if (!lightsSet)
        getConfig().lightsSet.SetValue(true);

    logger.info("Completed setup!");
}

extern "C" __attribute__((visibility("default"))) void late_load() {
    Hooks::Install();
}
