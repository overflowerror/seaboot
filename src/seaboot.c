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
#include <getopt.h>
#include <assert.h>

static const char* getEventName(event_t event) {
	// TODO
	if (IS_SIGNAL(event))
		return strsignal(event);
	switch(event) {
		case SHUTDOWN: return "Shutdown"; break;
		case LIBERROR: return "Lib-Error"; break;
		default: return "Unknown"; break;
	}
}

static const char* getEventDescription(event_t event) {
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

static void* allocate(size_t);
static void* reallocate(void*, size_t);

static bool addOption(char, const char*, optionArgument_t, bool, optionHandler_t);
static int parseOptions(void);
static const char* getNextArgument();

struct boot boot = {
	.events.addEventListener = addEventListener,
	.events.removeEventListener = removeEventListener,
	.events.enableSignal = enableSignal,
	.events.disableSignal = disableSignal,
	.events.getName = getEventName,
	.events.getDescription = getEventDescription,

	.time.getRealTime = getRealTime,
	.time.getRelativeTime = getRelativeTime,
	.time.getProcessTime = getProcessTime,
	.time.getThreadTime = getThreadTime,
	.time.timer = timer,
	.time.createSignalTimer = createSignalTimer,
	.time.createThreadTimer = createThreadTimer,
	.time.startTimer = startTimer,
	.time.startInterval = startInterval,
	.time.stopTimer = stopTimer,
	.time.deleteTimer = deleteTimer,

	.options.add = addOption,
	.options.parse = parseOptions,
	.options.getNextArgument = getNextArgument,

	.allocate = allocate,
	.reallocate = reallocate,

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
	events[event].listeners = reallocate(events[event].listeners, (events[event].number + 1) * sizeof(eventListener_t));
	if (events[event].listeners == NULL)
		return false;
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

static void timerHandler(union sigval target) {
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

void* allocate(size_t size) {
	void* result = malloc(size);
	if (result == NULL) {
		boot.error = strerror(errno);
		eventHandler(LIBERROR);
	}
	return result;
}

void* reallocate(void* pointer, size_t size) {
	void* result = realloc(pointer, size);
	if (result == NULL) {
		boot.error = strerror(errno);
		eventHandler(LIBERROR);
		result = pointer;
	}
	return result;
}

struct optionHandlers {
	char shortOption;
	const char* longOption;
	optionArgument_t argument;
	bool required;
	optionHandler_t handler;
	int seen;
};

int optionNumber = 0;
struct optionHandlers options[MAX_OPTIONS];
char** arguments;
int argumentCount;

bool addOption(char shortOption, const char* longOption, optionArgument_t argument, bool required, optionHandler_t handler) {
	if ((shortOption == NO_SHORT_OPTION && longOption == NO_LONG_OPTION) || handler == NULL) {
		boot.error = "Option settings are invalid.";
		return false;
	}

	options[optionNumber++] = (struct optionHandlers) {
		.shortOption = shortOption,
		.longOption = longOption,
		.argument = argument,
		.required = required,
		.handler = handler,
		.seen = 0
	};

	return true;
}

int parseOptions() {
	debug("Parseing options. Got %d options.\n", optionNumber);
	if (optionNumber == 0)
		return argumentCount - 1;

	debug("Generating short-option-string & long-option-array.\n");
	int number_short = 0;
	char short_options[optionNumber * 3 + 1];
	memset(short_options, 0, optionNumber * 3 + 1);
	int number_long = 0;
	struct option long_options[optionNumber + 1];
	memset(long_options, 0, (optionNumber + 1) * sizeof(struct option));
	for (int i = 0; i < optionNumber; i++) {
		struct optionHandlers handler = options[i];
		if (handler.shortOption != NO_SHORT_OPTION) {
			short_options[number_short++] = handler.shortOption;
			debug("Got short option: %c\n", handler.shortOption);
			if (handler.argument == REQUIRED_ARGUMENT)
				short_options[number_short++] = ':';
			if (handler.argument == OPTIONAL_ARGUMENT) {
				short_options[number_short++] = ':';
				short_options[number_short++] = ':';
			}
		}
		if (handler.longOption != NO_LONG_OPTION) {
			int arg;
			switch(handler.argument) {
				case NO_ARGUMENT:
					arg = no_argument;
					break;
				case OPTIONAL_ARGUMENT:
					arg = optional_argument;
					break;
				case REQUIRED_ARGUMENT:
					arg = required_argument;
					break;
				default:
					assert(false);
			}
			debug("Got long option: %s (arg: %d, short: %c)\n", handler.longOption, arg, handler.shortOption);
			long_options[number_long++] = (struct option) {
				.name = handler.longOption, 
				.has_arg = arg, 
				.flag = NULL, 
				.val = handler.shortOption
			};
		}
	}

	long_options[number_long] = (struct option) {
		.name = NULL, 
		.has_arg = 0, 
		.flag = NULL, 
		.val = 0
	};
	
	int tmp, option_index;
	debug("getopt_long(%d, %x, %s, %x, %x)\n", argumentCount, arguments, short_options, long_options, option_index);
	while((tmp = getopt_long(argumentCount, arguments, short_options, long_options, &option_index)) != -1) {
		debug("getopt: %d\n", tmp);
		struct optionHandlers* handler = NULL;
		if (tmp == '?') {
			boot.error = "Unknown option.";
			return OPTION_UNKNOWN;
		} else if (tmp == NO_SHORT_OPTION) {
			for (int i = 0; i < optionNumber; i++) {
				if (strcmp(options[i].longOption, long_options[option_index].name) == 0) {
					handler = &(options[i]);
					break;
				} 
			}
			if (handler == NULL)
				assert(false);
		} else {
			for (int i = 0; i < optionNumber; i++) {
				debug("Checking %c (%d) - %c (%d).\n", tmp, tmp, options[i].shortOption, options[i].shortOption);
				if (tmp == options[i].shortOption) {
					handler = &(options[i]);
					break;
				}
			}
			if (handler == NULL)
				assert(false);
		}

		handler->seen++;
		debug("Starting handler for %s (%c).\n", handler->longOption, (handler->shortOption == NO_SHORT_OPTION) ? '-' : handler->shortOption);
		if (!(handler->handler(optarg))) {
			boot.error = "Option handler returned an error.";
			return OPTION_HANDLER_ERROR;
		}
	}

	for (int i = 0; i < optionNumber; i++) {
		if (options[i].seen == 0 && options[i].required) {
			boot.error = "Required option is missing.";
			return OPTION_MISSING;
		}
	}

	return argumentCount - optind;
}

const char* getNextArgument() {
	if (argumentCount == optind)
		return NULL;
	return arguments[optind++];
}

int main(int argc, char** argv) {
	arguments = argv;
	argumentCount = argc;
	for(int i = 0; i < NUMBER_OF_EVENTS; i++) {		
		debug("Setup %s %d (%s)...\n", IS_SIGNAL(i) ? "signal" : "event", i, getEventName(i));
		events[i].number = 0;
		events[i].listeners = allocate(1 * sizeof(eventListener_t));
		if (events[i].listeners == NULL)
			exit(EXIT_ERROR); // just to make sure
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
