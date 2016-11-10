CC=gcc
PKGCONFIG=`pkg-config --cflags --libs gstreamer-1.0 gstreamer-plugins-bad-1.0 gstreamer-video-1.0 wayland-client wayland-egl gl egl`
FLAGS=-lgstwayland-1.0 $(PKGCONFIG)

debug:clean
	$(CC) $(CFLAGS) $(FLAGS) -Wall -g -o wayland-egl-gst main.c
stable:clean
	$(CC) $(CFLAGS) $(FLAGS) -o wayland-egl-gst main.c
clean:
	rm -vfr *~ wayland-egl-gst
install:
	cp wayland-egl-gst $(INSTALL_ROOT)/usr/bin
