#include "main.hpp"

#include "System/IO/File.hpp"
#include "System/Collections/Generic/List_1.hpp"

#include "GlobalNamespace/PlayerData.hpp"

#include "GlobalNamespace/PlayerAgreements.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/PlayerAllOverallStatsData.hpp"
#include "GlobalNamespace/PlayerLevelStatsData.hpp"
#include "GlobalNamespace/PlayerMissionStatsData.hpp"
#include "GlobalNamespace/ColorSchemesSettings.hpp"
#include "GlobalNamespace/OverrideEnvironmentSettings.hpp"
#include "GlobalNamespace/MultiplayerModeSettings.hpp"

#include <filesystem>

using namespace GlobalNamespace;
using namespace System::IO;
using namespace System::Collections::Generic;

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};
static bool lightsSet = false;
static std::string filesPath;
static std::string noBackupPath;

Configuration& getConfig() {
    static Configuration config = Configuration(modInfo);
    return config;
}

std::string GetBackupPath() {
    static std::string path = getDataDir(modInfo);
    return path;
}

const auto copyopt = std::filesystem::copy_options::overwrite_existing;

void HandleSave(std::string file_path) {
    Logger.info("File saved, path: {}", file_path.c_str());

    if (file_path.starts_with(filesPath) || file_path.starts_with(noBackupPath)) {
        for (const std::string& file : ALLOWED_FILES) {
            if(file_path.ends_with("/" + file)) {
                Logger.info("Copying for backup");
                std::filesystem::copy(file_path, GetBackupPath() + file, copyopt);
                break;
            }
        }
    }
}

/// @brief Creates the path if it needs to be and returns the canonical path.
/// @param path The input path.
/// @return The canonical version of this path.
std::string GetCanonical(std::string path) {
    using namespace std;

    filesystem::create_directory(path);

    return filesystem::canonical(path);
}

MAKE_HOOK_MATCH(File_WriteAllText, static_cast<void(*)(StringW, StringW)>(&File::WriteAllText), void, StringW path, StringW contents) {

    File_WriteAllText(path, contents);

    HandleSave(std::filesystem::canonical((std::string)path));
}

MAKE_HOOK_MATCH(File_Replace, static_cast<void(*)(StringW, StringW, StringW, bool)>(&File::Replace), void, StringW sourceFileName, StringW destinationFileName, StringW destinationBackupFileName, bool ignoreMetadataErrors) {

    File_Replace(sourceFileName, destinationFileName, destinationBackupFileName, ignoreMetadataErrors);

    HandleSave(std::filesystem::canonical((std::string)destinationFileName));
}

MAKE_HOOK_FIND_INSTANCE(PlayerData_ctor, classof(PlayerData*), ".ctor", void, PlayerData* self, StringW playerId, StringW playerName, bool shouldShowTutorialPrompt, bool shouldShow360Warning, bool agreedToEula, bool didSelectLanguage, bool agreedToMultiplayerDisclaimer, int32_t didSelectRegionVersion, StringW selectedAvatarSystemTypeId, PlayerAgreements* playerAgreements, BeatmapDifficulty lastSelectedBeatmapDifficulty, BeatmapCharacteristicSO* lastSelectedBeatmapCharacteristic, GameplayModifiers* gameplayModifiers, PlayerSpecificSettings* playerSpecificSettings, PracticeSettings* practiceSettings, PlayerAllOverallStatsData* playerAllOverallStatsData, List_1<PlayerLevelStatsData*>* levelsStatsData, List_1<PlayerMissionStatsData*>* missionsStatsData, List_1<StringW>* showedMissionHelpIds, List_1<StringW>* guestPlayerNames, ColorSchemesSettings* colorSchemesSettings, OverrideEnvironmentSettings* overrideEnvironmentSettings, List_1<StringW>* favoritesLevelIds, MultiplayerModeSettings* multiplayerModeSettings, int32_t currentDlcPromoDisplayCount, StringW currentDlcPromoId, UserAgeCategory userAgeCategory, PlayerSensitivityFlag desiredSensitivityFlag) {

    if(!lightsSet) {
        playerSpecificSettings->__cordl_internal_set__environmentEffectsFilterDefaultPreset(EnvironmentEffectsFilterPreset::AllEffects);
        playerSpecificSettings->__cordl_internal_set__environmentEffectsFilterExpertPlusPreset(EnvironmentEffectsFilterPreset::AllEffects);
        lightsSet = true;
    }

    PlayerData_ctor(self, playerId, playerName, shouldShowTutorialPrompt, shouldShow360Warning, agreedToEula, didSelectLanguage, agreedToMultiplayerDisclaimer, didSelectRegionVersion, selectedAvatarSystemTypeId, playerAgreements, lastSelectedBeatmapDifficulty, lastSelectedBeatmapCharacteristic, gameplayModifiers, playerSpecificSettings, practiceSettings, playerAllOverallStatsData, levelsStatsData, missionsStatsData, showedMissionHelpIds, guestPlayerNames, colorSchemesSettings, overrideEnvironmentSettings, favoritesLevelIds, multiplayerModeSettings, currentDlcPromoDisplayCount, currentDlcPromoId, userAgeCategory, desiredSensitivityFlag);
}

// Called at the early stages of game loading
PLAYERDATAKEEPER_EXPORT_FUNC void setup(CModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;

    getConfig().Load();
    lightsSet = getConfig().config.HasMember("lightsSet");
    if(!lightsSet) {
        getConfig().config.AddMember("lightsSet", true, getConfig().config.GetAllocator());
        getConfig().Write();
    }

    Logger.info("Completed setup!");
}

// Called later on in the game loading
PLAYERDATAKEEPER_EXPORT_FUNC void late_load() {
    using namespace std;

    filesPath = GetCanonical(modloader::get_external_dir()) + "/";
    noBackupPath = GetCanonical(filesPath + "../no_backup") + "/";

    Logger.info("files path: {}", filesPath);
    Logger.info("no_backup path: {}", noBackupPath);

    if(filesystem::exists(GetBackupPath())) {

        for(auto const& file : filesystem::directory_iterator(GetBackupPath())) {
            auto path = file.path();
            Logger.info("Using backup {}", path.string().c_str());

            if(!filesystem::is_directory(file)) {
                if (path.filename().string() == "settings.cfg") {
                    filesystem::copy(file, noBackupPath + path.filename().string(), copyopt);
                } else {
                    filesystem::copy(file, filesPath + path.filename().string(), copyopt);
                }
            }
        }
    } else
        filesystem::create_directory(GetBackupPath());

    Logger.info("Installing hooks...");
    INSTALL_HOOK(Logger, File_WriteAllText);
    INSTALL_HOOK(Logger, File_Replace);
    INSTALL_HOOK(Logger, PlayerData_ctor);
    Logger.info("Installed all hooks!");
}
