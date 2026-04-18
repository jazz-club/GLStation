#include "ui/Terminal.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#endif

namespace GLStation::UI {

const char *ANSI_RESET = "\033[0m";
const char *ANSI_GREEN = "\033[32m";
const char *ANSI_YELLOW = "\033[33m";
const char *ANSI_RED = "\033[31m";
const char *ANSI_CYAN = "\033[36m";

// we dont really need to bother with the colour stuff forever but still
static bool s_ansiEnabled = false;

bool isAnsiEnabled() { return s_ansiEnabled; }

/*
		Temporary TUI bandaid, theres a nicer way to do this while preventing dashboard flickering
*/
int visibleLength(const std::string &s) {
	int n = 0;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
			size_t j = i + 2;
			while (j < s.size() && (s[j] < 0x40 || s[j] > 0x7E))
				++j;
			i = j;
		} else
			++n;
	}
	return n;
}

void enableAnsiIfPossible() {
	if (s_ansiEnabled)
		return;
#ifdef _WIN32
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut != INVALID_HANDLE_VALUE) {
		DWORD mode = 0;
		if (GetConsoleMode(hOut, &mode)) {
			mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			if (SetConsoleMode(hOut, mode))
				s_ansiEnabled = true;
		}
	}
#else
	s_ansiEnabled = true;
#endif
}

#ifndef _WIN32
ScopedRawStdin::ScopedRawStdin() {
	isTty = ::isatty(STDIN_FILENO) == 1;
	if (!isTty)
		return;

	if (::tcgetattr(STDIN_FILENO, &oldTio) != 0)
		return;

	termios rawTio = oldTio;
	rawTio.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
	rawTio.c_cc[VMIN] = 0;
	rawTio.c_cc[VTIME] = 0;

	if (::tcsetattr(STDIN_FILENO, TCSANOW, &rawTio) != 0)
		return;

	oldFlags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
	if (oldFlags != -1)
		(void)::fcntl(STDIN_FILENO, F_SETFL, oldFlags | O_NONBLOCK);

	active = true;
}

ScopedRawStdin::~ScopedRawStdin() {
	if (!active)
		return;
	(void)::tcsetattr(STDIN_FILENO, TCSANOW, &oldTio);
	if (oldFlags != -1)
		(void)::fcntl(STDIN_FILENO, F_SETFL, oldFlags);
}
#endif

/*
	im not familiar with how CLI utilities usually do this, ill have to look at some OSS stuff
	this works for now but it is RANCID
*/
bool shouldStopRun() {
#ifdef _WIN32
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hIn == INVALID_HANDLE_VALUE)
		return false;

	INPUT_RECORD rec;
	DWORD n = 0;
	if (PeekConsoleInput(hIn, &rec, 1, &n) && n > 0) {
		if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
			ReadConsoleInput(hIn, &rec, 1, &n);
			char ch = rec.Event.KeyEvent.uChar.AsciiChar;
			return ch == 'x' || ch == 'X';
		}
		ReadConsoleInput(hIn, &rec, 1, &n);
	}
#else
	if (::isatty(STDIN_FILENO) != 1)
		return false;

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(STDIN_FILENO, &rfds);
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	int ready = ::select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
	if (ready <= 0)
		return false;

	unsigned char ch = 0;
	ssize_t n = ::read(STDIN_FILENO, &ch, 1);
	if (n == 1)
		return ch == 'x' || ch == 'X';
	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return false;
	return false;
#endif
	return false;
}

} // namespace GLStation::UI
