import SessionGlobal
import os
import gi
import threading
from gi.repository import Gtk as gtk
from gi.repository import GLib
import FITSViewer

################################################################
# Classes found here
#
# BlurInfo -- implements a single GtkLabel that holds gaussian blur
# value for the composite star in the image
#
# OneStar -- A single star from the stack file's starlist. Used in
# StackerViewInfo.star_data[]
#
# StackerViewInfo -- Information (and Gtk buttons/labels) for a single
# stacked file (and, thus, a single photometric color)
#
# StackTab -- Implements the entire "Stacked Image" tab on the right
# side of the screen.
#
# ImageInfo -- implements a single GtkLabel that holds pixel data for
# the currently-displayed stacked image.
################################################################

class BlurInfo:
    def __init__(self):
        self.root = gtk.Frame()
        self.blur_label = gtk.Label('')
        self.root.add(self.blur_label)

    def set_filename(self, full_path_filename):
        if full_path_filename != None and os.path.isfile(full_path_filename):
            blur_image = "/tmp/StackPaneComposite.fits"
            blur_data_file = "/tmp/StackPaneComposite.txt"
            command = "make_composite "
            command += (" -i " + full_path_filename)
            command += (" -o " + blur_image)
            os.system(command)
            command = "/home/mark/ASTRO/CURRENT/TOOLS/FOCUS_MODEL/analyze_composite "
            command += (" -i " + blur_image)
            command += (" > " + blur_data_file)
            os.system(command)
            f = open(blur_data_file, "r")
            all_blur_data = f.read()
            f.close()
            self.blur_label.set_text(all_blur_data)
        else:
            self.clear()

    def clear(self):
        self.blur_label.set_text('')

    def get_widget(self):
        return self.root

################################################################
#    StackerViewInfo:
# Holds lots of information related to stacking for a single color
# (Parent is the StackTab)
################################################################
class StackerViewInfo:
    def __init__(self, letter, parent):
        color = letter
        self.stack_avail = parent.builder.get_object("Stack_Avail_"+color)
        self.numstars = parent.builder.get_object("Numstars_"+color)
        self.show_button = parent.builder.get_object("Show_"+color)
        self.threshold_button = parent.builder.get_object("Find_Q_"+color)
        self.star_data = []

        self.color = letter
        self.current_file = None

    def set_current_file(self, filename):
        self.current_file = filename
        self.update_buttons()

    def load_starlist(self):
        #print "load_starlist() invoked for " + self.current_file
        starlist_file = "/tmp/list09.out"
        os.system("list_stars -i " + self.current_file + " > " + starlist_file)
        exists = os.path.isfile(starlist_file)
        if exists:
            f = open(starlist_file, "r")
            all_star_lines = f.readlines()
            f.close()

            self.numstars.set_text(str(len(all_star_lines)))
            self.star_data = []
            for oneline in all_star_lines:
                words = oneline.split()
                words[2] = words[2].replace(",", "")
                words[3] = words[3].replace(")", "")
                this_star = OneStar()
                this_star.starname = words[0]
                this_star.x_coord = int(0.5 + float(words[2]))
                this_star.y_coord = int(0.5 + float(words[3]))

                this_star.is_comp = (oneline.find(" COMP ") != -1)
                this_star.is_check = (oneline.find(" CHECK ") != -1)
                this_star.is_submit = (oneline.find(" SUBMIT ") != -1)
                
                self.star_data.append(this_star)
            #os.remove(starlist_file)
        else:
            self.numstars.set_text("  0")

    def update_buttons(self):
        filename = SessionGlobal.stack_files[self.color]
        if filename != None and os.path.isfile(filename):
            print("set_current_file(" + filename + ") invoked.")
            self.stack_avail.set_text("X")
            if self.current_file != filename:
                self.current_file = filename
            self.load_starlist()
        else:
            print("set_current_file(None) invoked.")
            if filename != None:
                print("    but filename was: ", filename)
            self.stack_avail.set_text("  ")
            self.star_data = []
            self.numstars.set_text("  0")
            self.current_file = None

class StackTab:
    def __init__(self):
        self.DoesNotExist = "/tmp/null01234567890741" # will never exist
        builder = gtk.Builder()
        self.builder = builder
        builder.add_from_file("/home/mark/ASTRO/SSUMMARY/TOOLS/SESSION_SUMMARY/StackPane.glade")
        self.svi = {}
        for color in ['B','V','R','I']:
            self.svi[color] = StackerViewInfo(color, self)
        self.renderer_box = builder.get_object("RendererBox")
        self.image_data = builder.get_object("ImageData")
        self.left_box = builder.get_object("stack_magnifier")
        self.top_box = builder.get_object("stack_pane_top")
        self.circle_stars = builder.get_object("Circlestars")
        self.label_stars = builder.get_object("Labelstars")
        self.starname_label = builder.get_object("StarName")
        self.homedir_label = builder.get_object("homedir_label")
        self.seq_summary = builder.get_object("SeqSummaryStuff")
        self.current_filename = self.DoesNotExist

        ## Handlers
        print("StackTab: connecting handlers.")
        for color in ['B','V','R','I']:
            builder.get_object("Stack_"+color).connect("clicked", self.stack_cb, color)
            builder.get_object("Find_"+color).connect("clicked", self.find_cb, color)
            builder.get_object("Show_"+color).connect("toggled", self.change_color_cb, color)
        builder.get_object("Stack_and_Find").connect("clicked", self.stack_and_find_cb, None)
        self.circle_stars.connect("toggled", self.change_starlabel_cb, None)
        self.label_stars.connect("toggled", self.change_starlabel_cb, None)

        self.magnifier = FITSViewer.FitsViewer("StackTab_mag")
        self.magnifier.set_size(300,300)
        self.magnifier.zoom_to(10)

        self.stackimage = FITSViewer.FitsViewer("StackImage")
        self.stackimage.set_size(1270, 1000)
        self.stackimage.SetZoomToFit()

        self.stackimage.follow_renderer(self.magnifier)
        self.stackimage.reset_view_on_click(self.magnifier)
        scroller = gtk.ScrolledWindow()
        scroller.add_with_viewport(self.stackimage.get_widget())

        self.renderer_pane = FITSViewer.RendererPane(self.stackimage, self.renderer_box)

        self.blur = BlurInfo()
        self.left_box.pack_start(self.blur.get_widget(), expand=False, fill=False, padding=0)

        self.top_box.pack_start(scroller, expand=True, fill=True, padding=0)
        self.left_box.pack_start(self.magnifier.get_widget(), expand=False, fill=False, padding=0)

        self.top_box.show_all()

        SessionGlobal.notifier.register(requestor=self,
                                        variable="current_star",
                                        condition="value_change",
                                        callback=self.starchange_cb,
                                        debug="StackTab")

    def update_image_data(self):
        if (self.current_filename != None and
            self.current_filename != self.DoesNotExist and
            os.path.isfile(self.current_filename)):
            (min_data_val, max_data_val, median_data_val,
             num_saturated, avg_data_val, data_max_val) = self.stackimage.get_statistics()
        else:
            (min_data_val, max_data_val, median_data_val,
             num_saturated, avg_data_val, data_max_val) = (0, 0, 0, 0,
                                                           0, 0)

        self.image_data.set_text("{:.0f}\n{:d}\n{:.0f}\n{:.0f}\n{:.0f}\n{:.0f}".
                                 format(data_max_val,
                                        int(num_saturated),
                                        max_data_val,
                                        median_data_val,
                                        avg_data_val,
                                        min_data_val))
                                        #self.stackimage.background_std,
                                        #self.stackimage.stddev_value))
        
    def change_starlabel_cb(self, widget, data):
        for color in ["B","V","R","I"]:
            if self.svi[color].show_button.get_active():
                self.filename_change(color)

    def change_color_cb(self, widget, data):
        color = data
        if widget.get_active():
            self.filename_change(color)

    def filename_change(self, color):
        # set magnifier (if displayed)
        # set main image (if displayed)
        self.current_filename = self.svi[color].current_file
        if self.svi[color].show_button.get_active():
            self.magnifier.load_file(self.current_filename)
            self.stackimage.load_file(self.current_filename)
            self.stackimage.set_starlist(self.svi[color].star_data)
            self.stackimage.set_circle_stars(self.circle_stars.get_active())
            self.stackimage.set_label_stars(self.label_stars.get_active())
            self.blur.set_filename(self.current_filename)
            self.update_image_data()

    def file_content_change(self, color):
        # update magnifier (if displayed)
        # update main image (if displayed)
        self.magnifier.reload_files(force=True)
        self.stackimage.reload_files(force=True)
        self.svi[color].load_starlist()
        self.stackimage.set_starlist(self.svi[color].star_data)
        self.stackimage.set_circle_stars(self.circle_stars.get_active())
        self.stackimage.set_label_stars(self.label_stars.get_active())
        self.blur.set_filename(self.current_filename)
        self.update_image_data()
        
    def update_stack_filenames(self):
        # We set the filenames for each color, even if nothing exists
        # in that color. If there is no current star, set the
        # filenames to a nonsense path.
        for color in ["B","V","R","I"]:
            if SessionGlobal.current_star != None:
                imagefilename = os.path.join(SessionGlobal.homedir,
                                             SessionGlobal.current_star.name + "_" + color + ".fits")
            else:
                imagefilename = self.DoesNotExist

            oldname = self.DoesNotExist
            if color in SessionGlobal.stack_files:
                oldname = SessionGlobal.stack_files[color]
            if imagefilename != oldname:
                SessionGlobal.stack_files[color] = imagefilename
                self.svi[color].set_current_file(imagefilename)
                self.filename_change(color)
                
    def starchange_cb(self, variable, condition, data):
        self.starname_label.set_text(SessionGlobal.current_star.name)
        self.homedir_label.set_text(SessionGlobal.homedir)
        self.update_stack_filenames()
        self.update_sequence_summary()

    def update_sequence_summary(self):
        seq_summary = ''
        for color in ["B","V","R","I"]:
            index_color = color
            if color not in SessionGlobal.current_star.obs_seq:
                index_color += 'c'
                if index_color not in SessionGlobal.current_star.obs_seq:
                    index_color = None
            if index_color != None:
                seq_summary += (color + ': ' +
                                str(len(SessionGlobal.current_star.obs_seq[index_color].exposures)) +
                                ' x ' +
                                str(SessionGlobal.current_star.obs_seq[index_color].exposure_time) +
                                '\n')
            else:
                print("Missing color:", SessionGlobal.current_star.obs_seq)
                seq_summary += (color + ':\n')
        self.seq_summary.set_text(seq_summary)

    def get_widget(self):
        return self.top_box

    def stack_and_find_cb(self, widget, data=None):
        self.pbar = ProgressBar()
        threading.Thread(target=self.stack_and_find_worker).start()

    def stack_and_find_done(self):
        self.pbar.close()

    def stack_and_find_worker(self):
        pbar = self.pbar
        GLib.idle_add(pbar.update, 1.0/16.0, "Stacking B...")
        self.stack_cb(None, 'B')
        GLib.idle_add(pbar.update, 1.0/8.0, "Stacking V...")
        self.stack_cb(None, 'V')
        GLib.idle_add(pbar.update, 2.0/8.0, "Stacking R...")
        self.stack_cb(None, 'R')
        GLib.idle_add(pbar.update, 3.0/8.0, "Stacking I...")
        self.stack_cb(None, 'I')
        GLib.idle_add(pbar.update, 4.0/8.0, "Find_Stars B...")
        self.find_cb(None, 'B')
        GLib.idle_add(pbar.update, 5.0/8.0, "Find_Stars V...")
        self.find_cb(None, 'V')
        GLib.idle_add(pbar.update, 6.0/8.0, "Find_Stars R...")
        self.find_cb(None, 'R')
        GLib.idle_add(pbar.update, 7.0/8.0, "Find_Stars I ...")
        self.find_cb(None, 'I')
        GLib.idle_add(self.stack_and_find_done)

    # Invoked to perform the stacking operation
    def stack_cb(self, widget, data):
        color = data
        print("stack_cb:", data)
        flat_name = os.path.join(SessionGlobal.homedir, "flat_" + data + "c.fits")
        output_file = SessionGlobal.stack_files[color]

        # append 'c' to convert "B" to "Bc"
        image_list = SessionGlobal.thumbs.get_stack_selected_images(data + "c")
        viewer_list = SessionGlobal.thumbs.get_stack_selected_viewers(data + "c")

        if viewer_list == None or viewer_list == []:
            message = gtk.MessageDialog(type=gtk.MessageType.ERROR,
                                        buttons=gtk.ButtonsType.OK)
            message.set_markup("No images selected for stacking in this color")
            message.run()
            message.destroy()
            return
        else:
            dark_name = os.path.join(SessionGlobal.homedir,
                                     "dark" + str(int(0.5+viewer_list[0].get_exposure_time())) + ".fits")

            print("image_list = ", image_list)
            print("dark_name = ", dark_name)
            print("flat_name = ", flat_name)
            print("output_file = ", output_file)

            # Build up the command
            for image_file in image_list.split():
                print("Preprocessing image file ", image_file)
                find_cmd = "find_stars -f "
                find_cmd += (" -d " + dark_name)
                find_cmd += (" -i " + image_file)
                #os.system(find_cmd)
                match_cmd = "star_match -f -e "
                match_cmd += (" -n " + SessionGlobal.current_star.name)
                match_cmd += (" -i " + image_file)
                #os.system(match_cmd)

            stack_cmd = "stack -L -e " # excludes "-x" to improve performance
            stack_cmd += (" -s " + flat_name)
            stack_cmd += (" -d " + dark_name)
            stack_cmd += (" -o " + output_file)
            stack_cmd += (" " + image_list)
            os.system(stack_cmd)
            print("stacking completed.")
            self.svi[color].update_buttons()
            if self.svi[color].show_button.get_active():
                self.file_content_change(color)

            analy_cmd = "analyzer "
            analy_cmd += (" -t " + SessionGlobal.current_star.name)
            analy_cmd += (" -d " + SessionGlobal.homedir)
            analy_cmd += (" > /tmp/analy.out 2>&1")
            os.system(analy_cmd) # This includes do_bvri
            SessionGlobal.RecomputeAllBVRI()
        
    def find_cb(self, widget, data):
        # find_stars -q n.n -i starname_Rc.fits
        # star_match -e -f -b -n starname -i starname_Rc.fits
        color = data
        image_file = SessionGlobal.stack_files[color]

        threshold_string = self.svi[color].threshold_button.get_text()
        command = "find_stars -f "
        command += " -q " + threshold_string
        command += " -i " + image_file
        print("Invoking: " + command)
        os.system(command)
        command = "star_match -h -e -f -b "
        command += " -n " + SessionGlobal.current_star.name
        command += " -i " + image_file
        os.system(command)
        self.svi[color].update_buttons()
        if self.svi[color].show_button.get_active():
            self.file_content_change(color)

class NotUsedAnymore:
    def release_slave(self, slave_to_release):
        self.main_image.release_slave(slave_to_release)

    def attach_slave_renderer(self, new_slave):
        self.main_image.follow_renderer(new_slave)

    def attach_slave_click(self, new_slave):
        self.main_image.reset_view_on_click(new_slave)

    def set_stack_images(self, image_name, color):
        self.main_image.load_file(image_name)
        self.magnifier.load_file(image_name)
        self.magnifier.zoom_to(10)
        if image_name != None:
            self.mainlabel.set_text(image_name);

            
        self.label_min.set_text("Min = {:.1f}".format(min_data_val))
        self.label_max.set_text("Max = {:.1f}".format(max_data_val))
        self.label_median.set_text("Median = {:.1f}".format(median_data_val))

        if image_name != None:
            this_button = self.letter_to_button(color)
            #print "setting starlist with ", len(this_button.star_data), " stars."
            self.main_image.set_starlist(this_button.star_data)
            self.star_info.set_starlist(this_button.star_data)
            self.main_image.set_circle_stars(self.circle_stars_button.get_active())
            self.blur_info.set_filename(image_name)
        else:
            self.blur_info.clear()

    # This will invoke set_stack_images() for whatever stacked
    # filename is appropriate for the current star and selected filter.
    def update_stack_image(self):
        for f_letter in [ 'B', 'V', 'R', 'I' ]:
            this_button = self.letter_to_button(f_letter)
            if this_button.show_button.get_active():
                s_name = os.path.join(SessionGlobal.homedir,
                                      SessionGlobal.current_star.name + '_' + f_letter + '.fits')
                self.set_stack_images(s_name, f_letter)
                #print "setting starlist with ", len(this_button.star_data), " stars."
                self.main_image.set_starlist(this_button.star_data)
                self.star_info.set_starlist(this_button.star_data)
                self.main_image.set_circle_stars(self.circle_stars_button.get_active())
                return

    def change_black_level_cb(self, widget, data=None):
        (black, white) = self.main_image.fitsimage.get_cut_levels()
        text = self.black_entry.get_text()
        new_black = int(text)
        self.main_image.fitsimage.cut_levels(new_black, white)
        self.update_white_black()

    def change_white_level_cb(self, widget, data=None):
        (black, white) = self.main_image.fitsimage.get_cut_levels()
        text = self.white_entry.get_text()
        new_white = int(text)
        self.main_image.fitsimage.cut_levels(black, new_white)
        self.update_white_black()

    def auto_set_button_cb(self, widget, data=None):
        #print "auto_set_button_cb()"
        self.main_image.fitsimage.enable_autocuts('on')
        self.main_image.fitsimage.auto_levels()
        self.update_white_black()

    def set_magnifier_xy(self, x, y):
        self.magnifier.set_center(x, y)

    def get_widget(self):
        return self.mainwin

    def update_white_black(self):
        (black, white) = self.main_image.fitsimage.get_cut_levels()
        #self.magnifier.fitsimage.cut_levels(black, white)
        self.black_entry.set_text(str(black))
        self.white_entry.set_text(str(white))
        print("black level = ", self.black_entry, "white level = ", self.white_entry)

    # Invoked to put the stacked image onto the screen
    def show_stack_cb(self, widget, data):
        print("show_stack_cb:", data)
        # unselect all buttons except this one
        for f_letter in [ 'B', 'V', 'R', 'I' ]:
            if f_letter != data:
                this_button = self.letter_to_button(f_letter)
                this_button.show_button.set_active(False)
                
        stack_name = os.path.join(SessionGlobal.homedir,
                                  SessionGlobal.current_star.name + "_" + data + ".fits")
        self.set_stack_images(stack_name, data)

################################################################
##    class OneStar
##        This is a helper class that holds the definition of a "star"
##    object.
################################################################
class OneStar:
    def __init__(self):
        self.starname = None
        self.x_coord = 0.0
        self.y_coord = 0.0
        self.is_comp = False;
        self.is_check = False;
        self.is_submit = False;

def stack_image_click_cb(widget, event):
    #print "stack_image_click_cb()"
    SessionGlobal.stacker.set_magnifier_xy(event.x, event.y)

################################################################
##    class ProgressBar
################################################################
class ProgressBar(gtk.Window):
    def __init__(self):
        gtk.Window.__init__(self, title="Stack & Find",
                            default_height = 50,
                            default_width = 200)
        self.set_border_width(10)

        vbox = gtk.VBox()
        self.add(vbox)
        self.pbar = gtk.ProgressBar()
        vbox.pack_start(self.pbar, expand=True, fill=True, padding=0)
        self.show_all()

    def update(self, fraction, message):
        self.pbar.set_text(message)
        self.pbar.set_show_text(True)

        self.pbar.set_fraction(fraction)

    def close(self):
        gtk.Window.destroy(self)
