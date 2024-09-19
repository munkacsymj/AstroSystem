#!/usr/bin/python3

#  transit_plotter.py -- plot airmass and elevation angle for an
# exoplanet transit
# 
#   Copyright (C) 2020 Mark J. Munkacsy
# 
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
# 
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
# 
#    You should have received a copy of the GNU General Public License
#    along with this program (file: COPYING).  If not, see
#    <http://www.gnu.org/licenses/>. 
# 

import datetime
import math
import getopt
import re
import sys
import matplotlib
import matplotlib.dates
import matplotlib.pyplot as plt

def Time2DecimalDays(time_object):
    return (time_object.hour + time_object.minute/60.0 +
            time_object.second/3600.0)/24.0

# Takes a JD and converts to Local Sidereal Time (in degrees)
def LST(jd_moment):
    global longitude_deg
    
    jd_midnight = int(jd_moment)+0.5
    x = 101.76+0.985647348*(jd_moment-2451543.5)+longitude_deg+(jd_moment-jd_midnight)*(3600*24)/240

    return x % 360.0

################################################################
## Class "Point"
## This is the big (only) data structure used in this tool. It holds
## all the data relevant to the host star at each moment in time that
## is used on the graph.
################################################################
class Point:
    def __init__(self, jd_moment):
        global sin_lat, sin_dec, cos_dec, cos_lat, ra_degrees
        self.jd_moment = jd_moment
        self.dt_moment = jd2dt(jd_moment)
        self.mpl_moment = matplotlib.dates.date2num(self.dt_moment)
        self.lst_deg = LST(jd_moment)
        self.ha_rad = math.radians(self.lst_deg) - math.radians(ra_degrees)
        if self.ha_rad > math.pi:
            self.ha_rad -= (2*math.pi);
        if self.ha_rad < -math.pi:
            self.ha_rad += (2*math.pi);
        self.alt_rad = max(0.0,math.asin(sin_dec*sin_lat + cos_dec*cos_lat*math.cos(self.ha_rad)))
        alt_degrees = math.degrees(self.alt_rad)
        self.alt_deg = alt_degrees
        if alt_degrees < 5.0:
            self.airmass = 10.0
        else:
            self.airmass = 1.0/(math.sin(math.radians(alt_degrees+244.0/(165.0+47.0*(alt_degrees**1.1)))))

        timestamp = (jd_moment - 2440587.5)*24*60*60
        dt = datetime.datetime.fromtimestamp(timestamp, tz=datetime.timezone.utc)
        self.local_dt = dt.astimezone() # convert to local timezone
        self.decimal_hours_local = (self.local_dt.hour +
                                    self.local_dt.minute/60.0 +
                                    self.local_dt.second/3600.0)

################################################################
## Convert back and forth between JD and Python datetime
################################################################

def jd2dt(julian):
    global jd_reference, dt_reference
    delta_jd = julian - jd_reference
    delta = datetime.timedelta(days=delta_jd)
    return dt_reference + delta

def dt2jd(dtime):
    global jd_reference, dt_reference
    delta = dtime - dt_reference
    return jd_reference + delta.total_seconds()/(24.0*3600.0)

################################################################
## Calculate UTC hour corresponding to Local Sunrise/Sunset
##    (event equals either "sunrise" or "sunset";
##     the first param is used to get the date)
################################################################
def SunSetRise(dtime, event):
    if event == "sunset":
        sunset = True
    else:
        sunset = False
        
    # Calculate day_of_year
    N1 = int(275*dtime.month/9)
    N2 = int((dtime.month+9)/12)
    N3 = (1 + int((dtime.year - 4*int(dtime.year/4)+2)/3))
    N = N1 - (N2*N3) + dtime.day - 30
    print('N = ', N)

    # Calculate approximate hour
    longitude_hour = longitude_deg/15.0
    if sunset:
        t_approx = N + ((18 - longitude_hour)/24.0)
    else:
        t_approx = N + ((6 - longitude_hour)/24.0)
    print('t_approx = ', t_approx)

    # Calculate sun's mean anomaly
    M = math.radians((0.9856*t_approx) - 3.289)

    # Calculate sun's true longitude
    L = math.degrees(M) + (1.916*math.sin(M)) + (0.020 * math.sin(2*M)) + 282.634
    if L < 0.0:
        L += 360.0
    if L >= 360.0:
        L -= 360.0
    L = math.radians(L)

    # Calculate the sun's right ascension
    RA = math.atan(0.91764 * math.tan(L))
    if RA < 0.0:
        RA += (2*math.pi)
    print('Sun RA = ', RA)

    # Put RA into correct quadrant
    Lquad = int(L/(math.pi/2)) * math.pi/2.0
    RAquad = int(RA/(math.pi/2)) * math.pi/2.0
    RA += (Lquad - RAquad)
    RAhour = RA * (12.0/math.pi)
    print('Sun RA = ', RAhour, " (hours)")

    # Calculate the sun's declination
    sin_dec = 0.39782 * math.sin(L)
    cos_dec = math.cos(math.asin(sin_dec))
    print('Sun dec = ', math.degrees(math.asin(sin_dec)))

    #calculate the sun's local hour angle
    zenith = math.radians(96.0) # 96-deg == civil twilight
    cosH = (math.cos(zenith) - (sin_dec * sin_lat))/(cos_dec * cos_lat)

    # Adjust Hour Angle
    if sunset:
        H = math.acos(cosH)
    else:
        H = (2*math.pi) - math.acos(cosH)

    # Calculate local mean time of rising/setting (in hours)
    T = (H + RA )*12.0/math.pi - (0.06571*t_approx) - 6.622
    print('Local mean time of rising/setting (hours) = ', T)
    # Adjust to UTC
    UT = T - longitude_hour
    if UT < 0.0:
        UT += 24.0
    if UT >= 24.0:
        UT -= 24.0

    print('UT (hours) = ', UT)
    return UT

def GetDecRA(starname):
    with open('/home/ASTRO/REF_DATA/star_list.txt', 'r') as f:
        for line in f:
            words = re.split(r'[ \t]+', line)
            #print(words)
            if len(words) == 3 and words[0] == starname:
                dec_str = words[1]
                dec_sign = +1
                if dec_str[0] == '-':
                    dec_sign = -1
                    dec_str = dec_str[1:]
                ra_str = words[2]
                d_words = dec_str.split(':')
                if len(d_words) == 2:
                    deg = int(d_words[0])
                    min = float(d_words[1])
                    sec = 0.0
                elif len(d_words) == 3:
                    deg = int(d_words[0])
                    min = int(d_words[1])
                    sec = float(d_words[2])
                declination = dec_sign * (deg+min/60.0+sec/3600.0)

                ra_words = ra_str.split(':')
                if len(ra_words) != 3:
                    print("error: bad dec/ra: ra_str")
                    return (0,0)
                else:
                    hour = int(ra_words[0])
                    min = int(ra_words[1])
                    sec = float(ra_words[2])
                    right_ascension = (hour+min/60.0+sec/3600.0)*(360.0/24.0)
                    return (declination,right_ascension)
        print('Object ', starname, ' not found in star_list.txt')
        exit
    
def main():
    global longitude_deg, ra_degrees
    global sin_lat, sin_dec, cos_dec, cos_lat
    global jd_reference, dt_reference
    
    opts, args = getopt.getopt(sys.argv[1:], "n:")
    object_is_named = False
    print('opts = ', opts)
    for opt,arg in opts:
        if opt == '-n':
            title = arg

            jd_ingress_start = 2460545.75027
            jd_egress_end =  2460545.87955
            #jd_ingress_start = None
            #jd_egress_end = None

            dec_degrees,ra_degrees = GetDecRA(arg)
            object_is_named = True
            print(arg, " is at dec/ra = ",
                  (dec_degrees, ra_degrees))
        
    override_date = None
################################################################
## USER ENTRY AREA
################################################################

    if not object_is_named:
        title = 'WISE-Gem'
        jd_ingress_start = None
        jd_egress_end =  None

        ra_degrees = (7+09.0/60.0+41.4/3600)*(360.0/24.0) # 23:58:23
        dec_degrees = 12+46/60.0+43.0/3600 # +61:12.4

        #override_date = 2459823.54167 # Sept 1, 2022

    expand_transit = False

    latitude_deg = 41.569029
    longitude_deg = -71.238451

################################################################
## END OF USER ENTRY AREA
################################################################

    sin_dec = math.sin(math.radians(dec_degrees))
    cos_dec = math.cos(math.radians(dec_degrees))
    sin_lat = math.sin(math.radians(latitude_deg))
    cos_lat = math.cos(math.radians(latitude_deg))


    # The reference date is midnight at UTC on 1/1/2020
    jd_reference = 2458849.5
    dt_reference = datetime.datetime(year=2020,month=1,day=1,tzinfo=datetime.timezone.utc)

    if jd_ingress_start != None:
        jd_date_basis = jd_ingress_start
        dt_date_basis = jd2dt(jd_date_basis)
    else:
        if jd_egress_end != None:
            jd_date_basis = jd_egress_end
            dt_date_basis = jd2dt(jd_date_basis)
        else:
            #neither ingress nor egress is available
            if override_date != None:
                today = override_date
                dt_date_basis = jd2dt(today)
                jd_date_basis = today
            else:
                today = datetime.datetime.now(datetime.timezone.utc)
                dt_date_basis = today
            #jd_date_basis = dt2jd(dt_date_basis)
        
#dt_date_basis = jd2dt(jd_date_basis)
    if jd_ingress_start != None:
        dt_ingress = jd2dt(jd_ingress_start)

    if jd_egress_end != None:
        dt_egress = jd2dt(jd_egress_end)
#jd_transit_midpoint = (jd_egress_end+jd_ingress_start)/2.0

    sunset = SunSetRise(dt_date_basis, "sunset") # UTC hour
    if sunset > 12.0:
        sunset -= 24.0
    
    sunrise = SunSetRise(dt_date_basis, "sunrise") # UTC hour

    ## Calculate midnight at UTC as a Python datetime
    if dt_date_basis.hour > 12:
        midnight_utc = dt_date_basis.replace(hour=23,minute=59,second=59,microsecond=900000)
    else:
        midnight_utc = dt_date_basis.replace(hour=0,minute=0,second=0,microsecond=0)

    # Correct sunrise/sunset so that the dates are correct (since sunset
    # can occur on either UTC today or UTC tomorrow 
    sunset_delta = datetime.timedelta(hours=sunset)
    sunrise_delta = datetime.timedelta(hours=sunrise)
    sunset_utc = midnight_utc + sunset_delta
    sunrise_utc = midnight_utc + sunrise_delta

    # Find out what our local offset is from UTC; also remember the local
    # timezone (for the graph)
    n_local = dt_date_basis.astimezone()
    local_tz = n_local.tzinfo
    tz_offset_from_utc = local_tz.utcoffset(n_local)

    if jd_ingress_start != None and jd_egress_end != None:
        print('Ingress time: ', dt_ingress.astimezone())
        print('Egress time: ', dt_egress.astimezone())

    print('Sunset: ', sunset_utc.astimezone())
    print('Sunrise: ', sunrise_utc.astimezone())

    # Compute start and end times of interest. Use "expand_transit" to
    # zero in on the times of interest
    if expand_transit:
        start_time = dt_ingress
        stop_time = dt_egress
    else:
        if jd_ingress_start != None:
            start_time = min(sunset_utc, dt_ingress)
        else:
            start_time = sunset_utc
        if jd_egress_end != None:
            stop_time = max(sunrise_utc, dt_egress)
        else:
            stop_time = sunrise_utc

    print('Period of interest = ', start_time, ' to ', stop_time)
    # divide the period into 100 intervals...
    start_jd = dt2jd(start_time)
    end_jd = dt2jd(stop_time)
    delta_t_jd = (end_jd - start_jd)/100.0

    points = []
    jd_counter = start_jd
    while jd_counter <= end_jd:
        points.append(Point(jd_counter))
        jd_counter += delta_t_jd

    ################################################################
    ## Search for a meridian flip
    ################################################################
    meridian_flip_dt = None
    
    if points[0].ha_rad < 0.0:
        meridian_flip_dt = next((x.dt_moment for x in points if x.ha_rad >= 0.0), None)

    if meridian_flip_dt != None:
        print('meridian_flip = ', meridian_flip_dt.astimezone())
    else:
        print('meridian_flip = None')

    x_coords = [x.dt_moment for x in points]
    y_altitude_deg = [x.alt_deg for x in points]
    y_airmass = [x.airmass for x in points]
    y_ha = [math.degrees(x.ha_rad) for x in points]

    max_alt = max(y_altitude_deg)
    min_alt = min(y_altitude_deg)
    delta_alt = max_alt - min_alt
    alt_90 = min_alt + 0.9*delta_alt
    alt_80 = min_alt + 0.8*delta_alt

    alt_30 = min_alt + 0.3*delta_alt
    alt_20 = min_alt + 0.2*delta_alt

    alt_50 = min_alt + 0.5*delta_alt

    fig, ax = plt.subplots()
    plt.plot_date(x_coords,
                  y_airmass,
                  fmt='-r',
                  xdate=True,
                  ydate=False,
                  label='Airmass')
    ax.legend()
    ax.set_title(title)
    ax.set_xlabel('Local Time')
    ax.set_ylim(bottom=1.0,top=5.0)
    ax.set_ylabel('Airmass')
    ax.grid()
    ax2 = ax.twinx()
    plt.plot_date(x_coords,
                  y_altitude_deg,
                  fmt='-g',
                  xdate=True,
                  ydate=False,
                  label='Elevation')
    ax2.set_ylabel('Elevation (deg)')

    ax2.set_ylabel('Elevation (deg)')

    ax2.legend()
    
    #midpoint_mpl = matplotlib.dates.date2num(jd2dt(jd_transit_midpoint))

    ################################
    ## Ingress & Egress Lines
    ################################
    if jd_ingress_start != None and dt_ingress >= start_time and dt_ingress <= stop_time:
        ingress_mpl = matplotlib.dates.date2num(dt_ingress)
        plt.plot_date([ingress_mpl for x in range(2)],
                      [min_alt, max_alt],
                      fmt='-b',
                      xdate=True,
                      ydate=False)
        plt.text(ingress_mpl, alt_90, " Ingress")
        plt.scatter(ingress_mpl, alt_90, s=12, c='blue')

    if jd_egress_end != None and dt_egress >= start_time and dt_egress <= stop_time:
        egress_mpl = matplotlib.dates.date2num(dt_egress)
        plt.plot_date([egress_mpl for x in range(2)],
                      [min_alt, max_alt],
                      fmt='-b',
                      xdate=True,
                      ydate=False)
        plt.text(egress_mpl, alt_80, " Egress")
        plt.scatter(egress_mpl, alt_80, s=12, c='blue')
                            
        ################################
        ## Sunset & Sunrise Lines
        ################################
    if sunset_utc >= start_time and sunset_utc <= stop_time:
        sunset_mpl = matplotlib.dates.date2num(sunset_utc)
        plt.plot_date([sunset_mpl for x in range(2)],
                      [min_alt, max_alt],
                      fmt='--k',
                      xdate=True,
                      ydate=False)
        plt.text(sunset_mpl, alt_30, " Sunset")
        plt.scatter(sunset_mpl, alt_30, s=12, c='black')

    if sunrise_utc >= start_time and sunrise_utc <= stop_time:
        sunrise_mpl = matplotlib.dates.date2num(sunrise_utc)
        plt.plot_date([sunrise_mpl for x in range(2)],
                      [min_alt, max_alt],
                      fmt='--k',
                      xdate=True,
                      ydate=False)
        plt.text(sunrise_mpl, alt_20, " Sunrise")
        plt.scatter(sunrise_mpl, alt_20, s=12, c='black')
                            
    ################################
    ## Meridian Flip
    ################################
    if meridian_flip_dt != None:
        meridian_mpl = matplotlib.dates.date2num(meridian_flip_dt)
        plt.plot_date([meridian_mpl for x in range(2)],
                      [min_alt, max_alt],
                      fmt=':y',
                      xdate=True,
                      ydate=False)
        plt.text(meridian_mpl, alt_30, " Meridian Crossing")
        plt.scatter(meridian_mpl, alt_50, s=12, c='yellow')

    # Tick format: %H:%M
    # matplotlib.dates.DateFormatter('%H:%M', tz=None)
    ax.xaxis.set_major_formatter(matplotlib.dates.DateFormatter('%H:%M', tz=local_tz))
    plt.show()

if __name__ == "__main__":
    main()

                                 
