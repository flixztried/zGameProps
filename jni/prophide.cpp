#include "prophide.h"
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <android/log.h>

#define LOG_TAG "zGameProps"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
constexpr const char *kPropFile = "/dev/__properties__/properties_serial";

struct cow_prop_info {
    uint32_t serial;
    char value[92];
    char name[0];
};

void* find_prop_arena_base() {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return nullptr;

    char line[512];
    void* base = nullptr;

    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, kPropFile)) continue;

        unsigned long start = 0;
        if (sscanf(line, "%lx-", &start) == 1) {
            base = reinterpret_cast<void*>(start);
            break;
        }
    }
    fclose(fp);
    return base;
}

bool remap_private(void** addr_out, size_t* size_out) {
    void* arena = find_prop_arena_base();
    if (!arena) { LOGE("COW: Failed to find property arena in /proc/self/maps"); return false; }

    int fd = open(kPropFile, O_RDONLY | O_CLOEXEC);
    if (fd < 0) { LOGE("COW: Failed to open properties_serial"); return false; }

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        LOGE("COW: fstat failed or file empty");
        return false;
    }

    void* m = mmap(arena, (size_t)st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, 0);
    close(fd);

    if (m == MAP_FAILED) { LOGE("COW: mmap failed"); return false; }

    *addr_out = m;
    *size_out = (size_t)st.st_size;
    return true;
}
} // namespace

bool cowApplyProperties(const std::unordered_map<std::string, std::string> &props) {
    if (props.empty()) return false;

    void* base = nullptr;
    size_t size = 0;
    if (!remap_private(&base, &size)) return false;

    LOGI("COW: Successfully remapped property arena (%zu bytes)", size);
    const char* arena = static_cast<const char*>(base);

    for (const auto &p : props) {
        const void* pi_raw = __system_property_find(p.first.c_str());
        if (!pi_raw) { LOGE("COW: Property %s not found", p.first.c_str()); continue; }

        const cow_prop_info* pi = static_cast<const cow_prop_info*>(pi_raw);

        if (reinterpret_cast<const char*>(pi) < arena || reinterpret_cast<const char*>(pi) + sizeof(cow_prop_info) > arena + size) {
            LOGE("COW: Property %s out of bounds", p.first.c_str());
            continue;
        }

        const char* pi_name = pi->value + sizeof(pi->value);
        if (strcmp(pi_name, p.first.c_str()) != 0) {
            LOGE("COW: Name mismatch for %s", p.first.c_str());
            continue;
        }

        strncpy(const_cast<char*>(pi->value), p.second.c_str(), sizeof(pi->value) - 1);
        const_cast<char*>(pi->value)[sizeof(pi->value) - 1] = '\0';
        LOGI("COW: Spoofed Native %s to %s", p.first.c_str(), p.second.c_str());
    }

    return true;
}
