#ifndef SEABOOT_H
#define SEABOOT_H

#include <stdbool.h>
#include <time.h>

#define EXIT_SUCCESS 0
#define EXIT_ERROR 3

#define BOOT(i) void i(void); __attribute__((constructor)) static void seaboot_init() { boot.init = &i; }
#define LOOP(i) void i(void); __attribute__((constructor)) static void seaboot_loop() { boot.loop = &i; }

#define IS_SIGNAL(v) (v >= 1 && v <= 31)

typedef unsigned long long int nstime_t;

typedef enum bootmode {
	STANDARD,
	LOOP,
	WAIT
} bootmode_t;

#define NUMBER_OF_EVENTS 33
typedef enum event {
	SIGHUP = 1,
	SIGINT = 2,
	SIGQUIT = 3,
	SIGILL = 4,
	SIGTRAP = 5,
	SIGABRT = 6,
	SIGIOT = 6,
	SIGBUS = 7,
	SIGFPE = 8,
	SIGKILL = 9,
	SIGUSR1 = 10,
	SIGSEGV = 11,
	SIGUSR2 = 12,
	SIGPIPE = 13,
	SIGALRM = 14,
	SIGTERM = 15,
	SIGSTKFLT = 16,
	SIGCHLD = 17,
	SIGCLD = 17,
	SIGCONT = 18,
	SIGSTOP = 19,
	SIGTSTP = 20,
	SIGTTIN = 21,
	SIGTTOU = 22,
	SIGURG = 23,
	SIGXCPU = 24,
	SIGXFSZ = 25,
	SIGVTALRM = 26,
	SIGPROF = 27,
	SIGWINCH = 28,
	SIGIO = 29,
	SIGPOLL = 29,
	SIGPWR = 30,
	SIGSYS = 31,
	
	SHUTDOWN = 0,
	LIBERROR = 32
} event_t;

typedef void (*eventListener_t)(event_t);
typedef void (*init_t)(void);
typedef void (*loop_t)(nstime_t);

extern struct boot {
	init_t init;
	loop_t loop;
	bool (*addEventListener)(event_t, eventListener_t);
	bool (*removeEventListener)(event_t, eventListener_t);
	bool (*enableSignal)(event_t);
	bool (*disableSignal)(event_t);

	nstime_t (*getRealTime)(void);
	nstime_t (*getRelativeTime)(void);
	nstime_t (*getProcessTime)(void);
	nstime_t (*getThreadTime)(void);
	nstime_t (*timer)(void (*)(void));

	timer_t (*createSignalTimer)(event_t);
	timer_t (*createThreadTimer)(void (*)(void));
	void (*startTimer)(timer_t, unsigned long);
	void (*startInterval)(timer_t, unsigned long);
	void (*stopTimer)(timer_t);
	void (*deleteTimer)(timer_t);

	void* (*allocate)(size_t);
	void* (*reallocate)(void*, size_t);

	bootmode_t mode;
	bool debug;

	const char* error;
} boot;

#endif
