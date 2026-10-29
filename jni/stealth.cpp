#include "stealth.h"

#include <cstring>
#include <cstdlib>
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
