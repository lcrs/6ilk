COMPILER = gcc -c -Wall -g
LINKER = gcc -Wall
LIBS = -L/usr/local/lib -lpthread -lGL -lavformat -lavcodec -lavutil -lz -lrt -liec61883 -lm

silk: main.o gui.o events.o thumbs.o draw.o grid.o clips.o decode.o render.o channels.o anim.o sampler.o
	$(LINKER) main.o gui.o events.o thumbs.o draw.o grid.o clips.o decode.o render.o channels.o anim.o sampler.o $(LIBS) -o silk

main.o:      main.h main.c
	$(COMPILER) main.c -o main.o
gui.o:       main.h gui.c
	$(COMPILER) gui.c -o gui.o
events.o:    main.h events.c
	$(COMPILER) events.c -o events.o
thumbs.o:    main.h thumbs.c
	$(COMPILER) thumbs.c -o thumbs.o
draw.o:      main.h draw.c
	$(COMPILER) draw.c -o draw.o
grid.o:      main.h grid.c
	$(COMPILER) grid.c -o grid.o
clips.o:     main.h clips.c
	$(COMPILER) clips.c -o clips.o
decode.o:    main.h decode.c
	$(COMPILER) decode.c -o decode.o
render.o:    main.h render.c
	$(COMPILER) render.c -o render.o
channels.o:  main.h channels.c
	$(COMPILER) channels.c -o channels.o
anim.o:      main.h anim.c
	$(COMPILER) anim.c -o anim.o
sampler.o:   main.h sampler.c
	$(COMPILER) sampler.c -o sampler.o

clean:
	rm *.o silk
