#include "main.h"

/* Window damage handler */
void handleExpose(XEvent *event) {
	/* We don't handle rectangle damages, so just wait until there are no more expose events on their way */
	if(((XExposeEvent *)event)->count == 0) {
		state.dirty = 1;
	}
}

/* Keydown handler */
void handleKeyPress(XEvent *event) {
	uint8_t		i;
	
	switch(((XKeyEvent *)event)->keycode) {
		case 9: /* Escape */
			sem_post(&globals.exitSem);
			/* GUI thread stops here */
			pthread_exit(0);
			break;
		case 107: /* Delete */
			if((state.over == grid) | (state.over == gridSwipe)) {
				/* If mouse is over a file, move that file to the trash folder */
				if(state.currentFolder == 0) {
					/* Sampler is running */
					thumbsDeleteFile(state.currentFolder, samplerOverClip(state.overSub));
					state.dirty = 1;
				} else {
					thumbsDeleteFile(state.currentFolder, state.overSub + state.gridScrollOffset * 8);
					state.dirty = 1;
				}
			}
		case 10: /* 1 */
			if(state.channels[0].clipCount > 0) {
				clipMakeCurrent(0, 0);
			}
			break;
		case 24: /* q */
			if(state.channels[0].clipCount > 1) {
				clipMakeCurrent(0, 1);
			}
			break;
		case 38: /* a */
			if(state.channels[0].clipCount > 2) {
				clipMakeCurrent(0, 2);
			}
			break;
		case 52: /* z */
			if(state.channels[0].clipCount > 3) {
				clipMakeCurrent(0, 3);
			}
			break;
		case 11: /* 2 */
			if(state.channels[1].clipCount > 0) {
				clipMakeCurrent(1, 0);
			}
			break;
		case 25: /* w */
			if(state.channels[1].clipCount > 1) {
				clipMakeCurrent(1, 1);
			}
			break;
		case 39: /* s */
			if(state.channels[1].clipCount > 2) {
				clipMakeCurrent(1, 2);
			}
			break;
		case 53: /* x */
			if(state.channels[1].clipCount > 3) {
				clipMakeCurrent(1, 3);
			}
			break;
		case 12: /* 3 */
			if(state.channels[2].clipCount > 0) {
				clipMakeCurrent(2, 0);
			}
			break;
		case 26: /* e */
			if(state.channels[2].clipCount > 1) {
				clipMakeCurrent(2, 1);
			}
			break;
		case 40: /* d */
			if(state.channels[2].clipCount > 2) {
				clipMakeCurrent(2, 2);
			}
			break;
		case 54: /* c */
			if(state.channels[2].clipCount > 3) {
				clipMakeCurrent(2, 3);
			}
			break;
		case 31: /* i */
			for(i = 0; i < 3; i++) {
				if(animDragging(i) == 1) {
					state.channels[i].currentClip->in = state.channels[i].now.pos;
				}
			}
			break;
		case 32: /* o */
			for(i = 0; i < 3; i++) {
				if(animDragging(i) == 1) {
					state.channels[i].currentClip->out = state.channels[i].now.pos;
				}
			}
			break;
		case 65: /* space */
			tap();
			break;
		case 98: /* up */
			/* FIXME: lazy! */
			if(state.over == channel0) {
				state.channels[0].currentClip->multiplier *= 2;
			} else if(state.over == channel1) {
				state.channels[1].currentClip->multiplier *= 2;
			} else if(state.over == channel2) {
				state.channels[2].currentClip->multiplier *= 2;
			}
			break;
		case 104: /* down */
			if(state.over == channel0) {
				state.channels[0].currentClip->multiplier /= 2;
			} else if(state.over == channel1) {
				state.channels[1].currentClip->multiplier /= 2;
			} else if(state.over == channel2) {
				state.channels[2].currentClip->multiplier /= 2;
			}
			break;
		case 20: /* - */
			if(state.over == channel0) {
				state.channels[0].currentClip->multiplier *= -1;
			} else if(state.over == channel1) {
				state.channels[1].currentClip->multiplier *= -1;
			} else if(state.over == channel2) {
				state.channels[2].currentClip->multiplier *= -1;
			}
			break;
		case 100: /* left */
			if(state.over == channel0) {
				if(state.channels[0].now.pos >= 0.05) state.channels[0].now.pos -= 0.05;
			} else if(state.over == channel1) {
				if(state.channels[1].now.pos >= 0.05) state.channels[1].now.pos -= 0.05;
			} else if(state.over == channel2) {
				if(state.channels[2].now.pos >= 0.05) state.channels[2].now.pos -= 0.05;
			}
			break;
		case 102: /* right */
			if(state.over == channel0) {
				if(state.channels[0].now.pos <= 0.95) state.channels[0].now.pos += 0.05;
			} else if(state.over == channel1) {
				if(state.channels[1].now.pos <= 0.95) state.channels[1].now.pos += 0.05;
			} else if(state.over == channel2) {
				if(state.channels[2].now.pos <= 0.95) state.channels[2].now.pos += 0.05;
			}
			break;
	}
}

/* Keyup handler */
void handleKeyRelease(XEvent *event) {
}

/* Mouse clicks */
void handleButtonPress(XEvent *event) {
	XButtonEvent		*e;
	e = (XButtonEvent *)event;
		
	/* Look at values of state.over and state.overSub and call appropriately */
	switch(state.over) {
		case folders:
		case foldersSwipe:
			if(e->button > Button1) {
				gridFolderScrollWheel(e);
			} else {
				if(state.overSub < globals.folderCount) {
					if(state.currentFolder == 0) {
						/* We were showing the sampler folder, turn it off */
						sem_post(&state.sampler.stop);
					}
					state.currentFolder = state.overSub + state.folderScrollOffset;
					state.gridScrollOffset = 0;
					if(state.currentFolder == 0) {
						/* Switched to the sampler folder, turn it on */
						sem_post(&state.sampler.start);
					}
					state.dirty = 1;
				}
			}
			break;
		case grid:
		case gridSwipe:
			if(e->button > Button3) {
				gridScrollWheel(e);
			} else {
				if(state.currentFolder == 0) {
					/* Sampler is active, clicks can start record or playback clip */
					samplerClick(e);
					break;
				}
				if(state.overSub + state.gridScrollOffset * 8 < globals.folders[state.currentFolder].fileCount) {
					pthread_mutex_lock(&globals.channelsMutex);
					gridClick(e);
					pthread_mutex_unlock(&globals.channelsMutex);
				}
			}
			break;
		case channel0:
		case channel1:
		case channel2:
			if(e->button == Button1) {
				/* Opacity - start drag */
				dragStart(state.over - 6, 2, e);
			}
			break;
		case channel0clips:
		case channel1clips:
		case channel2clips:
			pthread_mutex_lock(&globals.channelsMutex);
			if(state.overSub < state.channels[state.over - 9].clipCount) {
				clipClick(e);
			}
			pthread_mutex_unlock(&globals.channelsMutex);
			break;
		case channel0bar:
		case channel1bar:
		case channel2bar:
			if(e->button == Button1) {
				/* Pos - video scratch start */
				dragStart(state.over - 12, 1, e);
			}
			break;
		default:
			break;
	}
}

/* Mouse up */
void handleButtonRelease(XEvent *event) {
	XButtonEvent			*e;
	uint8_t				i;
	
	e = (XButtonEvent *)event;
	
	/* If we've just let go of a drag, handle that and skip the rest */
	for(i = 0; i < 3; i++) {
		if(animDragging(i)) {
			dragStop(i, animDragging(i));
			return;
		}
	}
	
	/* If the sampler is recording, stop it */
	if(state.sampler.fd != 0) {
		samplerStop();
		return;
	}
			
	/* Look at values of state.over and state.overSub and call appropriately */
	switch(state.over) {
		case channel0:
		case channel1:
		case channel2:
			break;
		case channel0bar:
		case channel1bar:
		case channel2bar:
			break;
		default:
			break;
	}
}

/* Mouse moves */
void handleMotion(XEvent *event) {
	XMotionEvent			*e;
	uint8_t				i;
	
	e = (XMotionEvent *)event;
		
	/* If we're interactively dragging something, handle that and skip everything else */
	for(i = 0; i < 3; i++) {
		if(animDragging(i)) {
			dragMotion(e);
			return;
		}
	}
	
	/* Test if we're over each interface element */
	if(e->x >= 720 - 10 && e->y < 576 + 10) {
		state.over = tx;
	} else if(e->x >= 0 && e->x < 720 && e->y == 0) {
		state.over = foldersSwipe;
		state.overSub = e->x / 90;
	} else if(e->x == 0 && e->y >= 0 && e->y < 576) {
		state.over = gridSwipe;
		state.overSub = ((e->y - 72) / 72) * 8 + (e->x / 90);
	} else if(e->x >= 0 && e->x < 720 && e->y >= 0 && e->y < 90) {
		state.over = folders;
		state.overSub = e->x / 90;
	} else if(e->x >= 0 && e->x < 720 && e->y >= 90 && e->y < 576) {
		state.over = grid;
		state.overSub = ((e->y - 72) / 72) * 8 + (e->x / 90);
	} else if(e->x >= 45 + 90 && e->x < 45 + 90 + 360 && e->y >= 576 && e->y < 576 + 288) {
		state.over = channel0;
	} else if(e->x >= 45 + 90 + 360 + 90 && e->x < 45 + 90 + 360 + 90 + 360 && e->y >= 576 && e->y < 576 + 288) {
		state.over = channel1;
	} else if(e->x >= 45 + 90 + 360 + 90 + 360 + 90 && e->x < 45 + 90 + 360 + 90 + 360 + 90 + 360 && e->y >= 576 && e->y < 576 + 288) {
		state.over = channel2;
	} else if(e->x >= 45 && e->x < 45 + 90 && e->y >= 576 && e->y < 576 + 288) {
		state.over = channel0clips;
		state.overSub = (e->y - 576) / 72;
	} else if(e->x >= 45 + 90 + 360 && e->x < 45 + 90 + 360 + 90 && e->y >= 576 && e->y < 576 + 288) {
		state.over = channel1clips;
		state.overSub = (e->y - 576) / 72;
	} else if(e->x >= 45 + 90 + 360 + 90 + 360 && e->x < 45 + 90 + 360 + 90 + 360 + 90 && e->y >= 576 && e->y < 576 + 288) {
		state.over = channel2clips;
		state.overSub = (e->y - 576) / 72;
	} else if(e->x >= 45 + 90 && e->x < 45 + 90 + 360 && e->y >= 576 + 288 && e->y < 900) {
		state.over = channel0bar;
	} else if(e->x >= 45 + 90 + 360 + 90 && e->x < 45 + 90 + 360 + 90 + 360 && e->y >= 576 + 288 && e->y < 900) {
		state.over = channel1bar;
	} else if(e->x >= 45 + 90 + 360 + 90 + 360 + 90 && e->x < 45 + 90 + 360 + 90 + 360 + 90 + 360 && e->y >= 576 + 288 && e->y < 900) {
		state.over = channel2bar;
	} else {
		state.over = nothing;
	}

	if(state.over != state.overPrev) {
		/* We've moved between something and something else, so unhighlight the old... */
		leave(e);
		/* ...and highlight the new */
		enter(e);
	} else {
		/* Still over the same element */
		within(e);
	}
	
	state.overPrev = state.over;
	state.overSubPrev = state.overSub;
}

/* Mouse leaves the given element */
void leave(XMotionEvent *e) {
	uint8_t			c;
	switch(state.overPrev) {
		case tx:
			cursorUnhide();
			break;
		case folders:
			/* If we're going from folders to folderSwipe, don't redraw */
			if(state.over == foldersSwipe) {
				break;
			} else {
				state.dirty = 1;
			}
			break;
		case grid:
			state.dirty = 1;
			break;
		case foldersSwipe:
			state.dirty = 1;
			break;
		case gridSwipe:
			state.dirty = 1;
			break;
		case channel0:
			break;
		case channel1:
			break;
		case channel2:
			break;
		case channel0clips:
		case channel1clips:
		case channel2clips:
			c = state.overPrev - 9;
			state.dirty = 1;
			break;
		case channel0bar:
		case channel1bar:
		case channel2bar:
			break;
		case nothing:
			break;
	}
}

/* Mouse going over a new element */
void enter(XMotionEvent *e) {
	uint8_t			c;
	switch(state.over) {
		case tx:
			cursorHide();
			break;
		case folders:
			state.dirty = 1;
			break;
		case grid:
			state.dirty = 1;
			break;
		case foldersSwipe:
			gridFolderScrollStart(e);
			break;
		case gridSwipe:
			gridScrollStart(e);
			break;
		case channel0:
		case channel1:
		case channel2:
			c = state.over - 6;
			break;
		case channel0clips:
		case channel1clips:
		case channel2clips:
			state.dirty = 1;
			break;
		case channel0bar:
		case channel1bar:
		case channel2bar:
			state.dirty = 1;
			break;
		case nothing:
			break;
	}
}


/* Mouse moves within given element */
void within(XMotionEvent *e) {
	switch(state.over) {
		case folders:
			if(state.overSub != state.overSubPrev) {
				/* Mouse has moved from one folder to another */
				state.dirty = 1;
			}
			break;
		case foldersSwipe:
			if(gridFolderScroll(e) | (state.overSub != state.overSubPrev)) {
				/* Folders have scrolled, or mouse has moved from one folder to another */
				state.dirty = 1;
			}
			break;
		case grid:
			if(state.overSub != state.overSubPrev) {
				/* Moved from one file to another */
				state.dirty = 1;
			}
			break;
		case gridSwipe:
			if(gridScroll(e) | (state.overSub != state.overSubPrev)) {
				/* Grid has scrolled, or mouse moved from one file to another */
				state.dirty = 1;
			}
			break;
		case channel0clips:
		case channel1clips:
		case channel2clips:
			if(state.overSub != state.overSubPrev) {
				state.dirty = 1;
			}
			break;
		case channel0:
		case channel1:
		case channel2:
			break;
		case channel0bar:
		case channel1bar:
		case channel2bar:
			break;
		default:
			break;
	}
}
