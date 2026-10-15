#include "platform.hpp"
#include <stdexcept>
#include <cstdio>
#include <memory>

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
}
#else
#include "http_native.hpp"
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

namespace {
void check(int rc, const char* operation) {
    if (rc < 0) {
        char code[32];
        std::snprintf(code, sizeof(code), " (0x%08X)", static_cast<unsigned>(rc));
        throw std::runtime_error(std::string(operation) + code);
    }
}
struct Network {
    int pool = -1, ssl = -1, http = -1;
    Network() {
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET), "Load Net");
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NETCTL), "Load NetCtl");
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL), "Load SSL");
        check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP), "Load HTTP");
        check(sceNetInit(), "Net init");
        check(sceNetCtlInit(), "NetCtl init");
        pool = sceNetPoolCreate("psgi", 1024 * 1024, 0);
        check(pool, "Network pool");
        ssl = sceSslInit(1024 * 1024);
        if (ssl < 0) { sceNetPoolDestroy(pool); check(ssl, "SSL init"); }
        http = sceHttpInit(pool, ssl, 1024 * 1024);
        if (http < 0) { psgiSslTerm(ssl); sceNetPoolDestroy(pool); check(http, "HTTP init"); }
    }
    ~Network() {
        if (http >= 0) sceHttpTerm(http);
        if (ssl >= 0) psgiSslTerm(ssl);
        if (pool >= 0) sceNetPoolDestroy(pool);
    }
};
struct Request {
    int tmpl = -1, conn = -1, req = -1;
    ~Request() {
        if (req >= 0) sceHttpDeleteRequest(req);
        if (conn >= 0) sceHttpDeleteConnection(conn);
        if (tmpl >= 0) sceHttpDeleteTemplate(tmpl);
    }
};
void setRequestPolicy(int id) {
    check(psgiHttpSetAutoRedirect(id, 0), "Disable redirects");
    check(psgiHttpSetRecvTimeOut(id, 10000000), "Set receive timeout");
}
}

namespace psgi {
std::string httpGet(const std::string& url, size_t limit, const std::atomic<bool>* cancelled) {
    if (!isHttpUrl(url)) throw std::runtime_error("Invalid catalog URL");
    if (cancelled && *cancelled) throw std::runtime_error("Download cancelled");
    static Network net;
    Request r;
    r.tmpl = sceHttpCreateTemplate(net.http, "PS4-Games-Installer/1.0", ORBIS_HTTP_VERSION_1_1, 0);
    check(r.tmpl, "HTTP template");
    setRequestPolicy(r.tmpl);
    check(sceHttpSetConnectTimeOut(r.tmpl, 10000000), "Connect timeout");
    check(sceHttpSetResolveTimeOut(r.tmpl, 10000000), "DNS timeout");
    check(sceHttpSetSendTimeOut(r.tmpl, 10000000), "Send timeout");
    // Keep the platform's certificate verification enabled.
    r.conn = sceHttpCreateConnectionWithURL(r.tmpl, url.c_str(), false);
    check(r.conn, "HTTP connection");
    r.req = sceHttpCreateRequestWithURL(r.conn, ORBIS_METHOD_GET, url.c_str(), 0);
    check(r.req, "HTTP request");
    check(sceHttpSendRequest(r.req, nullptr, 0), "Send request");
    int status = 0;
    check(sceHttpGetStatusCode(r.req, &status), "HTTP status");
    if (status != 200) throw std::runtime_error("Catalog HTTP " + std::to_string(status) + "; use a direct URL");
    std::string out;
    char block[16384];
    const uint64_t deadline = sceKernelGetProcessTime() + 30000000;
    for (;;) {
        if (cancelled && *cancelled) { sceHttpAbortRequest(r.req); throw std::runtime_error("Download cancelled"); }
        if (sceKernelGetProcessTime() > deadline) throw std::runtime_error("Catalog download timed out");
        int n = sceHttpReadData(r.req, block, sizeof(block));
        check(n, "Read catalog");
        if (!n) break;
        if (static_cast<size_t>(n) > limit - out.size()) throw std::runtime_error("Download exceeds limit");
        out.append(block, n);
    }
    return out;
}
}
#endif
