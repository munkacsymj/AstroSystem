import SessionGlobal
import os
import gi
from gi.repository import Gtk as gtk
import FITSViewer

class BlurInfo:
    def __init__(self):
        self.root = gtk.Frame()
        self.blur_label = gtk.Label('')
        self.root.add(self.blur_label)

    def set_filename(self, full_path_filename):
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

    def clear(self):
        self.blur_label.set_text('')

class StarInfo:
    def __init__(self):
        self.root = gtk.Frame()
        self.box = gtk.VBox()
        self.root.add(self.box)

        self.star_label = gtk.Label('')
        self.star_xy_location = gtk.Label('')
        self.star_flags = gtk.Label('')

        self.box.pack_start(self.star_label,expand=False,fill=False,padding=0)
        self.box.pack_start(self.star_xy_location,expand=False,fill=False,padding=0)
        self.box.pack_start(self.star_flags,expand=False,fill=False,padding=0)

    def set_associated_viewer(self, fits_viewer):
        self.fits_viewer = fits_viewer

    def set_starlist(self, stars):
        self.all_stars = stars

    def set_center(self, x, y):
        if self.fits_viewer.flipped:
            self.set_xy(self.fits_viewer.width - x,
                        self.fits_viewer.height - y)
        else:
            self.set_xy(x, y)

    def set_xy(self, x, y):
        # Search all_stars for a close star
        closest = (512.0*512.0)
        closest_star = None

        for star in self.all_stars:
            distance_x = (x - star.x_coord)
            distance_y = (y - star.y_coord)
            distance_sq = (distance_x*distance_x +
                           distance_y*distance_y)
            if distance_sq < closest:
                closest = distance_sq
                closest_star = star

        if closest < 25.0: # 25 => 5 pixels (squared)
            self.star_label.set_text(closest_star.starname)
            self.star_xy_location.set_text('Pixel center = ({:.1f},{:.1f})'.
                                           format(closest_star.x_coord,
                                                  closest_star.y_coord))
            flags = ''
            if closest_star.is_submit:
                flags += 'Variable '
                print("Selected star is variable.")
            if closest_star.is_comp:
                flags += 'Comp '
                print("Selected star is comp.")
            if closest_star.is_check:
                flags += 'Check '
                print("Selected star is check.")
            self.star_flags.set_text(flags)

    def clear_star(self):
        self.star_label.set_text('')
        self.star_xy_location.set_text('')
        self.star_flags.set_text('')
        
class StackControlPane:
    def __init__(self):
        self.main_win = gtk.VBox()
        #self.magnifier = FITSViewer.FitsViewer("StackWin.Magnifier")
        #self.magnifier.set_size(200, 200)
        #self.magnifier.zoom_to(10)
        #self.attach_slave_renderer(self.magnifier)
        #self.attach_slave_click(self.magnifier)

        self.stacker_table = gtk.Table(rows=5, columns=8, homogeneous=False)
        self.main_win.pack_start(self.stacker_table, fill=False, expand=False, padding=0)
        #self.main_win.pack_start(self.magnifier.get_widget(), fill=False, expand=False, padding=0)

        # BLUE buttons

        self.button_set = []
        blue_buttons = StackColorData("B", self)

        self.stacker_table.attach(blue_buttons.color_label, left_attach=0,
                                  right_attach=1, top_attach=1,
                                  bottom_attach=2) 
        self.stacker_table.attach(blue_buttons.stack_button, left_attach= 1,
                                  right_attach= 2, top_attach=1,
                                  bottom_attach=2)
        self.stacker_table.attach(blue_buttons.stack_exists_button, left_attach= 2,
                                  right_attach= 3, top_attach=1,
                                  bottom_attach=2)
        self.stacker_table.attach(blue_buttons.threshold_button, left_attach= 3,
                                  right_attach= 4, top_attach=1,
                                  bottom_attach=2)
        self.stacker_table.attach(blue_buttons.find_button, left_attach= 4,
                                  right_attach= 5, top_attach=1,
                                  bottom_attach=2) 
        self.stacker_table.attach(blue_buttons.num_star_field, left_attach= 5,
                                  right_attach= 6, top_attach=1,
                                  bottom_attach=2)
        self.button_set.append(blue_buttons)
        
        # VISUAL buttons
        green_buttons = StackColorData("V", self)

        self.stacker_table.attach(green_buttons.color_label, left_attach=0,
                                  right_attach=1, top_attach=2,
                                  bottom_attach=3) 
        self.stacker_table.attach(green_buttons.stack_button, left_attach= 1,
                                  right_attach= 2, top_attach=2,
                                  bottom_attach=3)
        self.stacker_table.attach(green_buttons.stack_exists_button, left_attach= 2,
                                  right_attach= 3, top_attach=2,
                                  bottom_attach=3)
        self.stacker_table.attach(green_buttons.threshold_button, left_attach= 3,
                                  right_attach= 4, top_attach=2,
                                  bottom_attach=3)
        self.stacker_table.attach(green_buttons.find_button, left_attach= 4,
                                  right_attach= 5, top_attach=2,
                                  bottom_attach=3) 
        self.stacker_table.attach(green_buttons.num_star_field, left_attach= 5,
                                  right_attach= 6, top_attach=2,
                                  bottom_attach=3)
        self.button_set.append(green_buttons)
        
        # RED buttons
        red_buttons = StackColorData("R", self)

        self.stacker_table.attach(red_buttons.color_label, left_attach=0,
                                  right_attach=1, top_attach=3,
                                  bottom_attach=4) 
        self.stacker_table.attach(red_buttons.stack_button, left_attach= 1,
                                  right_attach= 2, top_attach=3,
                                  bottom_attach=4)
        self.stacker_table.attach(red_buttons.stack_exists_button, left_attach= 2,
                                  right_attach= 3, top_attach=3,
                                  bottom_attach=4)
        self.stacker_table.attach(red_buttons.threshold_button, left_attach= 3,
                                  right_attach= 4, top_attach=3,
                                  bottom_attach=4)
        self.stacker_table.attach(red_buttons.find_button, left_attach= 4,
                                  right_attach= 5, top_attach=3,
                                  bottom_attach=4) 
        self.stacker_table.attach(red_buttons.num_star_field, left_attach= 5,
                                  right_attach= 6, top_attach=3,
                                  bottom_attach=4)
        self.button_set.append(red_buttons)

        
        # IR buttons
        ir_buttons = StackColorData("I", self)

        self.stacker_table.attach(ir_buttons.color_label, left_attach=0,
                                  right_attach=1, top_attach=4,
                                  bottom_attach=5) 
        self.stacker_table.attach(ir_buttons.stack_button, left_attach= 1,
                                  right_attach= 2, top_attach=4,
                                  bottom_attach=5)
        self.stacker_table.attach(ir_buttons.stack_exists_button, left_attach= 2,
                                  right_attach= 3, top_attach=4,
                                  bottom_attach=5)
        self.stacker_table.attach(ir_buttons.threshold_button, left_attach= 3,
                                  right_attach= 4, top_attach=4,
                                  bottom_attach=5)
        self.stacker_table.attach(ir_buttons.find_button, left_attach= 4,
                                  right_attach= 5, top_attach=4,
                                  bottom_attach=5) 
        self.stacker_table.attach(ir_buttons.num_star_field, left_attach= 5,
                                  right_attach= 6, top_attach=4,
                                  bottom_attach=5)
        self.button_set.append(ir_buttons)

##----------------
        self.stack_and_find_button = gtk.Button(label="Stack & Find")
        self.stack_and_find_button.connect("clicked", self.stack_and_find_cb, None)
        self.main_win.pack_start(self.stack_and_find_button, fill=False, expand=False, padding=10)

    def get_widget(self):
        return self.main_win

    def stack_and_find_cb(self, widget, data=None):
        self.stack_cb(None, 'B')
        self.stack_cb(None, 'V')
        self.stack_cb(None, 'R')
        self.stack_cb(None, 'I')
        self.find_cb(None, 'B')
        self.find_cb(None, 'V')
        self.find_cb(None, 'R')
        self.find_cb(None, 'I')

    # Invoked to perform the stacking operation
    def stack_cb(self, widget, data):
        print("stack_cb:", data)
        flat_name = os.path.join(SessionGlobal.homedir, "flat_" + data + "c.fits")
        output_file = os.path.join(SessionGlobal.homedir,
                                   SessionGlobal.current_star.name + "_" + data + ".fits")

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

            stack_cmd = "stack -e " # excludes "-x" to improve performance
            stack_cmd += (" -s " + flat_name)
            stack_cmd += (" -d " + dark_name)
            stack_cmd += (" -o " + output_file)
            stack_cmd += (" " + image_list)
            os.system(stack_cmd)
            print("stacking completed.")
            SessionGlobal.notifier.trigger(trigger_source=self,
                                           variable="stack_files"+data,
                                           condition="value_change")
        
    def find_cb(self, widget, data):
        # find_stars -q n.n -i starname_Rc.fits
        # star_match -e -f -b -n starname -i starname_Rc.fits
        image_file = os.path.join(SessionGlobal.homedir,
                                  SessionGlobal.current_star.name + "_" + data + ".fits")
        buttons = self.letter_to_button(data)
        threshold_string = buttons.threshold_button.get_text()
        command = "find_stars -f "
        command += " -q " + threshold_string
        command += " -i " + image_file
        print("Invoking: " + command)
        os.system(command)
        command = "star_match -h -e -f -b "
        command += " -n " + SessionGlobal.current_star.name
        command += " -i " + image_file
        os.system(command)
        SessionGlobal.notifier.trigger(trigger_source=self,
                                       variable="stack_files"+data,
                                       condition="content_change")
        command = "analyzer "
        command += " -t " + SessionGlobal.current_star.name
        command += " -d " + SessionGlobal.homedir
        command += " > /tmp/analyzer.out 2>&1"
        os.system(command)
        SessionGlobal.RecomputeAllBVRI()

    def letter_to_button(self, letter):
        if letter == 'B':
            return self.button_set[0]
        if letter == 'V':
            return self.button_set[1]
        if letter == 'R':
            return self.button_set[2]
        if letter == 'I':
            return self.button_set[3]
        print("ERROR: letter_to_button: bad letter: ", letter)

class StackerViewInfo:
    def __init__(self, letter, parent):
        self.show_button = gtk.RadioButton(label=letter)
        self.show_button.connect("toggled", parent.show_stack_cb, letter)
        self.letter = letter
        self.current_file = None

    def set_current_file(self, filename):
        self.current_file = filename

class StackerViewButtons:
    def __init__(self, stack_control_pane):
        self.mainwin = gtk.VBox();
        self.button_set = {}
        self.associated_viewer = None
        first = True
        for letter in ["B","V","R","I"]:
            svi = StackerViewInfo(letter, self)
            self.button_set[letter]=svi
            if  first:
                first = False
                group = svi.show_button
            else:
                svi.show_button.join_group(group)
            self.mainwin.pack_start(svi.show_button, fill=True, expand=False, padding=0)
        SessionGlobal.notifier.register(requestor=self,
                                        variable="current_star",
                                        condition="value_change",
                                        callback=self.starchange_cb,
                                        debug="StackerViewButtons")
        SessionGlobal.notifier.register(requestor=self,
                                        variable="stack_files",
                                        condition="value_change",
                                        callback=self.filechange_cb,
                                        debug="StackerViewButtons")
        SessionGlobal.notifier.register(requestor=self,
                                        variable="stack_files",
                                        condition="content_change",
                                        callback=self.filechange_cb,
                                        debug="StackerViewButtons")
        
        extra_buttons = gtk.HBox(spacing=2)
##----------------
        self.circle_stars_button = gtk.CheckButton(label="Circle Stars")
        self.circle_stars_button.connect("toggled", self.circle_stars_callback, None)
        extra_buttons.pack_start(self.circle_stars_button, fill=False, expand=False, padding=0)
##----------------
        self.label_stars_button = gtk.CheckButton(label="Label Stars")
        self.label_stars_button.connect("toggled", self.label_stars_callback, None)
        extra_buttons.pack_start(self.label_stars_button, fill=False, expand=False, padding=0)
##----------------
        self.mainwin.pack_start(extra_buttons, fill=False, expand=False, padding=0)

    def filechange_cb(self, variable, condition, data):
        self.update_show_buttons()

    def starchange_cb(self, variable, condition, data):
        self.update_show_buttons()
        for color in ["B","V","R","I"]:
            SessionGlobal.notifier.trigger(trigger_source=self,
                                           variable="stack_files"+color,
                                           condition="value_change")

    def get_widget(self):
        return self.mainwin

    def set_associated_viewer(self, fits_viewer):
        self.associated_viewer = fits_viewer

    def clear_all_stack_buttons(self):
        for f in self.button_set:
            self.button_set[f].show_button.set_active(False)
        
    def show_stack_cb(self, widget, filter_letter):
        if self.associated_viewer == None:
            print("StackerViewButtons.show_stack_cb(): invoked while viewer == None.")
        else:
            image_file = self.button_set[filter_letter].current_file
            if image_file != None:
                self.associated_viewer.load_file(image_file)

    def update_show_buttons(self, force=None):
        # First, figure out what's available

        #self.clear_all_stack_buttons()
        #self.set_stack_images(None, 'V')
        for f in self.button_set:
            color = f
            if SessionGlobal.current_star != None:
                imagefilename = os.path.join(SessionGlobal.homedir,
                                             SessionGlobal.current_star.name + "_" + color + ".fits")
            else:
                imagefilename = None

            oldname = None
            if color in SessionGlobal.stack_files:
                oldname = SessionGlobal.stack_files[color]
            if imagefilename != oldname:
                SessionGlobal.stack_files[color] = imagefilename
                SessionGlobal.notifier.trigger(trigger_source = self,
                                               variable="stack_files"+color,
                                               condition="value_change")
                

        # Turn on the V image if it exists
        #if self.button_set["V"].current_file != None:
        #    self.button_set["V"].show_button.set_active(True)

    def circle_stars_callback(self, widget, data=None):
        self.associated_viewer.set_circle_stars(self.circle_stars_button.get_active())

    def label_stars_callback(self, widget, data=None):
        self.associated_viewer.set_label_stars(self.label_stars_button.get_active())

class NotUsedAnymore:
    def __init__(self):
        self.mainwin = gtk.HBox(spacing = 0)
        self.left_win = gtk.VBox(spacing=0)

        # This is the label where the stack filename will be listed
        self.mainlabel = gtk.Label(None)
        self.mainlabel.set_alignment(0.0, 0.5)
        
        self.left_win.pack_start(self.mainlabel, fill=False,expand=False,padding=0)
        self.mainwin.pack_start(self.left_win, fill=False, expand=False, padding = 0)
        self.main_image = FITSViewer.FitsViewer("StackWin")
        self.main_image.set_callback('button-release-event', stack_image_click_cb)

        self.renderer = FITSViewer.RendererPane(self.main_image)

        self.mainwin.pack_start(self.left_win, fill=False, expand=False, padding = 0)

        self.widget_area = gtk.VBox(spacing=2)

        self.stat_box = gtk.HBox(spacing=2)
        self.stat_box.set_homogeneous(True)

        self.label_max = gtk.Label('Max = ')
        self.label_max.set_alignment(0.0, 0.5)
        self.label_min = gtk.Label('Min = ')
        self.label_min.set_alignment(0.0, 0.5)
        self.label_median = gtk.Label('Median = ')
        self.label_median.set_alignment(0.0, 0.5)
        self.label_median_bkg = gtk.Label('Background median = ')
        self.label_median_bkg.set_alignment(0.0, 0.5)

        self.stat_box.pack_start(self.label_max, fill=False, expand=False, padding=0)
        self.stat_box.pack_start(self.label_min, fill=False, expand=False, padding=0)
        self.stat_box.pack_start(self.label_median, fill=False, expand=False, padding=0)
        self.stat_box.pack_start(self.label_median_bkg, fill=False, expand=False, padding=0)
        self.widget_area.pack_start(self.stat_box, fill=False,
                                    expand=False, padding=0)

        self.widget_area.pack_start(self.renderer.widget(), fill=False,
                                    expand=False, padding=0)

        self.mag_region = gtk.HBox(spacing=2)
        self.extra_info_region = gtk.VBox(spacing=2)
        self.mag_region.add(self.magnifier.get_widget())
        self.mag_region.add(self.extra_info_region)

        self.star_info = StarInfo()
        self.star_info.set_associated_viewer(self.main_image)
        self.extra_info_region.pack_start(self.star_info.root,fill=False,expand=False,padding=0)
        self.star_info.clear_star()

        self.blur_info = BlurInfo()
        self.extra_info_region.pack_start(self.blur_info.root,fill=False,expand=False,padding=0)

        self.attach_slave_click(self.star_info)

        self.widget_area.pack_start(self.mag_region, fill=False, expand=False, padding=0)

        

        self.left_win.pack_start(self.widget_area, fill=False, expand=False, padding=0)
        self.mainwin.pack_start(self.main_image.get_widget(),
                                 expand=False, fill=False, padding=0)

        self.update_show_buttons()

        self.mainwin.show_all()

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

            (min_data_val, max_data_val, median_data_val) = self.main_image.get_statistics()
        else:
            (min_data_val, max_data_val, median_data_val) = (0, 0, 0)
            
        self.label_min.set_text("Min = {:.1f}".format(min_data_val))
        self.label_max.set_text("Max = {:.1f}".format(max_data_val))
        self.label_median.set_text("Median = {:.1f}".format(median_data_val))
        self.label_median_bkg.set_text("Bkg stddev = {:.1f}".format(self.main_image.background_std))

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

################################################################
##    class StackColorData
################################################################
class StackColorData:
    def __init__(self, color, parent):
        self.colorLetter = color

        self.color_label = gtk.Label(color)

        self.stack_button = gtk.Button(label="Stack")
        self.stack_button.connect("clicked", parent.stack_cb, color)

        self.stack_exists_button = gtk.Label(" ")

        self.threshold_button = gtk.Entry()
        self.threshold_button.set_max_length(4)
        self.threshold_button.set_text("9.0")
        req_size = self.threshold_button.get_size_request()
        self.threshold_button.set_size_request(7, req_size[1])
        
        self.find_button = gtk.Button(label="Find")
        self.find_button.connect("clicked", parent.find_cb, color)

        self.num_star_field = gtk.Label("0")
        self.num_star_field.set_justify(gtk.Justification.RIGHT)

        #self.show_button = gtk.ToggleButton("Show")
        #self.show_button.connect("clicked", parent.show_stack_cb,
        #                        color)

        self.current_file = None

        SessionGlobal.notifier.register(requestor=self,
                                        variable="stack_files"+self.colorLetter,
                                        condition="value_change",
                                        callback=self.stackfilechange_cb)
        SessionGlobal.notifier.register(requestor=self,
                                        variable="stack_files"+self.colorLetter,
                                        condition="content_change",
                                        callback=self.stackfilechange_cb)

    def load_starlist(self):
        #print "load_starlist() invoked for " + self.current_file
        starlist_file = "/tmp/list09.out"
        os.system("list_stars -i " + self.current_file + " > " + starlist_file)
        exists = os.path.isfile(starlist_file)
        if exists:
            f = open(starlist_file, "r")
            all_star_lines = f.readlines()
            f.close()

            self.num_star_field.set_text(str(len(all_star_lines)))
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
            self.num_star_field.set_text("  0")

    def stackfilechange_cb(self, variable, condition, data):
        self.set_current_file(SessionGlobal.stack_files[self.colorLetter])

    def set_current_file(self, filename):
        if filename != None and os.path.isfile(filename):
            print("set_current_file(" + filename + ") invoked.")
            self.stack_exists_button.set_text("X")
            if self.current_file != filename:
                self.current_file = filename
            self.load_starlist()
        else:
            print("set_current_file(None) invoked.")
            if filename != None:
                print("    but filename was: ", filename)
            self.stack_exists_button.set_text("  ")
            self.star_data = []
            self.num_star_field.set_text("  0")
            self.current_file = None
        

def stack_image_click_cb(widget, event):
    #print "stack_image_click_cb()"
    SessionGlobal.stacker.set_magnifier_xy(event.x, event.y)

