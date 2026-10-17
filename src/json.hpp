#pragma once
#include "cJSON.h"

namespace psgi {
// The pinned console allocator's mmap realloc fallback copies the old size even
// when shrinking. Buffered printing grows as needed and omits the final shrink.
// Keep this policy local: global cJSON allocator hooks would race worker parsing.
inline char* printJson(const cJSON* value, bool formatted = false) {
    return cJSON_PrintBuffered(value,256,formatted);
}
}
