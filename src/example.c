#include <config.h>
#include "seaboot.h"

#include <stdio.h>
#include <time.h>

BOOT(init);

void init() {
	printf("Hello World!\n");

	boot.mode = WAIT;
	boot.debug = true;

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
