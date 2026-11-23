#pragma once

#include <string>

// Renders GitHub-flavoured Markdown to an HTML fragment (body contents only).
// Heading tags get sequential ids ("h-1", "h-2", ...) so the outline can link
// to them, and relative link/image URLs are rewritten to https://doc.local/...
std::string RenderMarkdown(const std::string& markdownUtf8);
