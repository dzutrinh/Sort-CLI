/*
 *	SORT-CLI.H
 *	----------
 *	Pulls the program together as one translation unit
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_H
#define SORTCLI_H

#if !defined(_WIN32)
#	ifndef _POSIX_C_SOURCE
#		define _POSIX_C_SOURCE 200809L	/* clock_gettime, nanosleep, sigaction under -std=c99 */
#	endif
#	define _DEFAULT_SOURCE				/* SIGWINCH and TIOCGWINSZ on Linux */
#	define _DARWIN_C_SOURCE				/* ... and on macOS */
#endif

#define APP_VERSION	"1.0.0"

#ifdef _WIN32
#	define APP_PLATFORM	"Windows"
#elif defined(__APPLE__)
#	define APP_PLATFORM	"macOS"
#elif defined(__linux__)
#	define APP_PLATFORM	"Linux"
#else
#	define APP_PLATFORM	"Unknown"
#endif

#include <time.h>
#include "term.h"
#include "text.h"
#include "gradient.h"
#include "data.h"
#include "trace.h"
#include "algs.h"
#include "view.h"
#include "ui.h"
#include "pet.h"
#include "cmds.h"

#endif
