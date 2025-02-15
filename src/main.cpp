#include "main.hpp"

#include <regex>
#include <string>

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
#include "config.h"
#include "scotland2/shared/modloader.h"

using namespace GlobalNamespace;
using namespace System::IO;
using namespace System::Collections::Generic;

static bool lightsSet = false;
static std::string filesPath;

std::string GetBackupPath() {
    return getConfig().backupPath.GetValue();
}

constexpr auto copyopt = std::filesystem::copy_options::overwrite_existing;

void HandleSave(std::string filePath) {
    auto fullPath = std::filesystem::canonical(filePath);
    logger.info("File saved, path: {} ({})", fullPath.string(), filePath);

    if (fullPath.parent_path() == filesPath && !std::filesystem::is_directory(fullPath) && !std::regex_search(filePath, BLACKLIST)) {
        logger.info("Copying for backup: {} ({})", fullPath.string(), filePath);
        std::filesystem::copy(fullPath, GetBackupPath() + fullPath.filename().string(), copyopt);
    }
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
    List_1<PlayerLevelStatsData*>* levelsStatsData,
    List_1<PlayerMissionStatsData*>* missionsStatsData,
    List_1<StringW>* showedMissionHelpIds,
    List_1<StringW>* guestPlayerNames,
    ColorSchemesSettings* colorSchemesSettings,
    OverrideEnvironmentSettings* overrideEnvironmentSettings,
    List_1<StringW>* favoritesLevelIds,
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

// Called at the early stages of game loading
PLAYERDATAKEEPER_EXPORT_FUNC void setup(CModInfo* info) {
    *info = modInfo.to_c();

    Paper::Logger::RegisterFileContextId(MOD_ID);

    getConfig().Init(modInfo);

    using namespace std;

    filesPath = filesystem::canonical(modloader::get_external_dir());
    filesystem::create_directories(filesPath);

    if (filesystem::exists(GetBackupPath())) {
        for (auto const& file : filesystem::directory_iterator(GetBackupPath())) {
            if (!filesystem::is_directory(file)) {
                logger.info("Loading backup: {}", file.path().string());
                filesystem::copy(file, filesPath / file.path().filename(), copyopt);
            }
        }
    } else
        filesystem::create_directory(GetBackupPath());

    lightsSet = getConfig().lightsSet.GetValue();
    if (!lightsSet) {
        getConfig().lightsSet.SetValue(true);
    }

    logger.info("Completed setup!");
}

PLAYERDATAKEEPER_EXPORT_FUNC void load() {
    logger.info("Installing hooks...");
    INSTALL_HOOK(logger, File_WriteAllText);
    INSTALL_HOOK(logger, File_Replace);
    INSTALL_HOOK(logger, PlayerData_ctor);
    logger.info("Installed all hooks!");
}
