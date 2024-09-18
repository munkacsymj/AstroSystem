#  ccd.py
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

import coordinates

class CCD:
    def __init__(self, center_pcs):
        self.center = center_pcs
        self.width = 512
        self.height = 512

    def Recenter(self, center_pcs):
        self.center = center_pcs

    def UpperRight(self):
        return coordinates.PCS(self.center.x + self.width/2, self.center.y - self.height/2).ToDCS()

    def UpperLeft(self):
        return coordinates.PCS(self.center.x - self.width/2, self.center.y - self.height/2).ToDCS()

    def LowerRight(self):
        return coordinates.PCS(self.center.x + self.width/2, self.center.y + self.height/2).ToDCS()

    def LowerLeft(self):
        return coordinates.PCS(self.center.x - self.width/2, self.center.y + self.height/2).ToDCS()
        
    def Draw(self, ctx):
        ctx.set_source_rgb(0.75, 0.75, 0.95)
        ctx.rectangle(self.UpperLeft().x, self.UpperLeft().y, self.width, self.height)
        ctx.stroke()
