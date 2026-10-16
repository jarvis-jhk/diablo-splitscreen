#pragma once

#include <string_view>

namespace devilution {

/**
 * @brief Sends an anonymous fault report (fork version, platform, category).
 *
 * Fire and forget: a helper process posts it, so a slow or missing network can
 * never delay the game. Nothing personal is sent: no message text, paths, save
 * data or device identifiers. Only builds made with DEVILUTIONX_FAULT_REPORTS
 * report; DEVILUTIONX_NO_REPORT=1 turns it off.
 */
void ReportFault(std::string_view category);

} // namespace devilution
