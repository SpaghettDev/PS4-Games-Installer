#include "json.hpp"
#include "catalog.hpp"
#include "userdata.hpp"
#include <cassert>
#include <cstdlib>
#include <malloc.h>
#include <memory>
#include <cstdio>
namespace { size_t shrinking=0; }
// Emulate the failing SDK path without performing an out-of-bounds copy on host.
extern "C" void* psgiTestRealloc(void* pointer,size_t size) {
    if(pointer&&malloc_usable_size(pointer)>=128*1024&&size<malloc_usable_size(pointer)) {
        ++shrinking;return nullptr;
    }
    return std::realloc(pointer,size);
}
int main(){
    using Json=std::unique_ptr<cJSON,decltype(&cJSON_Delete)>;
    using Printed=std::unique_ptr<char,decltype(&cJSON_free)>;
    const std::string payload(600000,'x');
    Json root(cJSON_CreateObject(),cJSON_Delete);cJSON_AddStringToObject(root.get(),"description",payload.c_str());
    Printed unsafe(cJSON_PrintUnformatted(root.get()),cJSON_free);
    assert(!unsafe&&shrinking==1); // Standard print attempts a large final shrink.
    shrinking=0;
    for(bool formatted:{false,true}){
        Printed safe(psgi::printJson(root.get(),formatted),cJSON_free);assert(safe);
        Json parsed(cJSON_Parse(safe.get()),cJSON_Delete);
        assert(parsed&&std::string(cJSON_GetObjectItem(parsed.get(),"description")->valuestring)==payload);
    }
    psgi::Entry entry;entry.url="https://example.org/game.pkg";entry.name="Large catalog fixture";entry.description=payload;
    auto encoded=psgi::serializeCatalog({entry});
    for(int i=0;i<3;++i){auto entries=psgi::parseCatalog(encoded,"games");assert(entries[0].description==payload);encoded=psgi::serializeCatalog(entries);}
    psgi::PlayerData personal;
    for(unsigned i=0;i<200;++i){char id[10];std::snprintf(id,sizeof(id),"CUSA%05u",i);personal.setInfo(id,{"Adventure",std::string(1800,'n'),{"Weekend"}});}
    assert(psgi::parsePlayerData(psgi::serializePlayerData(personal)).games.size()==200);
    assert(shrinking==0);
    std::puts("Large JSON caches/personal data survive an allocator that rejects mapped-block shrinking; unsafe printing reproduces the failure");
}
