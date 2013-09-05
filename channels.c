#include "main.h"

/* Begin atomic modification of channels */
void channelsLock(void) {
	pthread_mutex_lock(&globals.channelsMutex);
}

/* End atomic modification of channels */
void channelsUnlock(void) {
	pthread_mutex_unlock(&globals.channelsMutex);
}

/* Draw channel UI elements */
void channelDraw(uint8_t channelIndex) {
	struct channel_t	*c;
	uint16_t		x, y;
	
	c = &state.channels[channelIndex];
	x = 45 + channelIndex * (90 + 360);
	y = 900 - 36;
	
	/* Clear area under channel monitor */
	glClearRect(x, 900 - 36, 360 + 90, 36);
	
	/* If there are no clips on this channel, do nothing else */
	if(c->currentClip == NULL) {
		return;
	}
	
	/* Draw playhead */
	glColor3f(0.1, 0.1, 0.1);
	glBegin(GL_QUADS);
	glVertex2i(x + 90, y);
	glVertex2i(x + 90 + c->now.pos * 360, y);
	glVertex2i(x + 90 + c->now.pos * 360, y + 36);
	glVertex2i(x + 90, y + 36);
	glEnd();
	
	/* Draw in point */
	glColor3f(0.5, 0.0, 0.0);
	glBegin(GL_LINES);
	glVertex2i(x + 90 + c->currentClip->in * 360, y);
	glVertex2i(x + 90 + c->currentClip->in * 360, y + 36);
	glEnd();
	
	/* Draw out point */
	glColor3f(0.0, 0.5, 0.0);
	glBegin(GL_LINES);
	glVertex2i(x + 90 - 1 + c->currentClip->out * 360, y);
	glVertex2i(x + 90 - 1 + c->currentClip->out * 360, y + 36);
	glEnd();
	
	/* Mouseover highlight if nessecary */
	if(state.over == 12 + channelIndex) {
		lightenRect(x + 90, y, 360, 36);
	}
	
	glColor3f(1.0, 1.0, 1.0);
}

/* Handle pos drags - video scratching */
void dragPos(uint8_t channelIndex, XMotionEvent *e) {
	struct channel_t	*c;
	float			frameLen;	/* Length of one frame of clip as multiple of clip length */
	float			speed;		/* Speed as multiple of 25.0 */
	
	c = &state.channels[channelIndex];
	
	c->now.pos = c->base.pos + (float)(e->x - state.scrollBase) / 360.0;
	
	frameLen = 1.0 / (float)c->currentClip->length;
	speed = c->currentClip->fps / 25.0;
	
	if(c->now.pos > 1.0 - frameLen) {
		/* Reached the end. Move mouse back to other end and reset drag */
		/* FIXME: XWarpPointer doesn't work with tablet in absolute mode */
		XWarpPointer(globals.dpy, None, globals.win, 0, 0, 0, 0, 45 + channelIndex * (360 + 90) + 90, 900 - 18);
		c->base.pos = 0.0;
		c->now.pos = 0.0;
		state.scrollBase = 45 + channelIndex * (360 + 90) + 90;
	} else if(c->now.pos < 0.0) {
		/* Reached the begining. Move to end, reset */
		XWarpPointer(globals.dpy, None, globals.win, 0, 0, 0, 0, 45 + channelIndex * (360 + 90) + 90 + (1.0 - frameLen) * 360, 900 - 18);
		c->base.pos = 1.0 - frameLen;
		c->now.pos = 1.0 - frameLen;
		state.scrollBase = 45 + channelIndex * (360 + 90) + 90 + (1.0 - frameLen) * 360 + 1;
	}
}

/* Handle opacity drags */
void dragOpacity(uint8_t channelIndex, XMotionEvent *e) {
	struct channel_t	*c;
	
	c = &state.channels[channelIndex];
	
	c->now.opacity = c->base.opacity - (float)(e->y - state.scrollBase) / 288.0;
	
	if(c->now.opacity > 1.0) {
		XWarpPointer(globals.dpy, None, globals.win, 0, 0, 0, 0, 45 + channelIndex * (360 + 90) + 288, 576);
		c->now.opacity = 1.0;
		c->base.opacity = 1.0;
		state.scrollBase = 576;
	} else if(c->now.opacity < 0.0) {
		XWarpPointer(globals.dpy, None, globals.win, 0, 0, 0, 0, 45 + channelIndex * (360 + 90) + 288, 576 + 288);
		c->now.opacity = 0.0;
		c->base.opacity = 0.0;
		state.scrollBase = 576 + 288;
	}
	
	if(e->y > 900 - 36) {
		/* Mouse now below channel preview window, move mouse up and reset scroll to prevent hitting bottom */
		c->base.opacity = c->now.opacity;
		state.scrollBase = 576 + 288;
		XWarpPointer(globals.dpy, None, globals.win, 0, 0, 0, 0, 45 + channelIndex * (360 + 90) + 288, 576 + 288);
	}
}

/* Handles mouse drags for interactive changes */
void dragMotion(XMotionEvent *e) {
	uint8_t			i;
	
	for(i = 0; i < 3; i++) {
		switch(animDragging(i)) {
			case 1:
				dragPos(i, e);
				break;
			case 2:
				dragOpacity(i, e);
			default:
				break;
		}
	}
}

/* Begin an interactive change */
void dragStart(uint8_t channelIndex, uint8_t attrIndex, XButtonEvent *e) {
	struct channel_t		*c;
	
	c = &state.channels[channelIndex];

	switch(attrIndex) {
		case 1: /* Pos */
			state.scrollBase = e->x;
			break;
		default:
			state.scrollBase = e->y;
			break;
	}

	/* Store the attribute in the now key in the base key */
	animAttrSet(&c->base, attrIndex, animAttrGet(&c->now, attrIndex));
	/* Set the attribute's interactive lock */
	animAttrSet(&c->locks, attrIndex, 1.0);

	cursorHide();
}

/* End interactive change */
void dragStop(uint8_t channelIndex, uint8_t attrIndex) {
	/* Unlock interactive lock */
	animAttrSet(&state.channels[channelIndex].locks, attrIndex, 0.0);
	
	/* Put the mouse cursor back somewhere sensible */
	switch(attrIndex) {
		case 1: /* Pos */
			XWarpPointer(globals.dpy, None, globals.win, 0, 0, 0, 0, 45 + (channelIndex * (360 + 90)) + 90 + (360 / 2), 900 - 18);
			break;
		case 2: /* Opacity */
			XWarpPointer(globals.dpy, None, globals.win, 0, 0, 0, 0, 45 + (channelIndex * (360 + 90)) + 90 + (360 / 2), 576 + 288 / 2);
			break;
		default:
			break;
	}
	
	cursorUnhide();
}

/* Startup */
void channelsInit(void) {
	uint8_t			i;
	struct channel_t 	*c;

	for(i = 0; i < 3; i++) {
		c = &state.channels[i];
		
		/* Create the colour LUT for each channel and fill with the default YCbCr to RGB values */
		glGenTextures(1, &c->lut);
		glBindTexture(GL_TEXTURE_3D, c->lut);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);		
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 8, 8, 8, 0, GL_RGB, GL_FLOAT, globals.defaultlut);

		/* And the output textures for the video planes, filled with black initially */
		glGenTextures(1, &c->ytex);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, c->ytex);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
		
		glGenTextures(1, &c->cbtex);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, c->cbtex);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
		
		glGenTextures(1, &c->crtex);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, c->crtex);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
		
		glGenTextures(1, &c->rgbtex);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, c->rgbtex);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		
		/* Init animatible attributes */
		c->now.pos = 0.0;
		c->now.opacity = 1.0;
	}
	
	pthread_mutex_init(&globals.channelsMutex, NULL);
	pthread_mutex_init(&globals.deletionMutex, NULL);
}

/* Shutdown */
void channelsUninit(void) {
	uint8_t			i;
	
	for(i = 0; i < 3; i++) {
		free(state.channels[i].clips);
		free(state.channels[i].deletions);
		glDeleteTextures(1, &state.channels[i].ytex);
		glDeleteTextures(1, &state.channels[i].cbtex);
		glDeleteTextures(1, &state.channels[i].crtex);
	}
	
	pthread_mutex_destroy(&globals.deletionMutex);
}
