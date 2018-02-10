#include <config.h>
#include "seaboot.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

BOOT(init);

void init() {
	printf("Hello World!\n");

	boot.mode = WAIT;
	boot.debug = false;

	int test = 0;
	boot.options.add('t', "test", OPTIONAL_ARGUMENT, true, lambda(bool, (const char* argument) {
		if (argument != NULL)
			test = strtol(argument, NULL, 10);
		return true;
	}));
	if (boot.options.parse() < 0) {
		fprintf(stderr, "Error: %s\n", boot.error);
		fprintf(stderr, "The only possible option is -t.\n");
		exit(EXIT_ERROR);
	}
	printf("Test-value is %d.\n", test);

	boot.events.addEventListener(SHUTDOWN, lambda(void, (event_t event) {
		fprintf(stderr, "Shuting down.\n");
	}));
	boot.events.addEventListener(SIGUSR1, lambda(void, (event_t event) {
		fprintf(stderr, "Got SIGUSR1.\n");
	}));
	boot.events.enableSignal(SIGUSR1);

	timer_t timer = boot.time.createSignalTimer(SIGUSR1);
	boot.time.startTimer(timer, 1000);
}
