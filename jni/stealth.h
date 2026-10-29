#pragma once

#include <cstdint>
#include <cstddef>

bool stealthInit(uintptr_t* base_out, size_t* size_out);
void stealthHideMounts();
