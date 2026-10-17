#include "platform.hpp"
#include <cstdio>
#ifndef PSGI_HOST
#include <orbis/Sysmodule.h>
// The pinned SDK declares this export without arguments. Keep its known ABI
// in a separate translation unit rather than using that placeholder prototype.
extern "C" int32_t openBrowserService(const char*, void*) __asm__("sceSystemServiceLaunchWebBrowser");
#endif
namespace psgi {
void openMediaUrl(const std::string& url) {
    if(!isHttpUrl(url))throw std::runtime_error("Invalid video URL");
#ifdef PSGI_HOST
    throw std::runtime_error("Video links open in the PS4 browser on console");
#else
    auto check=[](int32_t result,const char* action){
        if(result<0){char code[32];std::snprintf(code,sizeof(code),"0x%08X",unsigned(result));throw std::runtime_error(std::string(action)+": "+code);}
    };
    check(static_cast<int32_t>(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE)),"Load browser service");
    check(openBrowserService(url.c_str(),nullptr),"Open video in PS4 browser");
#endif
}
}
