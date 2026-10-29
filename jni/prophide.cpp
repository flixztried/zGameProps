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
    if (!arena) return false;

    int fd = open(kPropFile, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        return false;
    }

    // MAP_FIXED over the existing read-only mapping. After this, the
    // process has a private writable copy of the entire property arena.
    void* m = mmap(arena, (size_t)st.st_size,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_FIXED,
                   fd, 0);
    close(fd);

    if (m == MAP_FAILED) return false;

    *addr_out = m;
    *size_out = (size_t)st.st_size;
    return true;
}

} // namespace

bool cowApplyProperties(
    const std::unordered_map<std::string, std::string> &props)
{
    if (props.empty()) return false;

    void* base = nullptr;
    size_t size = 0;
    if (!remap_private(&base, &size)) return false;

    const char* arena = static_cast<const char*>(base);

    for (const auto &p : props) {
        const void* pi_raw = __system_property_find(p.first.c_str());
        if (!pi_raw) continue;

        const cow_prop_info* pi = static_cast<const cow_prop_info*>(pi_raw);

        if (reinterpret_cast<const char*>(pi) < arena ||
            reinterpret_cast<const char*>(pi) + sizeof(cow_prop_info) > arena + size) {
            continue;
        }

        // prop_info is followed by its name. Verify the name matches
        // before writing so we never corrupt an adjacent entry.
        const char* pi_name = pi->value + sizeof(pi->value);
        if (strcmp(pi_name, p.first.c_str()) != 0) continue;

        // Overwrite the value in the private COW mapping.
        strncpy(const_cast<char*>(pi->value), p.second.c_str(),
                sizeof(pi->value) - 1);
        const_cast<char*>(pi->value)[sizeof(pi->value) - 1] = '\0';
    }

    return true;
}
