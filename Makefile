CC = gcc -c -g
LD = gcc
LIBS = -lpthread -lGL -lX11 -lavformat -lavcodec -lavutil -lz -lrt -liec61883 -lraw1394 -lm

silk: main.o gui.o events.o thumbs.o draw.o grid.o clips.o decode.o render.o channels.o anim.o sampler.o
	$(LD) main.o gui.o events.o thumbs.o draw.o grid.o clips.o decode.o render.o channels.o anim.o sampler.o $(LIBS) -o silk

main.o: main.h main.c
	$(CC) main.c -o main.o
gui.o: main.h gui.c
	$(CC) gui.c -o gui.o
events.o: main.h events.c
	$(CC) events.c -o events.o
thumbs.o: main.h thumbs.c
	$(CC) thumbs.c -o thumbs.o
draw.o: main.h draw.c
	$(CC) draw.c -o draw.o
grid.o: main.h grid.c
	$(CC) grid.c -o grid.o
clips.o: main.h clips.c
	$(CC) clips.c -o clips.o
decode.o: main.h decode.c
	$(CC) decode.c -o decode.o
render.o: main.h render.c
	$(CC) render.c -o render.o
channels.o: main.h channels.c
	$(CC) channels.c -o channels.o
anim.o: main.h anim.c
	$(CC) anim.c -o anim.o
sampler.o: main.h sampler.c
	$(CC) sampler.c -o sampler.o

clean:
	rm *.o silk
