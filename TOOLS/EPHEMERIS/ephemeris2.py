#!/usr/bin/python3

#  ephemeris2.py -- provide sunrise/sunset times
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
import matplotlib
import matplotlib.dates
import matplotlib.pyplot as plt

################################################################
## USER ENTRY AREA
################################################################

latitude_deg = 41.569029
longitude_deg = -71.238451

################################################################
## END OF USER ENTRY AREA
################################################################

sin_lat = math.sin(math.radians(latitude_deg))
cos_lat = math.cos(math.radians(latitude_deg))


# The reference date is midnight at UTC on 1/1/2020
jd_reference = 2458849.5
dt_reference = datetime.datetime(year=2020,month=1,day=1,tzinfo=datetime.timezone.utc)

################################################################
## Convert back and forth between JD and Python datetime
################################################################

def jd2dt(julian):
    delta_jd = julian - jd_reference
    delta = datetime.timedelta(days=delta_jd)
    return dt_reference + delta

def dt2jd(dtime):
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
    zenith = math.radians(96.5) # 96-deg == civil twilight
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

import datetime
today = datetime.datetime.now(datetime.timezone.utc)
dt_date_basis = today

sunset = SunSetRise(dt_date_basis, "sunset") # UTC hour

if sunset > 12.0:
    sunset -= 24.0
    
sunrise = SunSetRise(dt_date_basis, "sunrise") # UTC hour

## Calculate midnight at UTC as a Python datetime
if dt_date_basis.hour > 12:
    midnight_utc = dt_date_basis.replace(day=dt_date_basis.day+1,hour=0,minute=0,second=0,microsecond=0)
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

print('Sunset: ', sunset_utc.astimezone())
print('Sunrise: ', sunrise_utc.astimezone())

