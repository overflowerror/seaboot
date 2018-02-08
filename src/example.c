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
		boot.enableSignal(SHUTDOWN); // SHUTDOWN is not a signal
	}
}

void init() {
	printf("Hello World!\n");

	boot.mode = WAIT;
	boot.debug = true;

	boot.addEventListener(SHUTDOWN, shutdownHandler);
	boot.addEventListener(SIGUSR1, eventHandler);
	boot.enableSignal(SIGUSR1);

	timer_t timer = boot.createSignalTimer(SIGUSR1);
	boot.startTimer(timer, 5000);

	timer_t interval = boot.createThreadTimer(&intervalHandler);
	boot.startInterval(interval, 2000);
}
