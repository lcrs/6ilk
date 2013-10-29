#include "main.h"

/* DV frame reception callback function */
int samplerCallback(unsigned char *data, int len, int complete, void *dontcare) {
	uint8_t			*temp;
	
	/* Copy data out */
	memcpy(state.sampler.dvPartial, data, len);
	
	/* And swap the buffers */
	temp = state.sampler.dvFrame;
	state.sampler.dvFrame = state.sampler.dvPartial;
	state.sampler.dvPartial = temp;
	
	/* Set flag saying it's okay to decode first frame */
	state.sampler.doneFirst = 1;

	/* Write to file if we're recording */
	if(state.sampler.fd != 0) {
		pthread_mutex_lock(&state.sampler.writeMutex);
		write(state.sampler.fd, data, len);
		pthread_mutex_unlock(&state.sampler.writeMutex);
		state.sampler.recLen++;
	}
	
	return(0);
}

/* This thread handles the raw1394 loop */
void samplerWorker(void *dontcare) {
	raw1394handle_t		h;
	iec61883_dv_fb_t	dvfb;
	
	h = raw1394_new_handle_on_port(0);
	pthread_cleanup_push((void *)raw1394_destroy_handle, (void *)h);
	
	dvfb = iec61883_dv_fb_init(h, *samplerCallback, NULL);
	pthread_cleanup_push((void *)iec61883_dv_fb_close, (void *)dvfb);
	
	/* 63 is normally broadcast isosynchronous channel... this seems to work */
	iec61883_dv_fb_start(dvfb, 63);
	for(;;) {
		raw1394_loop_iterate(h);
		pthread_testcancel();
	}
	
	/* As usual, never gets here but macros don't work without these */
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
}

/* Thread entry point */
void sampler(void *dontcare) {
	pthread_t		workerThread;
	
	for(;;) {
		/* Wait for the sampler to be told to start, spawn a thread, wait for stop, cull, rinse and repeat */
		sem_wait(&state.sampler.start);
		pthread_create(&workerThread, NULL, (void *)samplerWorker, NULL);
		state.dirty = 1;
		sem_wait(&state.sampler.stop);
		pthread_cancel(workerThread);
		state.sampler.doneFirst = 0;
	}
}

/* Returns 1 if over input monitor */
uint8_t samplerOverMonitor(enum over_t o) {
	switch(o) {
		case 10:
		case 11:
		case 12:
		case 13:
		case 18:
		case 19:
		case 20:
		case 21:
		case 26:
		case 27:
		case 28:
		case 29:
		case 34:
		case 35:
		case 36:
		case 37:
			return(1);
			break;
		default:
			return(0);
			break;
	}
}

/* Returns index of clip mouse is over, or 255 if none */
uint8_t samplerOverClip(enum over_t o) {
	uint8_t			i, c;
	
	if(samplerOverMonitor(o)) {
		return(255);
	} else {
		c = state.gridScrollOffset * 8;
		for(i = 0; i < 56; i++) {
			if(c == globals.folders[state.currentFolder].fileCount) {
				break;
			}
			if(samplerOverMonitor(i)) {
				continue;
			} else {
				if(o == i) {
					return(c);
				} else {
					c++;
				}
			}
		}
	}
	
	return(255);
}

/* Called from the render loop to paint input monitor */
/* FIXME: PAL DV frame size / dimensions / pixel format hardcoded in here */
void samplerRender(void) {
	int32_t			worked;
	uint16_t		x, y, w, h;
	
	/* Decode... shouldn't this be in another thread? */
	if(state.sampler.doneFirst != 1) {
		/* There's no valid DV frame to decode yet */
		return;
	}
	
	avcodec_decode_video2(state.sampler.cctx, state.sampler.frame, &worked, &state.sampler.packet);
		
	/* Paint YCbCr straight through fragment program to back buffer, no render-to-texture like the other channels */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.sampler.ytex);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, state.sampler.frame->linesize[0], 576, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, state.sampler.frame->data[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.sampler.cbtex);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, state.sampler.frame->linesize[1], 288, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, state.sampler.frame->data[1]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.sampler.crtex);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, state.sampler.frame->linesize[2], 288, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, state.sampler.frame->data[2]);
	
	glSetupColourspaceConversion();
	
	x = 90 + 90;
	y = 72 + 72;
	w = 360;
	h = 288;
	
	if(state.sampler.fd != 0) {
		glColor3f(1.0, 0.2, 0.2);
	}
	
	glBegin(GL_QUADS);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 0);
		glMultiTexCoord2i(GL_TEXTURE1, 0, 0);
		glMultiTexCoord2i(GL_TEXTURE2, 0, 0);
		glVertex2i(x, y);
		glMultiTexCoord2i(GL_TEXTURE0, 720, 0);
		glMultiTexCoord2i(GL_TEXTURE1, 360, 0);
		glMultiTexCoord2i(GL_TEXTURE2, 360, 0);
		glVertex2i(x + w, y);
		glMultiTexCoord2i(GL_TEXTURE0, 720, 576);
		glMultiTexCoord2i(GL_TEXTURE1, 360, 288);
		glMultiTexCoord2i(GL_TEXTURE2, 360, 288);
		glVertex2i(x + w, y + h);
		glMultiTexCoord2i(GL_TEXTURE0, 0, 576);
		glMultiTexCoord2i(GL_TEXTURE1, 0, 288);
		glMultiTexCoord2i(GL_TEXTURE2, 0, 288);
		glVertex2i(x, y + h);
	glEnd();
	
	glColor3f(1.0, 1.0, 1.0);
		
	glUnsetupColourspaceConversion();
	
	/* Mouseover highlight the input monitor */
	if((state.over == grid) && samplerOverMonitor(state.overSub)) {
		lightenRect(x, y, w, h);
	}
}

/* Draw the sampler "folder" in the grid */
void samplerDraw(void) {
	uint8_t			i, f;
	uint16_t		x, y;

	f = 0;

	for(i = 0; i < 56; i++) {
		x = (i % 8) * 90;
		y = (i / 8) * 72 + 72;
		
		if(samplerOverMonitor(i)) {
			/* This slot is part of the input monitor, do nothing */
			continue;
		}
		
		if(samplerOverClip(i) == 255 && !samplerOverMonitor(i)) {
			/* Got to the last file */
			break;
		}
		
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_RECTANGLE_NV);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, globals.folders[state.currentFolder].files[f + 8 * state.gridScrollOffset].texture);
		
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
		
		f++;
	}
	
	/* Draw mouseover highlight if nessecary */
	if(((state.over == grid) | (state.over == gridSwipe))) {
		if(samplerOverClip(state.overSub) == 255) {
			/* Actually over the input monitor or over no clip, don't highlight */
		} else {
			lightenRect(90 * (state.overSub % 8), 72 + ((state.overSub / 8) * 72), 90, 72);
		}
	}
}

/* Click in the grid while sampler active - can start record or choose a clip */
void samplerClick(XButtonEvent *e) {
	struct folder_t		*folder;
	struct file_t		*file;
	uint8_t			last;
	uint8_t			new;
	char			*digits;
	
	if(samplerOverMonitor(state.overSub)) {
		/* Mouse is over input monitor, start recording */
		/* Generate sensible filename - sampler/silksampleXXX.dv where XXX is a zero padded integer */
		
		/* Get last file in folder */
		folder = &globals.folders[state.currentFolder];
		
		if(folder->fileCount == 0) {
			new = 0;
		} else {
			file = &folder->files[folder->fileCount - 1];
			
			/* Extract digits from the filename */
			last = atoi(file->name + strlen(file->name) - 6);
			new = last + 1;
		}
				
		/* Create a new filename with the digits one higher */
		state.sampler.path = malloc(strlen(globals.rootPath) + strlen(folder->name) + strlen("/silksample") + 3 + strlen(".dv"));
		strcpy(state.sampler.path, globals.rootPath);
		strcat(state.sampler.path, folder->name);
		strcat(state.sampler.path, "/silksample");
		
		digits = malloc(3 + 1);
		sprintf(digits, "%03d", new);
		
		strcat(state.sampler.path, digits);
		strcat(state.sampler.path, ".dv");
		
		state.sampler.fd = open(state.sampler.path, O_WRONLY | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);
		
		state.sampler.recLen = 0;
	} else {
		/* Clicked a clip, add to channel */
		gridClick(e);
	}
}

/* Stop current record */
void samplerStop(void) {
	
	pthread_mutex_lock(&state.sampler.writeMutex);
	close(state.sampler.fd);
	state.sampler.fd = 0;
	pthread_mutex_unlock(&state.sampler.writeMutex);
	
	if(state.sampler.recLen == 0) {
		/* Didn't record for long enough to get any frames, delete file and do nothing */
		unlink(state.sampler.path);
	} else {
		/* Update folder */
		state.sampler.thumbToAdd = 1;
		
		state.dirty = 1;
	}
}

/* Startup */
void samplerInit(void) {
	AVCodec			*codec;

	sem_init(&state.sampler.start, 0, 0);
	sem_init(&state.sampler.stop, 0, 0);
	
	pthread_mutex_init(&state.sampler.writeMutex, NULL);
	
	state.sampler.buf1 = malloc(144000);
	state.sampler.buf2 = malloc(144000);
	
	state.sampler.dvFrame = state.sampler.buf2;
	state.sampler.dvPartial = state.sampler.buf1;
	
	state.sampler.frame = avcodec_alloc_frame();
	codec = avcodec_find_decoder(CODEC_ID_DVVIDEO);
	state.sampler.cctx = avcodec_alloc_context3(codec);
	
	avcodec_open2(state.sampler.cctx, codec, NULL);
	av_init_packet(&state.sampler.packet);
	state.sampler.packet.size = 144000; /* PAL DV frame size in bytes */
	state.sampler.packet.data = state.sampler.dvFrame;
		
	glGenTextures(1, &state.sampler.ytex);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.sampler.ytex);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	
	glGenTextures(1, &state.sampler.cbtex);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.sampler.cbtex);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	
	glGenTextures(1, &state.sampler.crtex);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, state.sampler.crtex);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);		
}

/* Shutdown */
void samplerUninit(void) {
	if(state.currentFolder == 0) {
		/* Sampler is active, stop it */
		sem_post(&state.sampler.stop);
	}
	
	sem_destroy(&state.sampler.start);
	sem_destroy(&state.sampler.stop);

	av_free_packet(&state.sampler.packet);
}
