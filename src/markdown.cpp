// SPDX-License-Identifier: MIT
#include "markdown.h"

#include "md4c-html.h"
#include "md4c.h"

#include <string>

namespace {

void AppendOutput(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    static_cast<std::string*>(userdata)->append(text, static_cast<size_t>(size));
}

// md4c-html emits headings as bare "<h1>" ... "<h6>" tags. Insert a stable id
// into each opening tag, in document order.
std::string InjectHeadingIds(const std::string& html) {
    std::string out;
    out.reserve(html.size() + 64);
    int counter = 0;
    size_t i = 0;
    while (i < html.size()) {
        if (i + 3 < html.size() && html[i] == '<' && html[i + 1] == 'h' &&
            html[i + 2] >= '1' && html[i + 2] <= '6' && html[i + 3] == '>') {
            ++counter;
            out += "<h";
            out += html[i + 2];
            out += " id=\"h-";
            out += std::to_string(counter);
            out += "\">";
            i += 4;
            continue;
        }
        out += html[i++];
    }
    return out;
}

bool IsRelativeUrl(const std::string& value) {
    if (value.empty()) return false;
    if (value[0] == '#') return false;
    if (value.size() >= 2 && value[0] == '/' && value[1] == '/') return false;  // protocol-relative
    if (value[0] == '/') return true;  // root-relative, resolved against the document folder
    // A scheme is present if there is a ':' before the first '/'.
    size_t colon = value.find(':');
    size_t slash = value.find('/');
    if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
        return false;
    }
    return true;
}

std::string RewriteUrl(const std::string& value) {
    if (!IsRelativeUrl(value)) return value;
    std::string path = value;
    while (!path.empty() && path[0] == '/') path.erase(0, 1);
    while (path.size() >= 2 && path[0] == '.' && path[1] == '/') path.erase(0, 2);
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    return "https://doc.local/" + path;
}

// Rewrite relative src="..." / href="..." values so images and sibling files
// resolve through the doc.local folder mapping.
std::string RewriteRelativeUrls(const std::string& html) {
    std::string out;
    out.reserve(html.size() + 64);
    size_t i = 0;
    while (i < html.size()) {
        bool isHref = html.compare(i, 6, "href=\"") == 0;
        bool isSrc = !isHref && html.compare(i, 5, "src=\"") == 0;
        if (isHref || isSrc) {
            size_t prefixLen = isHref ? 6 : 5;
            size_t valueStart = i + prefixLen;
            size_t valueEnd = html.find('"', valueStart);
            if (valueEnd == std::string::npos) {
                out.append(html, i, std::string::npos);
                break;
            }
            std::string value = html.substr(valueStart, valueEnd - valueStart);
            out.append(html, i, prefixLen);
            out += RewriteUrl(value);
            out += '"';
            i = valueEnd + 1;
            continue;
        }
        out += html[i++];
    }
    return out;
}

}  // namespace

std::string RenderMarkdown(const std::string& markdownUtf8) {
    std::string html;
    int rc = md_html(markdownUtf8.data(), static_cast<MD_SIZE>(markdownUtf8.size()),
                     AppendOutput, &html, MD_DIALECT_GITHUB, 0);
    if (rc != 0) {
        return "<div class=\"error\"><h2>Could not parse this file</h2>"
               "<p>The Markdown parser reported an error.</p></div>";
    }
    html = InjectHeadingIds(html);
    html = RewriteRelativeUrls(html);
    return html;
}
