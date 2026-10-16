#include "ui.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>

using namespace psgi;
template<class F> void rejects(F fn) { bool failed = false; try { fn(); } catch (const std::exception&) { failed = true; } assert(failed); }
void drain(Ui& ui) { Command command; while (ui.take(command)) {} }
int main() {
    auto games = parseCatalog(R"({"DATA":{"http://nas/base":{"name":"Alpha","title_id":"CUSA00001","version":"1.00","category":"games","size":100},"http://nas/update":{"name":"Patch","title_id":"CUSA00001","version":"1.20","category":"updates","required_base_version":"1.10","min_fw":"11.00"},"http://nas/dlc":{"name":"Expansion","title_id":"CUSA00001","category":"dlc"},"http://nas/other":{"name":"Beta","title_id":"CUSA00002","category":"games","size":200}}})", "");
    auto local = games[0]; local.installed = true; local.version = "1.10";
    matchInstalled(games,{local},{});
    Settings settings;
    auto updates = makeView(games,settings,Library::Available,"",{},true);
    assert(updates.size()==1 && updates[0].index==1);
    local.version="1.20"; matchInstalled(games,{local},{}); assert(makeView(games,settings,Library::Available,"",{},true).empty());
    local.version="unknown"; matchInstalled(games,{local},{}); assert(!newerUpdate(games[1]));
    local.version="1.10";
    assert(!installationRequirements(games[1],{}).blockers.empty());
    assert(installationRequirements(games[1],{local}).blockers.empty());
    local.version="1.00"; assert(!installationRequirements(games[1],{local}).blockers.empty());
    auto required=installationRequirements(games[1],{}, {games[0]}); assert(required.blockers.empty() && required.dependencyUrl==games[0].url);
    assert(installationRequirements(games[2],{local}).blockers.empty());
    assert(!installationRequirements(games[2],{}).blockers.empty());
    auto invalid=games[1]; invalid.metadata={{"required_base_version","latest"}}; assert(!installationRequirements(invalid,{local}).blockers.empty());
    invalid=games[2]; invalid.titleId=""; assert(!installationRequirements(invalid,{local}).blockers.empty());
    assert(installationRequirements(games[0],{}).blockers.empty());
    std::vector<QueueItem> queue; enqueueJob(queue,games[0],100); enqueueJob(queue,games[1],101); queue[1].dependsOn=games[0].url;
    const auto loaded=parseQueue(serializeQueue(queue)); assert(loaded[1].dependsOn==games[0].url && loaded[1].entry.minFirmware=="11.00" && requiredBaseVersion(loaded[1].entry)=="1.10");
    queue[1].dependsOn=queue[1].entry.url; rejects([&]{parseQueue(serializeQueue(queue));}); queue[1].dependsOn=games[0].url;
    auto a=games[0],b=games[3],c=a; c.name="Gamma"; c.url="http://nas/unknown";
    a.rating=3;b.rating=5;c.rating=0;a.lastLaunch=100;b.lastLaunch=200;c.lastLaunch=0;
    a.firstSeenInstalled=100;b.firstSeenInstalled=200;c.firstSeenInstalled=0;c.size=0;
    const std::vector<Entry> list{a,b,c};
    for (auto field : {SortField::Size,SortField::Rating,SortField::LastLaunch,SortField::FirstInstalled}) {
        settings.sort=field; settings.descending=true;
        auto view=makeView(list,settings,Library::Available); assert(view[0].index==1 && view[2].index==2);
        settings.descending=false;view=makeView(list,settings,Library::Available);assert(view[0].index==0 && view[2].index==2);
        const auto decoded=parseSettings(serializeSettings(settings)); assert(decoded.sort==field && !decoded.descending);
    }
    settings.sort=SortField::Name;settings.descending=true;assert(makeView(list,settings,Library::Available)[0].index==2);
    auto migrated=parseSettings(R"({"schema":1,"content_urls":{"games":""},"size_sort":true})");assert(migrated.sort==SortField::Size && migrated.descending);
    rejects([]{parseSettings(R"({"schema":1,"content_urls":{"games":""},"sort_field":"invalid"})");});

    Ui ui; ui.screen=Screen::Dashboard;local.version="1.10";ui.setLibraries(std::vector<Entry>{local},games);
    ui.handle(Button::Left);ui.handle(Button::Down);ui.handle(Button::Down); assert(ui.updatesOnly && ui.sidebarIndex==2 && ui.visible.size()==1);
    auto state=parseViewState(serializeViewState(ui.captureView())); Ui reopened;reopened.setLibraries(std::vector<Entry>{local},games);reopened.restoreView(state);
    assert(reopened.updatesOnly && reopened.sidebar && reopened.sidebarIndex==2 && reopened.current()->url==games[1].url);
    ui.handle(Button::Down);ui.handle(Button::Cross);assert(ui.screen==Screen::Queue);ui.handle(Button::Circle);assert(ui.screen==Screen::Dashboard && ui.sidebarIndex==3);
    ui.handle(Button::Down);ui.handle(Button::Cross);assert(ui.screen==Screen::SavedCollections);ui.handle(Button::Circle);assert(ui.screen==Screen::Dashboard && ui.sidebarIndex==4);
    ui.screen=Screen::Packages; ui.packageTitle=games[0].titleId; ui.packageIndex=0; ui.available[0].queued=true; ui.handle(Button::Cross);
    assert(ui.screen==Screen::Queue); ui.handle(Button::Circle); assert(ui.screen==Screen::Packages); ui.available[0].queued=false;
    ui.screen=Screen::Dashboard; ui.handle(Button::L1); assert(!ui.updatesOnly);
    ui.pending=games[1]; ui.installed.clear(); ui.screen=Screen::ConfirmInstall;drain(ui);ui.handle(Button::Cross);Command command;assert(!ui.take(command) && ui.screen==Screen::ConfirmInstall);
    ui.downloads={loaded[0]};ui.handle(Button::Cross);assert(ui.take(command) && command.type==CommandType::Install);
    ui.screen=Screen::Options;ui.menuIndex=17;ui.handle(Button::Cross);assert(ui.screen==Screen::Cleanup);drain(ui);
    ui.handle(Button::Cross);ui.handle(Button::Circle);assert(ui.screen==Screen::Cleanup && !ui.take(command));
    ui.handle(Button::Cross);ui.handle(Button::Cross);assert(ui.take(command) && command.type==CommandType::CleanStaging);ui.handle(Button::Circle);assert(ui.screen==Screen::Options);
    ui.menuIndex=18;ui.handle(Button::Cross);drain(ui);ui.backupDevices={{"/mnt/usb0",{}}};ui.handle(Button::Cross);assert(ui.take(command) && command.type==CommandType::ExportDiagnostics && command.text=="/mnt/usb0");

    char temporary[]="/tmp/psgi-maintenance-XXXXXX";assert(mkdtemp(temporary));const std::string root=temporary;
    std::filesystem::create_directory(root+"/downloads");std::filesystem::create_directory(root+"/artwork");std::filesystem::create_directory(root+"/usb0");
    const auto released=root+"/downloads/0000000000000001.pkg",active=root+"/downloads/0000000000000002.pkg",uncertain=root+"/downloads/0000000000000003.pkg",legacy=root+"/downloads/0000000000000004.pkg",partial=root+"/downloads/0000000000000005.pkg.part";
    for(const auto& path:{released,active,uncertain,legacy,partial})atomicWrite(path,"1234567890");
    atomicWrite(released+".lease","released\n");atomicWrite(active+".lease","released\n");atomicWrite(uncertain+".lease","in-use\n");atomicWrite(root+"/artwork/art-0000000000000001.img","12345");
    atomicWrite(root+"/outside.pkg","keep");std::filesystem::create_symlink(root+"/outside.pkg",root+"/downloads/0000000000000006.pkg");
    atomicWrite(root+"/downloads/unrecognized.pkg","keep");
    const auto usage=scanStorageUsage(root,{active},true);assert(usage.checked && usage.artwork==5 && usage.staging==50 && usage.reclaimable==20 && usage.protectedBytes==30 && usage.removableFiles==2 && usage.protectedFiles==3);
    assert(scanStorageUsage(root,{active},false).reclaimable==0);rejects([&]{cleanStaging(root,{},false);});
    auto cleaned=cleanStaging(root,{active},true);assert(cleaned.files==2 && cleaned.bytes==20 && !cleaned.failed);
    assert(!std::filesystem::exists(released) && !std::filesystem::exists(released+".lease") && !std::filesystem::exists(partial));
    for(const auto& path:{active,uncertain,legacy,root+"/outside.pkg",root+"/downloads/unrecognized.pkg"})assert(std::filesystem::exists(path));
    std::filesystem::create_symlink(root+"/downloads",root+"/replaced");std::filesystem::rename(root+"/downloads",root+"/original");std::filesystem::create_directory(root+"/symlink-test");std::filesystem::create_directory_symlink(root+"/original",root+"/symlink-test/downloads");
    rejects([&]{cleanStaging(root+"/symlink-test",{},true);});

    DiagnosticSnapshot snapshot;snapshot.revision=25;snapshot.installed=16;snapshot.available=300;snapshot.usage=usage;snapshot.queue=loaded;
    snapshot.queue[1].entry.name="PRIVATE GAME NAME";snapshot.queue[1].entry.url="https://username:pass123@server/pkg?token=abcdef";
    snapshot.queue[1].entry.version="secret-version";snapshot.queue[1].message="Install error 0x80990086";
    snapshot.logTail="Install error 0x80990086\nDownload HTTP status code: 404\nLaunch failed CE-34878-0\nAuthorization: Bearer PRIVATEBEARER 0x12345678\nCookie: PRIVATECOOKIE\nsource https://username:pass123@host/pkg?access_token=0x11111111\nsource https://privatehost/pkg?key=0x22222222\npassword PRIVATEPASSWORD\nMy PRIVATE NOTE\n";
    const auto report=diagnosticReport(snapshot,1000);
    for(const char* secret:{"PRIVATE","username","pass123","abcdef","secret-version","privatehost","0x12345678","0x11111111","0x22222222","http://nas"})assert(report.find(secret)==std::string::npos);
    for(const char* code:{"0x80990086","HTTP 404","ce-34878-0"})assert(report.find(code)!=std::string::npos);
    const auto name=exportDiagnostics(root+"/usb0",snapshot,1000);assert(readFile(root+"/usb0/"+name)==report);assert(exportDiagnostics(root+"/usb0",snapshot,1000)!=name);
    assert(scanBackupDevices(root,false)[0].files.empty()); // Reports never appear as restorable player backups.
    std::filesystem::remove_all(root);
    std::puts("Sorting migration, Updates/sidebar persistence, dependency guards, protected cleanup and sanitized USB reports passed");
}
