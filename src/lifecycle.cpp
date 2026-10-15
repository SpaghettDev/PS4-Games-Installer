#include "platform.hpp"
#include <cstdio>
#include <stdexcept>
#include <libjbc.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>

namespace {
jbc_cred originalCredentials{};
bool restorePending = false;

void check(int result, const char* action) {
    if (result == 0) return;
    char code[16];
    std::snprintf(code, sizeof(code), "0x%08X", static_cast<unsigned>(result));
    throw std::runtime_error(std::string(action) + ": " + code);
}
}

namespace psgi {
void prepareConsole() {
    check(jbc_get_cred(&originalCredentials), "Read homebrew credentials");
    auto elevated = originalCredentials;
    check(jbc_jailbreak_cred(&elevated), "Prepare homebrew credentials");
    // jbc_set_cred can fail after changing part of the process credentials.
    restorePending = true;
    check(jbc_set_cred(&elevated), "Apply homebrew credentials");
}

int finishApplication(int status) {
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    if (restorePending) {
        const int result = jbc_set_cred(&originalCredentials);
        if (result != 0)
            std::fprintf(stderr, "Restore homebrew credentials: 0x%08X\n", static_cast<unsigned>(result));
        else restorePending = false;
    }
    // Returning through the C runtime's _exit is not the PS4 shell exit path.
    // This is also used when Circle dismisses a startup/runtime error screen.
    const int result = sceSystemServiceLoadExec("exit", nullptr);
    if (result < 0)
        std::fprintf(stderr, "PS4 shell exit failed: 0x%08X (application status %d)\n",
                     static_cast<unsigned>(result), status);
    // Allow an accepted asynchronous request to complete. If it failed, leave
    // termination to Close Application rather than falling into libc _exit.
    for (;;) sceKernelUsleep(1000000);
}
}
