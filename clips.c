#include "main.h"

/* Returns a pointer to the nth element in the linked list of clips */
struct clip_t *clipGet(uint8_t channelIndex, uint8_t n) {
	uint8_t			i;
	struct clip_t		*s;
		
	s = state.channels[channelIndex].clips;	
	for(i = 0; i < n; i++) {
		s = s->next;
	}	
	return(s);
}

/* Draws the clips interface elements */
void clipsDraw(uint8_t channelIndex) {
	uint8_t			i;
	uint16_t		x, y;
	struct clip_t		*s;
	
	x = channelIndex * (360 + 90) + 45;
		
	for(i = 0; i < state.channels[channelIndex].clipCount; i++) {
		s = clipGet(channelIndex, i);
				
		y = 576 + i * 72;
		
		if(s->ready) {
			glColor3f(1.0, 1.0, 1.0);
		} else {
			glColor3f(0.2, 0.2, 0.2);
		}
		
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_RECTANGLE_NV);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, s->texture);
		
		glBegin(GL_QUADS);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 72);
		glVertex2i(x, y);
		glMultiTexCoord2i(GL_TEXTURE0, 90, 72);
		glVertex2i(x + 90, y);
		glMultiTexCoord2i(GL_TEXTURE0, 90, 0);
		glVertex2i(x + 90, y + 72);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 0);
		glVertex2i(x, y + 72);
		glEnd();
				
		glDisable(GL_TEXTURE_RECTANGLE_NV);
		
		/* Current clip highlight */
		if(i == state.channels[channelIndex].current) {
			lightenRect(x, y, 90, 72);
		}
	}
	
	/* Mouseove highlight */
	if(state.over == 9 + channelIndex
	   && state.overSub < state.channels[channelIndex].clipCount
	   && state.overSub != state.channels[channelIndex].current) {
		lightenRect(45 + channelIndex * (90 + 360), 576 + state.overSub * 72, 90, 72);
	}
		
	glColor3f(1.0, 1.0, 1.0);
}

/* A clip's been clicked */
void clipClick(XButtonEvent *e) {
	uint8_t			c;
	
	c = state.over - 9;
	
	switch(e->button) {
		case Button1:
			/* Make current */
			if(state.overSub == state.channels[c].current) {
				/* Already current */
				state.channels[c].now.pos = 0.0;
			} else {
				clipMakeCurrent(c, state.overSub);
				state.dirty = 1;
			}
			break;
		case Button3:
			/* Delete */
			clipDelete(c, state.overSub);
			state.dirty = 1;
			break;
	}	
}

/* Change the current clip */
void clipMakeCurrent(uint8_t channelIndex, uint8_t clipIndex) {
	if(!clipGet(channelIndex, clipIndex)->ready) {
		/* Clip not ready to be played yet - do nothing */
		return;
	}
	state.channels[channelIndex].current = clipIndex;
	state.channels[channelIndex].currentClip = clipGet(channelIndex, clipIndex);
	state.channels[channelIndex].now.pos = state.channels[channelIndex].currentClip->in;
}

/* Add a clip to a channel */
void clipAdd(uint8_t channelIndex, uint8_t position, char *path, uint32_t size, uint32_t texture) {
	struct clip_t		*prev, *s, *next;
	pthread_attr_t		a;
	uint8_t			*toThread;
	
	s = malloc(sizeof(struct clip_t));
	memset(s, 0, sizeof(struct clip_t));
	
	/* Update the linked list */
	if(state.channels[channelIndex].clipCount == 0) {
		/* We're adding the first clip */
		state.channels[channelIndex].clips = s;
		s->next = NULL;
		clipMakeCurrent(channelIndex, 0);
	} else {
		/* Get pointers to the clips before and after the one we're about to add */
		prev = clipGet(channelIndex, position - 1);
		next = prev->next;
		
		/* Modify the previous clip's next pointer */
		prev->next = s;
		
		/* Set our new next pointer to the next clip's address */
		s->next = next;
	}
	
	state.channels[channelIndex].clipCount++;
	
	/* Fill out our new clip struct */
	s->path = malloc(strlen(path) + 1);
	strcpy(s->path, path);
	
	s->size = size;
	s->texture = texture;
		
	sem_init(&s->decodeReady, 0, 0);
	sem_init(&s->decodeComplete, 0, 0);
	
	s->in = 0.0;
	s->out = 1.0;
	s->multiplier = 1.0;
	
	pthread_attr_init(&a);
	pthread_attr_setstacksize(&a, 256 * 1024);
	toThread = malloc(2 * sizeof(uint8_t)); /* Freed from decode thread in a second */
	toThread[0] = channelIndex;
	toThread[1] = position;
	pthread_create(&s->thread, &a, (void *)decode, (void *)toThread);
	pthread_attr_destroy(&a);
	
	state.dirty = 1;
}

/* Delete a clip - actually just unlinks it from the interface */
void clipDelete(uint8_t channelIndex, uint8_t clipIndex) {
	struct channel_t	*c;
	struct clip_t		*prev, *it;
	int8_t			i;
	
	c = &state.channels[channelIndex];
	it = clipGet(channelIndex, clipIndex);
	
	if(!it->ready) {
		/* Clip still being buffered, don't delete it */
		return;
	}

	/* Update the linked list */
	if(clipIndex == 0) {
		/* This is the first clip */
		c->clips = it->next;
	} else if(clipIndex == c->clipCount) {
		/* This is the last clip */
		prev->next = NULL;
	} else {
		/* Some other clip */
		prev = clipGet(channelIndex, clipIndex - 1);
		prev->next = it->next;
	}
	
	if(clipIndex < c->current) {
		/* We've deleted a clip that was before the current clip - update current to point to the correct clip still, following clips shunt up */
		c->current--;
	} else if(clipIndex == c->current) {
		/* Deleted the current clip */
		if(clipIndex == c->clipCount - 1) {
			/* Current and last */
			if(c->clipCount == 1) {
				/* Current, last and only - no new current clip */
				c->currentClip = NULL;
			} else {
				/* Current, last but not only - go back through clips until we find a ready one and make that current */
				i = c->clipCount - 2;
				while(!clipGet(channelIndex, i)->ready && i >= 0) {
					i--;
				}
				clipMakeCurrent(channelIndex, i);
			}
		} else {
			/* Current but not last - make clip which falls up into same slot current */
			clipMakeCurrent(channelIndex, clipIndex);
		}
	}
	
	state.channels[channelIndex].clipCount--;
	
	/* Add the deleted clip to the deletions list */
	pthread_mutex_lock(&globals.deletionMutex);
	state.channels[channelIndex].deletionsCount++;
	state.channels[channelIndex].deletions = realloc(state.channels[channelIndex].deletions, state.channels[channelIndex].deletionsCount * sizeof(struct clip_t *));
	state.channels[channelIndex].deletions[state.channels[channelIndex].deletionsCount - 1] = it;
	pthread_mutex_unlock(&globals.deletionMutex);
	
	state.dirty = 1;
}


/* Actually free the clips that have been deleted */
void clipsDeleteReally(void) {
	uint8_t			i, j;
	struct clip_t		*s;
	
	pthread_mutex_lock(&globals.deletionMutex);
	for(j = 0; j < 3; j++) {
		for(i = 0; i < state.channels[j].deletionsCount; i++) {
			s = state.channels[j].deletions[i];
			pthread_cancel(s->thread);
			free(s->path);
			sem_destroy(&s->decodeReady);
			sem_destroy(&s->decodeComplete);
			free(s);
		}
		state.channels[j].deletionsCount = 0;
	}
	pthread_mutex_unlock(&globals.deletionMutex);
}

/* Shutdown */
void clipsUninit(void) {
	uint8_t			c, i;
	struct clip_t		*s;

	/* Delete all clips */	
	for(c = 0; c < 3; c++) {
		for(i = 0; i < state.channels[c].clipCount; i++) {
			s = clipGet(c, i);
			pthread_cancel(s->thread);
			sem_destroy(&s->decodeReady);
			sem_destroy(&s->decodeComplete);
			free(s->path);
		}
	}
	
	pthread_mutex_destroy(&globals.deletionMutex);
}
