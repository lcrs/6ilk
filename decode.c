#include "main.h"

/* Returns 1 if the codec context is using an intra-frame only codec */
uint8_t isIntra(AVCodecContext *c) {
	switch(c->codec_id){
		case CODEC_ID_MJPEG:
		case CODEC_ID_MJPEGB:
		case CODEC_ID_LJPEG:
		case CODEC_ID_DVVIDEO:
		case CODEC_ID_HUFFYUV:
		case CODEC_ID_FFVHUFF:
			return(1);
			break;
		default:
			return(0);
	}
}

/* Video decode threads entry point */
void decode(void *v) {
	uint8_t			channel, clip;
	struct clip_t		*s;
	uint8_t			i;
	uint32_t		r;
	uint8_t			stream;
	int32_t		worked;
	uint32_t		bufferSize;
	AVFormatContext		*fctx;
	AVCodecContext		*cctx;
	AVCodec			*codec;
	AVFrame			*frame;
	AVPacket		*packet;
/*	struct sched_param	param; */
	
	/* Set a low priority while we set up and buffer */
	/* param.sched_priority = 1;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param); */
	
	channel = ((uint8_t *)v)[0];
	clip = ((uint8_t *)v)[1];
	free(v);
	
	s = clipGet(channel, clip);
	
	/* If it would take too long/too much memory to buffer, don't */
	if(s->size > 50 * 1024 * 1024) {
		bufferSize = 0;
	} else {
		bufferSize = s->size;
	}
	
	/* ffmpeg setup */
	pthread_testcancel();
	r = av_open_input_file(&fctx, s->path, NULL, bufferSize, NULL);
	pthread_testcancel();
	if(r) {
		printf("av_open_input_file() returned %d\n", r);
		exit(1);
	}
	av_find_stream_info(fctx);
	for(i = 0; i < fctx->nb_streams; i++) {
		if(fctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
			stream = i;
			break;
		}
	}
	cctx = fctx->streams[stream]->codec;
	codec = avcodec_find_decoder(cctx->codec_id);
	avcodec_open(cctx, codec);
	packet = av_mallocz(sizeof(AVPacket));
	frame = avcodec_alloc_frame();
			
	pthread_cleanup_push((void *)av_close_input_file, (void *)fctx);
	pthread_cleanup_push((void *)avcodec_close, (void *)cctx);
	pthread_cleanup_push((void *)av_free, (void *)packet);
	pthread_cleanup_push((void *)av_free, (void *)frame);
	
	/* Update state */
	s->fps = (float)fctx->streams[stream]->r_frame_rate.num / (float)fctx->streams[stream]->r_frame_rate.den;
	s->length = fctx->duration / ((float)AV_TIME_BASE / s->fps);
	
	s->planeHeight[0] = cctx->height;
	s->planeWidth[0] = cctx->width;
	s->planeHeight[1] = cctx->height;
	s->planeWidth[1] = cctx->width;
	s->planeHeight[2] = cctx->height;
	s->planeWidth[2] = cctx->width;
	
	sizeChromaPlanes(cctx->pix_fmt, s->planeWidth, s->planeHeight);
	
	s->ready = 1;
	if(state.channels[channel].currentClip == NULL) {
		/* There's no current clip and this one has just become ready - make it current */
		channelsLock();
		clipMakeCurrent(channel, 0);
		channelsUnlock();
	}
	
	state.dirty = 1;
	
	/* Set a higher priority for decoding */
	/* param.sched_priority = 2;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param); */
	
	for(;;) {
		pthread_testcancel();
		sem_wait(&s->decodeReady);
				
		/* Decode */
		/* We don't handle inter-frame codecs, basically, but not seeking might help them a little */
		if(isIntra(cctx)) {
			av_seek_frame(fctx, stream, (uint64_t)round(state.channels[channel].now.pos * s->length), 0);
		} else {
			/* We can only properly seek to keyframes... might be okay for looping but not for speed changes */
			/* Do nothing for now */
			printf("silk decode: ignoring seek for inter-frame codec\n");
		}
		
		do {
			av_read_frame(fctx, packet);
		} while(packet->stream_index != stream);
		
		pthread_cleanup_push((void *)av_free_packet, (void *)packet);
		avcodec_decode_video(cctx, frame, &worked, packet->data, packet->size);
		pthread_cleanup_pop(1);
		
		state.channels[channel].frameToTexture = frame;
		
		sem_post(&s->decodeComplete);
	}
	
	/* Never gets here, but these are nessecary because pthread_cleanup_push() above is a nasty macro */
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
}
