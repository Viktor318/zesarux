/* pt3player.h: Pro Tracker 3 music module player
   Copyright (c) 2026 Viktor (GitHub: Viktor318)

   Based on ZXTune (https://zxtune.bitbucket.io/), Copyright (c)
   Vitamin/CAIG <vitamin.caig@gmail.com>, GNU LGPL version 3: the module
   parser (src/formats/chiptune/aym/protracker3_compiled.cpp), the player
   (src/module/players/aym/vortex.cpp), the track cursor
   (src/module/players/tracking.h) and the tables
   (src/module/players/aym/freq_tables.cpp).

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#ifndef FUSE_PT3PLAYER_H
#define FUSE_PT3PLAYER_H

#include <stddef.h>

/* Plays a Pro Tracker 3 (.pt3, also Vortex Tracker II, and its text
   modules: .vt2, .txt) module: works out the AY registers of every frame,
   as the player of the tracker does. A TurboSound module in one file (the
   mode byte of the header) plays on two AY chips, see pt3_chips() */

typedef struct pt3_module pt3_module;

/* The registers written in a frame: bit n for register n. Register 13
   (envelope shape) is written only when the music sets it, as writing it
   starts the envelope again */
#define PT3_REGISTERS 14

/* Whether the data looks like a Pro Tracker 3 module */
int pt3_check( const unsigned char *data, size_t length );

/* NULL if the data is not a module that can be played */
pt3_module *pt3_load( const unsigned char *data, size_t length );
void pt3_free( pt3_module *module );

const char *pt3_title( const pt3_module *module );
const char *pt3_author( const pt3_module *module );

/* Back to the start */
void pt3_reset( pt3_module *module );

/* The registers of the next frame; *mask tells which ones to write */
void pt3_frame( pt3_module *module, unsigned char registers[ PT3_REGISTERS ],
                unsigned int *mask );

/* 2 for a TurboSound module in one file, else 1 */
int pt3_chips( const pt3_module *module );

/* The next frame of both chips of such a module */
void pt3_frame_ts( pt3_module *module,
                   unsigned char registers[][ PT3_REGISTERS ],
                   unsigned int mask[] );

/* The length in frames, up to the end of the last position, and the frame
   of the loop position (*loop), as ZXTune tells them; the module is left at
   its start */
long pt3_length( pt3_module *module, long *loop );

/* How many times the music has gone back to its loop position */
int pt3_loops( const pt3_module *module );

#endif			/* #ifndef FUSE_PT3PLAYER_H */
