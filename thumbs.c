#include "main.h"

/* Comparison of two folder_t structs by name, for qsort() */
int folderCompare(const void *a, const void *b) {
	struct folder_t		x, y;
	
	x = *(struct folder_t *)a;
	y = *(struct folder_t *)b;
	
	/* Samples "folder" always first */
	if(strcmp(x.name, "sampler") == 0) {
		return(-1);
	} else if(strcmp(y.name, "sampler") == 0) {
		return(1);
	} else {
		return(strcmp(x.name, y.name));
	}
}

/* Comparison of two file_t structs by name, for qsort() */
int fileCompare(const void *a, const void *b) {
	struct file_t		x, y;
	
	x = *(struct file_t *)a;
	y = *(struct file_t *)b;
	return(strcmp(x.name, y.name));
}

/* Add a file to a folder */
void thumbsAddFile(uint8_t folderIndex, char *path) {
	struct folder_t		*folder;
	struct file_t		*file;
	char			*s;
	struct stat		st;
	
	folder = &globals.folders[0];
	folder->files = realloc(folder->files, (folder->fileCount + 1) * sizeof(struct file_t));
	
	file = &folder->files[folder->fileCount];
	
	s = basename(path);
	file->name = malloc(strlen(s) + 1);
	strcpy(file->name, s);
	
	stat(path, &st);
	file->size = st.st_size;
	
	/* FIXME: this flickers! Could we readPixels(), do it then drawPixels() or something? */
	makeThumb(state.sampler.path, &file->texture);
	
	folder->fileCount++;
}

/* Move a file from a folder into the trash folder */
void thumbsDeleteFile(uint8_t folderIndex, uint8_t fileIndex) {
	char 			*oldpath, *newpath;
	uint8_t			trashIndex;
	struct folder_t		*folder, *trash;
	struct file_t		*file;
	
	folder = &globals.folders[folderIndex];
	
	for(trashIndex = 0; strcmp(globals.folders[trashIndex].name, "trash") != 0; trashIndex++);
	trash = &globals.folders[trashIndex];
	
	file = &folder->files[fileIndex];
	
	/* Move file on disk */
	oldpath = malloc(strlen(globals.rootPath) + strlen(folder->name) + strlen("/") + strlen(file->name) + 1);
	strcpy(oldpath, globals.rootPath);
	strcat(oldpath, folder->name);
	strcat(oldpath, "/");
	strcat(oldpath, file->name);
	
	newpath = malloc(strlen(globals.rootPath) + strlen("trash/") + strlen(file->name) + 1);
	strcpy(newpath, globals.rootPath);
	strcat(newpath, "trash/");
	strcat(newpath, file->name);
	
	rename(oldpath, newpath);
	
	/* Make space in trash folder files array */
	trash->files = realloc(trash->files, (trash->fileCount + 1) * sizeof(struct file_t));
	
	/* Copy file struct into trash folder array */
	memcpy(&trash->files[trash->fileCount], file, sizeof(struct file_t));
	trash->fileCount++;
	
	/* Sort trash folder files array */
	qsort(trash->files, trash->fileCount, sizeof(struct file_t), fileCompare);
	
	/* Remove space from source folder files array */
	memcpy(&folder->files[fileIndex], &folder->files[fileIndex + 1], (folder->fileCount - fileIndex) * sizeof(struct file_t));
	folder->fileCount--;
}

/* Invent a thumbnail for folders with no icon.avi */
void fakeThumb(char *path, uint32_t *texture) {
	char		*s, *t;

	glGenTextures(1, texture);
	
	/* Reduce the path to it's folder name only */
	s = (char *)dirname(path);
	t = (char *)basename(s);

	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1.0, 1.0, 1.0);

	/* Draw full folder path under thumbnail */
	glRasterPos2i(360 - 90 / 2, 288 + 72 / 2 + 12);
	glPrint(path);

	/* Draw folder name as thumbnail */
	glRasterPos2i(360 - glPrintWidth(t) / 2, 288 + 2);
	glPrint(t);

	/* Copy the drawn name into an RGB texture */
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, *texture);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB, 360 - 90 / 2, 900 - (288 + 72 / 2), 90, 72, 0);

	glXSwapBuffers(globals.dpy, globals.win);
}

/* Decode first frame of path and put into texture */
void makeThumb(char *path, uint32_t *texture) {
	AVFormatContext		*fctx;
	AVCodecContext		*cctx;
	AVCodec			*c;
	AVFrame			*f;
	AVPacket		p;
	uint8_t			stream, i;
	int32_t			worked;
	uint32_t		ytex, cbtex, crtex;
	uint16_t		planeHeight[3], planeWidth[3];
	
	glColor3f(1.0, 1.0, 1.0);
	
	glGenTextures(1, texture);
	
	/* Draw full path under thumbnail */
	glClearRect(360 - 90 / 2, 288 + 72 / 2, glPrintWidth(path), 20);
	glRasterPos2i(360 - 90 / 2, 288 + 72 / 2 + 12);
	glPrint(path);

	/* Regular ffmpeg setup and single frame-decode */
	avformat_open_input(&fctx, path, NULL, NULL);
	avformat_find_stream_info(fctx, NULL);
	for(i = 0; i < fctx->nb_streams; i++) {
		if(fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			stream = i;
			break;
		}
	}
	cctx = fctx->streams[stream]->codec;
	c = avcodec_find_decoder(cctx->codec_id);
	avcodec_open2(cctx, c, NULL);
	av_init_packet(&p);
	f = avcodec_alloc_frame();
	
	planeHeight[0] = cctx->height;
	planeWidth[0] = cctx->width;
	planeHeight[1] = cctx->height;
	planeWidth[1] = cctx->width;
	planeHeight[2] = cctx->height;
	planeWidth[2] = cctx->width;
	
	sizeChromaPlanes(cctx->pix_fmt, planeWidth, planeHeight);
	
	if(isIntra(cctx)) {
		av_seek_frame(fctx, stream, rand() % fctx->duration / 40000, 0);
	}
	do {
		av_read_frame(fctx, &p);
	} while(p.stream_index != stream);
	avcodec_decode_video2(cctx, f, &worked, &p);

	/* Draw frame to back buffer */
	glSetupColourspaceConversion();

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &ytex);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, ytex);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, f->linesize[0], planeHeight[0], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, f->data[0]);
	glActiveTexture(GL_TEXTURE1);
	glGenTextures(1, &cbtex);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, cbtex);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, f->linesize[1], planeHeight[1], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, f->data[1]);
	glActiveTexture(GL_TEXTURE2);
	glGenTextures(1, &crtex);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, crtex);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 1, f->linesize[1], planeHeight[2], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, f->data[2]);

	glBegin(GL_QUADS);
	glMultiTexCoord2i(GL_TEXTURE0, 0, 0);
	glMultiTexCoord2i(GL_TEXTURE1, 0, 0);
	glMultiTexCoord2i(GL_TEXTURE2, 0, 0);
	glVertex2i(360 - 90 / 2, 288 - 72 / 2);
	glMultiTexCoord2i(GL_TEXTURE0, planeWidth[0], 0);
	glMultiTexCoord2i(GL_TEXTURE1, planeWidth[1], 0);
	glMultiTexCoord2i(GL_TEXTURE2, planeWidth[2], 0);
	glVertex2i(360 + 90 / 2, 288 - 72 / 2);
	glMultiTexCoord2i(GL_TEXTURE0, planeWidth[0], planeHeight[0]);
	glMultiTexCoord2i(GL_TEXTURE1, planeWidth[1], planeHeight[1]);
	glMultiTexCoord2i(GL_TEXTURE2, planeWidth[2], planeHeight[2]);
	glVertex2i(360 + 90 / 2, 288 + 72 / 2);
	glMultiTexCoord2i(GL_TEXTURE0, 0, planeHeight[0]);
	glMultiTexCoord2i(GL_TEXTURE1, 0, planeHeight[1]);
	glMultiTexCoord2i(GL_TEXTURE2, 0, planeHeight[2]);
	glVertex2i(360 - 90 / 2, 288 + 72 / 2);
	glEnd();
	
	glUnsetupColourspaceConversion();
	
	/* Copy the drawn image into an RGB texture */
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, *texture);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB, 360 - 90 / 2, 900 - (288 + 72 / 2), 90, 72, 0);

	glXSwapBuffers(globals.dpy, globals.win);

	av_free_packet(&p);
	av_free(f);
	avcodec_close(cctx);
	avformat_close_input(&fctx);
	
	glDeleteTextures(1, &ytex);
	glDeleteTextures(1, &cbtex);
	glDeleteTextures(1, &crtex);
}

/* Walk the filesystem, filling global folders array */
void thumbsInit(void) {
	uint16_t		i, j;
	char			*s;
	struct folder_t		*f;
	struct file_t		*c;
	struct stat		st;
	DIR			*dir;
	struct dirent		*d;
	struct timespec		ts;
	
	/* Initialize the random number generator */
	clock_gettime(CLOCK_REALTIME, &ts);
	srand(ts.tv_nsec);
	
	/* Get names of folders into folders array */
	dir = opendir(globals.rootPath);
	while((d = readdir(dir))) {
		/* We only want folders with no . at the start of their names */
		if(d->d_name[0] != '.') {
			/* Make the array of folders one bigger */
			globals.folders = realloc(globals.folders, (globals.folderCount + 1) * sizeof(struct folder_t));
			
			/* Initialize the new folder entry */
			f = &globals.folders[globals.folderCount];
			f->fileCount = 0;
			f->files = NULL;
			
			/* Copy name into the folder struct */
			f->name = malloc(strlen(d->d_name) + 1);
			strcpy(f->name, d->d_name);
			
			globals.folderCount++;
		}
	}
	closedir(dir);

	/* Sort folders array */
	qsort(globals.folders, globals.folderCount, sizeof(struct folder_t), folderCompare);

	/* For each folder, create it's thumbnail and handle its files */
	for(i = 0; i < globals.folderCount; i++) {
		f = &globals.folders[i];

		/* Populate array of files */
		s = malloc(strlen(globals.rootPath) + strlen(f->name) + 1);
		strcpy(s, globals.rootPath);
		strcat(s, f->name);
		
		dir = opendir(s);
		if(dir == NULL) {
			perror("opendir() failed");
			exit(1);
		}
		while((d = readdir(dir))) {
			/* We only want files with no . at the start of their names, and not icon.avi either */
			if(d->d_name[0] != '.' && strcmp(d->d_name, "icon.avi") != 0) {
				/* Make the array of files one bigger */
				f->files = realloc(f->files, (f->fileCount + 1) * sizeof(struct file_t));
				
				/* Copy name into the file struct */
				f->files[f->fileCount].name = malloc(strlen(d->d_name) + 1);
				strcpy(f->files[f->fileCount].name, d->d_name);
				
				/* And move on to the next one */
				f->fileCount++;
				if(f->fileCount == 255) {
					printf("More than 254 files in folder: %s!\n", f->name);
					exit(1);
				}
			}
		}
		closedir(dir);
		
		/* Check icon.avi is there */
		s = realloc(s, strlen(s) + strlen("/icon.avi") + 1);
		strcat(s, "/icon.avi");
		if(!access(s, F_OK)) {
			makeThumb(s, &f->texture);
		} else {
			fakeThumb(s, &f->texture);
		}

		free(s);

		/* Sort files in this folder by name */
		qsort(f->files, f->fileCount, sizeof(struct file_t), fileCompare);

		/* For each file in this folder, set size and create thumbnail */
		for(j = 0; j < f->fileCount; j++) {
			c = &f->files[j];
			
			s = malloc(strlen(globals.rootPath) + strlen(f->name) + strlen("/") + strlen(c->name) + 1);
			strcpy(s, globals.rootPath);
			strcat(s, f->name);
			strcat(s, "/");
			strcat(s, c->name);

			/* Set size */
			stat(s, &st);
			c->size = st.st_size;

			/* Create thumbnail */
			makeThumb(s, &c->texture);

			free(s);
		}
	}

	/* Blank the screen now that we're done */
	glClear(GL_COLOR_BUFFER_BIT);
	glXSwapBuffers(globals.dpy, globals.win);

}

/* Free everything allocated for thumbnails */
void thumbsUninit(void) {
	uint16_t		i, j;
	struct folder_t		*f;
	struct file_t		*c;

	for(i = 0; i < globals.folderCount; i++) {
		f = &globals.folders[i];
		free(f->name);
		glDeleteTextures(1, &(f->texture));
		for(j = 0; j < f->fileCount; j++) {
			c = &f->files[j];
			free(c->name);
			glDeleteTextures(1, &(c->texture));;
		}
		free(f->files);
	}
	free(globals.folders);
}
