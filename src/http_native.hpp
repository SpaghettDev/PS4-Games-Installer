#pragma once
#include <cstdint>

// These pinned SDK declarations omit their parameters. Bind the verified ABI
// directly to the same imports used by sceHttpInit/sceSslInit; loading another
// SPRX just to resolve them can select a separate, uninitialized module instance.
extern "C" {
int psgiHttpSetAutoRedirect(int id, int enabled) __asm__("sceHttpSetAutoRedirect");
int psgiHttpSetRecvTimeOut(int id, uint32_t microseconds) __asm__("sceHttpSetRecvTimeOut");
int psgiSslTerm(int id) __asm__("sceSslTerm");
}
