#include "stealth.h"

#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <link.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mount.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "zGameProps"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

using set_size_fn    = void (*)(void*, size_t);
using remove_map_fn  = void (*)(void*);

set_size_fn   g_set_size   = nullptr;
remove_map_fn g_remove_map = nullptr;

bool resolve_linker() {
    if (g_set_size && g_remove_map) return true;

    void* linker = dlopen("linker64", RTLD_NOW | RTLD_NOLOAD);
    if (!linker) linker = dlopen("linker", RTLD_NOW | RTLD_NOLOAD);
    if (!linker) { LOGE("Could not dlopen linker"); return false; }

    g_set_size = (set_size_fn)dlsym(linker, "_ZN6soinfo7setSizeEm");
    if (!g_set_size)
        g_set_size = (set_size_fn)dlsym(linker, "_ZN6soinfo12set_size_implEm");
    
    if (!g_set_size) LOGE("Failed to resolve set_size");

    g_remove_map = (remove_map_fn)dlsym(linker, "_ZL15remove_link_mapP6soinfo");
    if (!g_remove_map)
        g_remove_map = (remove_map_fn)dlsym(linker, "_ZN6soinfo15remove_link_mapEv");
    
    if (!g_remove_map) LOGE("Failed to resolve remove_link_map");

    dlclose(linker);
    return g_set_size && g_remove_map;
}

struct find_ctx {
    const char* target;
    void*       found;
};

int find_cb(struct dl_phdr_info* info, size_t, void* data) {
    auto* ctx = static_cast<find_ctx*>(data);
    if (info->dlpi_name && strstr(info->dlpi_name, ctx->target)) {
        ctx->found = (void*)info;
        return 1;
    }
    return 0;
}

} // namespace

bool stealthInit(uintptr_t* base_out, size_t* size_out) {
    *base_out = 0;
    *size_out = 0;

    if (!resolve_linker()) {
        LOGI("Stealth: soinfo hiding DISABLED (linker symbols missing)");
        return false;
    }

    find_ctx ctx{ "libzygisk.so", nullptr };
    dl_iterate_phdr(find_cb, &ctx);
    if (!ctx.found) {
        LOGE("Could not find libzygisk.so in dl_iterate_phdr");
        return false;
    }

    void* si = ctx.found;

    Dl_info dli{};
    if (dladdr((void*)&stealthInit, &dli))
        *base_out = (uintptr_t)dli.dli_fbase;

    g_set_size(si, 0);
    g_remove_map(si);
    LOGI("Stealth: soinfo hiding SUCCESS");
    return true;
}

void stealthHideMounts() {
    if (unshare(CLONE_NEWNS) == 0) {
        mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

        std::vector<std::string> targets;
        std::ifstream mi("/proc/self/mountinfo");
        std::string line;
        int unmounted = 0;
        while (std::getline(mi, line)) {
            std::vector<std::string> f;
            std::istringstream ss(line);
            std::string tok;
            while (ss >> tok) f.push_back(tok);
            if (f.size() < 7) continue;

            size_t dash = 0;
            for (size_t i = 0; i < f.size(); ++i)
                if (f[i] == "-") { dash = i; break; }
            if (dash == 0 || dash + 2 >= f.size()) continue;

            const std::string& mp = f[4];
            const std::string& src = f[dash + 2];
            if (mp.find("/data/adb") != std::string::npos ||
                src.find("magisk") != std::string::npos ||
                src.find("KSU") != std::string::npos ||
                src.find("APatch") != std::string::npos) {
                targets.push_back(mp);
            }
        }
        mi.close();

        std::sort(targets.begin(), targets.end(),
                  [](const std::string& a, const std::string& b) {
                      return a.size() > b.size();
                  });
        for (auto& t : targets) {
            if (umount2(t.c_str(), MNT_DETACH) == 0) unmounted++;
        }
        LOGI("Stealth: mount isolation SUCCESS (%d unmounted)", unmounted);
    } else {
        LOGE("Stealth: mount isolation FAILED (unshare error %d)", errno);
    }

    unsetenv("MAGISK_VER");
    unsetenv("MAGISK_VER_CODE");
    unsetenv("MAGISKTMP");
    unsetenv("KSU");
    unsetenv("KSU_VER");
    unsetenv("APATCH");
}
