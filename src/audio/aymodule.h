/* aymodule.h: The music formats played by register (trackers and register
   dumps), behind one interface
   Copyright (c) 2026 Viktor (GitHub: Viktor318)

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

#ifndef ZESARUX_AYMODULE_H
#define ZESARUX_AYMODULE_H

#include <stddef.h>

/* Every format but .ay (whose songs run on a Z80, aysource.c) works out
   the AY registers of each frame itself: a tracker module by playing it as
   the tracker's player does, a register dump by reading them. Each format
   is one aymodule_format; a new one is added to the list in aymodule.c */

#define AYMODULE_REGISTERS 14

/* TurboSound: up to three AY chips, each playing a module of its own (two
   or three modules and a 02TS / 03TS footer, as made by Vortex Tracker
   and ZXTune, or a Vortex Tracker text file with two modules) */
#define AYMODULE_CHIPS 3

typedef struct aymodule_format {
  const char *name;		/* shown in messages: "PT3", "STC"... */

  /* Whether the data looks like this format (cheap) */
  int ( *check )( const unsigned char *data, size_t length );
  /* NULL if it cannot be played */
  void *( *load )( const unsigned char *data, size_t length );
  void ( *free )( void *module );

  /* Back to the start / the registers of the next frame, *mask: those
     written (register 13 only when written, as that starts the envelope
     again) */
  void ( *reset )( void *module );
  void ( *frame )( void *module, unsigned char *registers,
                   unsigned int *mask );

  /* Texts of the module; NULL: the format has none */
  const char *( *title )( const void *module );
  const char *( *author )( const void *module );
  const char *( *comment )( const void *module );

  /* The length in frames if the file tells it exactly (register dumps;
     trackers: up to the end of the last position), with the frame the
     music goes on from (-1: it ends); NULL or 0: to be measured. It may
     move a tracker's cursor back to the start */
  long ( *exact_length )( const void *module, long *loop );

  /* A stereo mode stored in the file ("ABC"...); NULL or "": none */
  const char *( *stereo )( const void *module );
  /* A module playing more chips by itself (TurboSound in one Pro Tracker
     3 module): how many, and their registers; NULL: one chip */
  int ( *chips )( const void *module );
  void ( *frame_chips )( void *module, unsigned char registers[][ 14 ],
                         unsigned int mask[] );
} aymodule_format;

typedef struct aymodule aymodule;

/* The format of the data; NULL if none of them */
const aymodule_format *aymodule_identify( const unsigned char *data,
                                          size_t length );

/* NULL if the data is not a module that can be played */
aymodule *aymodule_load( const unsigned char *data, size_t length );
void aymodule_free( aymodule *module );

const aymodule_format *aymodule_format_of( const aymodule *module );

void aymodule_reset( aymodule *module );
/* The chips the module plays (1-3) */
int aymodule_chips( const aymodule *module );
/* The next frame: the registers of each chip (as many as
   aymodule_chips()), and those written */
void aymodule_frame( aymodule *module,
                     unsigned char registers[][ AYMODULE_REGISTERS ],
                     unsigned int mask[] );

/* "" when the format has none */
const char *aymodule_title( const aymodule *module );
const char *aymodule_author( const aymodule *module );
const char *aymodule_comment( const aymodule *module );
const char *aymodule_stereo( const aymodule *module );

/* As exact_length above; 0 (and *loop -1) if not known */
long aymodule_exact_length( const aymodule *module, long *loop );

#endif			/* #ifndef ZESARUX_AYMODULE_H */
