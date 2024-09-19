import os
import gi
from gi.repository import Gtk as gtk
import SessionGlobal

class TreeViewNode:
    NODE_STAR = 1
    NODE_FILTER = 2
    NODE_STACK = 3
    NODE_IMAGE = 4

class BasicTreeView:

    # click/select callback
    def button_release_event(self, button, event):
        if event.button == 1:
            # get an iterator that contains what's selected
            model, sel_iter = self.treeselection.get_selected()
            if sel_iter == None:
                pass
                #self.deselect_all()
            else:
                v_type = self.treestore.get_value(sel_iter, 1)
                v_image = None
                v_filter = None
                
                if v_type == TreeViewNode.NODE_STAR:
                    v_starname = self.treestore.get_value(sel_iter, 0)
                    v_star = SessionGlobal.star_dictionary[v_starname]
                    
                else:
                    v_parent_iter = self.treestore.iter_parent(sel_iter)

                    if v_type == TreeViewNode.NODE_FILTER:
                        v_starname = self.treestore.get_value(v_parent_iter, 0)
                        v_star = SessionGlobal.star_dictionary[v_starname]
                        v_filter = v_star.obs_seq[self.treestore.get_value(sel_iter, 0)]
                                          
                    else:
                        v_parent_iter = self.treestore.iter_parent(sel_iter)
                        v_grandparent_iter = self.treestore.iter_parent(v_parent_iter)
                        v_starname = self.treestore.get_value(v_grandparent_iter, 0)
                        v_star = SessionGlobal.star_dictionary[v_starname]
                        v_filter = v_star.obs_seq[self.treestore.get_value(v_parent_iter, 0)]
                        
                        if v_type == TreeViewNode.NODE_STACK or v_type == TreeViewNode.NODE_IMAGE:
                            v_image = self.treestore.get_value(sel_iter, 0)

                ChangeSelected(v_star, v_filter, v_image, (v_type == TreeViewNode.NODE_STACK))
                    
    # close the window and quit
    def delete_event(self, widget, event, data=None):
        gtk.main_quit()
        return False

    def check_and_insert_stack_image(self, filter_node, obs_seq):
        is_avail = os.path.isfile(obs_seq.stackfilename)
        exposure_iter = self.treestore.iter_children(filter_node)
        if exposure_iter == None:
            already_present = False
        else:
            # see if the type of the first node is a stacked_exposure...
            already_present = self.treestore.get_value(exposure_iter, 1) == \
                              TreeViewNode.NODE_STACK
        if is_avail == already_present:
            return
        elif is_avail:
            # need to add entry
            self.treestore.prepend(filter_node, \
                                   [obs_seq.stackfilename, TreeViewNode.NODE_STACK] )
        elif already_present:
            # need to delete entry
            self.treestore.remove(exposure_iter)

        else:
            raise AssertionError
        
    def __init__(self, parent_window):
        # create a TreeStore with one string column to use as the model
        # Column 0: str: holds the node name (starname, filtername, image)
        # Column 1: int: holds the node type (see TreeViewNode)
        # Column 2: holds reference to a TargetStar, ObsSeq, or Exposure
        self.treestore = gtk.TreeStore(str, int)

        for one_star in SessionGlobal.all_targets:
            piter = self.treestore.append(None, \
                                [one_star.name, TreeViewNode.NODE_STAR])
            for filter in one_star.obs_seq:
                f = self.treestore.append(piter, \
                                [filter, TreeViewNode.NODE_FILTER] )
                # is a stacked image available?
                self.check_and_insert_stack_image(f, one_star.obs_seq[filter])

                # insert the normal images
                for image in one_star.obs_seq[filter].exposures:
                    self.treestore.append(f, \
                                [os.path.basename(image.filename), \
                                 TreeViewNode.NODE_IMAGE] )


        # create the TreeView
        self.treeview = gtk.TreeView(self.treestore)
        self.tvcolumn = gtk.TreeViewColumn('Session')
        self.treeview.append_column(self.tvcolumn)

        # create a CellRendererText to render the data
        self.cell = gtk.CellRendererText()
        self.tvcolumn.pack_start(self.cell, True)
        self.tvcolumn.add_attribute(self.cell, 'text', 0)
        self.tvcolumn.set_max_width(170)
        self.tvcolumn.set_min_width(130)
        self.treeview.set_search_column(0)
        self.treeview.show()

        self.treeselection = self.treeview.get_selection()
        self.treeselection.set_mode(gtk.SelectionMode.SINGLE)
        self.treeview.connect('button-release-event', self.button_release_event)

    def window(self):
        return self.treeview

def ChangeSelected(target_star, obs_seq, exposure, is_stacked_image):
    global main_fits_image
    
    print("ChangeSelected() invoked")
    print("    target_star = ", target_star)
    print("    obs_seq = ", obs_seq)
    print("    exposure = ", exposure)
    print("    is_stacked = ", is_stacked_image)

    SessionGlobal.chart_window.set_star(target_star.name)

    if exposure != None:
        SessionGlobal.image_region.set_images(os.path.join(SessionGlobal.homedir, exposure), None)
        SessionGlobal.current_image_name = exposure
    else:
        SessionGlobal.current_image_name = None
        
    if target_star != SessionGlobal.current_star:
        SessionGlobal.current_star = target_star
        SessionGlobal.thumbs.set_star(SessionGlobal.current_star)
        SessionGlobal.stacker.update_show_buttons()
        SessionGlobal.stacker.update_stack_image()
    SessionGlobal.text_tabs.update(target_star, obs_seq)
    SessionGlobal.root.show_all()
    SessionGlobal.root_r.show_all()

