/* This may look like C code, but it is really -*-c++-*- */
/*  lx_FlatListMessage.h -- Scope message to control flat box
 *
 *  Copyright (C) 2020 Mark J. Munkacsy

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (file: COPYING).  If not, see
 *   <http://www.gnu.org/licenses/>. 
 */
#include "lx_gen_message.h"

class lxFlatLightMessage : public lxGenMessage {
public:
  lxFlatLightMessage(int Socket);
  lxFlatLightMessage(lxGenMessage *message);
  ~lxFlatLightMessage(void);

  bool        MoveCommanded(void);
  bool        GetFlatLightDirUp(void); // used in the server
  unsigned char GetStatusByte(void); // used in the client
  void        SetStatusByte(unsigned char status);
  // direction set to either FLAT_MOVE_UP or FLAT_MOVE_DOWN
  void        SetDirectionByte(unsigned char direction);

  // Status Byte Definitions
  static const int FLAT_FULLY_UP = 0x01;
  static const int FLAT_FULLY_DOWN = 0x02;
  static const int FLAT_LIGHT_ON = 0x04;


  // Command (Direction) Byte Definitions
  static const int FLAT_MOVE_UP = 0x01;
  static const int FLAT_MOVE_DOWN = 0x02;
};
