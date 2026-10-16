#include "task_thread.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace { int creates = 0, joins = 0, destroys = 0; size_t stack = 0; bool fail = false; }
extern "C" {
int32_t scePthreadAttrInit(OrbisPthreadAttr*) { stack = 0; return 0; }
int32_t scePthreadAttrSetstacksize(OrbisPthreadAttr*, size_t bytes) { stack = bytes; return 0; }
int32_t scePthreadAttrDestroy(OrbisPthreadAttr*) { ++destroys; return 0; }
int32_t scePthreadCreate(OrbisPthread* out, const OrbisPthreadAttr*, void* (*start)(void*), void* arg, const char* name) {
    assert(stack == 512 * 1024 && std::strcmp(name, "psgi-worker") == 0); ++creates;
    if (fail) return -1;
    *out = {}; start(arg); return 0;
}
int32_t scePthreadJoin(OrbisPthread, void** result) { assert(!result); ++joins; return 0; }
}
int main() {
    int calls = 0;
    psgi::TaskThread thread([&] { ++calls; }); assert(thread.joinable() && calls == 1);
    psgi::TaskThread moved(std::move(thread)); assert(!thread.joinable() && moved.joinable()); moved.join();
    thread = psgi::TaskThread([&] { ++calls; }); thread.join();
    assert(calls == 2 && creates == 2 && joins == 2 && destroys == 2);
    fail = true; bool rejected = false;
    try { psgi::TaskThread failed([&] { ++calls; }); } catch (const std::runtime_error&) { rejected = true; }
    assert(rejected && calls == 2 && destroys == 3);
    std::puts("Native worker stack size, ownership transfer, joins and failed-start cleanup passed");
}
