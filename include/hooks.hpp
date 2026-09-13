#pragma once

#include "main.hpp"
#include "beatsaber-hook/shared/hooking.hpp"

class Hooks {
   private:
    static inline std::vector<void (*)()> installFuncs;

   public:
    static inline void AddInstallFunc(void (*installFunc)()) { installFuncs.push_back(installFunc); }

    static inline void Install() {
        logger.info("Installing hooks...");
        for (auto& func : installFuncs)
            func();
        logger.info("Installed all hooks!");
    }
};

#define AUTO_HOOK_FUNCTION(name_)                                      \
    namespace {                                                       \
        template <class Hook>                                         \
        struct Auto_Install_##name_ {                                 \
            static void Install();                                    \
            Auto_Install_##name_() { ::Hooks::AddInstallFunc(Install); } \
        };                                                            \
    }                                                                 \
    static Auto_Install_##name_<hook_##name_> Auto_Install_Instance_##name_; \
    template <class Hook>                                             \
    void Auto_Install_##name_<Hook>::Install()

#define AUTO_INSTALL_ORIG(name_) \
    AUTO_HOOK_FUNCTION(name_) { using hook_Target = Hook; INSTALL_HOOK_ORIG(logger, Target); }

#define AUTO_INSTALL(name_) \
    AUTO_HOOK_FUNCTION(name_) { using hook_Target = Hook; INSTALL_HOOK(logger, Target); }

#define MAKE_AUTO_HOOK_MATCH(name_, mPtr, retval, ...) \
    struct hook_##name_;                             \
    AUTO_INSTALL(name_)                              \
    MAKE_HOOK_MATCH(name_, mPtr, retval, __VA_ARGS__)

#define MAKE_AUTO_ORIG_HOOK_MATCH(name_, mPtr, retval, ...) \
    struct hook_##name_;                                  \
    AUTO_INSTALL_ORIG(name_)                              \
    MAKE_HOOK_MATCH(name_, mPtr, retval, __VA_ARGS__)

void Camera_Pause();
void Camera_Unpause();

namespace UnityEngine {
    class Camera;
}
extern UnityEngine::Camera* mainCamera;
