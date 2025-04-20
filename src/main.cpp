#include "main.hpp"

#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/ColorSchemesSettings.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/MultiplayerModeSettings.hpp"
#include "GlobalNamespace/OverrideEnvironmentSettings.hpp"
#include "GlobalNamespace/PlayerAgreements.hpp"
#include "GlobalNamespace/PlayerAllOverallStatsData.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/PlayerLevelStatsData.hpp"
#include "GlobalNamespace/PlayerMissionStatsData.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "System/Collections/Generic/List_1.hpp"
#include "System/IO/File.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "config.hpp"
#include "scotland2/shared/modloader.h"

using namespace GlobalNamespace;
using namespace System::IO;

static bool lightsSet = false;
static std::string filesPath;
static std::string localFilesPath;

static constexpr auto copyopt = std::filesystem::copy_options::overwrite_existing;

static inline std::string GetBackupPath() {
    return getConfig().backupPath.GetValue();
}

static inline std::string GetLocalsBackupPath() {
    return std::filesystem::path(getConfig().backupPath.GetValue()) / LOCAL_FILES_DIR;
}

static void HandleSave(std::string filePath, std::string checkPath, std::string backupPath) {
    auto fullPath = std::filesystem::canonical(filePath);
    logger.info("File saved, path: {} ({})", fullPath.string(), filePath);

    if (fullPath.parent_path() == checkPath && !std::filesystem::is_directory(fullPath) && !std::regex_search(filePath, BLACKLIST)) {
        logger.info("Copying for backup: {} ({})", fullPath.string(), filePath);
        std::filesystem::copy(fullPath, backupPath / fullPath.filename(), copyopt);
    }
}

static void HandleSave(std::string filePath) {
    HandleSave(filePath, filesPath, GetBackupPath());
    HandleSave(filePath, localFilesPath, GetLocalsBackupPath());
}

MAKE_HOOK_MATCH(File_WriteAllText, static_cast<void (*)(StringW, StringW)>(&File::WriteAllText), void, StringW path, StringW contents) {

    File_WriteAllText(path, contents);

    HandleSave(path);
}

MAKE_HOOK_MATCH(
    File_Replace,
    static_cast<void (*)(StringW, StringW, StringW, bool)>(&File::Replace),
    void,
    StringW sourceFileName,
    StringW destinationFileName,
    StringW destinationBackupFileName,
    bool ignoreMetadataErrors
) {
    File_Replace(sourceFileName, destinationFileName, destinationBackupFileName, ignoreMetadataErrors);

    HandleSave(destinationFileName);
}

MAKE_HOOK_FIND_INSTANCE(
    PlayerData_ctor,
    classof(PlayerData*),
    ".ctor",
    void,
    PlayerData* self,
    StringW playerId,
    StringW playerName,
    bool shouldShowTutorialPrompt,
    bool shouldShow360Warning,
    bool agreedToEula,
    bool didSelectLanguage,
    bool agreedToMultiplayerDisclaimer,
    int32_t didSelectRegionVersion,
    StringW selectedAvatarSystemTypeId,
    PlayerAgreements* playerAgreements,
    BeatmapDifficulty lastSelectedBeatmapDifficulty,
    BeatmapCharacteristicSO* lastSelectedBeatmapCharacteristic,
    GameplayModifiers* gameplayModifiers,
    PlayerSpecificSettings* playerSpecificSettings,
    PracticeSettings* practiceSettings,
    PlayerAllOverallStatsData* playerAllOverallStatsData,
    List<PlayerLevelStatsData*>* levelsStatsData,
    List<PlayerMissionStatsData*>* missionsStatsData,
    List<StringW>* showedMissionHelpIds,
    List<StringW>* guestPlayerNames,
    ColorSchemesSettings* colorSchemesSettings,
    OverrideEnvironmentSettings* overrideEnvironmentSettings,
    List<StringW>* favoritesLevelIds,
    MultiplayerModeSettings* multiplayerModeSettings,
    int32_t currentDlcPromoDisplayCount,
    StringW currentDlcPromoId,
    UserAgeCategory userAgeCategory,
    PlayerSensitivityFlag desiredSensitivityFlag
) {
    if (!lightsSet) {
        playerSpecificSettings->_environmentEffectsFilterDefaultPreset = EnvironmentEffectsFilterPreset::AllEffects;
        playerSpecificSettings->_environmentEffectsFilterExpertPlusPreset = EnvironmentEffectsFilterPreset::AllEffects;
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
        desiredSensitivityFlag
    );
}

static void CopyFolder(std::string backupFolder, std::string destFolder) {
    if (std::filesystem::exists(backupFolder)) {
        for (auto const& file : std::filesystem::directory_iterator(backupFolder)) {
            if (!std::filesystem::is_directory(file)) {
                logger.info("Loading backup: {}", file.path().string());
                std::filesystem::copy(file, destFolder / file.path().filename(), copyopt);
            }
        }
    } else
        std::filesystem::create_directories(backupFolder);
}

PLAYERDATAKEEPER_EXPORT_FUNC void setup(CModInfo* info) {
    *info = modInfo.to_c();
    Paper::Logger::RegisterFileContextId(MOD_ID);
    getConfig().Init(modInfo);

    filesPath = std::filesystem::canonical(modloader::get_external_dir());
    localFilesPath = std::filesystem::path(filesPath).parent_path() / LOCAL_FILES_DIR;
    std::filesystem::create_directories(filesPath);
    std::filesystem::create_directories(localFilesPath);

    CopyFolder(GetBackupPath(), filesPath);
    CopyFolder(GetLocalsBackupPath(), localFilesPath);

    lightsSet = getConfig().lightsSet.GetValue();
    if (!lightsSet)
        getConfig().lightsSet.SetValue(true);

    logger.info("Completed setup!");
}

PLAYERDATAKEEPER_EXPORT_FUNC void load() {
    logger.info("Installing hooks...");
    INSTALL_HOOK(logger, File_WriteAllText);
    INSTALL_HOOK(logger, File_Replace);
    INSTALL_HOOK(logger, PlayerData_ctor);
    logger.info("Installed all hooks!");
}
