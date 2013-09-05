#include "main.h"

/* Clear a rectangle given in object coordinates */
void glClearRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	glEnable(GL_SCISSOR_TEST);
	glScissor(x, 900 - h - y, w, h);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
}

/* Lighten a rectangle of the GUI */
void lightenRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {	
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(1.0, 1.0, 1.0, 0.2);

	glBegin(GL_QUADS);
	glVertex2i(x, y);
	glVertex2i(x + w, y);
	glVertex2i(x + w, y + h);
	glVertex2i(x, y + h);
	glEnd();
	
	glDisable(GL_BLEND);
	glColor4f(1.0, 1.0, 1.0, 1.0);
}

/* Darken a rectangle of the GUI */
void darkenRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {	
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(1.0, 1.0, 1.0, 0.2);

	glBegin(GL_QUADS);
	glVertex2i(x, y);
	glVertex2i(x + w, y);
	glVertex2i(x + w, y + h);
	glVertex2i(x, y + h);
	glEnd();
	
	glDisable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glColor4f(1.0, 1.0, 1.0, 1.0);
}

/* Setup GL to draw YCbCr 4:2:0 video */
void glSetupColourspaceConversion(void) {
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_RECTANGLE_NV);

	glActiveTexture(GL_TEXTURE1);
	glEnable(GL_TEXTURE_RECTANGLE_NV);

	glActiveTexture(GL_TEXTURE2);
	glEnable(GL_TEXTURE_RECTANGLE_NV);

	glActiveTexture(GL_TEXTURE3);
	glEnable(GL_TEXTURE_3D);

	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(globals.ycbcrtorgb), globals.ycbcrtorgb);
}

/* Setup GL to draw RGB interface elements */
void glUnsetupColourspaceConversion(void) {
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_TEXTURE_RECTANGLE_NV);

	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_RECTANGLE_NV);

	glActiveTexture(GL_TEXTURE2);
	glDisable(GL_TEXTURE_RECTANGLE_NV);

	glActiveTexture(GL_TEXTURE3);
	glDisable(GL_TEXTURE_3D);

	glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

/* Quits on error */
void glDetectError(void) {
	uint32_t		r;
	
	r = glGetError();
	
	if(r) {
		printf("GL error: ");
		switch(r) {
			case GL_INVALID_OPERATION:
				printf("GL_INVALID_OPERATION\n");
				break;
			default:
				printf("Something else\n");
		}
		exit(1);
	}
}

/* Modify arrays of image plane widths and heights given a pixelformat */
void sizeChromaPlanes(enum PixelFormat pf, uint16_t widths[3], uint16_t heights[3]) {
	switch(pf) {
		case PIX_FMT_YUV420P:
		case PIX_FMT_YUVJ420P:
			heights[1] /= 2;
			heights[2] /= 2;
			widths[1] /= 2;
			widths[2] /= 2;
			break;
		case PIX_FMT_YUV422P:
		case PIX_FMT_YUVJ422P:
			heights[1] /= 1;
			heights[2] /= 1;
			widths[1] /= 2;
			widths[2] /= 2;
			break;
		case PIX_FMT_YUV410P:
			heights[1] /= 4;
			heights[2] /= 4;
			widths[1] /= 4;
			widths[2] /= 4;
			break;
		default:
			printf("sizeChromaPlanes(): unsupported pixel format: %d\n", pf);
			exit(1);
	}
}

/* Setup texture units and fragment program */
void drawInit(void) {
	uint8_t		i, j, k;
	float		y, cb, cr;
	char 		fragProg[] = "!!ARBfp1.0\n\
	                              TEMP ycbcr;\n\
				      TEMP rgb;\n\
				      PARAM scale = { 0.8032226, 0.875, 0.875, 1.0 };\n\
				      TEX ycbcr.r, fragment.texcoord[0], texture[0], RECT;\n\
			              TEX ycbcr.g, fragment.texcoord[1], texture[1], RECT;\n\
			              TEX ycbcr.b, fragment.texcoord[2], texture[2], RECT;\n\
			              MUL ycbcr, ycbcr, scale;\n\
			              TEX rgb, ycbcr, texture[3], 3D;\n\
				      MUL result.color, rgb, fragment.color;\n\
				      END";
	
	/* YCbCR to RGB fragment program */
	globals.ycbcrtorgb = malloc(strlen(fragProg) + 1);
	strcpy(globals.ycbcrtorgb, fragProg);
	
	/* 3D lookup table for YCrCb to RGB conversion */
	glGenTextures(1, &globals.lut);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_3D, globals.lut);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	
	/* Build and load YCrCb to RGB lookup table */
	for(i = 0; i < 7; i++) {
		for(j = 0; j < 7; j++) {
			for(k = 0; k < 7; k++) {
				y = (float)k / 6.0;
				cb = (float)j / 6.0 - 0.5;
				cr = (float)i / 6.0 - 0.5;
				globals.defaultlut[i][j][k][0] = y + 1.402 * cr;
				globals.defaultlut[i][j][k][1] = y - 0.34414 * cb - 0.71414 * cr;
				globals.defaultlut[i][j][k][2] = y + 1.772 * cb;
			}
		}
	}
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 8, 8, 8, 0, GL_RGB, GL_FLOAT, globals.defaultlut);

	pthread_mutex_init(&globals.glMutex, NULL);
}

void drawUninit(void) {
	glDeleteTextures(1, &globals.lut);
	free(globals.ycbcrtorgb);
}
