#pragma once

// Copied this here because one of the static_asserts was falsely claiming to fail.
#define MAKE_HOOK_MATCH_PDK(name_, mPtr, retval, ...) \
struct Hook_##name_ { \
    using funcType = retval (*)(__VA_ARGS__); \
    static_assert(std::is_same_v<funcType, ::Hooking::InternalMethodCheck<decltype(mPtr)>::funcType>, "Hook method signature does not match!"); \
    constexpr static const char* name() { return #name_; } \
    static const MethodInfo* getInfo() { return ::il2cpp_utils::il2cpp_type_check::MetadataGetter<mPtr>::methodInfo(); } \
    static funcType* trampoline() { return &name_; } \
    static inline retval (*name_)(__VA_ARGS__) = nullptr; \
    static funcType hook() { return &::Hooking::HookCatchWrapper<&hook_##name_, funcType>::wrapper; } \
    static retval hook_##name_(__VA_ARGS__); \
}; \
retval Hook_##name_::hook_##name_(__VA_ARGS__)
