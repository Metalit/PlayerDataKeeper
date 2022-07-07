#include "main.hpp"

#include "GlobalNamespace/FileHelpers.hpp"

#include <filesystem>

using namespace GlobalNamespace;

static ModInfo modInfo;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

std::string GetBackupPath() {
    static std::string path = getDataDir(modInfo);
    return path;
}

const auto copyopt = std::filesystem::copy_options::overwrite_existing;

MAKE_HOOK_MATCH(FileHelpers_SaveToJSONFile, static_cast<void(*)(Il2CppObject*, StringW, StringW, StringW)>(&FileHelpers::SaveToJSONFile),
        void, Il2CppObject* obj, StringW filePath, StringW tempFilePath, StringW backupFilePath) {

    FileHelpers_SaveToJSONFile(obj, filePath, tempFilePath, backupFilePath);

    getLogger().info("File saved, path: %s", filePath.operator std::string().c_str());

    if(filePath.starts_with(DATA_PATH)) {
        getLogger().info("Copying for backup");
        std::string file = filePath;
        std::filesystem::copy(file, GetBackupPath() + file.substr(sizeof(DATA_PATH) - 1), copyopt);
    }
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
    } else {
        filesystem::create_directory(GetBackupPath());
        for(auto const& file : filesystem::directory_iterator(DATA_PATH)) {
            if(!filesystem::is_directory(file))
                filesystem::copy(file, GetBackupPath() + file.path().filename().string(), copyopt);
        }
    }
	
    getLogger().info("Completed setup!");
}

// Called later on in the game loading
extern "C" void load() {
    il2cpp_functions::Init();

    getLogger().info("Installing hooks...");
    INSTALL_HOOK(getLogger(), FileHelpers_SaveToJSONFile);
    getLogger().info("Installed all hooks!");
}