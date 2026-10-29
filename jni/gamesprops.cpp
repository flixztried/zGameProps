#include "gamesprops.h"
#include <android/log.h>

#define LOG_TAG "zGameProps"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void applyBuildFields(JNIEnv *env,
                      const std::unordered_map<std::string, std::string> &props)
{
    if (props.empty()) return;

    jclass c = env->FindClass("android/os/Build");
    if (!c) { env->ExceptionClear(); LOGE("Failed to find android.os.Build class"); return; }

    for (const auto &p : props) {
        jfieldID f = env->GetStaticFieldID(c, p.first.c_str(), "Ljava/lang/String;");
        if (!f) { env->ExceptionClear(); LOGE("Failed to find field %s", p.first.c_str()); continue; }

        jstring v = env->NewStringUTF(p.second.c_str());
        if (!v) { env->ExceptionClear(); LOGE("Failed to create string for %s", p.first.c_str()); continue; }

        env->SetStaticObjectField(c, f, v);
        env->DeleteLocalRef(v);
        LOGI("Spoofed Java Build.%s to %s", p.first.c_str(), p.second.c_str());
    }

    env->DeleteLocalRef(c);
}
