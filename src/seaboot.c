#include <config.h>
#include "seaboot.h"

#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

static const char* getEventName(event_t event) {
	if (IS_SIGNAL(event))
		return strsignal(event);
	switch(event) {
		case SHUTDOWN: return "Shutdown"; break;
		case LIBERROR: return "Lib-Error"; break;
		default: return "Unknown"; break;
	}
}

static bool addEventListener(event_t, eventListener_t);
static bool removeEventListener(event_t, eventListener_t);
static bool enableSignal(event_t);
static bool disableSignal(event_t);

static nstime_t getRealTime(void);
static nstime_t getRelativeTime(void);
static nstime_t getProcessTime(void);
static nstime_t getThreadTime(void);
static nstime_t timer(void (*)(void));

static timer_t createSignalTimer(event_t);
static timer_t createThreadTimer(void (*)(void));
static void startTimer(timer_t, unsigned long);
static void startInterval(timer_t, unsigned long);
static void stopTimer(timer_t);
static void deleteTimer(timer_t);

static void eventHandler(event_t);

struct boot boot = {
	.addEventListener = addEventListener,
	.removeEventListener = removeEventListener,
	.enableSignal = enableSignal,
	.disableSignal = disableSignal,

	.getRealTime = getRealTime,
	.getRelativeTime = getRelativeTime,
	.getProcessTime = getProcessTime,
	.getThreadTime = getThreadTime,
	.timer = timer,

	.createSignalTimer = createSignalTimer,
	.createThreadTimer = createThreadTimer,
	.startTimer = startTimer,
	.startInterval = startInterval,
	.stopTimer = stopTimer,
	.deleteTimer = deleteTimer,

	.mode = STANDARD,
	.debug = false
};

struct listener {
	int number;
	eventListener_t* listeners;
	bool override;

	bool isSignalHandler;
	struct sigaction defaultHandler;
	struct sigaction handler;
};

struct listener events[NUMBER_OF_EVENTS];

static void debug(const char* format, ...) {
	if (!boot.debug)
		return;

	va_list arguments;
	
	int done;

	va_start(arguments, format);

	fprintf(stderr, "[seaboot] ");
	done = vfprintf(stderr, format, arguments);

	va_end(arguments);
}

bool addEventListener(event_t event, eventListener_t listener) {
	if (event >= NUMBER_OF_EVENTS) {
		boot.error = "No such event (addEventListener).";
		eventHandler(LIBERROR);
		return false;
	}
	if (events[event].override) {
		events[event].override = false;
		events[event].number = 0;
	}
	events[event].listeners = realloc(events[event].listeners, (events[event].number + 1) * sizeof(eventListener_t)); // TODO replace with own function
	events[event].listeners[events[event].number] = listener;
	debug("New event listener for event %d (%s) on position %d.\n", event, getEventName(event), events[event].number);
	events[event].number++;
}

bool removeEventListener(event_t event, eventListener_t listener) {
	if (event >= NUMBER_OF_EVENTS) {
		boot.error = "No such event (removeEventListener).";
		eventHandler(LIBERROR);
		return false;
	}
	// TODO
} 

bool enableSignal(event_t signal) {
	if (!IS_SIGNAL(signal)) {
		boot.error = "Not a signal (enableSignal).";
		eventHandler(LIBERROR);
		return false;
	}
	if (sigaction(signal, &(events[signal].handler), NULL) < 0) {
		boot.error = "Setting signal failed (sigaction).";
		eventHandler(LIBERROR);
		return false;
	}
	debug("Signal %d (%s) is enabled.\n", signal, getEventName(signal));
	events[signal].isSignalHandler = true;
	return true;
} 

bool disableSignal(event_t signal) {
	if (!IS_SIGNAL(signal)) {
		boot.error = "Not a signal (enableSignal).";
		eventHandler(LIBERROR);
		return false;
	}
	if (sigaction(signal, &(events[signal].defaultHandler), NULL) < 0) {
		boot.error = "Setting signal failed (sigaction).";
		eventHandler(LIBERROR);
		return false;
	}
	debug("Signal %d (%s) is disabled.\n", signal, getEventName(signal));
	events[signal].isSignalHandler = false;
	return true;
} 

void eventHandler(event_t event) {
	debug("Event handler for event %d (%s).\n", event, getEventName(event));
	debug("Event has %d registered handler(s).\n", events[event].number);
	for (int i = 0; i < events[event].number; i++) {
		if (events[event].listeners[i] == NULL)
			continue;
		debug("Found handler %d.\n", i);
		events[event].listeners[i](event);
	}
}

nstime_t getRealTime() {
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);
	return time.tv_sec * 1000000000 + time.tv_nsec;
}

nstime_t getRelativeTime() {
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return time.tv_sec * 1000000000 + time.tv_nsec;
}

nstime_t getProcessTime() {
	struct timespec time;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
	return time.tv_sec * 1000000000 + time.tv_nsec;
}

nstime_t getThreadTime() {
	struct timespec time;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time);
	return time.tv_sec * 1000000000 + time.tv_nsec;
}

nstime_t timer(void (*function)(void)) {
	nstime_t start = getRelativeTime();
	function();
	nstime_t end = getRelativeTime();
	return end - start;
}

void timerHandler(union sigval target) {
	debug("Timer handler: Starting target.\n");
	((void (*)(void))(target.sival_ptr))();
}

timer_t createSignalTimer(event_t signal) {
	debug("Creating signal timer (%d)\n", signal);

	struct sigevent sevp;
	sevp.sigev_notify = SIGEV_SIGNAL;
	sevp.sigev_signo = signal;
	
	timer_t timer;

	if (timer_create(CLOCK_BOOTTIME, &sevp, &timer) < 0) {
		boot.error = strerror(errno);
		eventHandler(LIBERROR);

		timer = NULL;
	}
	
	return timer;
}

timer_t createThreadTimer(void (*handler)(void)) {
	debug("Creating thread timer\n");

	struct sigevent sevp;
	sevp.sigev_notify = SIGEV_THREAD;
	sevp.sigev_notify_function = timerHandler;
	sevp.sigev_value.sival_ptr = handler;

	timer_t timer;

	if (timer_create(CLOCK_BOOTTIME, &sevp, &timer) < 0) {
		boot.error = strerror(errno);
		eventHandler(LIBERROR);
		timer = NULL;
	}
	return timer;
}

void startTimer(timer_t timer, unsigned long ms) {
	debug("Starting timer %x (%d ms)\n", timer, ms);

	struct itimerspec time, old;

	time.it_value.tv_sec = ms / 1000;
	time.it_value.tv_nsec = ((ms % 1000) * 1000000);
	time.it_interval.tv_sec = 0;
	time.it_interval.tv_nsec = 0;

	if (timer_settime(timer, 0, &time, &old) < 0) {
		boot.error = strerror(errno);
		eventHandler(LIBERROR);
	}
}

void startInterval(timer_t timer, unsigned long ms) {
	debug("Starting timer (interval) %x (%d ms)\n", timer, ms);

	struct itimerspec time, old;

	time.it_value.tv_sec = ms / 1000;
	time.it_value.tv_nsec = ((ms % 1000) * 1000000);
	time.it_interval.tv_sec = ms / 1000;
	time.it_interval.tv_nsec = ((ms % 1000) * 1000000);

	if (timer_settime(timer, 0, &time, &old) < 0) {
		boot.error = strerror(errno);
		eventHandler(LIBERROR);
	}
}

void stopTimer(timer_t timer) {
	debug("Stoping timer %x\n", timer);

	struct itimerspec time, old;

	time.it_value.tv_sec = 0;
	time.it_value.tv_nsec = 0;
	time.it_interval.tv_sec = 0;
	time.it_interval.tv_nsec = 0;

	if (timer_settime(timer, 0, &time, &old) < 0) {
		boot.error = strerror(errno);
		eventHandler(LIBERROR);
	}
}

void deleteTimer(timer_t timer) {
	debug("Deleting timer %x\n", timer);
	if (timer_delete(timer) < 0) {
		boot.error = strerror(errno);
		eventHandler(LIBERROR);
	}
}

static void signalHandler(int signal) {	
	debug("Got signal %d (%s). Involking event handler.\n", signal, getEventName(signal));
	eventHandler(signal);
}

static void exitHandler(void) {
	eventHandler(SHUTDOWN);
}

static void errorHandler(event_t event) {
	fprintf(stderr, "\n[seaboot] Error: %s\n", boot.error);
	fprintf(stderr, "[seaboot] No error handler given.\n");
	fprintf(stderr, "[seaboot] Shuting down.\n");
	exit(EXIT_ERROR);
}

int main(char** argv, int argc) {
	for(int i = 0; i < NUMBER_OF_EVENTS; i++) {		
		debug("Setup %s %d (%s)...\n", IS_SIGNAL(i) ? "signal" : "event", i, getEventName(i));
		events[i].number = 0;
		events[i].listeners = malloc(1 * sizeof(eventListener_t)); // TODO replace by own function
		events[i].listeners[0] = NULL;
		events[i].override = false;
		events[i].isSignalHandler = false;
		if (IS_SIGNAL(i)) {
			sigaction(i, NULL, &events[i].defaultHandler);
			
		boot.error = "Setting exit handler failed (atexit).";
		eventHandler(LIBERROR);
			events[i].handler.sa_handler = &signalHandler;
			sigemptyset(&(events[i].handler.sa_mask));
			
		}
	}

	debug("Setup default error handler.\n");
	addEventListener(LIBERROR, errorHandler);
	events[LIBERROR].override = true;

	debug("Setup exit handler.\n");
	if (atexit(exitHandler) != 0) {
		boot.error = "Setting exit handler failed (atexit).";
		eventHandler(LIBERROR);
	}


	// start init
	if (boot.init == NULL) {
		boot.error = "No init handler given (main).";
		eventHandler(LIBERROR);
	} else {
		boot.init();
	}

	if (boot.mode == LOOP) {
		if (boot.loop == NULL) {
			boot.error = "No loop handler given (loop).";
			eventHandler(LIBERROR);
		} else {
			nstime_t last = getRelativeTime();
			nstime_t current;
			while(true) {
				current = getRelativeTime(); 
				boot.loop(current - last);
				last = current;
			}
		}
	} else if (boot.mode == WAIT) {
		while(true)
			sleep(60);
	}

	return EXIT_SUCCESS;
}
