#include "main.hpp"

#include "System/IO/File.hpp"

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

static ModInfo modInfo;
static bool lightsSet = false;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

Configuration& getConfig() {
    static Configuration config = Configuration(modInfo);
    return config;
}

std::string GetBackupPath() {
    static std::string path = getDataDir(modInfo);
    return path;
}

const auto copyopt = std::filesystem::copy_options::overwrite_existing;

MAKE_HOOK_MATCH(File_WriteAllText, static_cast<void(*)(StringW, StringW)>(&File::WriteAllText), void, StringW path, StringW contents) {

    File_WriteAllText(path, contents);

    getLogger().info("File saved, path: %s", static_cast<std::string>(path).c_str());

    if(path.starts_with(DATA_PATH)) {
        getLogger().info("Copying for backup");
        std::string file = path;
        std::filesystem::copy(file, GetBackupPath() + file.substr(sizeof(DATA_PATH) - 1), copyopt);
    }
}

MAKE_HOOK_FIND(PlayerData_ctor, classof(PlayerData*), ".ctor", PlayerData*, StringW playerId, StringW playerName, bool shouldShowTutorialPrompt, bool shouldShow360Warning, bool agreedToEula, bool didSelectLanguage, bool agreedToMultiplayerDisclaimer, bool avatarCreated, int didSelectRegionVersion, PlayerAgreements* playerAgreements, BeatmapDifficulty lastSelectedBeatmapDifficulty, BeatmapCharacteristicSO* lastSelectedBeatmapCharacteristic, GameplayModifiers* gameplayModifiers, PlayerSpecificSettings* playerSpecificSettings, PracticeSettings* practiceSettings, PlayerAllOverallStatsData* playerAllOverallStatsData, List<PlayerLevelStatsData*>* levelsStatsData, List<PlayerMissionStatsData*>* missionsStatsData, List<StringW>* showedMissionHelpIds, List<StringW>* guestPlayerNames, ColorSchemesSettings* colorSchemesSettings, OverrideEnvironmentSettings* overrideEnvironmentSettings, List<StringW>* favoritesLevelIds, MultiplayerModeSettings* multiplayerModeSettings, int currentDlcPromoDisplayCount, StringW currentDlcPromoId) {

    if(!lightsSet) {
        playerSpecificSettings->environmentEffectsFilterDefaultPreset = EnvironmentEffectsFilterPreset::AllEffects;
        playerSpecificSettings->environmentEffectsFilterExpertPlusPreset = EnvironmentEffectsFilterPreset::AllEffects;
        lightsSet = true;
    }

    return PlayerData_ctor(playerId, playerName, shouldShowTutorialPrompt, shouldShow360Warning, agreedToEula, didSelectLanguage, agreedToMultiplayerDisclaimer, avatarCreated, didSelectRegionVersion, playerAgreements, lastSelectedBeatmapDifficulty, lastSelectedBeatmapCharacteristic, gameplayModifiers, playerSpecificSettings, practiceSettings, playerAllOverallStatsData, levelsStatsData, missionsStatsData, showedMissionHelpIds, guestPlayerNames, colorSchemesSettings, overrideEnvironmentSettings, favoritesLevelIds, multiplayerModeSettings, currentDlcPromoDisplayCount, currentDlcPromoId);
}

// Called at the early stages of game loading
extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;

    using namespace std;

    if(filesystem::exists(GetBackupPath())) {
        for(auto const& file : filesystem::directory_iterator(GetBackupPath())) {
            if(!filesystem::is_directory(file))
                filesystem::copy(file, DATA_PATH + file.path().filename().string(), copyopt);
        }
    } else
        filesystem::create_directory(GetBackupPath());
        
    getConfig().Load();
    lightsSet = getConfig().config.HasMember("lightsSet");
    if(!lightsSet) {
        getConfig().config.AddMember("lightsSet", true, getConfig().config.GetAllocator());
        getConfig().Write();
    }
	
    getLogger().info("Completed setup!");
}

// Called later on in the game loading
extern "C" void load() {
    il2cpp_functions::Init();

    getLogger().info("Installing hooks...");
    INSTALL_HOOK(getLogger(), File_WriteAllText);
    INSTALL_HOOK(getLogger(), PlayerData_ctor);
    getLogger().info("Installed all hooks!");
}