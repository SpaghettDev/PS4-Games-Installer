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
#include <fstream>
#include <stdexcept>

namespace {
const std::string body = R"({"DATA":{"http://fixture/game.pkg":{"name":"A game"}}})";
constexpr const char* caPath = "build/host/test-cacert.pem";
const std::string caOne = "-----BEGIN CERTIFICATE-----\r\nQQ==\r\n-----END CERTIFICATE-----\r\n";
const std::string caTwo = "-----BEGIN CERTIFICATE-----\r\nQg==\r\n-----END CERTIFICATE-----\r\n";
struct Response { std::string url; int status; std::string headers; int tlsError = 0; };
std::vector<Response> responses;
std::vector<std::string> requestedUrls;
std::string scenario, connectionUrl;
bool initialized = false, poolAlive = false, sslAlive = false;
bool tmpl = false, conn = false, req = false, policy = false, aborted = false;
bool imageCallback = false, keepalive = false, requestAborted = false;
int initializations = 0, moduleLoads = 0, resolutions = 0, caLoads = 0;
int poolDestructions = 0, sslTerminations = 0, httpTerminations = 0, rangeHeaders = 0;
int sslErrorReads = 0, errnoReads = 0;
SslPem** retainedCaList = nullptr;
size_t offset = 0;
std::atomic<bool>* cancellation = nullptr;
void writeCa(const std::string& value) {
    std::ofstream out(caPath, std::ios::binary);
    assert(out); out << value; assert(out);
}
void checkCaLifetime() {
    assert(retainedCaList && retainedCaList[0] && retainedCaList[1]);
    assert(retainedCaList[0] != retainedCaList[1]);
    assert(std::string(retainedCaList[0]->ptr, retainedCaList[0]->size) == caOne);
    assert(std::string(retainedCaList[1]->ptr, retainedCaList[1]->size) == caTwo);
}
void reset(const char* mode) {
    assert(!tmpl && !conn && !req);
    scenario = mode; offset = 0; policy = aborted = imageCallback = false;
    responses.clear(); requestedUrls.clear(); rangeHeaders = 0;
    sslErrorReads = errnoReads = 0;
}
const Response& response() { assert(!responses.empty()); return responses.at(requestedUrls.size() - 1); }
template<class F> std::string rejects(F fn, const char* text) {
    bool rejected = false;
    std::string message;
    try { fn(); } catch (const std::runtime_error& error) {
        message = error.what();
        if (std::string(error.what()).find(text) == std::string::npos)
            std::fprintf(stderr, "Expected '%s', got '%s'\n", text, error.what());
        assert(std::string(error.what()).find(text) != std::string::npos); rejected = true;
    }
    assert(rejected && !tmpl && !conn && !req);
    return message;
}
void networkReleased() { assert(!initialized && !poolAlive && !sslAlive && !retainedCaList); }
}

extern "C" {
uint32_t sceSysmoduleLoadModuleInternal(OrbisSysModuleInternal) { return 0; }
int32_t sceNetInit() { return 0; }
int32_t sceNetCtlInit() { return 0; }
int32_t sceNetPoolCreate(const char*, int32_t, int32_t) { assert(!poolAlive); poolAlive = true; return 1; }
void sceNetPoolDestroy(int32_t id) { assert(id == 1 && poolAlive && !sslAlive); poolAlive = false; ++poolDestructions; }
int32_t sceSslInit(size_t) { assert(poolAlive && !sslAlive); sslAlive = true; return 2; }
int psgiSslTerm(int id) { assert(id == 2 && sslAlive && !initialized); sslAlive = false; ++sslTerminations; return 0; }
void sceSystemServicePowerTick() {}
}

// These functions belong only to the explicitly resolved module. There are no
// linked HTTP entry points in the fixture: mixing in imports fails at link time.
namespace HttpFixture {
int32_t sceHttpInit(int32_t pool, int32_t ssl, size_t) {
    assert(pool == 1 && ssl == 2 && poolAlive && sslAlive && !initialized);
    initialized = true; ++initializations; return 3;
}
int32_t sceHttpTerm(int32_t id) {
    assert(id == 3 && initialized && !tmpl && !conn && !req);
    if (retainedCaList) checkCaLifetime();
    retainedCaList = nullptr; initialized = false; ++httpTerminations; return 0;
}
int32_t sceHttpsLoadCert(int32_t id, int32_t count, void* list, void* client, void* key) {
    assert(id == 3 && initialized && !tmpl && !conn && !req);
    assert(count == 2 && list && !client && !key);
    auto** certificates = static_cast<SslPem**>(list);
    assert(certificates[0] != certificates[1]);
    assert(std::string(certificates[0]->ptr, certificates[0]->size) == caOne);
    assert(std::string(certificates[1]->ptr, certificates[1]->size) == caTwo);
    ++caLoads;
    if (scenario == "ca-rejected") return int32_t(0x8095F00Au);
    retainedCaList = certificates;
    return 0;
}
int32_t sceHttpCreateTemplate(int32_t id, const char*, int32_t, int32_t) {
    assert(id == 3 && initialized && !tmpl && !conn && !req); checkCaLifetime();
    tmpl = true; policy = imageCallback = false; return 4;
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
int32_t sceHttpCreateConnectionWithURL(int32_t id, const char* url, bool enableKeepalive) {
    assert(initialized && policy && id == 4 && !conn);
    connectionUrl = url; keepalive = enableKeepalive; conn = true; return 5;
}
int32_t sceHttpCreateRequestWithURL(int32_t id, int32_t method, const char* url, uint64_t) {
    assert(conn && id == 5 && !req && method == ORBIS_METHOD_GET && connectionUrl == url);
    if (!responses.empty()) assert(responses.at(requestedUrls.size()).url == url);
    requestedUrls.emplace_back(url); offset = 0; requestAborted = false; req = true; return 6;
}
int32_t sceHttpSendRequest(int32_t id, const void*, size_t) {
    assert(req && id == 6); checkCaLifetime();
    // Model the native trust-context compatibility contract documented by SSPI.
    // This fixture checks our API selection; it is not a firmware reproduction.
    if (connectionUrl.compare(0, 8, "https://") == 0 && !keepalive && !imageCallback)
        return int32_t(0x8095F00Cu);
    if (!responses.empty() && response().tlsError && !imageCallback) return response().tlsError;
    if (scenario == "strict-tls") { assert(!imageCallback); return int32_t(0x8095F00Cu); }
    return scenario == "image-tls" && !imageCallback ? int32_t(0x80431075u) : 0;
}
int psgiHttpsGetSslError(int id, int* number, uint32_t* detail) {
    assert(id == 6 && req && !requestAborted && number && detail);
    ++sslErrorReads;
    if (scenario == "diagnostic-getter-failure") return int32_t(0x80431001u);
    *number = int32_t(0x8095F00Cu); *detail = 0x20;
    return 0;
}
int32_t sceHttpGetLastErrno(int32_t id, int32_t* number) {
    assert(id == 6 && req && !requestAborted && number);
    ++errnoReads;
    if (scenario == "diagnostic-getter-failure") return int32_t(0x80431002u);
    *number = 54;
    return 0;
}
int32_t sceHttpsSetSslCallback(int32_t id, OrbisHttpsCallback callback, void* argument) {
    assert(id == 4 && tmpl && callback && !argument);
    assert(callback(2, 0x20, nullptr, 0, nullptr) == 1);
    if (scenario == "image-policy-error") return int32_t(0x80431001u);
    imageCallback = true; return 0;
}
int32_t sceHttpGetStatusCode(int32_t id, int32_t* status) {
    assert(id == 6);
    *status = responses.empty() ? (scenario == "partial" ? 206 : 200) : response().status;
    return 0;
}
int32_t sceHttpGetAllResponseHeaders(int32_t id, char** header, size_t* length) {
    assert(req && id == 6 && !responses.empty());
    *header = const_cast<char*>(response().headers.data()); *length = response().headers.size();
    if (scenario == "cancel-redirect") *cancellation = true;
    return 0;
}
int32_t sceHttpReadData(int32_t id, void* data, uint32_t capacity) {
    assert(req && id == 6);
    assert(responses.empty() || response().status == 200 || response().status == 206);
    size_t count = std::min({size_t(capacity), size_t(7), body.size() - offset});
    std::memcpy(data, body.data() + offset, count); offset += count;
    if (scenario == "cancel") *cancellation = true;
    return count;
}
int32_t sceHttpAbortRequest(int32_t id) { assert(id == 6 && req); requestAborted = aborted = true; return 0; }
int32_t sceHttpAddRequestHeader(int32_t id, const char* name, const char* value, int32_t mode) {
    assert(id == 6 && std::strcmp(name, "Range") == 0 && std::strcmp(value, "bytes=0-3") == 0 && mode == 0);
    ++rangeHeaders; return 0;
}
int32_t sceHttpGetResponseContentLength(int32_t id, int32_t* type, size_t* length) {
    assert(id == 6);
    if (scenario == "no-document-length") return int32_t(0x80431071u);
    *type = ORBIS_HTTP_CONTENTLEN_EXIST; *length = body.size(); return 0;
}
int32_t sceHttpDeleteRequest(int32_t id) { assert(id == 6 && req); req = false; return 0; }
int32_t sceHttpDeleteConnection(int32_t id) { assert(id == 5 && conn && !req); conn = false; return 0; }
int32_t sceHttpDeleteTemplate(int32_t id) { assert(id == 4 && tmpl && !conn); tmpl = false; return 0; }
}

extern "C" {
uint64_t sceKernelGetProcessTime() { static uint64_t time = 0; return ++time; }
uint32_t sceKernelLoadStartModule(const char* path, size_t size, const void* args,
                                uint32_t flags, void* options, void* result) {
    assert(std::strcmp(path, "/system/common/lib/libSceHttp.sprx") == 0);
    assert(size == 0 && !args && flags == 0 && !options && !result);
    ++moduleLoads;
    return scenario == "load-failure" ? 0x80020002u : 42;
}
int32_t sceKernelDlsym(int32_t module, const char* name, void** address) {
    assert(module == 42 && address); ++resolutions;
    if (scenario == "resolve-failure") return static_cast<int32_t>(0x80020002u);
    if (scenario == "null-symbol") { *address = nullptr; return 0; }
#define RESOLVE(symbol, fixture) \
    if (std::strcmp(name, symbol) == 0) { \
        *address = reinterpret_cast<void*>(&HttpFixture::fixture); return 0; \
    }
    RESOLVE("sceHttpInit", sceHttpInit)
    RESOLVE("sceHttpTerm", sceHttpTerm)
    RESOLVE("sceHttpCreateTemplate", sceHttpCreateTemplate)
    RESOLVE("sceHttpDeleteTemplate", sceHttpDeleteTemplate)
    RESOLVE("sceHttpCreateConnectionWithURL", sceHttpCreateConnectionWithURL)
    RESOLVE("sceHttpDeleteConnection", sceHttpDeleteConnection)
    RESOLVE("sceHttpCreateRequestWithURL", sceHttpCreateRequestWithURL)
    RESOLVE("sceHttpDeleteRequest", sceHttpDeleteRequest)
    RESOLVE("sceHttpSetAutoRedirect", psgiHttpSetAutoRedirect)
    RESOLVE("sceHttpSetRecvTimeOut", psgiHttpSetRecvTimeOut)
    RESOLVE("sceHttpSetConnectTimeOut", sceHttpSetConnectTimeOut)
    RESOLVE("sceHttpSetResolveTimeOut", sceHttpSetResolveTimeOut)
    RESOLVE("sceHttpSetSendTimeOut", sceHttpSetSendTimeOut)
    RESOLVE("sceHttpSendRequest", sceHttpSendRequest)
    RESOLVE("sceHttpsGetSslError", psgiHttpsGetSslError)
    RESOLVE("sceHttpGetLastErrno", sceHttpGetLastErrno)
    RESOLVE("sceHttpGetStatusCode", sceHttpGetStatusCode)
    RESOLVE("sceHttpReadData", sceHttpReadData)
    RESOLVE("sceHttpAbortRequest", sceHttpAbortRequest)
    RESOLVE("sceHttpAddRequestHeader", sceHttpAddRequestHeader)
    RESOLVE("sceHttpGetResponseContentLength", sceHttpGetResponseContentLength)
    RESOLVE("sceHttpsSetSslCallback", sceHttpsSetSslCallback)
    RESOLVE("sceHttpsLoadCert", sceHttpsLoadCert)
    RESOLVE("sceHttpGetAllResponseHeaders", sceHttpGetAllResponseHeaders)
#undef RESOLVE
    assert(false && "Unexpected HTTP symbol"); return -1;
}
}

int main() {
    const char* url = "http://fixture/games";
    const char* secure = "https://fixture/games";
    std::atomic<bool> cancelled{true}; cancellation = &cancelled;
    rejects([&] { psgi::httpGet(url, 1024, &cancelled); }, "cancelled");
    assert(initializations == 0 && moduleLoads == 0 && resolutions == 0);
    reset("ca-missing"); std::remove(caPath);
    rejects([&] { psgi::prepareHttpTrust(); }, "Cannot open"); networkReleased();
    reset("ca-malformed"); writeCa("-----BEGIN CERTIFICATE-----\nQQ==\n");
    rejects([&] { psgi::prepareHttpTrust(); }, "HTTPS CA"); networkReleased();
    assert(initializations == 0 && moduleLoads == 0 && resolutions == 0);
    // /app0 is no longer reachable after the console's privilege/root change.
    // Prewarm the bundle first and prove all later network work uses that copy.
    writeCa(caOne + caTwo); psgi::prepareHttpTrust(); std::remove(caPath);
    psgi::prepareHttpTrust();
    reset("load-failure"); rejects([&] { psgi::httpGet(url); }, "Load HTTP client (0x80020002)");
    assert(moduleLoads == 1 && initializations == 0);
    reset("resolve-failure"); rejects([&] { psgi::httpGet(url); }, "sceHttpInit (0x80020002)");
    assert(moduleLoads == 2 && initializations == 0);
    reset("null-symbol"); rejects([&] { psgi::httpGet(url); }, "Missing HTTP function: sceHttpInit");
    reset("ca-rejected");
    const int beforeCaFailure = httpTerminations;
    rejects([&] { psgi::httpGet(url); }, "0x8095F00A"); networkReleased();
    assert(caLoads == 1 && httpTerminations == beforeCaFailure + 1);
    assert(poolDestructions == sslTerminations && sslTerminations == httpTerminations);
    reset("success"); assert(psgi::httpGet(url) == body);
    assert(caLoads == 2 && !tmpl && !conn && !req); checkCaLifetime();
    const int initializedOnce = initializations, resolvedOnce = resolutions, modulesOnce = moduleLoads;
    reset("https-keepalive"); assert(psgi::httpGet(secure) == body && keepalive);
    assert(sslErrorReads == 0 && errnoReads == 0);

    reset("redirect-relative");
    responses = {{"https://fixture/old/games", 302, "HTTP/1.1 302 Found\r\nLocation: ../new/catalog?sig=a%2Fb\r\n"},
                 {"https://fixture/new/catalog?sig=a%2Fb", 200, ""}};
    assert(psgi::httpGet(responses[0].url) == body && requestedUrls.size() == 2);
    reset("redirect-query");
    responses = {{secure, 307, "location: ?token=ok\r\n"}, {"https://fixture/games?token=ok", 200, ""}};
    assert(psgi::httpGet(secure) == body && requestedUrls.size() == 2);
    reset("redirect-crosshost");
    responses = {{url, 301, "Location: https://cdn.fixture/final.pkg\r\n"},
                 {"https://cdn.fixture/final.pkg", 206, ""}};
    std::string streamed;
    auto info = psgi::httpTransfer(url, [&](const void* data, size_t count, uint64_t total) {
        assert(total == body.size()); streamed.append(static_cast<const char*>(data), count); return true;
    }, true);
    assert(streamed == body && info.status == 206 && info.length == body.size());
    assert(info.effectiveUrl == "https://cdn.fixture/final.pkg" && rangeHeaders == 2);
    reset("redirect-scheme-relative");
    responses = {{secure, 308, "Location: //cdn.fixture/final.pkg\r\n"}, {"https://cdn.fixture/final.pkg", 200, ""}};
    info = psgi::httpTransfer(secure, [](const void*, size_t, uint64_t) { return false; }, false);
    assert(info.effectiveUrl == "https://cdn.fixture/final.pkg" && aborted);
    for (const auto& invalid : std::vector<std::pair<std::string, std::string>>{
             {"", "missing Location"}, {"Location: ftp://fixture/file\r\n", "Invalid HTTP redirect URL"},
             {"Location: http://fixture/plain\r\n", "HTTPS redirect to HTTP"},
             {"Location: https://user:pass@fixture/file\r\n", "Invalid HTTP redirect URL"},
             {"Location: /one\r\nLocation: /two\r\n", "multiple Location"},
             {"Location: https://fixture/bad path\r\n", "Invalid HTTP redirect URL"}}) {
        reset("redirect-invalid"); responses = {{secure, 302, invalid.first}};
        rejects([&] { psgi::httpGet(secure); }, invalid.second.c_str());
        assert(offset == 0 && requestedUrls.size() == 1);
        reset("redirect-invalid"); responses = {{secure, 302, invalid.first}};
        rejects([&] { psgi::httpTransfer(secure, [](const void*, size_t, uint64_t) { assert(false); return true; }, false); }, invalid.second.c_str());
        assert(offset == 0 && requestedUrls.size() == 1);
    }
    reset("redirect-loop"); responses.assign(6, Response{secure, 302, "Location: /games\r\n"});
    rejects([&] { psgi::httpGet(secure); }, "Too many HTTP redirects"); assert(requestedUrls.size() == 6);
    reset("redirect-loop"); responses.assign(6, Response{secure, 302, "Location: /games\r\n"});
    rejects([&] { psgi::httpTransfer(secure, [](const void*, size_t, uint64_t) { assert(false); return true; }, false); }, "Too many HTTP redirects");
    assert(requestedUrls.size() == 6);
    reset("redirect-large"); responses = {{secure, 302, "Location: /games\r\nX-Large: " + std::string(64 * 1024, 'x')}};
    rejects([&] { psgi::httpGet(secure); }, "Invalid HTTP redirect headers");
    reset("cancel-redirect"); cancelled = false; responses = {{secure, 302, "Location: /next\r\n"}};
    rejects([&] { psgi::httpGet(secure, 1024, &cancelled); }, "cancelled"); assert(requestedUrls.size() == 1);

    reset("large"); rejects([&] { psgi::httpGet(url, 4); }, "exceeds limit");
    reset("cancel"); cancelled = false;
    rejects([&] { psgi::httpGet(url, 1024, &cancelled); }, "cancelled"); assert(aborted);
    reset("policy-error"); rejects([&] { psgi::httpGet(url); }, "Disable automatic redirects (0x80431001)");
    reset("success"); assert(psgi::httpGet(url) == body);
    reset("no-document-length"); assert(psgi::httpGet(secure) == body);
    reset("no-document-length"); assert(psgi::httpGetArtwork("https://fixture/image.jpg", 1024) == body);
    reset("success"); streamed.clear();
    info = psgi::httpTransfer(url, [&](const void* data, size_t count, uint64_t total) {
        assert(total == body.size()); streamed.append(static_cast<const char*>(data), count); return true;
    }, false);
    assert(streamed == body && info.status == 200 && info.length == body.size() && info.effectiveUrl == url);
    reset("success");
    psgi::httpTransfer(url, [](const void*, size_t, uint64_t) { return false; }, true);
    assert(aborted && offset <= 7 && !tmpl && !conn && !req);
    reset("partial");
    rejects([&] { psgi::httpTransfer(url, [](const void*, size_t, uint64_t) { return true; }, false); }, "Package HTTP 206");
    assert(aborted && offset == 0);
    reset("success");
    rejects([&] { psgi::httpTransfer(url, [](const void*, size_t, uint64_t) -> bool { throw std::runtime_error("sink error"); }, false); }, "sink error");
    assert(aborted && offset == 7);
    reset("cancel"); cancelled = false;
    rejects([&] { psgi::httpTransfer(url, [](const void*, size_t, uint64_t) { return true; }, false, &cancelled); }, "cancelled");
    assert(aborted);
    reset("strict-tls"); rejects([&] { psgi::httpGet(secure); }, "0x8095F00C");
    reset("strict-tls"); rejects([&] { psgi::httpTransfer(secure, [](const void*, size_t, uint64_t) { assert(false); return true; }, false); }, "0x8095F00C");
    const char* privateUrl = "https://fixture/private/catalog?token=secret";
    for (bool package : {false, true}) {
        reset("redirect-tls-diagnostic");
        responses = {{privateUrl, 302, "Location: https://cdn.fixture/private/game.pkg?token=secret\r\n"},
                     {"https://cdn.fixture/private/game.pkg?token=secret", 200, "", int32_t(0x8095F00Cu)}};
        const auto message = rejects([&] {
            if (package) psgi::httpTransfer(privateUrl, [](const void*, size_t, uint64_t) { assert(false); return true; }, false);
            else psgi::httpGet(privateUrl);
        }, "cdn.fixture (redirect 1)");
        assert(message.find("0x8095F00C") != std::string::npos);
        for (const char* detail : {"ssl=0x8095F00C", "verify=0x00000020", "errno=54",
                                   "ca=2", "ca_load=0x00000000", "keepalive=1"})
            assert(message.find(detail) != std::string::npos);
        assert(sslErrorReads == 1 && errnoReads == 1 && aborted);
        assert(message.find("private") == std::string::npos && message.find("token") == std::string::npos &&
               message.find("secret") == std::string::npos && message.find("game.pkg") == std::string::npos);
    }
    for (bool package : {false, true}) {
        reset("diagnostic-getter-failure");
        responses = {{privateUrl, 200, "", int32_t(0x8095F00Cu)}};
        const auto message = rejects([&] {
            if (package) psgi::httpTransfer(privateUrl, [](const void*, size_t, uint64_t) { assert(false); return true; }, false);
            else psgi::httpGet(privateUrl);
        }, "fixture (redirect 0) (0x8095F00C)");
        assert(message.find("ssl_query=0x80431001") != std::string::npos &&
               message.find("errno_query=0x80431002") != std::string::npos);
        assert(message.find("verify=") == std::string::npos && message.find("errno=") == std::string::npos);
        assert(message.find("ca=2") != std::string::npos && message.find("ca_load=0x00000000") != std::string::npos);
        assert(message.find("private") == std::string::npos && message.find("secret") == std::string::npos);
        assert(sslErrorReads == 1 && errnoReads == 1 && aborted);
    }
    reset("plain-http-diagnostic"); responses = {{url, 200, "", int32_t(0x8002003Cu)}};
    const auto plainError = rejects([&] { psgi::httpGet(url); }, "fixture (redirect 0) (0x8002003C)");
    assert(sslErrorReads == 0 && errnoReads == 1 && plainError.find("errno=54") != std::string::npos);
    assert(plainError.find("ssl=") == std::string::npos && plainError.find("verify=") == std::string::npos);
    reset("strict-tls");
    const auto directTls = rejects([&] { psgi::httpGet(privateUrl); }, "fixture (redirect 0)");
    assert(directTls.find("private") == std::string::npos && directTls.find("secret") == std::string::npos);
    const char* image = "https://images.igdb.com/igdb/image/upload/t_1080p/fixture.jpg";
    reset("image-tls"); assert(psgi::httpGetArtwork(image, 1024) == body && imageCallback);
    reset("image-tls"); rejects([&] { psgi::httpGet(image); }, "0x80431075"); assert(!imageCallback);
    for (const char* strict : {"https://other.example/image.jpg", "https://images.igdb.com.evil.example/igdb/image/upload/a.jpg",
                              "https://images.igdb.com/private/a.jpg"}) {
        reset("image-tls"); rejects([&] { psgi::httpGetArtwork(strict, 1024); }, "0x80431075"); assert(!imageCallback);
    }
    reset("image-tls"); rejects([&] { psgi::httpGetArtwork("https://images.igdb.com@evil.example/igdb/image/upload/a.jpg", 1024); }, "Invalid catalog URL");
    reset("image-tls"); rejects([&] { psgi::httpTransfer(image, [](const void*, size_t, uint64_t) { return true; }, false); }, "0x80431075"); assert(!imageCallback);
    reset("image-policy-error"); rejects([&] { psgi::httpGetArtwork(image, 1024); }, "Image HTTPS compatibility (0x80431001)");
    reset("image-redirect");
    responses = {{image, 302, "Location: https://other.example/image.jpg\r\n"},
                 {"https://other.example/image.jpg", 200, "", int32_t(0x8095F00Cu)}};
    rejects([&] { psgi::httpGetArtwork(image, 1024); }, "0x8095F00C");
    assert(requestedUrls.size() == 2 && !imageCallback);
    assert(initializations == initializedOnce && resolutions == resolvedOnce && moduleLoads == modulesOnce && caLoads == 2);
    checkCaLifetime(); std::remove(caPath);
    std::puts("Native HTTP trust loading, redirects, TLS policies, limits, cancellation, cleanup and retry passed");
}
