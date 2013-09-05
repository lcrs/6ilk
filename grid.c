#include "main.h"

/* Startup */
void gridInit(void) {
	/* Let's not start up in the sampler folder */
	state.currentFolder = 1;
}

/* Draw the folders bar */
void foldersDraw(void) {
	uint16_t		i, last, x, y;

	/* Clear whatever might be there */
	glClearRect(0, 0, 720, 72);
	
	/* Draw folders */
	if(globals.folderCount < 8) {
		last = globals.folderCount;
	} else {
		last = 8;
	}
		
	for(i = 0; i < last; i++) {
		x = i * 90;
		y = 0;
		
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_RECTANGLE_NV);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, globals.folders[i + state.folderScrollOffset].texture);
		
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
	
		/* Draw current folder highlight */
		if(i + state.folderScrollOffset == state.currentFolder) {
			lightenRect(x, y, 90, 72);
		}
	}
	
	/* Draw mouseover highlight if nessecary */
	if(((state.over == folders) | (state.over == foldersSwipe))
	   && state.overSub < globals.folderCount
	   && state.overSub + state.folderScrollOffset != state.currentFolder) {
		lightenRect(90 * state.overSub, 0, 90, 72);
	}
}

/* Draw the entire file grid */
void gridDraw(void) {
	uint16_t		i, x, y;
	
	if(state.currentFolder == 0) {
		/* Showing sampler folder, use sampler's draw function */
		samplerDraw();
		return;
	}	
	
	/* Clear whatever might be there */
	glClearRect(0, 72, 720, 576 - 72);
	
	/* Draw files */
	for(i = 0; i < 56; i++) {
		x = (i % 8) * 90;
		y = (i / 8) * 72 + 72;
		
		if(i + 8 * state.gridScrollOffset > globals.folders[state.currentFolder].fileCount - 1) {
			break;
		}
		
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_RECTANGLE_NV);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, globals.folders[state.currentFolder].files[i + 8 * state.gridScrollOffset].texture);
		
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
	}
	
	/* Draw mouseover highlight if nessecary */
	if(((state.over == grid) | (state.over == gridSwipe)) && (state.overSub + state.gridScrollOffset * 8 < globals.folders[state.currentFolder].fileCount)) {
		lightenRect(90 * (state.overSub % 8), 72 + ((state.overSub / 8) * 72), 90, 72);
	}
}

/* Just hit the top swipe area, start a new scroll */
void gridFolderScrollStart(XMotionEvent *e) {
	state.scrollBase = e->x;
	state.folderScrollOffsetBase = state.folderScrollOffset;
}

/* Handle top scroll events - returns 1 if resulted in a move */
uint8_t gridFolderScroll(XMotionEvent *e) {
	uint8_t			prev;
	
	prev = state.folderScrollOffset;
	
	state.folderScrollOffset = state.folderScrollOffsetBase + (e->x - state.scrollBase) / 45;
	
	if(state.folderScrollOffset + 8 > globals.folderCount) {
		/* Tried to scroll too far to the right */
		state.folderScrollOffset = globals.folderCount - 8;
		/* Start a new scroll from here, just makes it work more like a real sliding bar */
		gridFolderScrollStart(e);
	}
	if(state.folderScrollOffset < 0) {
		/* Tried to scroll too far to the left */
		state.folderScrollOffset = 0;
		/* Start a new scroll from here, just makes it work more like a real sliding bar */
		gridFolderScrollStart(e);
	}
	
	if(state.folderScrollOffset != prev) {
		/* Mouse moved far enough to scroll */
		state.dirty = 1;
		return(1);
	} else {
		return(0);
	}
}

/* Handle button 4 and 5 events - returns 1 if it handled a button event */
void gridFolderScrollWheel(XButtonEvent *e) {
	uint8_t			prev;
	
	prev = state.folderScrollOffset;
		
	if(e->button == Button4) {
		state.folderScrollOffset--;
	} else if(e->button == Button5) {
		state.folderScrollOffset++;
	} else {
		return;
	}
	
	if(state.folderScrollOffset + 8 > globals.folderCount) {
		/* Tried to scroll too far to the right */
		state.folderScrollOffset = globals.folderCount - 8;
	}
	if(state.folderScrollOffset < 0) {
		/* Tried to scroll too far to the left */
		state.folderScrollOffset = 0;
	}
	
	if(state.folderScrollOffset != prev) {
		/* Mouse moved far enough to scroll */
		foldersDraw();
	}
}

/* Just hit the side swipe area, start a new scroll */
void gridScrollStart(XMotionEvent *e) {
	state.scrollBase = e->y;
	state.gridScrollOffsetBase = state.gridScrollOffset;	
}

/* Handle side scroll events, return 1 if moved */
uint8_t gridScroll(XMotionEvent *e) {
	uint8_t			prev;
	
	prev = state.folderScrollOffset;
	
	state.gridScrollOffset = state.gridScrollOffsetBase + (e->y - state.scrollBase) / 36;
	
	if(state.gridScrollOffset * 8 + (state.currentFolder == 0 ? 40 : 56) > globals.folders[state.currentFolder].fileCount + 7) {
		/* Tried to scroll too far down */
		state.gridScrollOffset = (uint8_t)ceil((float)(globals.folders[state.currentFolder].fileCount - (state.currentFolder == 0 ? 40 : 56)) / 8.0);
		/* Start a new scroll from here, just makes it work more like a real sliding bar */
		gridScrollStart(e);
	}
	if(state.gridScrollOffset < 0) {
		/* Tried to scroll too far up */
		state.gridScrollOffset = 0;
		/* Start a new scroll from here, just makes it work more like a real sliding bar */
		gridScrollStart(e);
	}
	
	if(state.gridScrollOffset != prev) {
		/* We've moved */
		state.dirty = 1;
		return(1);
	} else {
		return(0);
	}
}

/* Handles button four and five events */
void gridScrollWheel(XButtonEvent *e) {
	uint8_t			prev;
	
	prev = state.folderScrollOffset;
	
	if(e->button == Button4) {
		state.gridScrollOffset--;
	} else if(e->button == Button5) {
		state.gridScrollOffset++;
	}
	
	if(state.gridScrollOffset * 8 + (state.currentFolder == 0 ? 40 : 56) > globals.folders[state.currentFolder].fileCount + 7) {
		/* Tried to scroll too far down */
		state.gridScrollOffset = (uint8_t)ceil((float)(globals.folders[state.currentFolder].fileCount - (state.currentFolder == 0 ? 40 : 56)) / 8.0);
	}
	if(state.gridScrollOffset < 0) {
		/* Tried to scroll too far up */
		state.gridScrollOffset = 0;
	}
		
	if(state.gridScrollOffset != prev) {
		/* Changed */
		state.dirty = 1;
	}
}

/* Handle clicks */
void gridClick(XButtonEvent *e) {
	uint8_t			a;
	char			*s;

	if(state.currentFolder == 0) {
		/* Sampler specific */
		a = samplerOverClip(state.overSub);
	} else {
		a = state.overSub + state.gridScrollOffset * 8;
	}
	if(a >= globals.folders[state.currentFolder].fileCount) {
		/* Then click was not over a file */
		return;
	}
	
	/* Put file onto a channel */
	s = malloc(strlen(globals.rootPath) + strlen(globals.folders[state.currentFolder].name) + strlen("/") + strlen(globals.folders[state.currentFolder].files[a].name) + 1);
	strcpy(s, globals.rootPath);
	strcat(s, globals.folders[state.currentFolder].name);
	strcat(s, "/");
	strcat(s, globals.folders[state.currentFolder].files[a].name);
	switch(e->button) {
		case Button1:
			if(state.channels[0].clipCount == 4) {
				break;
			}
			clipAdd(0, state.channels[0].clipCount, s, globals.folders[state.currentFolder].files[a].size, globals.folders[state.currentFolder].files[a].texture);
		break;
		case Button2:
			if(state.channels[1].clipCount == 4) {
				break;
			}
			clipAdd(1, state.channels[1].clipCount, s, globals.folders[state.currentFolder].files[a].size, globals.folders[state.currentFolder].files[a].texture);
		break;
		case Button3:
			if(state.channels[2].clipCount == 4) {
				break;
			}
			clipAdd(2, state.channels[2].clipCount, s, globals.folders[state.currentFolder].files[a].size, globals.folders[state.currentFolder].files[a].texture);
		break;
	}
	free(s);
}
