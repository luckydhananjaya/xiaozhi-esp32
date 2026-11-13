#pragma once

#include <string>

namespace KawanVideo {
void Start(const std::string& url);
void Stop();
bool Playing();
std::string Url();
}  // namespace KawanVideo
