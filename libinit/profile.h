#pragma once

#include <string>

namespace meizu {
std::string BoardFromHandoff(const std::string& handoff);
std::string BoardFromCompatible(const std::string& compatible);
// The vendor profile a board runs: the M5 Note (m1621) has no profile of
// its own yet and runs the M681 one (same MT6755 Wingtech design, Android 7
// N0.MP7 blobs on both).
std::string ProfileForBoard(const std::string& board);
bool ActivateProfile(const std::string& vendor, const std::string& board,
                     std::string* error);
}
