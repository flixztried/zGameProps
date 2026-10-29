#pragma once

#include <jni.h>
#include <string>
#include <unordered_map>

void applyBuildFields(JNIEnv *env,
                      const std::unordered_map<std::string, std::string> &props);
