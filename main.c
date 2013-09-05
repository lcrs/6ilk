#include "main.h"

/* Globals */
struct globals_t globals;

/* State vector */
struct state_t state;

int main(int argc, char **argv) {
	pthread_t		guiThread, renderThread, samplerThread;
	pthread_attr_t		guiAttrs, renderAttrs, samplerAttrs;
	int32_t			r;
	struct	timespec	s;

	/* Init */
	memset(&globals, 0, sizeof(globals));
	memset(&state, 0, sizeof(state));
	
	sem_init(&globals.initSem, 0, 0);
	sem_init(&globals.exitSem, 0, 0);
	
	/* Set the root path to our clip tree, could come from argv */
	globals.rootPath = argv[1];
	
	av_register_all(); /* ffmpeg init */
	
	XInitThreads(); /* Tell X we'll want thread safety */
	
	/* Spawn render thread - big stack suggested for FreeBSD by Christian Zander at nVIDIA, dunno if still applies */
	pthread_attr_init(&renderAttrs);
	pthread_attr_setstacksize(&renderAttrs, 256 * 1024);
	pthread_create(&renderThread, &renderAttrs, (void *)render, NULL);
	pthread_attr_destroy(&renderAttrs);
	
	/* FIXME: Wait until X setup in render thread is done - fucking gdb/signals/sem_wait problem */
	/* sem_wait(&globals.initSem); */
	do {
		sem_getvalue(&globals.initSem, &r);
		s.tv_sec = 0;
		s.tv_nsec = 1000;
		nanosleep(&s, NULL);
	} while(r == 0);
		
	pthread_attr_init(&guiAttrs);
	pthread_attr_setstacksize(&guiAttrs, 256 * 1024);
	pthread_create(&guiThread, &guiAttrs, (void *)gui, NULL);
	pthread_attr_destroy(&guiAttrs);
		
	pthread_attr_init(&samplerAttrs);
	pthread_attr_setstacksize(&samplerAttrs, 256 * 1024);
	pthread_create(&samplerThread, &samplerAttrs, (void *)sampler, NULL);
	pthread_attr_destroy(&samplerAttrs);
	
	/* Wait for quit */
	/* sem_wait(&globals.exitSem); */
	do {
		sem_getvalue(&globals.exitSem, &r);
		s.tv_sec = 0;
		s.tv_nsec = 1000;
		nanosleep(&s, NULL);
	} while(r == 0);

	/* Teardown - GUI thread exits by itself */
	pthread_cancel(renderThread);
	pthread_cancel(samplerThread);
	
	samplerUninit();
	thumbsUninit();
	drawUninit();
	guiUninit();
	clipsUninit();
	channelsUninit();
	
	sem_destroy(&globals.initSem);
	sem_destroy(&globals.exitSem);
		
	exit(0);
}
