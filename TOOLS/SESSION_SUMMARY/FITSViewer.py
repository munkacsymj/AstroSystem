import gi
from gi.repository import Gtk as gtk
from gi.repository import Gdk, GdkPixbuf
import cairo
import math
import numpy
from astropy.io import fits

################################################################
##        Class RendererSettings
################################################################

class RendererSettings:
    def __init__(self):
        self.linear_white = 65535
        self.linear_black = 0
        self.asinh_alpha = 2.0
        self.asinh_min = 0
        self.asinh_max = 65535
        self.rendering_mode = "linear"

    def print(self):
        print(self.rendering_mode, self.linear_white,
              self.linear_black, self.asinh_alpha, self.asinh_min,
              self.asinh_max) 


################################################################
##        Class FitsViewer(parent)
##
## API summary:
##   .FitsViewer(parent) -- constructor
##   .set_size(height, width)
##   .zoom_to(factor)
##   .set_renderer(mode) -- mode is one of 'linear.auto' 'linear.abs' or 'asinh'
##   .get_maxmin() -- returns tuple
##   .flag_saturated(enable) -- enable either True/False
##   .circle_stars(enable) -- enable either True/False
##   .label_stars(enable) -- enable either True/False
##   .set_center(x, y)
##   .load_file(filename)
##   .load_dark(filename) # might be None
##   .load_flat(filename) # might be None
##   .reset_view_on_click(FitsViewer) # argument is center-"slaved" to
##        self
##   .follow_renderer(FitsViewer) # argument is slaved to pixel values
##   .get_widget() # returns the root Gtk.DrawingArea
##
################################################################

def ImageHDUNumber(hdu_list):
    for n in [0,1]:
        if 'EXPOSURE' in hdu_list[n].header:
            return n
    print("ImageHDUNumber: Cannot find valid HDU keywords")
    raise ValueError

class FitsViewer(object):

    def __init__(self, myname):

        self.root = gtk.DrawingArea()

        self.myname = myname
        self.set_size(512, 512) # default
        self.zoom = 1
        self.pixbuf0 = None
        self.pixbuf1 = None
        self.pixbuf_scaling_x = 1.0
        self.pixbuf_scaling_y = 1.0
        self.flipped = False
        self.darkfile_name = None
        self.flatfile_name = None
        self.filepath = None # image filename
        self.circle_stars = False
        self.star_list = []
        self.label_stars = False
        self.renderer = 'asinh' # none means simple linear stretch
        self.set_center(256, 256)
        self.set_callback("draw", self.draw_callback)
        self.root.add_events(Gdk.EventMask.BUTTON_PRESS_MASK|Gdk.EventMask.BUTTON_RELEASE_MASK)
        self.set_callback("button_release_event", self.image_click_cb)
        self.root.connect("button-press-event", self.image_click_cb)
        self.slaved_renderers = [] # list of other FitsViewers
        self.renderer_is_slaved = False
        self.slaved_views = [] # list of other FitsViewers
        self.view_is_slaved = False
        self.old_filepath = None
        self.associated_renderer = None # this is the RendererPane
        self.settings = RendererSettings() # this loads static defaults
        self.last_click_x = None
        self.last_click_y = None
        self.saturated_pixels_in_entire_image = False
        self.saturated_pixel_list = []
        self.min_data_value = 0.0
        self.max_data_value = 0.0
        self.median_data_value = 0.0
        self.zoom_to_fit = False

    def SetZoomToFit(self):
        self.zoom_to_fit = True

    def associate_with_renderer_pane(self, associated_renderer):
        self.associated_renderer = associated_renderer
        self.settings = associated_renderer.get_settings()

    def image_click_cb(self, widget, event):
        #print("image_click_cb in FitsViewer ", self.myname)
        #print("slaved_views[] has ", len(self.slaved_views), " entries.")
        self.last_click_x = event.x #/self.pixbuf_scaling_x
        self.last_click_y = event.y #/self.pixbuf_scaling_y

        for win in self.slaved_views:
            #print("   passing click on to slaved window ", win.myname)
            win.set_center(event.x/self.pixbuf_scaling_x,
                           event.y/self.pixbuf_scaling_y)

    def release_slave(self, other_viewer):
        if other_viewer in self.slaved_renderers:
            self.slaved_renderers.remove(other_viewer)
        if other_viewer in self.slaved_views:
            self.slaved_views.remove(other_viewer)

    def follow_renderer(self, other_viewer):
        print("follow_renderer() invoked from ", other_viewer.myname,
              " to ", self.myname)
        if other_viewer not in self.slaved_renderers:
            self.slaved_renderers.append(other_viewer)
            other_viewer.renderer_is_slaved = True

    def reset_view_on_click(self, other_viewer):
        if other_viewer not in self.slaved_views:
            self.slaved_views.append(other_viewer)
            other_viewer.view_is_slaved = True

    def zoom_to(self, factor):
        self.zoom = factor

    def set_size(self, horiz, vert):
        self.root.set_size_request(horiz, vert)
        self.window_horiz = horiz
        self.window_vert = vert
        self.root.show()

    def set_renderer(self, mode):
        self.renderer = mode
        
    def set_circle_stars(self, circle):
        self.circle_stars = circle
        self.reload_files()

    def set_starlist(self, stars):
        self.star_list = stars

    def set_label_stars(self, do_label):
        self.label_stars = do_label
        self.reload_files()

    def is_flipped(self):
        return self.flipped

    def set_statistics(self, data_array):
        self.min_data_value = numpy.amin(data_array)
        self.max_data_value = numpy.amax(data_array)
        self.median_data_value = numpy.median(data_array)
        self.stddev_value = numpy.std(data_array)
        self.average_data_value = numpy.average(data_array)
        self.num_saturated = numpy.size(numpy.where(data_array >= self.datamax))

        limits = numpy.quantile(data_array, [0.2, 0.8])
        print("limits = ", limits)
        background_pixels = data_array[numpy.logical_and(data_array > limits[0],
                                                         data_array < limits[1])]
        self.background_std = numpy.std(background_pixels)
        print("background_std = ", self.background_std)

    def get_statistics(self):
        return (self.min_data_value, self.max_data_value,
                self.median_data_value, self.num_saturated, self.average_data_value,
                self.datamax)

    def load_dark(self, darkfilename):
        self.darkfile_name = darkfilename
        self.reload_files()

    def load_flat(self, flatfilename):
        self.flatfile_name = flatfilename
        self.reload_files()

    def load_file(self, filepath):
        if filepath != None:
            print("load_file(" , filepath, ") invoked.")
        else:
            print("load_file(None) invoked.")
        self.filepath = filepath
        self.reload_files()

    def print_partial_row(self, pixels):
        for x in range(8):
            print (pixels[3*x], pixels[3*x+1], pixels[3*x+2])

    def draw_callback(self, widget, ctx):
        #print("draw_callback() for FITSView: ", self.myname)
        if self.pixbuf1 != None:
            ctx.reset_clip() # needed???
            ctx.new_path() # needed???
            #print("  pixbuf.get_n_channels = ", self.pixbuf1.get_n_channels())
            #print("  pixbuf.get_has_alpha = ", self.pixbuf1.get_has_alpha())
            #print("  pixbuf.dimensions = ", self.pixbuf1.get_width(),
            #      "x ", self.pixbuf1.get_height())
            #print("  pixbuf.get_rowstride = ",
            #      self.pixbuf1.get_rowstride())
            #print("  pixbuf.get_colorspace = ",
            #      self.pixbuf1.get_colorspace())
            #print("  callback widget = ", widget)
            #print("  callback context = ", ctx)
            #print("  callback surface = ", ctx.get_target())
            #all_pixels = self.pixbuf1.get_pixels()
            #print("   pixbuf = ", len(all_pixels))
            #self.print_partial_row(all_pixels)

            Gdk.cairo_set_source_pixbuf(ctx, self.pixbuf1, 0, 0)
            ctx.paint() # put the FITS image in place (includes saturated pixel adjustments)

            self.refresh_circles_labels(ctx)

    def set_default_settings(self):
        self.settings.linear_white = int(self.median_data_value + 500)
        whitespan = self.settings.linear_white - self.median_data_value
        #self.settings.linear_black = int(self.median_data_value - 3*self.background_std)
        self.settings.linear_black = int(self.median_data_value - whitespan/4)
        self.settings.asinh_alpha = 2.0
        self.settings.asinh_min = int(self.min_data_value)
        self.settings.asinh_max = int(min(self.max_data_value,
                                          self.min_data_value+3000,
                                          65535))
        if self.associated_renderer != None:
            self.associated_renderer.update_to_settings(self.settings)

    def accept_new_settings(self, settings):
        self.settings = settings
        print("accept_new_settings() received: ")
        settings.print()
        self.reload_files()

        for other in self.slaved_renderers:
            print("sending accept_new_settings() to renderer slave...")
            other.accept_new_settings(settings)

    ################################################################
    ##    renderer()
    ##        Returns numpy flattened array of uchar8
    ################################################################
    def render(self, data_in):
        print("render() invoked with array of ", len(data_in), " elements.")
        if self.settings.rendering_mode == 'asinh':
            alpha = max(0.1, self.settings.asinh_alpha)
            min_data_value = self.settings.asinh_min
            max_data_value = self.settings.asinh_max
            lim0 = math.asinh(min_data_value/alpha)
            lim99 = math.asinh((max_data_value-min_data_value)/alpha)
            span = max(1, (lim99 - lim0))
            rendering_function = numpy.vectorize(lambda
                                                 v,alpha=alpha,lim0=lim0: (math.asinh(v/alpha))/span)
            low_value = min(data_in)
            #rendering_function = numpy.vectorize(lambda v,low_value=low_value: (math.log(math.log(0.1+v-low_value))))
            new_data = rendering_function(data_in)
        else:
            low_limit = self.settings.linear_black
            high_limit = self.settings.linear_white
            print("FITSViewer.render: low_limit = ", low_limit,
                  ", high_limit = ", high_limit)
            new_data = data_in.clip(low_limit, high_limit)

        # Now handle clipping/scaling to range of 0..255
        d_max = max(new_data)
        d_min = min(new_data)
        dynamic_range = (d_max - d_min)
        print("FITSViewer image final data range is ", d_min, ":", d_max)
        scale = 255.0/dynamic_range
        mod_data = (new_data - d_min)*scale
        return numpy.rint(mod_data).astype(numpy.uint8)

    ################################################################
    ##    refresh_cicles_labels()
    ################################################################

    def refresh_circles_labels(self, ctx):
        #print("refresh_circles() invoked in ", self.myname, ".")

        ##################
        ## Draw last_click crosshairs
        ##################
        if self.last_click_x != None and self.last_click_y != None:
            crosshair_size = 5.0
            ctx.set_source_rgb(1.0, 0.0, 1.0) # cyan
            ctx.new_path()
            ctx.set_line_width(1.0)
            ctx.move_to(self.last_click_x,
                        self.last_click_y-crosshair_size)
            ctx.line_to(self.last_click_x,
                        self.last_click_y-1.0)
            ctx.move_to(self.last_click_x,
                        self.last_click_y+1.0)
            ctx.line_to(self.last_click_x,
                        self.last_click_y+crosshair_size)
            ctx.move_to(self.last_click_x-crosshair_size,
                        self.last_click_y)
            ctx.line_to(self.last_click_x-1.0,
                        self.last_click_y)
            ctx.move_to(self.last_click_x+1.0,
                        self.last_click_y)
            ctx.line_to(self.last_click_x+crosshair_size,
                        self.last_click_y)
            ctx.stroke()

        # Circle saturated pixels
        ctx.set_source_rgb(0.3, 0.3, 1.0) # odd, bluish color
        for pixel in self.saturated_pixel_list:
            true_x = pixel[0]
            true_y = pixel[1]
            #if self.flipped:
            #    true_x = self.width - true_x
            #    true_y = self.height - true_y
            ctx.new_path()
            ctx.arc(true_x, true_y, 4.5, 0, 2*math.pi)
            ctx.stroke()
                
            
        if self.circle_stars == False and self.label_stars == False:
            #self.root.queue_draw()
            return

        circle_all = self.circle_stars
        
        counter = 0
        ct_submit = 0
        ct_comp = 0
        ct_check = 0
        ctx.set_line_width(1)
        ctx.select_font_face("sans-serif", cairo.FontSlant.NORMAL, cairo.FontWeight.NORMAL)
        ctx.set_font_size(9)

        for star in self.star_list:
            FlagString = ""
            ctx.set_source_rgb(1.0, 1.0, 0.0) # yellow
            if star.is_check:
                FlagString += " Check"
                ct_check=ct_check+1
                ctx.set_source_rgb(1.0, 0.65, 0.0) # orange
            if star.is_submit:
                FlagString += " Submit"
                ct_submit=ct_submit+1
                ctx.set_source_rgb(1.0, 0.0, 0.0) # red
            if star.is_comp:
                FlagString += " Comp"
                ct_comp=ct_comp+1
                ctx.set_source_rgb(0.0, 1.0, 0.0) # green
                    
            if circle_all or FlagString != "":
                #print("circle center (", star.x_coord, ", ", star.y_coord, ") ", FlagString, star.starname)
                #ctx.translate(star.x_coord, star.y_coord) ## ???
                true_x = star.x_coord*self.pixbuf_scaling_x
                true_y = star.y_coord*self.pixbuf_scaling_y
                if self.flipped:
                    true_x = (self.width - star.x_coord)*self.pixbuf_scaling_x
                    true_y = (self.height - star.y_coord)*self.pixbuf_scaling_y
                ctx.new_path()
                ctx.arc(true_x, true_y, 4.5, 0, 2*math.pi)
                ctx.stroke()

                if self.label_stars:
                    ctx.move_to(true_x + 6, true_y + 4)
                    ctx.show_text(star.starname)
            
                counter=counter+1

        #print("refresh_circles() circled ", counter,
        #      " stars (var/comp/check = ", ct_submit, ct_comp, ct_check, ").")
        #self.root.queue_draw()

        
        
    def set_callback(self, callbackname, callback_func):
        self.root.connect(callbackname, callback_func)

    def click_cb(self, arg1, arg2):
        print("click_cb(", arg1, ", ", arg2, ")")

    def get_widget(self):
        return self.root

    def highlight_pixel(self, x, y):
        c = Circle(x=x, y=y, coord="data",
                   radius=1.0, color="blue")
        self.circle_canvas.add(c)

    # This method returns a numpy matrix with the image (or None)
    def image_data(self, fits_filename):
        if fits_filename == None:
            return None
        hdu_list = fits.open(fits_filename, ignore_missing_end=True)
        hdu_num = ImageHDUNumber(hdu_list)
        width = hdu_list[hdu_num].header['NAXIS1']
        height = hdu_list[hdu_num].header['NAXIS2']
        raw_data = hdu_list[hdu_num].data # numpy matrix of floats
        hdu_list.close()
        return raw_data
        
    def reload_files(self, force=False):
        usable_width = int(self.window_horiz/self.zoom)
        usable_height = int(self.window_vert/self.zoom)
        self.saturated_pixel_list = []
        if self.filepath == None:
            rendered_data = numpy.zeros((usable_width, usable_height),
                                        numpy.uint8)
            flattened_data = []
            self.old_filepath = None
            self.set_starlist([])
            render_width = usable_width
            render_height = usable_height
        else:
            if self.filepath != self.old_filepath or force:
                # Grab essential info from the image's .fits file
                self.old_filepath = self.filepath
                self.last_click_x = None
                self.last_click_y = None
                # a new image is being loaded
                print("reload_file(",'"', self.filepath, '")', "for ", self.myname)
                hdu_list = fits.open(self.filepath,ignore_missing_end=True)
                hdu_num = ImageHDUNumber(hdu_list)
                self.flipped = hdu_list[hdu_num].header['NORTH-UP']
                #print("self.flipped = ", self.flipped)
                if 'DATAMAX' in hdu_list[hdu_num].header:
                    self.datamax = hdu_list[hdu_num].header['DATAMAX']
                else:
                    self.datamax = 65530
                self.width = hdu_list[hdu_num].header['NAXIS1']
                self.height = hdu_list[hdu_num].header['NAXIS2']
                #print("Image width = ", self.width, ", height = ", self.height)
                #print("Image HDU number = ", hdu_num)
                raw_data = hdu_list[hdu_num].data # numpy matrix of floats
                #raw_data = hdu_list[].data # numpy matrix of floats
                #print("len(raw_data) = ", len(raw_data))
                self.saturated_pixels_in_entire_image = (numpy.max(raw_data) > self.datamax)
                self.exposure_time = hdu_list[hdu_num].header['EXPOSURE']
                #print("load_file: set self.exposure_time to [", self.exposure_time, "], self = ", self)
                hdu_list.close()
            
                # Load and subtract the dark
                if self.darkfile_name != None:
                    dark = self.image_data(self.darkfile_name)
                    self.composite_data = raw_data - dark
                else:
                    self.composite_data = raw_data

                # Load and scale by the flat
                if self.flatfile_name != None:
                    flat = self.image_data(self.flatfile_name)
                    self.composite_data = composite_data / flat
                
                self.set_statistics(self.composite_data)

                # Now okay to handle flip (after dark/flat adj)
                if self.is_flipped():
                    #print("Flipping the numpy array.")
                    temp = numpy.fliplr(self.composite_data)
                    self.composite_data = numpy.flipud(temp)

                #print("load_files using min_data = ", self.min_data_value)
                self.set_default_settings()
                if self.associated_renderer != None:
                    #print("updating associated RendererPane with black = ", self.settings.linear_black)
                    self.associated_renderer.update_to_settings(self.settings)
                else:
                    pass
                    #print("no associated_renderer in FITViewer ", self.myname)

            # Now compute the slice that we want
            #print("FITSViewer: ", usable_width, usable_height, self.zoom, self.center_x, self.center_y)
            #print("FITS window (width,height): ", self.window_horiz, self.window_vert)

            # remember that self.width and self.height describe the
            # original image, not the window that it will go into.
            if self.zoom_to_fit:
                usable_width = self.width
                usable_height = self.height
                
            if usable_width >= self.width:
                left_edge = 0
                right_edge = self.width-1
                render_width = self.width
            else:
                left_edge = int(max(0, self.center_x - usable_width/2))
                right_edge = int(min(left_edge + usable_width, self.width))-1
                left_edge = int(min(left_edge, 1+right_edge - usable_width))
                render_width = usable_width

            if usable_height >= self.height:
                top_edge = 0
                bottom_edge = self.height-1
                render_height = self.height
            else:
                top_edge = int(max(0, self.center_y - usable_height/2))
                bottom_edge = int(min(top_edge + usable_height, self.height))-1
                top_edge = int(min(top_edge, 1+bottom_edge - usable_height))
                render_height = usable_height
                
            self.left_edge = left_edge
            self.top_edge = top_edge
            self.right_edge = right_edge
            self.bottom_edge = bottom_edge
            #print("FITSViewer: center at ", self.center_x, self.center_y)
            #print("FITSViewer: using box corners (", left_edge, ", ", top_edge, "); (",
            #      right_edge, ", ", bottom_edge, ")")
            sliced_data = self.composite_data[top_edge:bottom_edge+1,left_edge:right_edge+1]
            #print("composite_data has ", len(composite_data), " elements; sliced_data has ",
            #      len(sliced_data), " elements.")

            # and flatten into a single numpy array
            flattened_data = sliced_data.flatten()

            # Now go through the rendering process on the flat numpy array
            rendered_data = self.render(flattened_data)
            # self.render() returns a flattened numpy array of uchar8

            #dummy_pixbuf = GdkPixbuf.Pixbuf(GdkPixbuf.Colorspace.RGB,
                                     #False, 8, self.window_horiz, self.window_vert)
            #print("flattened_data has ", len(flattened_data), " elements")
            #print("rendered_data  has ", len(rendered_data),  " elements")
        self.ba_orig = bytearray(rendered_data)
        # 3 channels: RGB
        self.ba_full = bytearray(3*render_width*render_height)
        self.ba_full[0::3] = self.ba_orig[::1]
        self.ba_full[1::3] = self.ba_orig[::1]
        self.ba_full[2::3] = self.ba_orig[::1]

        #print("ba_orig has ", len(self.ba_orig), " elements.")
        #self.print_partial_row(self.ba_orig)
        #print("ba_full has ", len(self.ba_full), " elements.")
        #self.print_partial_row(self.ba_full)

        #MISSING: Now perform saturated pixel flagging
        if len(flattened_data) > 0:
            self.ba_full = self.highlight_saturated_pixels(flattened_data, self.ba_full)
        #print("returned ", len(self.ba_full), " bytes.")

        #self.pixbuf0 = GdkPixbuf.Pixbuf.new_from_data(self.ba_full,
        #                                             GdkPixbuf.Colorspace.RGB,
        #                                             False,
        #                                             8,
        #                                             usable_width,
        #                                             usable_height,
        #                                             3*usable_width)

        header = b"P6 %d %d 255\n" % (render_width, render_height)
        data = bytes(self.ba_full)
        loader = GdkPixbuf.PixbufLoader.new()
        loader.write(header)
        loader.write(data)
        loader.close()
        self.pixbuf0 = loader.get_pixbuf()

        self.pixbuf_scaling_x = self.window_horiz/render_width
        self.pixbuf_scaling_y = self.window_vert/render_height

        if self.pixbuf_scaling_x < 1.0 or self.pixbuf_scaling_y < 1.0:
            if self.zoom > 1.01:
                scaling_type =GdkPixbuf.InterpType.NEAREST
            else:
                scaling_type =GdkPixbuf.InterpType.BILINEAR
            
                self.pixbuf1 = self.pixbuf0.scale_simple(self.window_horiz,
                                                 self.window_vert,
                                                 scaling_type)
        else:
            self.pixbuf1 = self.pixbuf0.copy()
            self.pixbuf_scaling_x = 1.0
            self.pixbuf_scaling_y = 1.0
            
        #print("window_h = ", self.window_horiz,
        #      ", window_v = ", self.window_vert,
        #      ", render_w = ", render_width,
        #      ", render_h = ", render_height);
        #print("scaling_x = ", self.pixbuf_scaling_x,
        #      ", scaling_y = ", self.pixbuf_scaling_y)
        
        #if self.zoom != 1:
         #   self.pixbuf1 = self.pixbuf0.scale_simple(self.window_horiz,
          #                                           self.window_vert,
         #                                            GdkPixbuf.InterpType.NEAREST)
        #else:
        #    self.pixbuf1 = self.pixbuf0.copy()

        self.root.queue_draw()
        #self.root.emit("draw") # force a draw event to refresh the window

    ############################################
    ## data_raw is a numpy array; data_to_display is a bytearray
    ## with 3 bytes per pixel
    ############################################
    def highlight_saturated_pixels(self, data_raw, data_to_display):
        print("highlight_saturated_pixels len(", len(data_raw), "x", len(data_to_display), ")")
        threshold = self.datamax
        # numpy array iterator
        if data_raw.max() < threshold:
            return data_to_display
        (x_coord, ) = numpy.where(data_raw > threshold)
        num = len(x_coord)
        if num > 0:
            print("Number saturated pixels = ", num)

        # saturated_pixel_list gets the [x,y] coords of all the
        # saturated pixels as long as there are no more than 20 of
        # them 
        if num < 20 and self.zoom == 1:
            self.saturated_pixel_list = [
                [i % self.width, int(i/self.height)]
                for i in x_coord]
            print("Saturated Pixel List: ", self.saturated_pixel_list)
            print("Saturated index list: ", x_coord)
            print("self.width = ", self.width, ", self.height = ",
                  self.height, ", self.zoom = ", self.zoom)
            print("self.center_xy = ", self.center_x, ", ",
                  self.center_y)
            print("self.window_horiz = ", self.window_horiz,
                  ", self.window_vert = ", self.window_vert)
            print("self.edge.bottom/top/left/right = ",
                  self.bottom_edge, self.top_edge, self.left_edge, self.right_edge)
        for i in x_coord:
            data_to_display[3*i] = 0 # Red
            data_to_display[3*i+1] = 0 # Green
            data_to_display[3*i+2] = 255 # blue
        return data_to_display

    def any_saturated_pixels(self):
        return self.saturated_pixels_in_entire_image

    def get_exposure_time(self):
        #print "get_exposure_time() invoked for self = ", self
        return self.exposure_time
    
    def open_file(self, w):
        self.select.popup("Open FITS file", self.load_file)

    def drop_file(self, fitsimage, paths):
        fileName = paths[0]
        self.load_file(fileName)

    def set_center(self, center_x, center_y):
        print("set_center called for FitsViewer ", self.myname)
        self.center_x = center_x
        self.center_y = center_y
        self.reload_files()

    def set_black_white(self, black, white):
        print("Unimplemented: set_cut_levels(black, white)")
        #self.fitsimage.cut_levels(black, white)

    def quit(self, w):
        gtk.main_quit()
        return True

################################################################
##        class RendererPane()
##    This class handles all the white/black level stuff
################################################################
class RendererPane:
    def __init__(self, associated_FITSViewer, parent_box):
        self.fits_viewer = associated_FITSViewer
        self.settings = RendererSettings()
        
        self.renderer_box = parent_box
        linear_frame = gtk.Frame()
        self.renderer_box.pack_start(linear_frame, fill=True,
                                     expand=True, padding=0)
        self.linear_box = gtk.VBox(spacing=2,border_width=2)
        linear_frame.add(self.linear_box)

        self.black_label = gtk.Label('Black = ')
        self.black_entry = gtk.Entry()
        self.black_entry.set_width_chars(5)
        #self.black_entry.set_max_width_chars(5)
        self.black_entry.connect("activate", self.change_setting_cb, "linear-black")
        self.white_label = gtk.Label('White = ')
        self.white_entry = gtk.Entry()
        self.white_entry.set_width_chars(5)
        #self.white_entry.set_max_width_chars(5)

        self.white_entry.connect("activate", self.change_setting_cb, "linear-white")

        self.linear_button = gtk.RadioButton(group=None, label='Linear')
        self.linear_box.pack_start(self.linear_button, fill=False,
                                   expand=False, padding=0)
        self.linear_button.connect("clicked", self.mode_select_cb, "linear")

        black_box = gtk.HBox(spacing=2)
        black_box.pack_start(self.black_label, fill=False, expand=False, padding=0)
        black_box.pack_start(self.black_entry, fill=False, expand=False, padding=0)
        self.linear_box.pack_start(black_box, fill=False, expand=False, padding=0)
        white_box = gtk.HBox(spacing=2)
        white_box.pack_start(self.white_label, fill=False, expand=False, padding=0)
        white_box.pack_start(self.white_entry, fill=False, expand=False, padding=0)
        self.linear_box.pack_start(white_box, fill=False, expand=False, padding=0)

        self.asinh_box = gtk.VBox(spacing=2,border_width=2)
        asinh_frame = gtk.Frame()
        self.renderer_box.pack_start(asinh_frame, fill=True,expand=True, padding=0)
        asinh_frame.add(self.asinh_box)

        self.asinh_alpha_label = gtk.Label('Alpha = ')
        self.asinh_alpha_entry = gtk.Entry()
        self.asinh_alpha_entry.set_width_chars(5)
        #self.asinh_alpha_entry.set_max_width_chars(5)
        self.asinh_alpha_entry.connect("activate",
                                       self.change_setting_cb, "asinh-alpha")
        self.asinh_black_label = gtk.Label('Black = ')
        self.asinh_black_entry = gtk.Entry()
        self.asinh_black_entry.set_width_chars(5)
        #self.asinh_black_entry.set_max_width_chars(5)
        self.asinh_black_entry.connect("activate",
                                       self.change_setting_cb, "asinh-min")
        self.asinh_white_label = gtk.Label('White = ')
        self.asinh_white_entry = gtk.Entry()
        self.asinh_white_entry.set_width_chars(5)
        #self.asinh_white_entry.set_max_width_chars(5)
        self.asinh_white_entry.connect("activate",
                                       self.change_setting_cb, "asinh-max")

        self.asinh_button = gtk.RadioButton(group=self.linear_button, label='Asinh')
        self.asinh_button.connect("clicked", self.mode_select_cb, "asinh")
        self.asinh_box.pack_start(self.asinh_button, fill=False,
                                  expand=False, padding=0)

        black_box = gtk.HBox(spacing=2)
        black_box.pack_start(self.asinh_black_label, fill=False, expand=False, padding=0)
        black_box.pack_start(self.asinh_black_entry, fill=False, expand=False, padding=0)
        self.asinh_box.pack_start(black_box, fill=False, expand=False, padding=0)
        white_box = gtk.HBox(spacing=2)
        white_box.pack_start(self.asinh_white_label, fill=False, expand=False, padding=0)
        white_box.pack_start(self.asinh_white_entry, fill=False, expand=False, padding=0)
        self.asinh_box.pack_start(white_box, fill=False, expand=False, padding=0)
        alpha_box = gtk.HBox(spacing=2)
        alpha_box.pack_start(self.asinh_alpha_label, fill=False, expand=False, padding=0)
        alpha_box.pack_start(self.asinh_alpha_entry, fill=False, expand=False, padding=0)
        self.asinh_box.pack_start(alpha_box, fill=False, expand=False, padding=0)

        self.fits_viewer.associate_with_renderer_pane(self)
        self.update_to_settings(self.settings)

    def mode_select_cb(self, widget, data):
        self.settings.rendering_mode = data
        self.fits_viewer.accept_new_settings(self.settings)

    def change_setting_cb(self, widget, data=None):
        print("New setting: ", data, " set to ", widget.get_text())

        if data == "asinh-max":
            self.settings.asinh_max = int(widget.get_text())
        elif data == "asinh-min":
            self.settings.asinh_min = int(widget.get_text())
        elif data == "asinh-alpha":
            self.settings.asinh_alpha = float(widget.get_text())
        elif data == "linear-black":
            self.settings.linear_black = int(widget.get_text())
        elif data == "linear-white":
            self.settings.linear_white = int(widget.get_text())
        else:
            print("RendererPane: invalid entry type in change_setting_cb: ", data)

        self.fits_viewer.accept_new_settings(self.settings)
    
    def get_settings(self):
        return self.settings

    def update_to_settings(self, settings):
        print("Settings reset from FITSViewer: ")
        settings.print()
        
        self.black_entry.set_text(str(settings.linear_black))
        self.white_entry.set_text(str(settings.linear_white))
        self.asinh_alpha_entry.set_text(str(settings.asinh_alpha))
        self.asinh_white_entry.set_text(str(settings.asinh_max))
        self.asinh_black_entry.set_text(str(settings.asinh_min))

        print("RendererPane set black_entry to ", settings.linear_black)

    def get_widget(self):
        return self.renderer_box
        
