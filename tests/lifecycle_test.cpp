// Exercise the native lifecycle implementation with the pinned SDK/libjbc
// declarations and fake service calls. No PS4 API is invoked on the host.
#include "platform.hpp"
#include <libjbc.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string scenario;
std::vector<std::string> calls;
const jbc_cred saved{1000, 1001, 1002, 1003, 1004, 11, 12, 13, 14, 15, 16, 17};
jbc_cred elevated;
int applications = 0;
struct WaitingForShell {};
}

extern "C" {
int jbc_get_cred(jbc_cred* cred) {
    calls.push_back("read");
    *cred = saved;
    return scenario == "read-failure" ? -1 : 0;
}
int jbc_jailbreak_cred(jbc_cred* cred) {
    calls.push_back("elevate");
    assert(std::memcmp(cred, &saved, sizeof(saved)) == 0);
    // Change both privileges and sandbox paths to catch restoring only a subset.
    cred->uid = cred->ruid = cred->svuid = cred->rgid = cred->svgid = 0;
    cred->prison = cred->cdir = cred->rdir = cred->jdir = 42;
    cred->sceProcType = cred->sonyCred = cred->sceProcCap = 43;
    elevated = *cred;
    return scenario == "prepare-failure" ? -1 : 0;
}
int jbc_set_cred(const jbc_cred* cred) {
    const bool apply = applications++ == 0;
    calls.push_back(apply ? "apply" : "restore");
    assert(std::memcmp(cred, apply ? &elevated : &saved, sizeof(saved)) == 0);
    return (apply && scenario == "apply-failure") ||
           (!apply && scenario == "restore-failure") ? -1 : 0;
}
uint32_t sceSysmoduleLoadModuleInternal(OrbisSysModuleInternal module) {
    assert(module == ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    calls.push_back("module");
    return 0;
}
int32_t sceSystemServiceLoadExec(const char* path, const char* args[]) {
    assert(std::strcmp(path, "exit") == 0 && args == nullptr);
    calls.push_back("exit");
    return scenario == "exit-failure" ? -1 : 0;
}
int32_t sceKernelUsleep(uint32_t microseconds) {
    assert(microseconds > 0);
    calls.push_back("wait");
    // Escape the production wait after checking it did not return to libc.
    throw WaitingForShell{};
}
}

int main(int argc, char** argv) {
    assert(argc == 2);
    scenario = argv[1];
    const bool startupFails = scenario == "read-failure" || scenario == "prepare-failure" || scenario == "apply-failure";
    bool failed = false;
    try { psgi::prepareConsole(); }
    catch (const std::runtime_error&) { failed = true; }
    assert(failed == startupFails);
    try {
        psgi::finishApplication(startupFails || scenario == "exit-failure" ? 1 : 0);
        assert(false && "Native exit returned to the C runtime");
    } catch (const WaitingForShell&) {}

    std::vector<std::string> expected{"read"};
    if (scenario != "read-failure") expected.push_back("elevate");
    if (scenario != "read-failure" && scenario != "prepare-failure") expected.push_back("apply");
    expected.push_back("module");
    if (scenario != "read-failure" && scenario != "prepare-failure") expected.push_back("restore");
    expected.push_back("exit");
    expected.push_back("wait");
    assert(calls == expected);
    std::printf("Native lifecycle passed: %s\n", scenario.c_str());
}
