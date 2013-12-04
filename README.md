6ilk
====

Started this in I guess 2003, and picked it up again in 2013 to make it work on modern Linux.

It records DV from a firewire camera, whilst also playing and mixing three streams of existing DV captures. Used it at a festival one time feeding a vision mixer and a couple projectors.

No big deal these days but I learnt a whole pile about threading, X11, fragment shaders and the ffmpeg APIs.

The interface is a little wild x

Building
========
On Ubuntu 13.04, this should get what you need:

    sudo apt-get install libavformat-dev libavcodec-dev libiec61883-dev mesa-common-dev

Todo
====
* Scrolling on edges is broken under WindowMaker because of the window border... bodged it by moving the window left and up a pixel
* Whiteness on the grid when you stop sampling
* You have to pass the path to the clips folder as argv[1] with a trailing /
* Using absolute-mode Wacom means scrolling past end of clip doesn't wrap like it does with a mouse
