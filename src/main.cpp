#include "main.hpp"

#include "System/IO/File.hpp"

#include <filesystem>

using namespace System::IO;

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

MAKE_HOOK_MATCH(File_WriteAllText, static_cast<void(*)(StringW, StringW)>(&File::WriteAllText), void, StringW path, StringW contents) {

    File_WriteAllText(path, contents);

    getLogger().info("File saved, path: %s", static_cast<std::string>(path).c_str());

    if(path.starts_with(DATA_PATH)) {
        getLogger().info("Copying for backup");
        std::string file = path;
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
    } else
        filesystem::create_directory(GetBackupPath());
	
    getLogger().info("Completed setup!");
}

// Called later on in the game loading
extern "C" void load() {
    il2cpp_functions::Init();

    getLogger().info("Installing hooks...");
    INSTALL_HOOK(getLogger(), File_WriteAllText);
    getLogger().info("Installed all hooks!");
}