#include "platform.hpp"
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <exception>

#ifdef PSGI_HOST
#include <curl/curl.h>
namespace {
struct Transfer { std::string text; size_t limit; const std::atomic<bool>* cancelled; };
int progress(void* p, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* t = static_cast<Transfer*>(p); return t->cancelled && *t->cancelled ? 1 : 0;
}
size_t receive(char* p, size_t size, size_t count, void* userdata) {
    auto& transfer = *static_cast<Transfer*>(userdata);
    auto& out = transfer.text;
    const size_t n = size * count;
    if (n > transfer.limit - out.size()) return 0;
    try { out.append(p, n); } catch (...) { return 0; }
    return n;
}
}
namespace psgi {
HttpTransferInfo httpTransfer(const std::string& url, const HttpSink& sink, bool probeRange,
                             const std::atomic<bool>* cancelled) {
    if (!isHttpUrl(url)) throw std::runtime_error("Invalid package URL");
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    static const CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw std::runtime_error("curl initialization failed");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) throw std::runtime_error("curl allocation failed");
    struct Stream {
        CURL* curl; const HttpSink& sink; const std::atomic<bool>* cancelled;
        bool range;
        bool stopped = false; std::exception_ptr error;
    } stream{curl.get(), sink, cancelled, probeRange, false, {}};
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PS4-Games-Installer/1.0");
    if (probeRange) curl_easy_setopt(curl.get(), CURLOPT_RANGE, "0-3");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, +[](char* data, size_t size, size_t count, void* ptr) -> size_t {
        auto& s = *static_cast<Stream*>(ptr);
        try {
            long status = 0; curl_off_t total = -1;
            curl_easy_getinfo(s.curl, CURLINFO_RESPONSE_CODE, &status);
            if (status != 200 && !(s.range && status == 206)) throw std::runtime_error("Package HTTP " + std::to_string(status) + "; check the package URL on your server");
            curl_easy_getinfo(s.curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
            if (s.cancelled && *s.cancelled) throw std::runtime_error("Download cancelled");
            if (!s.sink(data, size * count, total > 0 ? static_cast<uint64_t>(total) : 0)) { s.stopped = true; return 0; }
            return size * count;
        } catch (...) { s.error = std::current_exception(); return 0; }
    });
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &stream);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, +[](void* ptr, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
        auto& s = *static_cast<Stream*>(ptr); return s.cancelled && *s.cancelled ? 1 : 0;
    });
    const auto result = curl_easy_perform(curl.get());
    if (stream.error) std::rethrow_exception(stream.error);
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    if (result != CURLE_OK && !(stream.stopped && result == CURLE_WRITE_ERROR))
        throw std::runtime_error(std::string("Package transfer failed: ") + curl_easy_strerror(result));
    long status = 0; curl_off_t length = -1;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length);
    if (status != 200 && !(probeRange && status == 206)) throw std::runtime_error("Package HTTP " + std::to_string(status) + "; check the package URL on your server");
    return {static_cast<int>(status), length > 0 ? static_cast<uint64_t>(length) : 0};
}
std::string httpGet(const std::string& url, size_t limit, const std::atomic<bool>* cancelled) {
    if (!isHttpUrl(url)) throw std::runtime_error("Invalid catalog URL");
    static const CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw std::runtime_error("curl initialization failed");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) throw std::runtime_error("curl allocation failed");
    Transfer transfer{ "", limit, cancelled };
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "PS4-Games-Installer/1.0");
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &transfer);
    CURLcode rc = curl_easy_perform(curl.get());
    if (rc != CURLE_OK) throw std::runtime_error(std::string("Catalog download failed: ") + curl_easy_strerror(rc));
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) throw std::runtime_error("Catalog HTTP " + std::to_string(status) + "; use a direct URL");
    return transfer.text;
}
std::string httpGetArtwork(const std::string& url, size_t limit, const std::atomic<bool>* cancelled) {
    return httpGet(url,limit,cancelled);
}
}
#else
#include "http_native.hpp"
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>

namespace {
void check(int rc, const char* operation) {
    if (rc < 0) {
        char code[32];
        std::snprintf(code, sizeof(code), " (0x%08X)", static_cast<unsigned>(rc));
        throw std::runtime_error(std::string(operation) + code);
    }
}
struct HttpApi {
    decltype(&sceHttpInit) init = nullptr;
    decltype(&sceHttpTerm) term = nullptr;
    decltype(&sceHttpCreateTemplate) createTemplate = nullptr;
    decltype(&sceHttpDeleteTemplate) deleteTemplate = nullptr;
    decltype(&sceHttpCreateConnectionWithURL) createConnection = nullptr;
    decltype(&sceHttpDeleteConnection) deleteConnection = nullptr;
    decltype(&sceHttpCreateRequestWithURL) createRequest = nullptr;
    decltype(&sceHttpDeleteRequest) deleteRequest = nullptr;
    decltype(&psgiHttpSetAutoRedirect) setAutoRedirect = nullptr;
    decltype(&psgiHttpSetRecvTimeOut) setRecvTimeout = nullptr;
    decltype(&sceHttpSetConnectTimeOut) setConnectTimeout = nullptr;
    decltype(&sceHttpSetResolveTimeOut) setResolveTimeout = nullptr;
    decltype(&sceHttpSetSendTimeOut) setSendTimeout = nullptr;
    decltype(&sceHttpSendRequest) send = nullptr;
    decltype(&sceHttpGetStatusCode) getStatus = nullptr;
    decltype(&sceHttpReadData) read = nullptr;
    decltype(&sceHttpAbortRequest) abort = nullptr;
    decltype(&sceHttpAddRequestHeader) addHeader = nullptr;
    decltype(&sceHttpGetResponseContentLength) contentLength = nullptr;
    decltype(&sceHttpsSetSslCallback) setSslCallback = nullptr;

    template<class T> void bind(int module, const char* name, T& function) {
        check(sceKernelDlsym(module, name, reinterpret_cast<void**>(&function)), name);
        if (!function) throw std::runtime_error(std::string("Missing HTTP function: ") + name);
    }
    void load() {
        // Use the same module instance for every operation, including init and
        // cleanup. Resolving only setters separately can select different state.
        static const int module = [] {
            int handle = sceKernelLoadStartModule("/system/common/lib/libSceHttp.sprx", 0, nullptr, 0, nullptr, nullptr);
            check(handle, "Load HTTP client");
            return handle;
        }();
        bind(module, "sceHttpInit", init);
        bind(module, "sceHttpTerm", term);
        bind(module, "sceHttpCreateTemplate", createTemplate);
        bind(module, "sceHttpDeleteTemplate", deleteTemplate);
        bind(module, "sceHttpCreateConnectionWithURL", createConnection);
        bind(module, "sceHttpDeleteConnection", deleteConnection);
        bind(module, "sceHttpCreateRequestWithURL", createRequest);
        bind(module, "sceHttpDeleteRequest", deleteRequest);
        bind(module, "sceHttpSetAutoRedirect", setAutoRedirect);
        bind(module, "sceHttpSetRecvTimeOut", setRecvTimeout);
        bind(module, "sceHttpSetConnectTimeOut", setConnectTimeout);
        bind(module, "sceHttpSetResolveTimeOut", setResolveTimeout);
        bind(module, "sceHttpSetSendTimeOut", setSendTimeout);
        bind(module, "sceHttpSendRequest", send);
        bind(module, "sceHttpGetStatusCode", getStatus);
        bind(module, "sceHttpReadData", read);
        bind(module, "sceHttpAbortRequest", abort);
        bind(module, "sceHttpAddRequestHeader", addHeader);
        bind(module, "sceHttpGetResponseContentLength", contentLength);
        bind(module, "sceHttpsSetSslCallback", setSslCallback);
    }
};
struct Network {
    HttpApi api;
    int pool = -1, ssl = -1, http = -1;
    Network() {
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET), "Load Net");
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NETCTL), "Load NetCtl");
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL), "Load SSL");
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP), "Load HTTP");
        api.load();
        check(sceNetInit(), "Net init");
        check(sceNetCtlInit(), "NetCtl init");
        pool = sceNetPoolCreate("psgi", 1024 * 1024, 0);
        check(pool, "Network pool");
        ssl = sceSslInit(1024 * 1024);
        if (ssl < 0) { sceNetPoolDestroy(pool); check(ssl, "SSL init"); }
        http = api.init(pool, ssl, 1024 * 1024);
        if (http < 0) { psgiSslTerm(ssl); sceNetPoolDestroy(pool); check(http, "HTTP init"); }
    }
    ~Network() {
        if (http >= 0) api.term(http);
        if (ssl >= 0) psgiSslTerm(ssl);
        if (pool >= 0) sceNetPoolDestroy(pool);
    }
};
struct Request {
    HttpApi& api;
    int tmpl = -1, conn = -1, req = -1;
    ~Request() {
        if (req >= 0) api.deleteRequest(req);
        if (conn >= 0) api.deleteConnection(conn);
        if (tmpl >= 0) api.deleteTemplate(tmpl);
    }
};
Network& network() { static Network instance; return instance; }
void setRequestPolicy(HttpApi& api, int id) {
    check(api.setAutoRedirect(id, 0), "Disable redirects");
    check(api.setRecvTimeout(id, 10000000), "Set receive timeout");
}
int32_t publicImageCertificate(int32_t, uint32_t, void* const[], int32_t, void*) {
    // Same ABI/return convention as the pinned SDK net_http sample. Limited to
    // public IGDB image requests; never used for catalogs or install packages.
    return 1;
}
}

namespace psgi {
HttpTransferInfo httpTransfer(const std::string& url, const HttpSink& sink, bool probeRange,
                             const std::atomic<bool>* cancelled) {
    if (!isHttpUrl(url)) throw std::runtime_error("Invalid package URL");
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    auto& net = network(); auto& api = net.api;
    Request r{api};
    r.tmpl = api.createTemplate(net.http, "PS4-Games-Installer/1.0", ORBIS_HTTP_VERSION_1_1, 0);
    check(r.tmpl, "Package HTTP template"); setRequestPolicy(api, r.tmpl);
    check(api.setConnectTimeout(r.tmpl, 10000000), "Connect timeout");
    check(api.setResolveTimeout(r.tmpl, 10000000), "DNS timeout");
    check(api.setSendTimeout(r.tmpl, 10000000), "Send timeout");
    r.conn = api.createConnection(r.tmpl, url.c_str(), false); check(r.conn, "Package HTTP connection");
    r.req = api.createRequest(r.conn, ORBIS_METHOD_GET, url.c_str(), 0); check(r.req, "Package HTTP request");
    if (probeRange) check(api.addHeader(r.req, "Range", "bytes=0-3", 0), "Set package range");
    check(api.send(r.req, nullptr, 0), "Send package request");
    HttpTransferInfo info;
    try {
        check(api.getStatus(r.req, &info.status), "Package HTTP status");
        if (info.status != 200 && !(probeRange && info.status == 206))
            throw std::runtime_error("Package HTTP " + std::to_string(info.status) + "; check the package URL on your server");
        int type = -1; size_t length = 0;
        check(api.contentLength(r.req, &type, &length), "Package length");
        if (type == ORBIS_HTTP_CONTENTLEN_EXIST) info.length = length;
        std::vector<char> block(64 * 1024);
        uint64_t nextWake = 0;
        for (;;) {
            if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
            const uint64_t now = sceKernelGetProcessTime();
            if (now >= nextWake) { sceSystemServicePowerTick(); nextWake = now + 10000000; }
            const int n = api.read(r.req, block.data(), block.size()); check(n, "Read package");
            if (!n) break;
            if (!sink(block.data(), n, info.length)) { api.abort(r.req); break; }
        }
    } catch (...) { api.abort(r.req); throw; }
    return info;
}
static std::string getDocument(const std::string& url, size_t limit, const std::atomic<bool>* cancelled, bool artwork) {
    if (!isHttpUrl(url)) throw std::runtime_error("Invalid catalog URL");
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    auto& net = network();
    auto& api = net.api;
    Request r{api};
    r.tmpl = api.createTemplate(net.http, "PS4-Games-Installer/1.0", ORBIS_HTTP_VERSION_1_1, 0);
    check(r.tmpl, "HTTP template");
    setRequestPolicy(api, r.tmpl);
    check(api.setConnectTimeout(r.tmpl, 10000000), "Connect timeout");
    check(api.setResolveTimeout(r.tmpl, 10000000), "DNS timeout");
    check(api.setSendTimeout(r.tmpl, 10000000), "Send timeout");
    // Old firmware CA/date checks can reject the current IGDB CDN certificate.
    // Scope compatibility to the exact public image origin/path; redirects stay
    // disabled, and supplied catalog/package HTTPS never uses this callback.
    constexpr char imageOrigin[]="https://images.igdb.com/igdb/image/upload/";
    if(artwork&&url.compare(0,sizeof(imageOrigin)-1,imageOrigin)==0)
        check(api.setSslCallback(r.tmpl,publicImageCertificate,nullptr),"Image HTTPS compatibility");
    r.conn = api.createConnection(r.tmpl, url.c_str(), false);
    check(r.conn, "HTTP connection");
    r.req = api.createRequest(r.conn, ORBIS_METHOD_GET, url.c_str(), 0);
    check(r.req, "HTTP request");
    check(api.send(r.req, nullptr, 0), "Send request");
    int status = 0;
    check(api.getStatus(r.req, &status), "HTTP status");
    if (status != 200) throw std::runtime_error("Catalog HTTP " + std::to_string(status) + "; use a direct URL");
    std::string out;
    std::vector<char> block(16 * 1024);
    const uint64_t deadline = sceKernelGetProcessTime() + 30000000;
    for (;;) {
        if (cancelled && *cancelled) { api.abort(r.req); throw std::runtime_error("Download cancelled"); }
        if (sceKernelGetProcessTime() > deadline) throw std::runtime_error("Catalog download timed out");
        int n = api.read(r.req, block.data(), block.size());
        check(n, "Read catalog");
        if (!n) break;
        if (static_cast<size_t>(n) > limit - out.size()) throw std::runtime_error("Download exceeds limit");
        out.append(block.data(), n);
    }
    return out;
}
std::string httpGet(const std::string& url, size_t limit, const std::atomic<bool>* cancelled) {
    return getDocument(url,limit,cancelled,false);
}
std::string httpGetArtwork(const std::string& url, size_t limit, const std::atomic<bool>* cancelled) {
    return getDocument(url,limit,cancelled,true);
}
}
#endif
