#include "main.h"

/* Tap tempo */
void tap(void) {
	struct timespec		n;
	double			now, delta, total;
	float			i2o;
	uint8_t			i, j, count;
		
	clock_gettime(CLOCK_REALTIME, &n);
	now = (double)n.tv_sec + (double)n.tv_nsec / 1000000000.0;
	
	/* get time since last tap */
	delta = now - state.lastTap;
	
	/* check if time since last tap is ridiculous */
	if(delta > 2.0) {
		/* start of a new sequence - zero all times and do nothing*/
		/* printf("6ilk: starting new tap sequence\n"); */
		for(i = 0; i < 8; i++) {
			state.taps[i] = 0.0;
		}
	} else {
		/* store new delta, move around the ring buffer */
		if(state.latestTapIndex == 7) {	
			state.taps[0] = delta;
			state.latestTapIndex = 0;
		} else {
			state.taps[state.latestTapIndex + 1] = delta;
			state.latestTapIndex++;
		}

		/* average non-zero deltas to get tempo */
		total = 0.0;
		count = 0;
		for(i = 0; i < 8; i++) {
			if(state.taps[i] > 0.0) {
				total += state.taps[i];
				count++;
			}
		}

		state.tempo = total / (double)count;
		
		/* printf("6ilk: tempo = %f\n", 60.0 / state.tempo); */
		
		/* set new speeds for all clips */
		for(i = 0; i < 3; i++) {
			for(j = 0; j < state.channels[i].clipCount; j++) {
				i2o = state.channels[i].clips[j].out - state.channels[i].clips[j].in;
				state.channels[i].clips[j].fps = (i2o * (float)state.channels[i].clips[j].length) / (float)state.tempo / 4.0;
				while(state.channels[i].clips[j].fps > 50.0) {
					state.channels[i].clips[j].fps /= 2;
				}
				while(state.channels[i].clips[j].fps < 12.5) {
					state.channels[i].clips[j].fps *= 2;
				}
			}
		}
	}
	
	state.lastTap = now;
}

/* Returns index into key_t struct of currently dragged attribute, as if the struct were an array of floats, -1 if none */
uint8_t animDragging(uint8_t channelIndex) {
	struct channel_t		*c;
	uint8_t				i;
	
	c = &state.channels[channelIndex];
	
	for(i = 0; i < sizeof(struct key_t) / sizeof(float); i++) {
		if(animAttrGet(&c->locks, i) != 0.0) {
			return(i);
		}
	}
	
	/* No locks set */
	return(0);
}

/* Returns a pointer to the float at the given index into a given key_t struct */
float* animIndexToPointer(struct key_t *k, uint8_t i) {
	return(&((float *)k)[i]);
}

/* Sets the attribute at index i in key k to f  */
void animAttrSet(struct key_t *k, uint8_t i, float f) {
	*animIndexToPointer(k, i) = f;
}

/* Return the attribute at index i in key k */
float animAttrGet(struct key_t *k, uint8_t i) {
	return(*animIndexToPointer(k, i));
}

/* Playhead position and looping */
void animPos(struct channel_t *c, struct clip_t *d) {
	float			frameLen;	/* Length of one frame of clip as multiple of clip length */
	float			speed;		/* Speed as multiple of 25.0 */
	
	frameLen = 1.0 / (float)d->length;
	speed = d->fps / 25.0;
	
	c->now.pos += speed * frameLen * d->multiplier;
	
	if(c->now.pos > d->out - frameLen) {
		/* Time to loop */
		c->now.pos = d->in;
	}
	
	if(c->now.pos < d->in + frameLen && d->multiplier < 0.0) {
		c->now.pos = d->out;
	}
}

/* Update channel animation variables, called on each frame */
void animate(void) {
	uint8_t			i;
	struct channel_t	*c, *p;
	struct clip_t		*d;
	
	channelsLock();
	
	for(i = 0; i < 3; i++) {
			if(state.channels[i].clipCount == 0) {
				/* No clips on channel */
				continue;
			} else if(clipGet(i, state.channels[i].current)->ready == 0) {
				/* Current clip not ready so no clips on channel, really */
				continue;
			}
			
			c = &state.channels[i];
			p = &state.prev[i];
			d = clipGet(i, c->current);
			
			/* Playhead position */
			if(c->current == p->current && c->now.pos == p->now.pos && c->locks.pos == 0.0) {
				/* Same clip is current as at last frame and pos hasn't been changed and isn't locked */
				animPos(c, d);
			}
	}
	
	/* Store the current channel data for comparision next time around */
	memcpy(state.prev, state.channels, 3 * sizeof(struct channel_t));
		
	channelsUnlock();
}
