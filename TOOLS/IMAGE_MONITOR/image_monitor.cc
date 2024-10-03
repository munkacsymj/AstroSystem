#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <locale.h>
#include <iostream>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>

#include <Image.h>
#include <camera_api.h>
#include <image_notify.h>

#define NUM_SCALES 5

pthread_t periodic_thread;
bool request_thread_quit {false};

struct {
  GtkWidget *topwindow;		// top-level window

  GtkWidget *image_filename;	   // label holding image filename
  GtkWidget *dark_filename;	   // label holding dark filename
  GtkWidget *flat_filename;	   // label holding flat filename
  GtkWidget *image_data;	   // label holding image
				   // mode/gain/size
  GtkWidget *image_timestamp;	   // label holding image time
  GtkWidget *image_stats;	   // label holding image stats
  GtkWidget *show_grid_menu;	   // turn grid display on/off
  GtkWidget *cooler_data;	   // label holding temperatures
  GtkWidget *image_scale;	   // Says, for example: "Scale 1:1"
  GtkCheckButton *subtract_dark;   // toggle button
  GtkEntry *white_entry;
  GtkEntry *black_entry;
  GtkCheckButton *auto_min_max;
  GtkCheckButton *auto_file_select;
  GtkDrawingArea *magnifier_image; // magnifier image
  cairo_surface_t *magnifier_pixbuf; // magnifier pixbuf
  GtkDrawingArea *main_image;	     // main image
  cairo_surface_t *main_fpixbuf;     // pixbuf for main image FITS
  cairo_surface_t *main_gpixbuf; // pixbuf for graphics overlay
  cairo_surface_t *main_pixbuf;	 // integrated pixbuf (graphics +
				 // image)
  GtkWidget *scale_widget[NUM_SCALES];
} Widgets;

struct {
  int main_binning {1};		// main image binning
  int main_scaling {1};		// main image display scaling
  int main_image_width;		// *before* scaling
  int main_image_height;	// *before* scaling
  int magnifier_width;
  int magnifier_height;
  int magnifier_magnification;
  bool do_image_flip {0};
  double image_black;
  double image_white;
  double pixels_per_arcmin;
  int magnifier_centerx;	// pixel coordinates of main_image
  int magnifier_centery;
  Image *raw_image {nullptr};
} Settings;

struct {
  int pwm_actual;
  double temp_actual;
  double temp_command;
  double humidity;
  bool valid {false};
} Cooler;

//        FORWARD DECLARATIONS
void DisplayImage(void);
void RefreshImage(void);
void RefreshMainImage(void);
void RefreshMagnifier(void);
void FITS2Pixbuf(Image &image);
void ResizeImageWidgets(void);
void ClearOverlayGraphics(void);
void SetupMagnifier(void);
void SetupGrid(void);
void SetupCircles(void);
int Timeout(void *user_data);
void *CoolerThread(void *user_data);
void RefreshFITSHeaderInfo(void);
void DrawOverlayGraphics(void);
void SetImageBlackWhite(void);
void NewExposureCallback(char *ImageFilename);
void NewExposureRaw(char *ImageFilename);
int CoolerTimeout(void *user_data);
void SetBlackWhiteFromImage(void);
void RefreshImageInfo(void);	// Update the text areas in left side
				// of display
extern "C"
void scale_change_cb(GtkWidget *source, gpointer data);
extern "C"
void image_click_cb(GtkWidget *main, GdkEvent *event, gpointer user_data);

static void Terminate(void) {
  disconnect_camera();
  exit(-2);
}

void SetupWidgetsPart1(GtkBuilder *builder) {
  Widgets.topwindow = GTK_WIDGET(gtk_builder_get_object(builder, "topwindow"));
  Widgets.image_filename = GTK_WIDGET(gtk_builder_get_object(builder, "image_filename"));
  Widgets.dark_filename = GTK_WIDGET(gtk_builder_get_object(builder, "dark_filename"));
  Widgets.flat_filename = GTK_WIDGET(gtk_builder_get_object(builder, "flat_filename"));
  Widgets.magnifier_image = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "magnifier_image"));
  Widgets.main_image = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "main_image"));
  Widgets.image_timestamp = GTK_WIDGET(gtk_builder_get_object(builder, "image_timestamp_label"));
  Widgets.show_grid_menu = GTK_WIDGET(gtk_builder_get_object(builder, "show_grid_menu"));
  Widgets.cooler_data = GTK_WIDGET(gtk_builder_get_object(builder, "cooler_data"));
  Widgets.image_scale = GTK_WIDGET(gtk_builder_get_object(builder, "image_scale_widget"));
  Widgets.subtract_dark = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "subtract_dark_checkbutton"));

  Widgets.image_data = GTK_WIDGET(gtk_builder_get_object(builder, "image_data"));
  Widgets.image_stats = GTK_WIDGET(gtk_builder_get_object(builder, "image_stats"));
  Widgets.white_entry = GTK_ENTRY(gtk_builder_get_object(builder, "max_entry"));
  Widgets.black_entry = GTK_ENTRY(gtk_builder_get_object(builder, "min_entry"));
  Widgets.auto_min_max = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "auto_min_max"));
  Widgets.auto_file_select =
    GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "auto_monitor_checkbutton"));

  // Scale widgets (radio buttons)
  for (int i=0; i<NUM_SCALES; i++) {
    char widget_id[12];
    sprintf(widget_id, "scale_%d", i+1);
    Widgets.scale_widget[i] = GTK_WIDGET(gtk_builder_get_object(builder, widget_id));
    g_signal_connect(Widgets.scale_widget[i], "toggled", G_CALLBACK(scale_change_cb),
		     (void *) (intptr_t) (i+1));
  }
}  

void SetupWidgetsPart2(GtkBuilder *builder) {
  Settings.magnifier_width = gtk_widget_get_allocated_width(GTK_WIDGET(Widgets.magnifier_image));
  Settings.magnifier_height = gtk_widget_get_allocated_height(GTK_WIDGET(Widgets.magnifier_image));
  Settings.magnifier_magnification = 8;

  GdkWindow *top_gdk_win = gtk_widget_get_window(GTK_WIDGET(Widgets.topwindow));
  if (top_gdk_win == nullptr) {
    std::cerr << "top_gdk_win is nullptr." << std::endl;
  }
  Widgets.magnifier_pixbuf =
    gdk_window_create_similar_image_surface(top_gdk_win,
					    CAIRO_FORMAT_RGB24,
					    Settings.magnifier_width,
					    Settings.magnifier_height,
					    1);

  Widgets.main_fpixbuf = nullptr;
  Widgets.main_gpixbuf = nullptr;
  Widgets.main_pixbuf = nullptr;

  gtk_widget_add_events(GTK_WIDGET(Widgets.magnifier_image), GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events(GTK_WIDGET(Widgets.main_image), GDK_BUTTON_PRESS_MASK);
  g_signal_connect(GTK_WIDGET(Widgets.magnifier_image), "button-press-event",
		   G_CALLBACK(image_click_cb), nullptr);
  g_signal_connect(GTK_WIDGET(Widgets.main_image), "button-press-event",
		   G_CALLBACK(image_click_cb), nullptr);

  DisplayImage();
}  

void ResizeImageWidgets(void) {
  std::cerr << "ResizeImageWidgets()" << std::endl;
  int width = Settings.main_image_width/Settings.main_scaling;
  int height = Settings.main_image_height/Settings.main_scaling;

  if (Widgets.main_image == nullptr) {
    std::cerr << "main_image == nullptr" << std::endl;
  }
  GtkWidget *main_image_widget = GTK_WIDGET(Widgets.main_image);
  if (main_image_widget == nullptr) {
    std::cerr << "main_image_widget == nullptr" << std::endl;
  }
  gtk_widget_set_size_request(main_image_widget, width, height);
  if (Widgets.main_fpixbuf) cairo_surface_destroy(Widgets.main_fpixbuf);
  Widgets.main_fpixbuf =
    gdk_window_create_similar_image_surface(gtk_widget_get_window(GTK_WIDGET(Widgets.topwindow)),
					    CAIRO_FORMAT_RGB24,
					    width, height, 1);
  if (Widgets.main_gpixbuf) cairo_surface_destroy(Widgets.main_gpixbuf);
  Widgets.main_gpixbuf =
    gdk_window_create_similar_image_surface(gtk_widget_get_window(GTK_WIDGET(Widgets.topwindow)),
					    CAIRO_FORMAT_ARGB32,
					    width, height, 1);
  if (Widgets.main_pixbuf) cairo_surface_destroy(Widgets.main_pixbuf);
  Widgets.main_pixbuf =
    gdk_window_create_similar_image_surface(gtk_widget_get_window(GTK_WIDGET(Widgets.topwindow)),
					    CAIRO_FORMAT_RGB24,
					    width, height, 1);

  // compute image scale
  ImageInfo *info = Settings.raw_image->GetImageInfo();
  const double cdelt = (info->CDeltValid() ? info->GetCDelt1() : 1.52);
  Settings.pixels_per_arcmin = (60.0/cdelt)/(Settings.main_scaling*Settings.main_binning);
}

/* 
 * Unix signals that are cought are written to a pipe. The pipe connects 
 * the unix signal handler with GTK's event loop. The array signal_pipe will 
 * hold the file descriptors for the two ends of the pipe (index 0 for 
 * reading, 1 for writing).
 * As reaction to a unix signal we change the text of a label, hence the
 * label must be global.
 */
int signal_pipe[2];
static char *NewImageFilename;

// The event loop callback that handles the unix signals. Must be a GIOFunc.
// The source is the reading end of our pipe, cond is one of 
// G_IO_IN or G_IO_PRI (I don't know what could lead to G_IO_PRI)
// the pointer d is always NULL

gboolean deliver_signal(GIOChannel *source, GIOCondition cond, gpointer d)
{
  GError *error = NULL;		/* for error handling */

  // There is no g_io_channel_read or g_io_channel_read_int, so we read
  // char's and use a union to recover the unix signal number.
  union {
    gchar chars[sizeof(int)];
    int signal;
  } buf;
  GIOStatus status;		/* save the reading status */
  gsize bytes_read;		/* save the number of chars read */

  // Read from the pipe as long as data is available. The reading end is 
  // also in non-blocking mode, so if we have consumed all unix signals, 
  // the read returns G_IO_STATUS_AGAIN. 

  while((status = g_io_channel_read_chars(source, buf.chars, 
	      sizeof(int), &bytes_read, &error)) == G_IO_STATUS_NORMAL) {
    g_assert(error == NULL);	/* no error if reading returns normal */

    // There might be some problem resulting in too few char's read.
    //Check it.
    if(bytes_read != sizeof(int)){
      fprintf(stderr, "lost data in signal pipe (expected %lu, received %lu)\n",
	      sizeof(int), bytes_read);
      continue;	      /* discard the garbage and keep fingers crossed */
    }

    // Ok, we read a unix signal number, so let the label reflect it! */
    NewExposureCallback(NewImageFilename);
  }
  
  // Reading from the pipe has not returned with normal status. Check for 
  // potential errors and return from the callback.
  if(error != NULL){
    fprintf(stderr, "reading signal pipe failed: %s\n", error->message);
    Terminate();
  }
  if(status == G_IO_STATUS_EOF){
    fprintf(stderr, "signal pipe has been closed\n");
    Terminate();
  }

  g_assert(status == G_IO_STATUS_AGAIN);
  return (TRUE);		/* keep the event source */
}

int main(int argc, char **argv) {
  setlocale(LC_NUMERIC, "");
  gtk_init(&argc, &argv);
  GtkBuilder* builder = gtk_builder_new();
  gtk_builder_add_from_file(builder,
			    "/home/mark/ASTRO/CURRENT/TOOLS/IMAGE_MONITOR/image_monitor.glade",
			    nullptr);
  
  SetupWidgetsPart1(builder);

  gtk_widget_show(Widgets.topwindow);

  g_signal_connect(Widgets.topwindow, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

  gtk_builder_connect_signals(builder, nullptr); // what does this do?
  
  SetupWidgetsPart2(builder);

  int status = pthread_create(&periodic_thread, nullptr, CoolerThread, nullptr);
  if (status) {
    fprintf(stderr, "ERROR: pthread_create() failed: %d\n", status);
  }

  (void) g_timeout_add_seconds(2, CoolerTimeout, nullptr);

#if 0
  /* Teach Xt to use the Display that Gtk/Gdk have already opened.
   */
  XtToolkitInitialize ();
  XtAppContext app = XtCreateApplicationContext ();
  Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
  //XtAppSetFallbackResources (app, defaults);
  XtDisplayInitialize (app, dpy, "image_monitor", "image_monitor", 0, 0, &argc, argv);
  Widget toplevel_shell = XtAppCreateShell ("image_monitor", "image_monitor",
					    applicationShellWidgetClass,
					    dpy, 0, 0);

  dpy = XtDisplay (toplevel_shell);
  //db = XtDatabase (dpy);
  //XtGetApplicationNameAndClass (dpy, &progname, &progclass);
  //XSetErrorHandler (demo_ehandler);

  RegisterAsProvider(app, NewExposureCallback);
#else
  // Set the unix signal handling up.
  // First create a pipe.
  if(pipe(signal_pipe)) {
    perror("pipe");
    Terminate();
  }

  // put the write end of the pipe into nonblocking mode,
  // need to read the flags first, otherwise we would clear other flags too.
  int fd_flags = fcntl(signal_pipe[1], F_GETFL);
  if(fd_flags == -1) {
      perror("read descriptor flags");
      Terminate();
  }
  if(fcntl(signal_pipe[1], F_SETFL, fd_flags | O_NONBLOCK) == -1) {
    perror("write descriptor flags");
    Terminate();
  }

  /* convert the reading end of the pipe into a GIOChannel */
  GIOChannel *g_signal_in = g_io_channel_unix_new(signal_pipe[0]);

  // we only read raw binary data from the pipe, 
  // therefore clear any encoding on the channel
  GError *error = NULL;
  g_io_channel_set_encoding(g_signal_in, NULL, &error);
  if(error != NULL) {		// handle potential errors 
    fprintf(stderr, "g_io_channel_set_encoding failed %s\n",
	    error->message);
    Terminate();
  }

  // put the reading end also into non-blocking mode 
  g_io_channel_set_flags(g_signal_in,     
			 (GIOFlags) (g_io_channel_get_flags(g_signal_in) | G_IO_FLAG_NONBLOCK),
			 &error);

  if(error != NULL) {		/* tread errors */
    fprintf(stderr, "g_io_set_flags failed %s\n",
	    error->message);
    Terminate();
  }

  /* register the reading end with the event loop */
  g_io_add_watch(g_signal_in, (GIOCondition) (G_IO_IN | G_IO_PRI), deliver_signal, NULL);
  
  RegisterAsProviderRaw(NewExposureRaw);
#endif

  gtk_main();
  request_thread_quit = true;
  disconnect_camera();
  return 0;
}

extern "C" 
void callback_image_filename(GtkButton *b, gpointer data) {
  gchar *i_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(b));
  gtk_entry_set_text(GTK_ENTRY(Widgets.image_filename), i_filename);

  DisplayImage();
}

extern "C" 
void callback_dark_filename(GtkButton *b, gpointer data) {
  gchar *d_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(b));
  gtk_entry_set_text(GTK_ENTRY(Widgets.dark_filename), d_filename);

  DisplayImage();
}

extern "C"
void draw_main_image(GtkWidget *main, cairo_t *cr, gpointer data) {
  //fprintf(stderr, "draw_main_image: width = %d, height = %d\n",
  //	  gtk_widget_get_allocated_width(main),
  //	  gtk_widget_get_allocated_height(main));

  if (Widgets.main_pixbuf) {
    cairo_set_source_surface(cr, Widgets.main_pixbuf, 0.0, 0.0);
    cairo_paint(cr);
  }
}

extern "C"
void image_click_cb(GtkWidget *main, GdkEvent *event, gpointer user_data) {
  const bool is_magnifier = (main == GTK_WIDGET(Widgets.magnifier_image));
  GdkEventType event_type = event->type;
  fprintf(stderr, "image_click_cb(): event type = %d\n", event_type);
  if (event_type != GDK_BUTTON_PRESS) {
    fprintf(stderr, "    (wrong type)\n");
  } else {
    double x = event->button.x;
    double y = event->button.y;
    fprintf(stderr, "click at location (%.0lf, %.0lf)\n", x, y);
    if (not is_magnifier) {
      Settings.magnifier_centerx = x;
      Settings.magnifier_centery = y;
    } else {
      fprintf(stderr, "Don't know how to handle magnifier click yet.\n");
    }
  }
  DisplayImage();
}

extern "C"
void draw_magnifier_image(GtkWidget *main, cairo_t *cr, gpointer data) {
  //fprintf(stderr, "draw_magnifier_image() callback invoked.\n");
  if (Widgets.magnifier_pixbuf) {
    cairo_set_source_surface(cr, Widgets.magnifier_pixbuf, 0.0, 0.0);
    cairo_paint(cr);
  }
}

extern "C"
void rebuild_main_image(GtkWidget *source, gpointer data) {
  DisplayImage();
}

extern "C"
void reset_min_max(GtkWidget *b, gpointer data) {
  SetBlackWhiteFromImage();
  DisplayImage();
}

extern "C"
void set_black_white(GtkWidget *b, gpointer data) {
  SetImageBlackWhite();
  DisplayImage();
}

extern "C"
void auto_min_max_cb(GtkWidget *b, gpointer data) {
  bool is_auto = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Widgets.auto_min_max));
  if (is_auto) {
    gtk_widget_set_sensitive(GTK_WIDGET(Widgets.black_entry), false);
    gtk_widget_set_sensitive(GTK_WIDGET(Widgets.white_entry), false);
    SetBlackWhiteFromImage();
    DisplayImage();
  } else {
    gtk_widget_set_sensitive(GTK_WIDGET(Widgets.black_entry), true);
    gtk_widget_set_sensitive(GTK_WIDGET(Widgets.white_entry), true);
  }
}

extern "C"
void refresh_cb(GtkWidget *b, gpointer data) {
  DisplayImage();
}

extern "C"
void scale_change_cb(GtkWidget *source, gpointer data) {
  int button_num = (intptr_t) data;
  fprintf(stderr, "scale_change_cb(%d), widget = %s\n",
	  button_num, gtk_widget_get_name(source));
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(source))) {
    fprintf(stderr, "scale_change_cb() set to %d\n", button_num);
    Settings.main_scaling = button_num;
    DisplayImage();

    char scale_msg[32];
    sprintf(scale_msg, "Scale 1:%d", button_num);
    gtk_label_set_text(GTK_LABEL(Widgets.image_scale), scale_msg);
  }
}

// Key drawing routines:
// DisplayImage() - redraw everything from scratch
// RefreshImage() - refreshes from pixmaps
// RefreshMainImage() - just refreshes the main image from pixmap
// RefreshMagnifierImage() - just refreshes the magnifier from pixmap

void RefreshImage(void) {
  RefreshMainImage();
  RefreshMagnifier();
  gtk_widget_queue_draw_area(GTK_WIDGET(Widgets.main_image),
			     0, 0,
			     Settings.main_image_width/Settings.main_scaling,
			     Settings.main_image_height/Settings.main_scaling);
  gtk_widget_queue_draw_area(GTK_WIDGET(Widgets.magnifier_image),
			     0, 0,
			     Settings.magnifier_width, Settings.magnifier_height);
}

void RefreshMainImage(void) {
  // clear the target pixmap
  cairo_t *cr = cairo_create(Widgets.main_pixbuf);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_paint(cr);

  //fprintf(stderr, "RefreshMainImage() copying from fpixbuf, gpixbuf into main_pixbuf\n");
  
  cairo_set_source_surface(cr, Widgets.main_fpixbuf, 0.0, 0.0);
  cairo_paint(cr);

  cairo_set_source_surface(cr, Widgets.main_gpixbuf, 0.0, 0.0);
  cairo_paint(cr);

  cairo_destroy(cr);

}

void RefreshMagnifier(void) {
  if (Widgets.magnifier_pixbuf == nullptr or
      Widgets.main_fpixbuf == nullptr) return;
  
  cairo_t *cr = cairo_create(Widgets.magnifier_pixbuf);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_paint(cr);

  fprintf(stderr, "RefreshMagnifier() copying from fpixbuf into magnifier_pixbuf\n");
  int src_rootx = Settings.magnifier_centerx -
    Settings.magnifier_width/(2*Settings.magnifier_magnification);
  int src_rooty = Settings.magnifier_centery -
    Settings.magnifier_height/(2*Settings.magnifier_magnification);
  const int src_spanx = Settings.magnifier_width/Settings.magnifier_magnification;
  const int src_spany = Settings.magnifier_height/Settings.magnifier_magnification;

  if (src_rootx < 0) {
    src_rootx = 0;
  } else if (src_rootx >= Settings.main_image_width/Settings.main_scaling - src_spanx) {
    src_rootx = Settings.main_image_width/Settings.main_scaling - src_spanx;
  }
  if (src_rooty < 0) {
    src_rooty = 0;
  } else if (src_rooty >= Settings.main_image_height/Settings.main_scaling - src_spany) {
    src_rooty = Settings.main_image_height/Settings.main_scaling - src_spany;
  }

  const int magrowstride = cairo_image_surface_get_stride(Widgets.magnifier_pixbuf);
  const int irowstride = cairo_image_surface_get_stride(Widgets.main_fpixbuf);
  unsigned char *data_origin = cairo_image_surface_get_data(Widgets.main_fpixbuf);
  unsigned char *data_dest = cairo_image_surface_get_data(Widgets.magnifier_pixbuf);
  for (int y=0; y<Settings.magnifier_height; y++) {
    const int srcy = src_rooty + y/Settings.magnifier_magnification;
    for (int x=0; x<Settings.magnifier_width; x++) {
      const int srcx = src_rootx + x/Settings.magnifier_magnification;

      uint32_t *tgt_pixel = (uint32_t *) (data_dest + y*magrowstride + x*4);
      uint32_t *src_pixel = (uint32_t *) (data_origin + srcy*irowstride + srcx*4);
      *tgt_pixel = *src_pixel;
    }
  }
}

void FITS2Pixbuf(Image &image) {
  // Put the FITS image onto the main_fpixbuf (a cairo surface)
  const int rowstride = cairo_image_surface_get_stride(Widgets.main_fpixbuf);
  const int scaling = Settings.main_scaling;
  unsigned char *data_origin = cairo_image_surface_get_data(Widgets.main_fpixbuf);
  fprintf(stderr, "FITS2Pixbuf: FITS source is %d (w) x %d (h), scaling = %d\n",
	  Settings.main_image_width, Settings.main_image_height, scaling);
  const int dest_width = cairo_image_surface_get_width(Widgets.main_fpixbuf);
  const int dest_height = cairo_image_surface_get_height(Widgets.main_fpixbuf);

  //const int src_height = scaling*(Settings.main_image_height/scaling);
  //const int src_width =  scaling*(Settings.main_image_width/scaling);

  // x,y are in *target* coordinates
  for (int y = 0; y < dest_height; y++) {
    for (int x = 0; x < dest_width; x++) {
      double target = 0.0;
      for (int dy = 0; dy<scaling; dy++) {
	for (int dx = 0; dx<scaling; dx++) {
	  double source_pixel = image.pixel(x*scaling+dx,
					    y*scaling+dy);
	  target += source_pixel;
	}
      }
      target *= (1.0/(scaling*scaling));
      double d_dbl = (0.5+256.0*(target-Settings.image_black)/
		      (Settings.image_white-Settings.image_black));
      if (d_dbl < 0.0) d_dbl = 0.0;
      if (d_dbl > 255.0) d_dbl = 255.0;
      const uint8_t dest = (unsigned int) d_dbl;

      int dest_x = x;
      int dest_y = y;
      if (Settings.do_image_flip) {
	dest_x = (dest_width-1) - x;
	dest_y = (dest_height-1) - y;
      }
      uint32_t *tgt_pixel = (uint32_t *) (data_origin + dest_y*rowstride + dest_x*4);
      *tgt_pixel = (dest << 16) | (dest << 8) | dest;
    }
  }
}

void ClearPixbuf(cairo_surface_t *surf) {
  cairo_t *cr = cairo_create(surf);

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_paint(cr);

  cairo_destroy(cr);
}

void DisplayImage(void) {
  const char *image_filename = gtk_entry_get_text(GTK_ENTRY(Widgets.image_filename));
  const char *dark_filename = gtk_entry_get_text(GTK_ENTRY(Widgets.dark_filename));
  const char *flat_filename = gtk_entry_get_text(GTK_ENTRY(Widgets.flat_filename));

  if (image_filename == nullptr || *image_filename == 0) {
    ClearPixbuf(Widgets.main_fpixbuf);
    ClearPixbuf(Widgets.main_gpixbuf);
    ClearPixbuf(Widgets.magnifier_pixbuf);
  } else {
    if (Settings.raw_image) delete Settings.raw_image;
    Settings.raw_image = new Image(image_filename);
    Image &image = *Settings.raw_image;
    if (dark_filename != nullptr and *dark_filename != 0 and
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Widgets.subtract_dark))) {
      Image dark(dark_filename);
      image.subtract(&dark);
    }
    if (flat_filename != nullptr and *flat_filename != 0) {
      Image flat(flat_filename);
      image.scale(&flat);
    }

    ImageInfo *info = Settings.raw_image->GetImageInfo();
    Settings.do_image_flip = (info and
			      info->NorthIsUpValid() and
			      info->NorthIsUp());

    Settings.main_image_width = image.width;
    Settings.main_image_height = image.height;

    SetImageBlackWhite();
    ResizeImageWidgets();
    
    FITS2Pixbuf(image);
    ClearOverlayGraphics();
    DrawOverlayGraphics();
    SetupMagnifier();
    //SetupGrid();
    //SetupCircles();
    RefreshImageInfo();
    RefreshFITSHeaderInfo();
  }
  RefreshImage();
}

void SetBlackWhiteFromImage(void) {
  if (Settings.raw_image) {
    Statistics *stats = Settings.raw_image->statistics();
    double high_side = std::max(5*stats->StdDev, 1000.0);
    Settings.image_white = stats->MedianPixel + high_side;
    //const double span = Settings.image_white - stats->MedianPixel;
    Settings.image_black = stats->MedianPixel - high_side/4.0;
    std::cout << "image_black set to " << Settings.image_black << std::endl;
    std::cout << "image_white set to " << Settings.image_white << std::endl;

    char entry_string[32];
    sprintf(entry_string, "%.1lf", Settings.image_black);
    gtk_entry_set_text(Widgets.black_entry, entry_string);
    sprintf(entry_string, "%.1lf", Settings.image_white);
    gtk_entry_set_text(Widgets.white_entry, entry_string);
  }
}

void SetImageBlackWhite(void) {
  if (Settings.raw_image and
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Widgets.auto_min_max))) {
    SetBlackWhiteFromImage();
  } else {
    double black_data, white_data;
    sscanf(gtk_entry_get_text(Widgets.black_entry), "%lf", &black_data);
    sscanf(gtk_entry_get_text(Widgets.white_entry), "%lf", &white_data);
    fprintf(stderr, "Setting black/white from widget values (%lf, %lf).\n",
	    black_data, white_data);
    Settings.image_black = black_data;
    Settings.image_white = white_data;
  }
}

void ClearOverlayGraphics(void) {
  // clear the target pixmap
  //fprintf(stderr, "ClearOverlayGraphics()\n");
  cairo_t *cr = cairo_create(Widgets.main_gpixbuf);
  cairo_set_source_rgba(cr, 0, 0, 0, 0.0); // transparent
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);

  cairo_destroy(cr);
}

void SetupMagnifier(void) {
  const int magnification = Settings.magnifier_magnification;
  const int pix_width = cairo_image_surface_get_width(Widgets.main_fpixbuf);
  const int pix_height = cairo_image_surface_get_height(Widgets.main_fpixbuf);
  int center_x = pix_width/2;
  int center_y = pix_height/2;
  int mag_width_main_pixels = Settings.magnifier_width/magnification;
  int mag_height_main_pixels = Settings.magnifier_height/magnification;
  int low_x = center_x - mag_width_main_pixels/2;
  int high_x = center_x + mag_width_main_pixels/2;
  if (low_x < 0) {
    low_x = 0;
    high_x = mag_width_main_pixels;
  }
  if (high_x > Settings.main_image_width-1) {
    high_x = Settings.main_image_width-1;
    low_x = high_x - mag_width_main_pixels;
  }
  int low_y = center_y - mag_height_main_pixels/2;
  int high_y = center_y + mag_height_main_pixels/2;
  if (low_y < 0) {
    low_y = 0;
    high_y = mag_height_main_pixels;
  }
  if (high_y > Settings.main_image_height-1) {
    high_y = Settings.main_image_height-1;
    low_y = high_y - mag_height_main_pixels;
  }

  // Copy from the main_image pixbuf into the magnifier pixbuf
  const int main_rowstride = cairo_image_surface_get_stride(Widgets.main_fpixbuf);
  const int mag_rowstride = cairo_image_surface_get_stride(Widgets.magnifier_pixbuf);

  unsigned char *data_origin = cairo_image_surface_get_data(Widgets.main_fpixbuf);
  unsigned char *data_dest = cairo_image_surface_get_data(Widgets.magnifier_pixbuf);
  
  for (int y = low_y; y < high_y; y++) {
    for (int x = low_x; x < high_x; x++) {
      const uint32_t value = *(uint32_t *)(data_origin + y*main_rowstride + x*4);
      for (int tgty = (y-low_y)*magnification;
	   tgty < (y-low_y+1)*magnification;
	   tgty++) {
	for (int tgtx = (x-low_x)*magnification;
	     tgtx < (x-low_x+1)*magnification;
	     tgtx++) {

	  uint32_t *tgt_pixel = (uint32_t *) (data_dest + tgty*mag_rowstride + tgtx*4);
	  *tgt_pixel = value;
	}
      }
    }
  }
}
  
// Update the text areas in left side of display
void RefreshImageInfo(void) {
  Statistics *stats = Settings.raw_image->statistics();
  ImageInfo *info = Settings.raw_image->GetImageInfo();
  long data_max = 65535;
  char buffer[128];

  if (info and info->DatamaxValid()) {
    data_max = info->GetDatamax();
  }

  sprintf(buffer, "%'.1lf\n%'.1lf\n%'.1lf\n%'.1lf\n%'d\n%'ld",
	  stats->DarkestPixel,
	  stats->AveragePixel,
	  stats->MedianPixel,
	  stats->BrightestPixel,
	  stats->num_saturated_pixels,
	  data_max);		// DATAMAX
  gtk_label_set_text(GTK_LABEL(Widgets.image_stats), buffer);
}

void RefreshFITSHeaderInfo(void) {
  ImageInfo *info = Settings.raw_image->GetImageInfo();

  // Handle DEC/RA
  char dec_ra_string[80];
  if (info->NominalDecRAValid()) {
    DEC_RA *loc = info->GetNominalDecRA();
    sprintf(dec_ra_string, "%s\n%s\n",
	    loc->string_fulldec_of(),
	    loc->string_ra_of());
  } else {
    strcpy(dec_ra_string, "\n\n");
  }

  // Handle image time
  char loc_time[80];
  if (info->ExposureStartTimeValid()) {
    JULIAN t = info->GetExposureStartTime();
    time_t loc_t = t.to_unix();
    strcpy(loc_time, ctime(&loc_t));
    loc_time[strlen(loc_time)-1] = 0; // kill trailing newline
  } else {
    loc_time[0] = 0;
  }

  // Binning, Gain, Mode
  char bin_gain_mode[80];
  sprintf(bin_gain_mode, "%d X %d\n%d\n%d\n",
	  (info->BinningValid() ? info->GetBinning() : 1),
	  (info->BinningValid() ? info->GetBinning() : 1),
	  (info->CamGainValid() ? info->GetCamGain() : 999),
	  (info->ReadmodeValid() ? info->GetReadmode() : 999));
  
  // Exposure Time, Size, North-is-up
  char time_str[80];
  if (info->ExposureDurationValid()) {
    sprintf(time_str, "%.3lf\n", info->GetExposureDuration());
  } else {
    strcpy(time_str, "\n");
  }

  char size_str[80];
  sprintf(size_str, "%d x %d\n",
	  Settings.raw_image->width,
	  Settings.raw_image->height);

  char north_str[80];
  if (info->NorthIsUpValid()) {
    strcpy(north_str, (info->NorthIsUp() ? "true" : "false"));
  } else {
    strcpy(north_str, " ");
  }

  char entire_field[512];
  sprintf(entire_field, "%s%s%s%s%s\n%s",
	  dec_ra_string,
	  bin_gain_mode,
	  time_str,
	  size_str,
	  north_str,
	  info->GetFilter().NameOf());
  gtk_label_set_text(GTK_LABEL(Widgets.image_data), entire_field);
  gtk_label_set_text(GTK_LABEL(Widgets.image_timestamp), loc_time);
}

void DrawOverlayGraphics(void) {
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(Widgets.show_grid_menu))) {
    //fprintf(stderr, "DrawOverlayGraphics(Grid = true)\n");
    // draw grid
    cairo_t *cr = cairo_create(Widgets.main_gpixbuf);
    cairo_set_line_width(cr, 1.0);
    const int width = Settings.raw_image->width/Settings.main_scaling;
    const int height = Settings.raw_image->height/Settings.main_scaling;
    const double mid_x = width/2.0;
    const double mid_y = height/2.0;

    int line_num = 0;
    for (double x=0; x<mid_x; x += Settings.pixels_per_arcmin) {
      if (line_num++ % 5 == 0) {
	cairo_set_source_rgba(cr, 1, 0.65, 0, 1); // orange
      } else {
	cairo_set_source_rgba(cr, 0.85, 1, 0, 1); // yellow
      }	
      cairo_move_to(cr, mid_x+x, 0);
      cairo_rel_line_to(cr, 0, height);
      cairo_move_to(cr, mid_x-x, 0);
      cairo_rel_line_to(cr, 0, height);
      cairo_stroke(cr);
    }
    line_num = 0;
    for (double y=0; y<mid_y; y += Settings.pixels_per_arcmin) {
      if (line_num++ % 5 == 0) {
	cairo_set_source_rgba(cr, 1, 0.65, 0, 1); // orange
      } else {
	cairo_set_source_rgba(cr, 0.85, 1, 0, 1); // yellow
      }	
      cairo_move_to(cr, 0, mid_y+y);
      cairo_rel_line_to(cr, width, 0);
      cairo_move_to(cr, 0, mid_y-y);
      cairo_rel_line_to(cr, width, 0);
      cairo_stroke(cr);
    }
    const double del_rect = 6.0;
    cairo_set_source_rgba(cr, 1, 0.65, 0, 1); // orange
    cairo_rectangle(cr,
		    mid_x-del_rect, mid_y-del_rect,
		    del_rect*2, del_rect*2);
		    
    cairo_stroke(cr);
    cairo_destroy(cr);
  }
  
  // Draw circles and star labels??
}

// This is called from the main process (not the cooler thread)
int CoolerTimeout(void *user_data) {

  if (Cooler.valid) {
    char buffer[512];
    sprintf(buffer, "%d %%\n%.1lf C\n%.1lf C\n%.2lf %%",
	    Cooler.pwm_actual,
	    Cooler.temp_actual,
	    Cooler.temp_command,
	    Cooler.humidity);
  
    gtk_label_set_text(GTK_LABEL(Widgets.cooler_data), buffer);
  }

  return true;
}

void *CoolerThread(void *user_data) {
  connect_to_camera();
  if (not camera_is_available()) pthread_exit(0);

  while(not request_thread_quit) {
    int pwm_actual;
    double temp_commanded;
    double temp_actual;
    double temp_ambient; // unused
    double humidity;
    int cooler_mode;

    CCD_cooler_data(&temp_ambient,
		    &temp_actual,
		    &temp_commanded,
		    &pwm_actual,
		    &humidity,
		    &cooler_mode);

    Cooler.pwm_actual = pwm_actual;
    Cooler.temp_actual = temp_actual;
    Cooler.temp_command = temp_commanded;
    Cooler.humidity = humidity;
    Cooler.valid = true;

    sleep(2);
  }
  return nullptr;
}

void NewExposureCallback(char *ImageFilename) {
  // The sleep() is there because I started running into problems with
  // fitsio reading new image files. I suspect some kind of a timing
  // problem within fitsio is being triggered. This sleep seems to
  // make the problem go away.
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Widgets.auto_file_select))) {
    fprintf(stderr, "Received notification of new file %s available.\n",
	    ImageFilename);
    sleep(1);
    gtk_entry_set_text(GTK_ENTRY(Widgets.image_filename), ImageFilename);
    DisplayImage();
  } else {
    fprintf(stderr, "New image file available (%s), but not enabled.\n",
	    ImageFilename);
  }
}

void NewExposureRaw(char *ImageFilename) {
  int signal = SIGUSR1;
  fprintf(stderr, "Received raw notification of new file %s available.\n",
	  ImageFilename);
  NewImageFilename = ImageFilename;
  if(write(signal_pipe[1], &signal, sizeof(signal)) != sizeof(signal)) {
    fprintf(stderr, "unix signal %d lost\n", signal);
  }
}  
