CC=gcc
CFLAGS=-Wall `pkg-config --cflags --libs gstreamer-1.0 gstreamer-plugins-bad-1.0 gstreamer-video-1.0 wayland-client wayland-egl gl egl` -lgstwayland-1.0

debug:clean
	$(CC) $(CFLAGS) -g -o wayland-egl-gst main.c
stable:clean
	$(CC) $(CFLAGS) -o wayland-egl-gst main.c
clean:
	rm -vfr *~ wayland-egl-gst
