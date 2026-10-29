#include "prophide.h"
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/system_properties.h>
#include <android/log.h>

#define LOG_TAG "zGameProps"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
    struct cow_prop_info {
        uint32_t serial;
        char value[92];
        char name[0];
    };

    // Dynamically find which /dev/__properties__/ file contains the target pointer
    // and remap it as private/writable.
    bool make_writable(const void* target_ptr) {
        FILE* fp = fopen("/proc/self/maps", "r");
        if (!fp) return false;

        char line[512];
        bool success = false;

        while (fgets(line, sizeof(line), fp)) {
            unsigned long start, end, offset, inode;
            char perms[5], dev[12], path[256];
            path[0] = '\0';
            
            // Parse the standard /proc/self/maps format
            sscanf(line, "%lx-%lx %4s %lx %11s %lu %255s", &start, &end, perms, &offset, dev, &inode, path);

            // Check if our target pointer falls within this memory region
            if (target_ptr >= (void*)start && target_ptr < (void*)end) {
                if (strstr(path, "/dev/__properties__/") != NULL) {
                    int fd = open(path, O_RDONLY | O_CLOEXEC);
                    if (fd >= 0) {
                        // Remap exactly this chunk of memory privately
                        void* m = mmap((void*)start, end - start, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, offset);
                        close(fd);
                        success = (m != MAP_FAILED);
                    }
                }
                break; // Found the mapping containing the pointer, stop searching
            }
        }
        fclose(fp);
        return success;
    }
} // namespace

bool cowApplyProperties(const std::unordered_map<std::string, std::string> &props) {
    if (props.empty()) return false;

    for (const auto &p : props) {
        const void* pi_raw = __system_property_find(p.first.c_str());
        if (!pi_raw) { 
            LOGE("COW: Property %s not found", p.first.c_str()); 
            continue; 
        }

        if (!make_writable(pi_raw)) {
            LOGE("COW: Failed to remap memory for %s", p.first.c_str());
            continue;
        }

        const cow_prop_info* pi = static_cast<const cow_prop_info*>(pi_raw);
        
        // The name is stored immediately after the value in bionic's prop_info
        const char* pi_name = pi->value + sizeof(pi->value);
        if (strcmp(pi_name, p.first.c_str()) != 0) {
            LOGE("COW: Name mismatch for %s", p.first.c_str());
            continue;
        }

        // Overwrite the value
        strncpy(const_cast<char*>(pi->value), p.second.c_str(), sizeof(pi->value) - 1);
        const_cast<char*>(pi->value)[sizeof(pi->value) - 1] = '\0';
        LOGI("COW: Spoofed Native %s to %s", p.first.c_str(), p.second.c_str());
    }

    return true;
}
