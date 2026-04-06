#include "io/InputHandler.hpp"
#include <cctype>
#include <cstdio>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
static int custom_getch() {
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hIn, &mode);
	SetConsoleMode(hIn, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
	unsigned char c = 0;
	DWORD read;
	ReadConsoleA(hIn, &c, 1, &read, NULL);
	SetConsoleMode(hIn, mode);
	return c;
}
#else
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
static int custom_getch() {
	struct termios oldt, newt;
	int ch;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}
#endif

namespace GLStation::Util {

static bool isAsciiSpace(unsigned char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static size_t utf8SeqLength(unsigned char c) {
	if (c < 128)
		return 1;
	if (c >= 194 && c <= 223)
		return 2;
	if (c >= 224 && c <= 239)
		return 3;
	if (c >= 240 && c <= 244)
		return 4;
	return 1;
}

std::string InputHandler::trim(const std::string &s) {
	size_t start = 0;
	while (start < s.size() &&
		   isAsciiSpace(static_cast<unsigned char>(s[start])))
		++start;
	size_t end = s.size();
	while (end > start && isAsciiSpace(static_cast<unsigned char>(s[end - 1])))
		--end;
	return (start < end) ? s.substr(start, end - start) : std::string();
}

std::string InputHandler::normaliseForComparison(const std::string &s) {
	std::string trimmed = trim(s);
	std::string out;
	out.reserve(trimmed.size());
	for (size_t i = 0; i < trimmed.size();) {
		unsigned char c = static_cast<unsigned char>(trimmed[i]);
		if (c < 128) {
			out += static_cast<char>(std::tolower(c));
			++i;
		} else {
			size_t seqLen = utf8SeqLength(c);
			for (size_t j = 0; j < seqLen && i + j < trimmed.size(); ++j)
				out += trimmed[i + j];
			i += seqLen;
		}
	}
	return out;
}

std::string InputHandler::urlEncode(const std::string &s) {
	std::string encoded;
	encoded.reserve(s.size() * 3);
	for (unsigned char c : s) {
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			encoded += static_cast<char>(c);
		else if (c == ' ')
			encoded += "%20";
		else {
			char buf[4];
			std::snprintf(buf, sizeof(buf), "%%%02X", c);
			encoded += buf;
		}
	}
	return encoded;
}

std::string InputHandler::readLineWithHistory() {
	static std::vector<std::string> history;
	static size_t historyPos = 0;
	std::string currentLine = "";
	size_t cursor = 0;
	historyPos = history.size();

	while (true) {
		int ch = custom_getch();

		if (ch == 13 || ch == 10) {
			std::cout << "\n";
			if (!currentLine.empty()) {
				if (history.empty() || history.back() != currentLine) {
					history.push_back(currentLine);
				}
			}
			historyPos = history.size();
			return currentLine;
		} else if (ch == 8 || ch == 127) {
			if (cursor > 0) {
				currentLine.erase(cursor - 1, 1);
				cursor--;
				std::cout << "\b";
				for (size_t i = cursor; i < currentLine.size(); ++i)
					std::cout << currentLine[i];
				std::cout << " \b";
				for (size_t i = cursor; i < currentLine.size(); ++i)
					std::cout << "\b";
			}
		} else if (ch == 224 || ch == 0) {
			int sc = custom_getch();
			if (sc == 72)
				ch = 1000; // Up
			else if (sc == 80)
				ch = 1001; // Down
			else if (sc == 75)
				ch = 1002; // Left
			else if (sc == 77)
				ch = 1003; // Right
		} else if (ch == 27) { // posix esc
			int n1 = custom_getch();
			if (n1 == 91) { // '['
				int n2 = custom_getch();
				if (n2 == 'A')
					ch = 1000; // Up
				else if (n2 == 'B')
					ch = 1001; // Down
				else if (n2 == 'D')
					ch = 1002; // Left
				else if (n2 == 'C')
					ch = 1003; // Right
			}
		}

		if (ch == 1000) { // Up
			if (!history.empty() && historyPos > 0) {
				if (historyPos == history.size()) {
					history.push_back(currentLine);
				}
				historyPos--;
				while (cursor > 0) {
					std::cout << "\b";
					cursor--;
				}
				for (size_t i = 0; i < currentLine.size(); ++i)
					std::cout << " ";
				for (size_t i = 0; i < currentLine.size(); ++i)
					std::cout << "\b";

				currentLine = history[historyPos];
				std::cout << currentLine;
				cursor = currentLine.size();
			}
		} else if (ch == 1001) { // Down
			if (historyPos < history.size() &&
				historyPos + 1 < history.size()) {
				historyPos++;
				while (cursor > 0) {
					std::cout << "\b";
					cursor--;
				}
				for (size_t i = 0; i < currentLine.size(); ++i)
					std::cout << " ";
				for (size_t i = 0; i < currentLine.size(); ++i)
					std::cout << "\b";

				currentLine = history[historyPos];
				std::cout << currentLine;
				cursor = currentLine.size();
			} else if (historyPos + 1 == history.size() && history.size() > 0) {
				historyPos++;
				while (cursor > 0) {
					std::cout << "\b";
					cursor--;
				}
				for (size_t i = 0; i < currentLine.size(); ++i)
					std::cout << " ";
				for (size_t i = 0; i < currentLine.size(); ++i)
					std::cout << "\b";
				currentLine = history.back();
				history.pop_back();
				std::cout << currentLine;
				cursor = currentLine.size();
			}
		} else if (ch == 1002) { // Left
			if (cursor > 0) {
				std::cout << "\b";
				cursor--;
			}
		} else if (ch == 1003) { // Right
			if (cursor < currentLine.size()) {
				std::cout << currentLine[cursor];
				cursor++;
			}
		} else if (ch >= 32 && ch <= 126) { // asciis
			currentLine.insert(cursor, 1, static_cast<char>(ch));
			std::cout << static_cast<char>(ch);
			for (size_t i = cursor + 1; i < currentLine.size(); ++i) {
				std::cout << currentLine[i];
			}
			for (size_t i = cursor + 1; i < currentLine.size(); ++i) {
				std::cout << "\b";
			}
			cursor++;
		}

		std::cout << std::flush;
	}
}

} // namespace GLStation::Util
