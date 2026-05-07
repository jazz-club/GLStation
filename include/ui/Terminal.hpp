#pragma once

#include <string>

#ifndef _WIN32
#include <termios.h>
#endif

namespace GLStation::UI {

bool isAnsiEnabled();
void enableAnsiIfPossible();
int visibleLength(const std::string &s);
std::string truncateVisible(const std::string &s, int maxVisible);
bool shouldStopRun();

struct ScopedRawStdin {
#ifndef _WIN32
	bool active = false;
	bool isTty = false;
	int oldFlags = -1;
	termios oldTio{};

	ScopedRawStdin();
	~ScopedRawStdin();
#endif
};

} // namespace GLStation::UI
