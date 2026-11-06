#include <jni.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <android/log.h>

#include "zygisk.hpp"
#include "gamesprops.h"

#define LOG_TAG "zGameProps"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;
using zygisk::Option;

namespace {
using BuildProps = std::unordered_map<std::string, std::string>;
using PackageMap = std::unordered_map<std::string, std::string>;

constexpr const char *kConfigPath = "/data/adb/modules/zgameprops/config.conf";

std::string trim(const std::string &s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

std::string strip_sub(const std::string &n) {
    auto c = n.find(':');
    return c == std::string::npos ? n : n.substr(0, c);
}

void secure_zero(void* ptr, size_t len) {
    volatile char* p = (volatile char*)ptr;
    while (len--) *p++ = 0;
}

void scrub_map(std::unordered_map<std::string, std::string>& map) {
    for (auto& pair : map) {
        if (!pair.first.empty()) secure_zero(const_cast<char*>(pair.first.data()), pair.first.size());
        if (!pair.second.empty()) secure_zero(const_cast<char*>(pair.second.data()), pair.second.size());
    }
    map.clear();
}
} // namespace

class ZGameProps : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        api_ = api; env_ = env;
        LOGI(">>> zGameProps v2.0.0-hotfix loaded <<<");
        load_config();
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        LOGI(">>> Injected into system_server (Sanitizing environment) <<<");
        api_->setOption(Option::DLCLOSE_MODULE_LIBRARY);
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) { unload(); return; }
        const char *raw = env_->GetStringUTFChars(args->nice_name, nullptr);
        if (!raw) { env_->ExceptionClear(); unload(); return; }
        std::string pkg = strip_sub(raw);
        env_->ReleaseStringUTFChars(args->nice_name, raw);

        auto p = package_to_profile_.find(pkg);
        if (p == package_to_profile_.end()) { unload(); return; }
        auto prof = profiles_.find(p->second);
        if (prof == profiles_.end()) { unload(); return; }

        LOGI(">>> Matched target: %s (Profile: %s) <<<", pkg.c_str(), p->second.c_str());

        // PURE JAVA JNI SPOOF - No Native Hooks, No COW, No Mounts
        applyBuildFields(env_, prof->second);
        LOGI("Spoof: Java Build fields applied. Unloading process cleanly.");

        unload();
    }

private:
    Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    std::unordered_map<std::string, BuildProps> profiles_;
    PackageMap package_to_profile_;

    void load_config() {
        std::ifstream f(kConfigPath);
        if (!f) { LOGE("Config not found"); return; }
        std::string line, section;
        BuildProps props; std::vector<std::string> pkgs;
        auto commit = [&]() {
            if (section.empty()) return;
            profiles_[section] = props;
            for (auto &x : pkgs) package_to_profile_[x] = section;
            props.clear(); pkgs.clear();
        };
        while (std::getline(f, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']') {
                commit(); section = line.substr(1, line.size() - 2);
                if (section.rfind("profile.", 0) == 0) section = section.substr(8);
                continue;
            }
            auto eq = line.find('='); if (eq == std::string::npos) continue;
            auto k = trim(line.substr(0, eq)); auto v = trim(line.substr(eq + 1));
            if (k == "packages") {
                std::stringstream ss(v); std::string item;
                while (std::getline(ss, item, ',')) {
                    item = trim(item); if (!item.empty()) pkgs.push_back(item);
                }
            } else if (k == "BRAND" || k == "DEVICE" || k == "MANUFACTURER" || k == "MODEL") props[k] = v;
        }
        commit();
        LOGI("Loaded %zu profiles, %zu packages", profiles_.size(), package_to_profile_.size());
    }

    void unload() {
        for (auto& pair : profiles_) scrub_map(pair.second);
        scrub_map(package_to_profile_);
        api_->setOption(Option::DLCLOSE_MODULE_LIBRARY);
    }
};
REGISTER_ZYGISK_MODULE(ZGameProps)
