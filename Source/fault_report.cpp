#include "fault_report.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

#ifdef DEVILUTIONX_FAULT_REPORTS
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>
#endif
#endif

namespace devilution {

#ifdef DEVILUTIONX_FAULT_REPORTS
namespace {

// A public topic: anyone may post, the fault-reports workflow turns posts into
// GitHub issues. The maintainer's private endpoint never appears here.
constexpr const char *FaultTopicUrl = "https://ntfy.sh/diablo-splitscreen-faults-747d46c7c1c4de46";

#ifndef DIABLO_SPLITSCREEN_VERSION
#define DIABLO_SPLITSCREEN_VERSION "dev"
#endif

} // namespace
#endif

void ReportFault(std::string_view category)
{
#ifdef DEVILUTIONX_FAULT_REPORTS
	static bool sent = false;
	const char *optOut = std::getenv("DEVILUTIONX_NO_REPORT");
	if (sent || (optOut != nullptr && std::strcmp(optOut, "1") == 0))
		return;
	sent = true;
#ifdef _WIN32
	const std::string message = std::string("Diablo Splitscreen " DIABLO_SPLITSCREEN_VERSION " Windows ") + std::string(category);
	std::string command = std::string("curl.exe -fsS --max-time 8 -o NUL -d \"") + message + "\" " + FaultTopicUrl;
	STARTUPINFOA start {};
	PROCESS_INFORMATION process {};
	start.cb = sizeof(start);
	if (CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &start, &process)) {
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
	}
#elif defined(__linux__)
	const std::string message = std::string("Diablo Splitscreen " DIABLO_SPLITSCREEN_VERSION " Linux ") + std::string(category);
	// Double fork: the helper outlives us without becoming a zombie.
	const pid_t child = fork();
	if (child == 0) {
		if (fork() == 0) {
			execlp("curl", "curl", "-fsS", "--max-time", "8", "-o", "/dev/null", "-d", message.c_str(), FaultTopicUrl, static_cast<char *>(nullptr));
		}
		_exit(0);
	}
	if (child > 0)
		waitpid(child, nullptr, 0);
#else
	(void)category;
#endif
#else
	(void)category;
#endif
}

} // namespace devilution
