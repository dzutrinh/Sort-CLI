/*
 *	TERM.H
 *	------
 *	Terminal setup, raw keyboard input with timeouts, window size, output
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_TERM_H
#define SORTCLI_TERM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>

#ifdef _WIN32
#	include <windows.h>
#	include <conio.h>
#	include <io.h>
#	ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#		define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#	endif
#	define TERM_ISATTY(fd)	_isatty(fd)
#else
#	include <termios.h>
#	include <unistd.h>
#	include <poll.h>
#	include <time.h>
#	include <sys/ioctl.h>
#	define TERM_ISATTY(fd)	isatty(fd)
#endif

/* key codes returned by term_key(), outside the byte range */
enum {
	K_EOF = -1,
	K_TIMEOUT = 0x200, K_NONE, K_UP, K_DOWN, K_LEFT, K_RIGHT,
	K_HOME, K_END, K_DEL, K_PGUP, K_PGDN, K_SHIFT_TAB, K_ESC
};

#define K_CTRL(c)	((c) & 0x1F)
#define K_ENTER		13
#define K_TAB		9
#define K_BACKSPACE	127

static bool g_interactive = false;	/* stdin and stdout are both terminals */
static bool g_color = true;			/* emit SGR color sequences */
static bool g_raw = false;			/* terminal is in raw mode */

#ifdef _WIN32
static HANDLE g_hin, g_hout;
static DWORD g_in_mode, g_out_mode;
#else
static struct termios g_saved;
#endif

static long long now_ms(void) {
#ifdef _WIN32
	return (long long)GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* Window size; non-terminal output gets a fixed, roomy size */
static void term_size(int * cols, int * rows) {
	int c = 0, r = 0;
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO info;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
		c = info.srWindow.Right - info.srWindow.Left + 1;
		r = info.srWindow.Bottom - info.srWindow.Top + 1;
	}
#else
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
		c = ws.ws_col;
		r = ws.ws_row;
	}
#endif
	if (c <= 0) { const char * e = getenv("COLUMNS"); c = e ? atoi(e) : 0; }
	if (r <= 0) { const char * e = getenv("LINES");   r = e ? atoi(e) : 0; }
	if (c <= 0) c = 100;
	if (r <= 0) r = 32;
	if (c > 400) c = 400;
	if (r > 200) r = 200;
	*cols = c;
	*rows = r;
}

static void term_restore(void) {
	if (!g_raw) return;
	fputs("\x1b[?25h\x1b[0m", stdout);
	fflush(stdout);
#ifdef _WIN32
	SetConsoleMode(g_hin, g_in_mode);
	SetConsoleMode(g_hout, g_out_mode);
#else
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved);
#endif
	g_raw = false;
}

static void on_fatal_signal(int s) {
	term_restore();
	signal(s, SIG_DFL);
	raise(s);
}

#ifndef _WIN32
static void on_winch(int s) { (void)s; }	/* only wakes poll() up */
#endif

/* Switch to raw input when both ends are terminals; false on hard failure */
static bool term_start(void) {
	setlocale(LC_ALL, "");
	g_interactive = TERM_ISATTY(0) && TERM_ISATTY(1);
	if (!g_interactive) return true;
#ifdef _WIN32
	g_hout = GetStdHandle(STD_OUTPUT_HANDLE);
	g_hin = GetStdHandle(STD_INPUT_HANDLE);
	if (!GetConsoleMode(g_hout, &g_out_mode) || !GetConsoleMode(g_hin, &g_in_mode)) {
		g_interactive = false;
		return true;
	}
	if (!SetConsoleMode(g_hout, g_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
		return false;			/* no VT support: Windows 10 1511 or later needed */
	SetConsoleMode(g_hin, g_in_mode & ~(ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
	SetConsoleOutputCP(CP_UTF8);
#else
	if (tcgetattr(STDIN_FILENO, &g_saved) == -1) {
		g_interactive = false;
		return true;
	}
	struct termios t = g_saved;
	t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	t.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);	/* Ctrl-C arrives as a key */
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == -1) {
		g_interactive = false;
		return true;
	}
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_winch;		/* no SA_RESTART: a resize wakes the key wait */
	sigemptyset(&sa.sa_mask);
	sigaction(SIGWINCH, &sa, NULL);
	signal(SIGHUP, on_fatal_signal);
#endif
	signal(SIGTERM, on_fatal_signal);
	g_raw = true;
	atexit(term_restore);
	fputs("\x1b[?25l", stdout);		/* the prompt draws its own cursor */
	fflush(stdout);
	return true;
}

#ifndef _WIN32
/* One byte within ms milliseconds (-1 waits forever) */
static int read_byte(int ms) {
	struct pollfd p = { STDIN_FILENO, POLLIN, 0 };
	int r = poll(&p, 1, ms);
	if (r == 0) return K_TIMEOUT;
	if (r < 0) return errno == EINTR ? K_TIMEOUT : K_EOF;
	unsigned char c;
	ssize_t n = read(STDIN_FILENO, &c, 1);
	if (n == 1) return c;
	if (n < 0 && errno == EINTR) return K_TIMEOUT;
	return K_EOF;
}
#endif

/* Next key within ms milliseconds: a byte, a K_* code, K_TIMEOUT or K_EOF */
static int term_key(int ms) {
#ifdef _WIN32
	long long end = now_ms() + ms;
	while (!_kbhit()) {
		if (ms >= 0 && now_ms() >= end) return K_TIMEOUT;
		Sleep(10);
	}
	int c = _getch();
	if (c == 0 || c == 224) {
		switch (_getch()) {
			case 72: return K_UP;
			case 80: return K_DOWN;
			case 75: return K_LEFT;
			case 77: return K_RIGHT;
			case 71: return K_HOME;
			case 79: return K_END;
			case 83: return K_DEL;
			case 73: return K_PGUP;
			case 81: return K_PGDN;
			case 15: return K_SHIFT_TAB;
		}
		return K_NONE;
	}
	if (c == 27) return K_ESC;
	if (c == 8) return K_BACKSPACE;
	return c;
#else
	int c = read_byte(ms);
	if (c != 27) return c;
	int d = read_byte(30);
	if (d == K_TIMEOUT || d == K_EOF) return K_ESC;	/* a lone Esc */
	if (d != '[' && d != 'O') return K_NONE;		/* Alt+key */
	char num[16];
	int len = 0, e;
	while (1) {
		e = read_byte(30);
		if (e < 0 || e > 0xFF) return K_NONE;
		if (e >= 0x40 && e <= 0x7E) break;
		if (len < 15) num[len++] = (char)e;
	}
	num[len] = 0;
	switch (e) {
		case 'A': return K_UP;
		case 'B': return K_DOWN;
		case 'C': return K_RIGHT;
		case 'D': return K_LEFT;
		case 'H': return K_HOME;
		case 'F': return K_END;
		case 'Z': return K_SHIFT_TAB;
		case '~':
			switch (atoi(num)) {
				case 1: case 7: return K_HOME;
				case 4: case 8: return K_END;
				case 3: return K_DEL;
				case 5: return K_PGUP;
				case 6: return K_PGDN;
			}
	}
	return K_NONE;
#endif
}

/* Write a block of output in one go, dropping colors when they are off */
static void term_write(const char * s, size_t n) {
	if (g_color) {
		fwrite(s, 1, n, stdout);
	} else {						/* strip SGR sequences, keep cursor control */
		size_t i = 0;
		while (i < n) {
			if (s[i] == 27 && i + 1 < n && s[i + 1] == '[') {
				size_t j = i + 2;
				while (j < n && !(s[j] >= 0x40 && s[j] <= 0x7E)) j++;
				if (j < n && s[j] == 'm') { i = j + 1; continue; }
			}
			fputc(s[i++], stdout);
		}
	}
	fflush(stdout);
}

#endif
