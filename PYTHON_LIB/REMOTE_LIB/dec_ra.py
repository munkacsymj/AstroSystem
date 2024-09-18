#  dec_ra.py -- Positions in the Sky
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
# 
#    You should have received a copy of the GNU General Public License
#    along with this program (file: COPYING).  If not, see
#    <http://www.gnu.org/licenses/>. 
# 

class DecRA:
    def __init__(self, dec_string, ra_string):
        self.init_with_radians(float(dec_string), float(ra_string))

    def init_with_radians(self, dec_rad, ra_rad):
        self.dec_rad = dec_rad
        self.ra_rad = ra_rad

        
