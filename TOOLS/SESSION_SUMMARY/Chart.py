import threading
import gi
from gi.repository import Gtk as gtk
from gi.repository import Gdk, GdkPixbuf

import SessionGlobal

class ChartCache:
    def __init__(self):
        self.local_cache = {}
        print("ChartCache class initialized.")
        
    def fetch_chart(self, starname):
        def CleanStarname(s):
            return s.replace("-", "%20")
    
        if starname not in self.local_cache:
            print("Chart cache miss for star ", starname)
            
            import urllib.request
            import re
            chart_req_string = "https://www.aavso.org/apps/vsp/chart/?fov=20.0&scale=E&star="+\
                               CleanStarname(starname)+\
                               "&orientation=visual&maglimit=16.5&resolution=150&north=down&east=right&type=chart"
        
            print("Fetching initial chart via http:")
            f = urllib.request.urlopen(chart_req_string)
            chart = f.read()
            chart_image = re.search('img src="/vsp/chart/[0-9a-zA-Z]*.png"', str(chart))
#            chart_image = re.search('img src="/apps/vsp/chart/[0-9a-zA-Z]*.png"', str(chart))
            if chart_image == None:
                print("Chart fetch failed. http(GET) returned:")
                print(chart)
                chart = ""
                
            else:
                chart_path = chart_image.group(0)
                print("Resulting chart_image = ", chart_path)
        
                if chart_image != None:
                    chart_path = chart_path.replace("img src=", "")
                    chart_path = chart_path.replace('"', '')
                    chart_req_string = "https://app.aavso.org" + chart_path
                    print("Fetching .png via ", chart_req_string)
                    f = urllib.request.urlopen(chart_req_string)
                    #print "URL opened:", f
                    chart = f.read()
                else:
                    # couldn't fetch the chart
                    chart = ""
            self.local_cache[starname] = chart
            print("chart loaded into cache")
            f.close()
        #else:
            #print "Chart cache hit for ", starname
            
        return self.local_cache[starname]

# ChartCache is a class. Need to instantiate the singleton that actually holds the cache
chart_cache = ChartCache()

class ChartWindow:
    def __init__(self):
        self.currently_displayed = ""
        self.img = gtk.Image()
        self.img.set_size_request(780, 975)
        SessionGlobal.notifier.register(requestor=self,
                                        variable="current_star",
                                        condition="value_change",
                                        callback=self.star_change_cb,
                                        debug="ChartWindow")
                                        
    def set_star(self, starname):
        if starname == self.currently_displayed:
            return
        if starname == None:
            self.img.clear()
        else:
            self.my_thread = threading.Thread(target=self.run,args=(starname,))
            self.my_thread.start()
            
    def run(self, starname):
        chart = chart_cache.fetch_chart(starname)

        print("fetching chart for " + starname + "; chart being saved into /tmp")
        t = open("/tmp/chart_image.png", "wb")
        t.write(chart)
        t.close()
    
        print("reading chart from /tmp into pixbuf")
        pixbuf = GdkPixbuf.Pixbuf.new_from_file("/tmp/chart_image.png")
        pixbuf = pixbuf.scale_simple(int(1.3*600), int(1.3*750), GdkPixbuf.InterpType.BILINEAR)
        self.img.set_from_pixbuf(pixbuf)
        self.img.show()
        self.currently_displayed = starname
        
    def widget(self):
        return self.img

    def star_change_cb(self, variable, condition, data):
        self.set_star(SessionGlobal.current_star.name)


