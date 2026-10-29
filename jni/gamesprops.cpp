#include "gamesprops.h"

void applyBuildFields(JNIEnv *env,
                      const std::unordered_map<std::string, std::string> &props)
{
    if (props.empty()) return;

    jclass c = env->FindClass("android/os/Build");
    if (!c) { env->ExceptionClear(); return; }

    for (const auto &p : props) {
        jfieldID f = env->GetStaticFieldID(c, p.first.c_str(), "Ljava/lang/String;");
        if (!f) { env->ExceptionClear(); continue; }

        jstring v = env->NewStringUTF(p.second.c_str());
        if (!v) { env->ExceptionClear(); continue; }

        env->SetStaticObjectField(c, f, v);
        env->DeleteLocalRef(v);
    }

    env->DeleteLocalRef(c);
}
