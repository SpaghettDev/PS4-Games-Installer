#include "metadata.hpp"
#include "cJSON.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace psgi {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
const cJSON* get(const cJSON* root, const char* key) { return cJSON_GetObjectItemCaseSensitive(root, key); }
std::string text(const cJSON* item) { return cJSON_IsString(item) ? std::string(item->valuestring).substr(0, 32768) : ""; }
std::string field(const cJSON* root, const char* key) { return text(get(root,key)); }
void append(std::vector<std::string>& names, const std::string& name) {
    if (name.empty() || names.size() >= 64) return;
    const auto clean = name.substr(0, 128);
    if (std::none_of(names.begin(),names.end(),[&](const std::string& value){return lower(value)==lower(clean);})) names.push_back(clean);
}
std::vector<std::string> names(const cJSON* list) {
    std::vector<std::string> result;
    if (cJSON_IsArray(list)) for (auto* item=list->child;item;item=item->next) append(result,cJSON_IsObject(item) ? field(item,"name") : text(item));
    else append(result,text(list));
    return result;
}
std::string scalar(const cJSON* value) {
    if (cJSON_IsString(value)) return text(value);
    if (cJSON_IsBool(value)) return cJSON_IsTrue(value) ? "Yes" : "No";
    if (cJSON_IsNumber(value) && std::isfinite(value->valuedouble)) { char out[48];std::snprintf(out,sizeof(out),"%.6g",value->valuedouble);return out; }
    return "";
}
void flatten(RichMetadata& data, const cJSON* value, const std::string& key, unsigned depth=0) {
    if (!value || depth>4 || data.additional.size()>=160) return;
    if (cJSON_IsObject(value)) {
        for (auto* child=value->child;child;child=child->next) {
            std::string label=child->string ? child->string : "";std::replace(label.begin(),label.end(),'_',' ');
            flatten(data,child,key.empty() ? label : key+" / "+label,depth+1);
        }
    } else if (cJSON_IsArray(value)) {
        std::vector<std::string> values;
        for (auto* child=value->child;child;child=child->next) {
            if (cJSON_IsObject(child)) {
                auto name=field(child,"name"); if (!name.empty()) append(values,name);
                else flatten(data,child,key,depth+1);
            } else append(values,scalar(child));
        }
        if (!values.empty()) data.additional.emplace_back(key,joinedNames(values));
    } else { const auto out=scalar(value);if(!out.empty())data.additional.emplace_back(key,out); }
}
std::string mediaUrl(std::string value) {
    if (value.compare(0,2,"//")==0) value="https:"+value;
    return isHttpUrl(value) ? value : "";
}
}
std::string joinedNames(const std::vector<std::string>& names) {
    std::string out;for(const auto& name:names){if(!out.empty())out+=", ";out+=name;}return out;
}
void readIgdb(Entry& entry, const std::string& json) {
    entry.igdb={};entry.igdbJson.clear();if(json.empty())return;
    if(json.size()>262144)throw std::runtime_error("IGDB metadata exceeds 256 KiB per package");
    Json root(cJSON_ParseWithLengthOpts(json.c_str(),json.size()+1,nullptr,true),cJSON_Delete);
    if(!cJSON_IsObject(root.get()))return;
    entry.igdbJson=json;auto& data=entry.igdb;
    data.genres=names(get(root.get(),"genres"));data.themes=names(get(root.get(),"themes"));
    data.publishers=names(get(root.get(),"publishers"));data.developers=names(get(root.get(),"developers"));
    data.summary=field(root.get(),"summary");data.storyline=field(root.get(),"storyline");
    const auto* dates=get(root.get(),"ps4_release_dates");
    if(cJSON_IsArray(dates))for(auto* date=dates->child;date;date=date->next){
        auto value=field(date,"date");if(value.empty())value=field(date,"human");
        if(!value.empty()&&(data.releaseDate.empty()||value<data.releaseDate))data.releaseDate=value;
    }
    const auto* users=get(get(root.get(),"ratings"),"users");const auto* score=get(users,"score");
    if(!cJSON_IsNumber(score))score=get(root.get(),"rating");
    if(cJSON_IsNumber(score)&&std::isfinite(score->valuedouble)&&score->valuedouble>=0&&score->valuedouble<=100) {
        data.userRating=score->valuedouble; data.userRatingKnown=true;
    }
    const auto* count=get(users,"count");if(cJSON_IsNumber(count)&&count->valuedouble>0&&count->valuedouble<=9007199254740991.0)data.ratingCount=uint64_t(count->valuedouble);
    const auto* media=get(root.get(),"media");auto* shots=get(media,"screenshots");if(!shots)shots=get(root.get(),"screenshots");
    if(cJSON_IsArray(shots))for(auto* shot=shots->child;shot&&data.screenshots.size()<32;shot=shot->next){
        auto url=mediaUrl(field(shot,"url"));if(!url.empty())data.screenshots.push_back({"Screenshot "+std::to_string(data.screenshots.size()+1),url});
    }
    const auto* videos=get(root.get(),"videos");
    if(cJSON_IsArray(videos))for(auto* video=videos->child;video&&data.videos.size()<32;video=video->next){
        auto url=mediaUrl(field(video,"url"));const auto id=field(video,"video_id");
        if(url.empty()&&id.size()==11&&id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos)url="https://www.youtube.com/watch?v="+id;
        auto name=field(video,"name");if(name.empty())name="Video "+std::to_string(data.videos.size()+1);
        if(!url.empty())data.videos.push_back({name.substr(0,256),url});
    }
    if(!data.developers.empty())data.additional.emplace_back("Developer",joinedNames(data.developers));
    if(!data.storyline.empty())data.additional.emplace_back("Plot",data.storyline);
    for(const char* key:{"game_modes","multiplayer","languages","age_ratings","alternative_names","franchises","collections","relationships","websites"}){
        std::string label=key;std::replace(label.begin(),label.end(),'_',' ');flatten(data,get(root.get(),key),label);
    }
    const auto* critics=get(get(root.get(),"ratings"),"critics");flatten(data,critics,"Critics rating");
    const auto url=mediaUrl(field(root.get(),"url"));if(!url.empty())data.additional.emplace_back("IGDB",url);
}
std::vector<std::string> displayCategories(const Entry& entry) {
    return entry.info.category.empty() ? entry.igdb.genres : std::vector<std::string>{entry.info.category};
}
std::vector<std::string> displayTags(const Entry& entry) {
    auto out=entry.igdb.themes;for(const auto& tag:entry.info.tags)append(out,tag);return out;
}
std::string displayDescription(const Entry& entry) {
    if(!entry.info.description.empty())return entry.info.description;
    return entry.description.empty() ? entry.igdb.summary : entry.description;
}
double displayRating(const Entry& entry) { return entry.rating ? double(entry.rating) : entry.igdb.userRating/20.0; }
std::string ratingText(const Entry& entry) {
    if(entry.rating)return std::to_string(entry.rating)+"/5 (Your rating)";
    if(!entry.igdb.userRatingKnown)return "Not rated";
    char out[40];std::snprintf(out,sizeof(out),"%.1f/5",displayRating(entry));
    return std::string(out)+" (IGDB users"+(entry.igdb.ratingCount ? "; "+std::to_string(entry.igdb.ratingCount)+" ratings" : "")+")";
}
std::string readableUrl(const std::string& value) {
    auto hex=[](char c)->int {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
    std::string out;
    for(size_t i=0;i<value.size();++i){
        if(value[i]=='%'&&i+2<value.size()&&hex(value[i+1])>=0&&hex(value[i+2])>=0){
            const unsigned char c=hex(value[i+1])*16+hex(value[i+2]);
            if(c>=32&&c!=127){out+=char(c);i+=2;continue;}
        }
        out+=value[i];
    }
    return out;
}
}
