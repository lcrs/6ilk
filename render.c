#include "main.h"

/* Draws scale for stats bars */
void renderStatScale(void) {	
	/* Draw scale lines */
	glColor3f(0.1, 0.1, 0.1);
	glBegin(GL_LINES);
	glVertex2i(0, 900 - 18);
	glVertex2i(45, 900 - 18);
	glVertex2i(0, 900 - 36);
	glVertex2i(45, 900 - 36);
	glVertex2i(0, 900 - 54);
	glVertex2i(45, 900 - 54);
	glVertex2i(0, 900 - 72);
	glVertex2i(45, 900 - 72);
	glEnd();
	glColor3f(1.0, 1.0, 1.0);
}

/* Draws drawtime bar - time taken to decode and draw each frame */
void renderStatDraw(double ns) {
	/* If the bar would go too high, restrict it */
	if(ns > 80000000) {
		ns = 80000000;
	}
	/* If higher than it should be, colour it red*/
	if(ns > 41000000) {
		glColor3f(1.0, 0.0, 0.0);
	} else {
		glColor3f(0.2, 0.2, 0.2);
	}
	
	/* Draw the bar */
	glBegin(GL_QUADS);
	glVertex2i(0, 900);
	glVertex2i(0, 900 - (36.0 / 40.0) * ns / 1000000.0);
	glVertex2i(22, 900 - (36.0 / 40.0) * ns / 1000000.0);
	glVertex2i(22, 900);
	glEnd();
	
	glColor3f(1.0, 1.0, 1.0);
}

/* Draws swaptime bar - time between buffer swaps */
void renderStatSwap(double ns) {
	/* Restrict bar height */
	if(ns > 80000000) {
		ns = 80000000;
	}
	/* If higher than it should be, colour it red*/
	if(ns > 41000000) {
		glColor3f(1.0, 0.0, 0.0);
	} else {
		glColor3f(0.2, 0.2, 0.2);
	}
	
	/* Draw */
	glBegin(GL_QUADS);
	glVertex2i(23, 900);
	glVertex2i(23, 900 - (36.0 / 40.0) * ns / 1000000.0);
	glVertex2i(45, 900 - (36.0 / 40.0) * ns / 1000000.0);
	glVertex2i(45, 900);
	glEnd();
	
	glColor3f(1.0, 1.0, 1.0);
}

/* Renders YCbCr video textures into a single RGB texture per channel and draws channel monitors */
void renderChannels(struct clip_t **renderClips) {
	uint8_t			i;
	uint16_t		x, y;
	struct clip_t		*s;
		
	for(i = 0; i < 3; i++) {
		s = renderClips[i];
		if(s == NULL) {
			/* No clip to draw on this channel */
			continue;
		}
		
		x = 720;
		y = 0;
		
		/* Draw channel to Tx window in back buffer, yes it has to be upside down */
		glSetupColourspaceConversion();
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.channels[i].ytex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.channels[i].cbtex);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.channels[i].crtex);
		
		glBegin(GL_QUADS);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 0);
		glMultiTexCoord2i(GL_TEXTURE1, 0, 0);
		glMultiTexCoord2i(GL_TEXTURE2, 0, 0);
		glVertex2i(x, y + 576);
		glMultiTexCoord2i(GL_TEXTURE0, s->planeWidth[0], 0);
		glMultiTexCoord2i(GL_TEXTURE1, s->planeWidth[1], 0);
		glMultiTexCoord2i(GL_TEXTURE2, s->planeWidth[2], 0);
		glVertex2i(x + 720, y + 576);
		glMultiTexCoord2i(GL_TEXTURE0, s->planeWidth[0], s->planeHeight[0]);
		glMultiTexCoord2i(GL_TEXTURE1, s->planeWidth[1], s->planeHeight[1]);
		glMultiTexCoord2i(GL_TEXTURE2, s->planeWidth[2], s->planeHeight[2]);
		glVertex2i(x + 720, y);
		glMultiTexCoord2i(GL_TEXTURE0, 0, s->planeHeight[0]);
		glMultiTexCoord2i(GL_TEXTURE1, 0, s->planeHeight[1]);
		glMultiTexCoord2i(GL_TEXTURE2, 0, s->planeHeight[2]);
		glVertex2i(x, y);
		glEnd();
		
		glUnsetupColourspaceConversion();
				
		/* Copy drawn image to single RGB texture */
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_RECTANGLE_NV);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.channels[i].rgbtex);
		glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB, 720, 900 - 576, 720, 576, 0);
		
		/* Draw channel monitor */
		x = 45 + i * (90 + 360) + 90;
		y = 576;
		
		glBegin(GL_QUADS);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 0);
		glVertex2i(x, y);
		glMultiTexCoord2i(GL_TEXTURE0, 720, 0);
		glVertex2i(x + 360, y);
		glMultiTexCoord2i(GL_TEXTURE0, 720, 576);
		glVertex2i(x + 360, y + 288);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 576);
		glVertex2i(x, y + 288);
		glEnd();
		
		glDisable(GL_TEXTURE_RECTANGLE_NV);
		
		/* Darken a rect to indicate opacity */
		darkenRect(x, y, 360, (1.0 - state.channels[i].now.opacity) * 288);
		
		/* Mouseover highlight if nessecary */
		if(state.over == 6 + i) {
			lightenRect(x, y, 360, 288);
		}
		
		/* Draw per-channel UI */
		channelDraw(i);
	}
}

/* Main draw function, blends channels together onto TX output */
void renderTx(struct clip_t **renderClips) {
	uint8_t			i;
	uint16_t		x, y;
	struct clip_t		*s;
	
	/* Clear */
	glClearRect(720, 0, 720, 576);

	for(i = 0; i < 3; i++) {
		s = renderClips[i];
		if(s == NULL) {
			/* No clip to draw on this channel */
			continue;
		}
	
		x = 720;
		y = 0;
		
		/* Setup blending for second and third channels */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_MAX);
		
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_RECTANGLE_NV);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.channels[i].rgbtex);
		
		glColor4f(state.channels[i].now.opacity, state.channels[i].now.opacity, state.channels[i].now.opacity, state.channels[i].now.opacity);
		
		glBegin(GL_QUADS);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 0);
		glVertex2i(x, y);
		glMultiTexCoord2i(GL_TEXTURE0, 720, 0);
		glVertex2i(x + 720, y);
		glMultiTexCoord2i(GL_TEXTURE0, 720, 576);
		glVertex2i(x + 720, y + 576);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 576);
		glVertex2i(x, y + 576);
		glEnd();
		
		glColor4f(1.0, 1.0, 1.0, 1.0);
		
		glDisable(GL_TEXTURE_RECTANGLE_NV);
		
		/* Disable blending */
		glDisable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
	}
}

/* Thread entry point */
void render(void *dontcare) {
	uint8_t			i;
	struct clip_t	*renderClips[3];	/* Clips to render for each channel */
/*	struct sched_param	param; */
	struct timespec	start, afterDraw, afterSwap, sleepyTime;
	double			drawDelta, swapDelta;
	
	/* Make sure that when we are cancelled we quit immediately - the clip structs and stuff disappear in short order */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	
	/* Just in case we're cancelled whilst holding a mutex, if we don't have it no harm done */
	pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)&globals.glMutex);
	pthread_cleanup_push((void *)pthread_mutex_unlock, (void *)&globals.channelsMutex);
	
	/* High priority */
	/* param.sched_priority = 4;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param); */
	
	/* First time startup init - in here so they can use GL */
	guiInit();
	drawInit();
	thumbsInit();
	gridInit();
	channelsInit();
	samplerInit();
	
	/* Main thread can continue to spawn the others now */
	sem_post(&globals.initSem);
	
	drawDelta = swapDelta = 0;
	
	for(;;) {
		/* Store time at start of render */
		clock_gettime(CLOCK_REALTIME, &start);
		
		if(state.sampler.thumbToAdd) {
			thumbsAddFile(state.currentFolder, state.sampler.path);
			free(state.sampler.path);
			state.sampler.thumbToAdd = 0;
		}
		
		glClear(GL_COLOR_BUFFER_BIT);
		guiDrawAll();
		state.dirty = 0;
		
		renderStatDraw(drawDelta);
		renderStatSwap(swapDelta);
		renderStatScale();
				
		/* Populate renderClips array, set to NULL if no clip to render */
		pthread_mutex_lock(&globals.channelsMutex);
		for(i = 0; i < 3; i++) {
			if(state.channels[i].currentClip == NULL) {
				renderClips[i] = NULL;
			} else {
				renderClips[i] = state.channels[i].currentClip;
				if(!renderClips[i]->ready) {
					renderClips[i] = NULL;
				}
			}
		}
		pthread_mutex_unlock(&globals.channelsMutex);

		/* Decode */
		/* Wake up the decode threads */
		for(i = 0; i < 3; i++) {
			if(renderClips[i] != NULL) {
				sem_post(&renderClips[i]->decodeReady);
			}
		}
		/* ...and wait for them to finish, then upload their textures */
		for(i = 0; i < 3; i++) {
			if(renderClips[i] != NULL) {
				sem_wait(&renderClips[i]->decodeComplete);
			
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.channels[i].ytex);
				glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, state.channels[i].frameToTexture->linesize[0], renderClips[i]->planeHeight[0], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, state.channels[i].frameToTexture->data[0]);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.channels[i].cbtex);
				glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, state.channels[i].frameToTexture->linesize[1], renderClips[i]->planeHeight[1], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, state.channels[i].frameToTexture->data[1]);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.channels[i].crtex);
				glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, state.channels[i].frameToTexture->linesize[2], renderClips[i]->planeHeight[2], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, state.channels[i].frameToTexture->data[2]);
			}
		}
		
		animate();
		
		renderChannels(renderClips);
		
		renderTx(renderClips);
		
		if(state.currentFolder == 0) {
			/* Sampler is active, render its input monitor */
			samplerRender();
		}
				
		/* Record time after draw */
		clock_gettime(CLOCK_REALTIME, &afterDraw);
		if(start.tv_nsec > afterDraw.tv_nsec) {
			drawDelta = afterDraw.tv_nsec - start.tv_nsec + 1000000000.0;
		} else {
			drawDelta = afterDraw.tv_nsec - start.tv_nsec;
		}
		
		/* Wait a bit longer to ensure swap doesn't happen during first field time */
		sleepyTime.tv_sec = 0;
		sleepyTime.tv_nsec = 40000000 - drawDelta;
		nanosleep(&sleepyTime, NULL);
		
		glXSwapBuffers(globals.dpy, globals.win);
		
		/* Record time after swap */
		clock_gettime(CLOCK_REALTIME, &afterSwap);
		if(start.tv_nsec > afterSwap.tv_nsec) {
			swapDelta = afterSwap.tv_nsec - start.tv_nsec + 1000000000.0;
		} else {
			swapDelta = afterSwap.tv_nsec - start.tv_nsec;
		}
		
		/* Handle queued clip deletions */
		clipsDeleteReally();
		
		state.count++;
	}
	
	/* Never gets here, but nessecary for syntax */
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
}
