// Test the PS4 HTTP branch against the pinned SDK declarations and fake services.
#include "platform.hpp"
#include "http_native.hpp"
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
const std::string body = R"({"DATA":{"http://fixture/game.pkg":{"name":"A game"}}})";
std::string scenario;
bool initialized = false, tmpl = false, conn = false, req = false, policy = false, aborted = false;
int initializations = 0;
size_t offset = 0;
std::atomic<bool>* cancellation = nullptr;
void reset(const char* mode) { scenario = mode; offset = 0; policy = aborted = false; }
template<class F> void rejects(F fn, const char* text) {
    bool rejected = false;
    try { fn(); } catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(text) != std::string::npos); rejected = true;
    }
    assert(rejected && !tmpl && !conn && !req);
}
}

extern "C" {
uint32_t sceSysmoduleLoadModuleInternal(OrbisSysModuleInternal) { return 0; }
int32_t sceNetInit() { return 0; }
int32_t sceNetCtlInit() { return 0; }
int32_t sceNetPoolCreate(const char*, int32_t, int32_t) { return 1; }
void sceNetPoolDestroy(int32_t id) { assert(id == 1); }
int32_t sceSslInit(size_t) { return 2; }
int psgiSslTerm(int id) { assert(id == 2); return 0; }
int32_t sceHttpInit(int32_t pool, int32_t ssl, size_t) {
    assert(pool == 1 && ssl == 2); initialized = true; ++initializations; return 3;
}
int32_t sceHttpTerm(int32_t id) { assert(id == 3 && !tmpl && !conn && !req); initialized = false; return 0; }
int32_t sceHttpCreateTemplate(int32_t id, const char*, int32_t, int32_t) {
    assert(id == 3 && initialized && !tmpl); tmpl = true; return 4;
}
int psgiHttpSetAutoRedirect(int id, int enabled) {
    assert(initialized && tmpl && id == 4 && enabled == 0);
    if (scenario == "policy-error") return static_cast<int32_t>(0x80431001u);
    policy = true; return 0;
}
int psgiHttpSetRecvTimeOut(int id, uint32_t value) { assert(policy && id == 4 && value == 10000000); return 0; }
int32_t sceHttpSetConnectTimeOut(int32_t id, uint32_t) { assert(id == 4); return 0; }
int32_t sceHttpSetResolveTimeOut(int32_t id, uint32_t) { assert(id == 4); return 0; }
int32_t sceHttpSetSendTimeOut(int32_t id, uint32_t) { assert(id == 4); return 0; }
int32_t sceHttpCreateConnectionWithURL(int32_t id, const char*, bool) {
    assert(initialized && policy && id == 4); conn = true; return 5;
}
int32_t sceHttpCreateRequestWithURL(int32_t id, int32_t, const char*, uint64_t) {
    assert(conn && id == 5); req = true; return 6;
}
int32_t sceHttpSendRequest(int32_t id, const void*, size_t) { assert(req && id == 6); return 0; }
int32_t sceHttpGetStatusCode(int32_t id, int32_t* status) { assert(id == 6); *status = scenario == "redirect" ? 302 : 200; return 0; }
int32_t sceHttpReadData(int32_t id, void* data, uint32_t capacity) {
    assert(req && id == 6);
    size_t count = std::min({size_t(capacity), size_t(7), body.size() - offset});
    std::memcpy(data, body.data() + offset, count); offset += count;
    if (scenario == "cancel") *cancellation = true;
    return count;
}
int32_t sceHttpAbortRequest(int32_t id) { assert(id == 6); aborted = true; return 0; }
int32_t sceHttpDeleteRequest(int32_t id) { assert(id == 6 && req); req = false; return 0; }
int32_t sceHttpDeleteConnection(int32_t id) { assert(id == 5 && conn && !req); conn = false; return 0; }
int32_t sceHttpDeleteTemplate(int32_t id) { assert(id == 4 && tmpl && !conn); tmpl = false; return 0; }
uint64_t sceKernelGetProcessTime() { static uint64_t time = 0; return ++time; }
// A secondary HTTP/SSL SPRX can have separate initialization state. Never load it.
uint32_t sceKernelLoadStartModule(const char*, size_t, const void*, uint32_t, void*, void*) {
    throw std::runtime_error("Unexpected duplicate service module load");
}
}

int main() {
    const char* url = "http://fixture/games";
    std::atomic<bool> cancelled{true}; cancellation = &cancelled;
    rejects([&] { psgi::httpGet(url, 1024, &cancelled); }, "cancelled");
    assert(initializations == 0);
    reset("success"); assert(psgi::httpGet(url) == body);
    assert(initializations == 1 && !tmpl && !conn && !req);
    reset("redirect"); rejects([&] { psgi::httpGet(url); }, "HTTP 302");
    reset("large"); rejects([&] { psgi::httpGet(url, 4); }, "exceeds limit");
    reset("cancel"); cancelled = false;
    rejects([&] { psgi::httpGet(url, 1024, &cancelled); }, "cancelled"); assert(aborted);
    reset("policy-error"); rejects([&] { psgi::httpGet(url); }, "Disable redirects (0x80431001)");
    reset("success"); assert(psgi::httpGet(url) == body);
    assert(initializations == 1 && !tmpl && !conn && !req);
    std::puts("Native HTTP imports, initialization, policies, limits, cancellation and cleanup passed");
}
