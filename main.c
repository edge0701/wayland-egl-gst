#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <GLES3/gl3.h>
#include <string.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <wayland-client.h>

#define WIDTH 512
#define HEIGHT 512

static gboolean live = FALSE;

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct wl_shell *shell = NULL;
static EGLDisplay egl_display;
static char running = 1;

typedef struct {
	EGLContext egl_context;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_egl_window *egl_window;
	EGLSurface egl_surface;
	GstElement *pipeline;
	GstVideoOverlay *overlay;
	gchar **argv;
	gint current_uri;
} App;

// listeners
static void registry_add_object (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	if (!strcmp(interface,"wl_compositor")) {
		compositor = wl_registry_bind (registry, name, &wl_compositor_interface, 1);
	}
	else if (!strcmp(interface,"wl_shell")) {
		shell = wl_registry_bind (registry, name, &wl_shell_interface, 1);
	}
}
static void registry_remove_object (void *data, struct wl_registry *registry, uint32_t name) {
	
}
static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

static void shell_surface_ping (void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
	wl_shell_surface_pong (shell_surface, serial);
}
static void shell_surface_configure (void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height) {
	App *window = data;
	wl_egl_window_resize (window->egl_window, width, height, 0, 0);
}
static void shell_surface_popup_done (void *data, struct wl_shell_surface *shell_surface) {
	
}
static struct wl_shell_surface_listener shell_surface_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};

static void create_window (App *window, int32_t width, int32_t height) {
	eglBindAPI (EGL_OPENGL_API);
	EGLint attributes[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
	EGL_NONE};
	EGLConfig config;
	EGLint num_config;
	eglChooseConfig (egl_display, attributes, &config, 1, &num_config);
	window->egl_context = eglCreateContext (egl_display, config, EGL_NO_CONTEXT, NULL);
	
	window->surface = wl_compositor_create_surface (compositor);
	window->shell_surface = wl_shell_get_shell_surface (shell, window->surface);
	wl_shell_surface_add_listener (window->shell_surface, &shell_surface_listener, window);
	wl_shell_surface_set_toplevel (window->shell_surface);
	window->egl_window = wl_egl_window_create (window->surface, width, height);
	window->egl_surface = eglCreateWindowSurface (egl_display, config, window->egl_window, NULL);
	eglMakeCurrent (egl_display, window->egl_surface, window->egl_surface, window->egl_context);
}
static void delete_window (App *window) {
	eglDestroySurface (egl_display, window->egl_surface);
	wl_egl_window_destroy (window->egl_window);
	wl_shell_surface_destroy (window->shell_surface);
	wl_surface_destroy (window->surface);
	eglDestroyContext (egl_display, window->egl_context);
}
static void draw_window (App *window) {
	glClearColor (0.0, 1.0, 0.0, 1.0);
	glClear (GL_COLOR_BUFFER_BIT);
	eglSwapBuffers (egl_display, window->egl_surface);
}


////////////////////
/////GSTREAMER//////
////////////////////

static void
on_about_to_finish(GstElement *playbin, App *d) {
	if (d->argv[++d->current_uri] == NULL)
		d->current_uri = 1;

	g_print("Now playing %s\n", d->argv[d->current_uri]);
	g_object_set(playbin, "uri", d->argv[d->current_uri], NULL);
}

static void
error_cb(GstBus *bus, GstMessage *msg, gpointer user_data) {
	App *d = user_data;
	gchar *debug = NULL;
	GError *err = NULL;

	gst_message_parse_error(msg, &err, &debug);

	g_print("Error: %s\n", err->message);
	g_error_free(err);

	if (debug) {
		g_print("Debug details: %s\n", debug);
		g_free(debug);
	}

	gst_element_set_state(d->pipeline, GST_STATE_NULL);
}

static GstBusSyncReply
bus_sync_handler(GstBus *bus, GstMessage *message, gpointer user_data) {
	App *d = user_data;

	printf("bussync\n");

	if (gst_is_wayland_display_handle_need_context_message(message)) {
		GstContext *context;
		//GdkDisplay *display;
		//struct wl_display *display_handle;

		//display = gtk_widget_get_display(d->video_widget);
		//display_handle = gdk_wayland_display_get_wl_display(d->egl_surface);
		context = gst_wayland_display_handle_context_new(display);
		printf("context: %i\n", context);
		gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context);

		goto drop;
	} else if (gst_is_video_overlay_prepare_window_handle_message(message)) {
		//GtkAllocation allocation;
		//GdkWindow *window;
		//struct wl_surface *window_handle;

		/* GST_MESSAGE_SRC (message) will be the overlay object that we have to
         * use. This may be waylandsink, but it may also be playbin. In the latter
         * case, we must make sure to use playbin instead of waylandsink, because
         * playbin resets the window handle and render_rectangle after restarting
         * playback and the actual window size is lost */
		d->overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));

		//gtk_widget_get_allocation(d->video_widget, &allocation);
		//window = gtk_widget_get_window(d->video_widget);
		//window_handle = gdk_wayland_window_get_wl_surface(window);

		gst_video_overlay_set_window_handle(d->overlay, (guintptr) d->surface);
		gst_video_overlay_set_render_rectangle(d->overlay, 0,
											   0, WIDTH, HEIGHT);

		goto drop;
	}

	return GST_BUS_PASS;

	drop:
	gst_message_unref(message);
	return GST_BUS_DROP;
}

/* We use the "draw" callback to change the size of the sink
 * because the "configure-event" is only sent to top-level widgets. */
static gboolean
video_widget_draw_cb(gpointer user_data) {
	App *d = user_data;

	if (d->overlay) {
		gst_video_overlay_set_render_rectangle(d->overlay, 0,
											   0, WIDTH, HEIGHT);
	}

	/* There is no need to call gst_video_overlay_expose().
     * The wayland compositor can always re-draw the window
     * based on its last contents if necessary */

	return FALSE;
}

int main (int argc, char **argv) {
    char cmd[512];
	display = wl_display_connect (NULL);
	struct wl_registry *registry = wl_display_get_registry (display);
	wl_registry_add_listener (registry, &registry_listener, NULL);
	wl_display_roundtrip (display);
	
	egl_display = eglGetDisplay (display);
	eglInitialize (egl_display, NULL, NULL);
	
	//Window window;// = g_slice_new0(window);
	App* _window = g_slice_new0(App);
	create_window (_window, WIDTH, HEIGHT);

	GstBus *bus;
	GError *error = NULL;

	gst_init(&argc, &argv);

	if (argc > 2) {
		_window->argv = argv;
		_window->current_uri = 2;

		sprintf(cmd, "playbin video-sink=%s", argv[1]);

		_window->pipeline = gst_parse_launch(cmd, NULL);
		g_object_set(_window->pipeline, "uri", argv[_window->current_uri], NULL);

		g_signal_connect(_window->pipeline, "about-to-finish",
						 G_CALLBACK(on_about_to_finish), _window);
	} else {
		if (live) {
			_window->pipeline = gst_parse_launch("videotestsrc pattern=18 "
												   "background-color=0x000062FF is-live=true ! waylandsink", NULL);
		} else {
			sprintf(cmd, "videotestsrc pattern=18 "
                                                   "background-color=0x000062FF ! %s", argv[1]);
			_window->pipeline = gst_parse_launch(cmd, NULL);
		}
	}

	bus = gst_pipeline_get_bus(GST_PIPELINE(_window->pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(bus, "message::error", G_CALLBACK(error_cb), _window);
	gst_bus_set_sync_handler(bus, bus_sync_handler, _window, NULL);
	gst_object_unref(bus);

	gst_element_set_state(_window->pipeline, GST_STATE_PLAYING);

	
	while (running) {
		wl_display_dispatch_pending (display);
		draw_window (_window);
	}

	//gst_element_set_state(window.pipeline, GST_STATE_NULL);
	//gst_object_unref(window.pipeline);
	
	delete_window (_window);
	eglTerminate (egl_display);
	wl_display_disconnect (display);
	return 0;
}
