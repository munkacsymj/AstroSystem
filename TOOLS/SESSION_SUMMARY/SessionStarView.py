import os
import gi
from gi.repository import Gtk as gtk
import SessionGlobal

class SessionStarView:

    # click/select callback
    def button_release_event(self, button, event):
        if event.button == 1:
            # get an iterator that contains what's selected
            model, sel_iter = self.treeselection.get_selected()
            if sel_iter == None:
                pass
                #self.deselect_all()
            else:
                v_starname = self.treestore.get_value(sel_iter, 0)
                v_star = SessionGlobal.star_dictionary[v_starname]
                    
                ChangeSelected(v_star)
                    
    # close the window and quit
    def delete_event(self, widget, event, data=None):
        gtk.main_quit()
        return False

    def __init__(self, parent_window):
        # create a TreeStore with one string column to use as the model
        # Column 0: str: holds the node name (starname, filtername, image)
        # Column 1: int: holds the node type (see TreeViewNode)
        # Column 2: holds reference to a TargetStar, ObsSeq, or Exposure
        self.treestore = gtk.TreeStore(str)

        for one_star in SessionGlobal.all_targets:
            piter = self.treestore.append(None, [one_star.name])

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

        ChangeSelected(SessionGlobal.all_targets[0])

    def window(self):
        return self.treeview

def ChangeSelected(target_star):
    print("ChangeSelected() invoked")
    print("    target_star = ", target_star)

    if target_star != SessionGlobal.current_star:
        SessionGlobal.current_star = target_star
        SessionGlobal.notifier.trigger(trigger_source=target_star,
                                       variable="current_star",
                                       condition="value_change")

    #SessionGlobal.current_image_name = None
    #SessionGlobal.stacker.update_show_buttons()
    #SessionGlobal.stacker.update_stack_image()

