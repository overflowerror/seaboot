#include <config.h>
#include "seaboot.h"

#include <stdio.h>
#include <time.h>

BOOT(init);

void eventHandler(event_t event) {
	fprintf(stderr, "Got event %d.\n", event);
}
void shutdownHandler(event_t event) {
	fprintf(stderr, "Shuting down.\n");
}

void intervalHandler() {
	static int count = 0;
	count++;
	fprintf(stderr, "interval count %d.\n", count);
	if (count > 4) {
		fprintf(stderr, "Provoking error.\n");
		boot.events.enableSignal(SHUTDOWN); // SHUTDOWN is not a signal
	}
}

void init() {
	printf("Hello World!\n");

	boot.mode = WAIT;
	boot.debug = true;

	boot.events.addEventListener(SHUTDOWN, shutdownHandler);
	boot.events.addEventListener(SIGUSR1, eventHandler);
	boot.events.enableSignal(SIGUSR1);

	timer_t timer = boot.time.createSignalTimer(SIGUSR1);
	boot.time.startTimer(timer, 5000);

	timer_t interval = boot.time.createThreadTimer(&intervalHandler);
	boot.time.startInterval(interval, 2000);
}
