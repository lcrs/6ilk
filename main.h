#define _ISOC99_SOURCE

#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>

#include <sys/time.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>

#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>

#include <libiec61883/iec61883.h>

/* These would be in mwm.h if we had it */
#define MWM_HINTS_FUNCTIONS     1
#define MWM_HINTS_DECORATIONS   2
struct mwmHints_t {
	uint32_t flags;
	uint32_t functions;
	uint32_t decorations;
	uint32_t input_mode;
	uint32_t state;
};

/* Custom types */
/* The various things the mouse can be over - add to the END of this struct, the raw ints are used sometimes! */
enum over_t {
	nothing,	/* Nothing in particular */
	tx,		/* The big output window */
	folders,	/* The row of clip folders */
	grid,		/* The grid of clips */
	foldersSwipe,	/* The top of the window above the folders, for scrolling */
	gridSwipe,	/* The side of the window next to the grid, for scrolling */
	channel0,	/* The channel preview video windows */
	channel1,
	channel2,
	channel0clips,	/* The channels' clip columns */
	channel1clips,
	channel2clips,
	channel0bar,	/* The channels' UI bars below the previews */
	channel1bar,
	channel2bar
};

/* Folder (top row in grid) metadata */
struct folder_t {
	char		*name;			/* Folder name */
	uint32_t	texture;		/* Texture containing thumbnail */
	uint16_t	fileCount;		/* Number of files in folder */
	struct file_t	*files;			/* Dynamic array of files */
};

/* File (clip in grid body) metadata */
struct file_t {
	char		*name;			/* File name */
	uint32_t	texture;		/* Texture containing thumbnail */
	uint32_t	size;			/* Size of file on disk in bytes */
};

/* Clip (on channel) metadata */
struct clip_t {
	struct clip_t	*next;			/* Pointer to next clip, NULL if last */
	char		*path;			/* Path to clip file */
	uint32_t	size;			/* Clip size on disk in bytes */
	uint16_t	planeWidth[3];		/* Width of image planes */
	uint16_t	planeHeight[3];		/* Height of image planes */
	uint32_t	texture;		/* Texture containing thumbnail */
	pthread_t	thread;			/* Decode thread */
	uint8_t		ready;			/* 1 if contexts have been set up and decoding can begin */
	uint32_t	length;			/* Clip length in frames */
	float		fps;			/* Frames per second we're playing at */
	float		multiplier;		/* Speed by n times */
	float		in;			/* In point */
	float		out;			/* Out point */
	sem_t		decodeReady;		/* Decode thread waits on this semaphore for each frame decode */
	sem_t		decodeComplete;		/* Draw thread waits on this semaphore while decoding is done */
};

/* Per-channel animatable attributes which together make a key - add to END, these get indexed as an array of floats! */
struct key_t {
	const float 	shim;			/* 0th index when used as array - allows index 0 to be used to mean "false" */
	float		pos;			/* Position in clip, from 0.0 (begining) to 1.0 (end) */
	float		opacity;		/* Opacity in mix - 1.0 is fully opaque, 0.0 fully transparent */
};

/* Channel */
struct channel_t {
	struct clip_t	*clips;			/* Linked list of clips, NULL if empty */
	uint8_t		clipCount;		/* Number of clips on channel */
	uint8_t		current;		/* Currently playing clip index */
	struct clip_t	*currentClip;		/* Pointer to the currently playing clip */
	uint32_t	ytex, cbtex, crtex;	/* Textures to which video planes are output to by decode thread */
	uint32_t	rgbtex;			/* Texture to which complete video frame is output by renderChannels() in render thread */
	uint32_t	lut;			/* YCbCr to RGB LUT 3D texture */
	struct clip_t	**deletions;		/* Dynamic array of pointers to dead clips to be freed */
	uint8_t		deletionsCount;		/* Number of clips to be freed */
	
	AVFrame		*frameToTexture;	/* Pointer to frame to upload into texture */
	
	/* Animatable attributes */
	struct key_t	now;			/* Current values */
	struct key_t	bar[16];		/* One bar's worth of key frames */
	
	/* Interactive locks for each animatable attribute - animation system won't change atrributes which are != 0.0 here, will store values instead */
	struct key_t	locks;
	
	/* Base position of each attribute when drag started */
	struct key_t	base;
};

/* All sampler-related state in one place */
struct sampler_t {
	sem_t		start;			/* Signals sampler thread to start */
	sem_t		stop;			/* Signals sampler thread to stop */
	
	uint8_t		*dvFrame;		/* Pointer to last copmlete DV frame written by sampler thread */
	uint8_t		*dvPartial;		/* Pointer to DV frame being written by sampler thread */
	uint8_t		*buf1;			/* Pointers to the two actual DV frame buffers used by the sampler */
	uint8_t		*buf2;
	
	uint8_t		doneFirst;		/* Okay to decode sampler frames, first one has been written - nessecary so codec doesn't read a frame full of crap the first time! */
	
	AVCodecContext	*cctx;			/* DV decoder for its input window */
	AVFrame		*frame;			/* YCbCr frame output buffer */
	uint32_t	ytex;			/* Output textures */
	uint32_t	cbtex;
	uint32_t	crtex;
	
	uint32_t	fd;			/* File descriptor being written to, 0 if none */
	char		*path;			/* Path of file being written */
	uint16_t	recLen;			/* Length of current record in frames */
	
	pthread_mutex_t writeMutex;		/* Serialise write() in samplerCallback() and close() in samplerStop() */
	
	uint8_t		thumbToAdd;		/* If we need to generate a thumbnail for a just-recorded clip */
};

/* State vector */
struct state_t {
	uint8_t		dirty;			/* If we need to redraw */
	uint8_t		currentFolder;		/* Index of current folder in grid */
	
	int8_t		folderScrollOffset;	/* How many folders we have scrolled along */
	int8_t		gridScrollOffset;	/* How many rows of clips we have scrolled down */
	int8_t		folderScrollOffsetBase;	/* folderScrollOffset when current scroll started */
	int8_t		gridScrollOffsetBase; 	/* clipScrollOffset when current scroll started */
	
	uint16_t	scrollBase;		/* The window coordinate where the current scroll/drag started */
	
	enum over_t	over;			/* What the mouse is currently over - see enum definition above */
	enum over_t	overPrev;		/* What the mouse has just left */
	uint8_t		overSub;		/* Further information about what the mouse is over - index of folder (or clip) position */
	uint8_t		overSubPrev;		/* Further inormation about what we used to be over */
		
        struct channel_t channels[3];		/* Our 3 video playback channels */	
        struct channel_t prev[3];		/* Our 3 video playback channels as they were in the previous frame */
	
	uint32_t	count;			/* Increments every frame */
	
	struct sampler_t sampler;		/* Sampler-related state */
	
	double		lastTap;		/* Timestamp of last tap */
	double		taps[8];		/* Last 8 valid tempo tap times */
	uint8_t		latestTapIndex;		/* Index of last tap */
	double		tempo;			/* Current average beat time */
};
extern struct state_t state;

/* Global variables, in a stuct just to keep them in one place */
struct globals_t {
	char		*rootPath;		/* Path to root of clip folders */
	
	sem_t		initSem;		/* Semaphore which blocks main() until render thread init done  */
	sem_t		exitSem;		/* Semaphore which blocks main() before uninit */
	
	uint8_t		folderCount;		/* Number of top level folders */
	struct folder_t	*folders;		/* Dynamic array of folders */

	Display		*dpy;			/* X11 connection */
	Window		win;			/* X11 drawable */
	
	uint8_t		fontListBase;		/* Index of the display list where our font starts */
	uint8_t		*fontCharWidths;	/* Array containing pixel widths of each character in our font */
	
	float		defaultlut[8][8][8][3]; /* Normal YCbCr to RGB LUT */
	uint32_t	lut;			/* Normal YCbCr to RGB LUT 3D texture */
	char		*ycbcrtorgb;		/* YCbCr to RGB fragment program */
	
	pthread_mutex_t	glMutex;		/* To serialise GL drawing to prevent calling GL recursively */
	pthread_mutex_t	deletionMutex;		/* Exclusive channel dead clips array lock */
	pthread_mutex_t channelsMutex;		/* Exclusive channel clips array lock */
};
extern struct globals_t globals;

/* Function prototypes - all in here, no other headers */
/* Event handlers */
extern void handleButtonPress(XEvent *event);
extern void handleButtonRelease(XEvent *event);
extern void handleKeyPress(XEvent *event);
extern void handleKeyRelease(XEvent *event);
extern void handleExpose(XEvent *event);
extern void handleMotion(XEvent *event);

/* Mouse pointer hiding */
extern void cursorHide(void);
extern void cursorUnhide(void);

/* Called when mouse leaves, enters and moves within GUI elements */
extern void leave(XMotionEvent *e);
extern void enter(XMotionEvent *e);
extern void within(XMotionEvent *e);

/* Thread entry points */
extern void gui(void *dontcare);
extern void decode(void *v);
extern void render(void *dontcare);
extern void sampler(void *dontcare);

/* Init functions */
extern void guiInit(void);
extern void thumbsInit(void);
extern void drawInit(void);
extern void gridInit(void);
extern void channelsInit(void);
extern void samplerInit(void);

/* Uninit functions */
extern void guiUninit(void);
extern void thumbsUninit(void);
extern void drawUninit(void);
extern void clipsUninit(void);
extern void channelsUninit(void);
extern void samplerUninit(void);

/* GUI element draw functions */
extern void guiDrawAll(void);
extern void gridDraw(void);
extern void foldersDraw(void);
extern void clipsDraw(uint8_t channel);
extern void channelDraw(uint8_t channel);
extern void samplerRender(void);
extern void samplerDraw(void);

/* Assorted graphics stuff */
extern void glDetectError(void);
extern void glPrint(char *s);
extern uint16_t glPrintWidth(char *s);
extern void glSetupColourspaceConversion(void);
extern void glUnsetupColourspaceConversion(void);
extern void glClearRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
extern void lightenRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
extern void darkenRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
extern void sizeChromaPlanes(enum PixelFormat pf, uint16_t widths[3], uint16_t heights[3]);
extern uint8_t isIntra(AVCodecContext *cctx);

/* And the rest */
extern void tap(void);
extern void animate(void);
extern uint8_t animDragging(uint8_t channelIndex);
extern void animAttrSet(struct key_t *k, uint8_t i, float f);
extern float animAttrGet(struct key_t *k, uint8_t i);

extern void channelsLock(void);
extern void channelsUnlock(void);

extern void makeThumb(char *path, uint32_t *texture);
extern void thumbsAddFile(uint8_t folderIndex, char *path);
extern void thumbsDeleteFile(uint8_t folderIndex, uint8_t fileIndex);

extern void gridClick(XButtonEvent *e);
extern void gridFolderScrollStart(XMotionEvent *e);
extern void gridScrollStart(XMotionEvent *e);
extern uint8_t gridFolderScroll(XMotionEvent *e);
extern void gridFolderScrollWheel(XButtonEvent *e);
extern uint8_t gridScroll(XMotionEvent *e);
extern void gridScrollWheel(XButtonEvent *e);

extern void clipClick(XButtonEvent *e);
extern void clipAdd(uint8_t channel, uint8_t position, char *path, uint32_t size, uint32_t texture);
extern void clipMakeCurrent(uint8_t channelIndex, uint8_t clipIndex);
struct clip_t *clipGet(uint8_t channel, uint8_t n);
extern void clipDelete(uint8_t channelIndex, uint8_t clipIndex);
extern void clipsDeleteReally(void);

extern void dragStart(uint8_t channelIndex, uint8_t attrIndex, XButtonEvent *e);
extern void dragMotion(XMotionEvent *e);
extern void dragStop(uint8_t channelIndex, uint8_t attrIndex);

extern void samplerClick(XButtonEvent *e);
extern void samplerStop(void);
extern uint8_t samplerOverClip(enum over_t o);
