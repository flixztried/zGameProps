#include <jni.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <android/log.h>

#include "zygisk.hpp"
#include "gamesprops.h"
#include "prophide.h"
#include "stealth.h"

#define LOG_TAG "zGameProps"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::Option;

namespace {

using BuildProps = std::unordered_map<std::string, std::string>;
using PackageMap = std::unordered_map<std::string, std::string>;

constexpr const char *kConfigPath = "/data/adb/modules/zgameprops/config.conf";

const std::unordered_map<std::string, std::string> kFieldToProp = {
    {"BRAND",        "ro.product.brand"},
    {"DEVICE",       "ro.product.device"},
    {"MANUFACTURER", "ro.product.manufacturer"},
    {"MODEL",        "ro.product.model"},
};

std::string trim(const std::string &s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string strip_sub(const std::string &n) {
    auto c = n.find(':');
    return c == std::string::npos ? n : n.substr(0, c);
}

} // namespace

class ZGameProps : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        api_ = api;
        env_ = env;
        LOGI("Module loading into Zygote");
        load_config();
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

        LOGI("Applying profile '%s' for %s", p->second.c_str(), pkg.c_str());

        applyBuildFields(env_, prof->second);

        std::unordered_map<std::string, std::string> prop_overrides;
        for (const auto &kv : prof->second) {
            auto it = kFieldToProp.find(kv.first);
            if (it != kFieldToProp.end())
                prop_overrides[it->second] = kv.second;
        }
        if (!prop_overrides.empty()) {
            if (cowApplyProperties(prop_overrides)) LOGI("COW properties applied");
            else LOGE("COW properties failed");
        }

        stealthHideMounts();

        unload();
    }

private:
    Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    std::unordered_map<std::string, BuildProps> profiles_;
    PackageMap package_to_profile_;

    void load_config() {
        std::ifstream f(kConfigPath);
        if (!f) { LOGE("Config file not found"); return; }

        std::string line, section;
        BuildProps props;
        std::vector<std::string> pkgs;

        auto commit = [&]() {
            if (section.empty()) return;
            profiles_[section] = props;
            for (auto &x : pkgs) package_to_profile_[x] = section;
            props.clear();
            pkgs.clear();
        };

        while (std::getline(f, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            if (line.front() == '[' && line.back() == ']') {
                commit();
                section = line.substr(1, line.size() - 2);
                if (section.rfind("profile.", 0) == 0)
                    section = section.substr(8);
                continue;
            }

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            auto k = trim(line.substr(0, eq));
            auto v = trim(line.substr(eq + 1));

            if (k == "packages") {
                std::stringstream ss(v);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    item = trim(item);
                    if (!item.empty()) pkgs.push_back(item);
                }
            } else if (k == "BRAND" || k == "DEVICE" ||
                       k == "MANUFACTURER" || k == "MODEL") {
                props[k] = v;
            }
        }
        commit();
    }

    void unload() {
        profiles_.clear();
        package_to_profile_.clear();
        api_->setOption(Option::DLCLOSE_MODULE_LIBRARY);
    }
};

REGISTER_ZYGISK_MODULE(ZGameProps)
