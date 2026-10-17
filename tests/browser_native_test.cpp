#include "platform.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>
namespace { int loads=0,opens=0; uint32_t loadResult=0; int32_t openResult=0; }
extern "C" uint32_t sceSysmoduleLoadModuleInternal(uint32_t){++loads;return loadResult;}
extern "C" int32_t sceSystemServiceLaunchWebBrowser(const char* url,void* options){
    ++opens;assert(std::string(url)=="https://www.youtube.com/watch?v=abcdefghijk"&&options==nullptr);return openResult;
}
int main(){
    const std::string url="https://www.youtube.com/watch?v=abcdefghijk";
    psgi::openMediaUrl(url);assert(loads==1&&opens==1);
    for(const auto& invalid:{"file:///tmp/video","javascript:alert(1)","http://example.org/a\r\n"}){
        bool rejected=false;try{psgi::openMediaUrl(invalid);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&loads==1&&opens==1);
    }
    loadResult=0x80020002u;bool rejected=false;try{psgi::openMediaUrl(url);}catch(const std::runtime_error& e){rejected=std::string(e.what()).find("Load browser service")!=std::string::npos;}assert(rejected&&opens==1);
    loadResult=0;openResult=int32_t(0x80020003u);rejected=false;try{psgi::openMediaUrl(url);}catch(const std::runtime_error& e){rejected=std::string(e.what()).find("0x80020003")!=std::string::npos;}assert(rejected&&opens==2);
    std::puts("Browser native ABI, URL validation and service errors passed");
}
