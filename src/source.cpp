#include "source.hpp"
#include "library.hpp"
#include "sfo.hpp"
#include <stdexcept>

namespace psgi {
SourceReport inspectSource(const Source& source, const std::string& document) {
    if (!isHttpUrl(source.url)) throw std::runtime_error("Use a full http:// or https:// games JSON URL");
    const auto start = document.find_first_not_of(" \t\r\n");
    if (start != std::string::npos && document[start] == '<')
        throw std::runtime_error("The server returned a web page. Enter the JSON catalog endpoint, rather than its home page.");
    const auto entries = parseCatalog(document, source.category);
    SourceReport report; report.success = true; report.source = source; report.packages = entries.size();
    for (const auto& e : entries) {
        const auto kind = packageKind(e);
        if (kind == 0) ++report.games; else if (kind == 1) ++report.updates; else if (kind == 2) ++report.dlc;
        if (!validTitleId(e.titleId)) ++report.missingIds;
    }
    report.message = entries.empty() ? "Valid catalog, but no packages are listed." : "Valid catalog: " + std::to_string(entries.size()) + " packages.";
    if (report.missingIds) report.message += " " + std::to_string(report.missingIds) + " missing valid Title IDs; grouping and personal records need them.";
    return report;
}
std::string sourceFailure(const Source& source, const std::string& error) {
    return source.category + ": " + error + " Check the JSON endpoint, server address/port and PS4 network connection.";
}
}
