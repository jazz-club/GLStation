#pragma once

#include <string>

#ifndef _WIN32
#include <termios.h>
#endif

namespace GLStation::UI {

bool isAnsiEnabled();
void enableAnsiIfPossible();
int visibleLength(const std::string &s);
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
