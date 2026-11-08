/* pt3model.h: The Pro Tracker 3 module as the player uses it
   Copyright (c) 2026 Viktor (GitHub: Viktor318)

   Based on ZXTune (https://zxtune.bitbucket.io/), Copyright (c)
   Vitamin/CAIG <vitamin.caig@gmail.com>, GNU LGPL version 3.

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

/* Shared by the readers of the module (pt3player.c: compiled modules,
   pt3text.c: Vortex Tracker text modules) and the player */

#ifndef FUSE_PT3MODEL_H
#define FUSE_PT3MODEL_H

#include "pt3player.h"


#define PT3_NOTES 96
#define PT3_SAMPLES 32
#define PT3_ORNAMENTS 16
#define PT3_PATTERNS 85
#define PT3_MAX_POSITIONS 255
#define PT3_MAX_PATTERN_SIZE 256
#define PT3_MAX_COMMANDS 8
#define PT3_HEADER_SIZE 201	/* without the positions */
#define PT3_MIN_SIZE 200
#define PT3_MAX_SIZE 0xc000
#define PT3_SINGLE_AY_MODE 0x20

#define PT3_DEFAULT_SAMPLE 1
#define PT3_DEFAULT_ORNAMENT 0

/* No target note of a sliding */
#define PT3_NO_TARGET 0xffffffffu

/* ------------------------------------------------------------------------
   The module
   ------------------------------------------------------------------------ */

enum pt3_command_type {
  PT3_GLISS = 1,		/* period, delta */
  PT3_GLISS_NOTE,		/* period, delta, target note */
  PT3_SAMPLEOFFSET,		/* offset */
  PT3_ORNAMENTOFFSET,		/* offset */
  PT3_VIBRATE,			/* on time, off time */
  PT3_SLIDEENV,			/* period, delta */
  PT3_NOENVELOPE,
  PT3_ENVELOPE,			/* shape (r13), period */
  PT3_NOISEBASE,		/* base */
};

struct pt3_command {
  int type;
  int param1, param2, param3;
};

/* What a pattern line sets on a channel */
struct pt3_cell {
  int has_enabled, enabled;
  int has_note; unsigned int note;
  int has_sample; unsigned int sample;
  int has_ornament; unsigned int ornament;
  int has_volume; unsigned int volume;
  int command_count;
  struct pt3_command commands[ PT3_MAX_COMMANDS ];
};

struct pt3_line {
  int present;			/* the line has data */
  unsigned int tempo;		/* 0: no change */
  struct pt3_cell cells[3];
};

struct pt3_pattern {
  int size;			/* lines; 0: no such pattern */
  struct pt3_line *lines;
};

struct pt3_sample_line {
  unsigned int level;
  int volume_slide;
  int tone_mask;
  int tone_offset;
  int keep_tone_offset;
  int noise_mask;
  int envelope_mask;
  int noise_or_envelope_offset;
  int keep_noise_or_envelope_offset;
};

struct pt3_sample {
  unsigned int size, loop;
  struct pt3_sample_line *lines;
};

struct pt3_ornament {
  unsigned int size, loop;
  signed char *lines;
};

/* The player */

struct pt3_slider {
  unsigned int period;
  int value;
  unsigned int counter;
  int delta;
};

struct pt3_channel {
  int enabled;
  int envelope;
  unsigned int note;
  unsigned int sample;
  unsigned int position_in_sample;
  unsigned int ornament;
  unsigned int position_in_ornament;
  unsigned int volume;
  int volume_slide;
  struct pt3_slider tone_slider;
  unsigned int sliding_target_note;
  int sliding_delta;
  int tone_accumulator;
  int envelope_sliding;
  int noise_sliding;
  unsigned int vibrate_counter;
  unsigned int vibrate_on;
  unsigned int vibrate_off;
};

struct pt3_module {
  char title[ 72 ];
  char author[ 40 ];
  unsigned int version;
  const unsigned short *note_table;
  const unsigned char *volume_table;
  unsigned int initial_tempo;

  unsigned int position_count;
  unsigned int loop_position;
  unsigned char positions[ PT3_MAX_POSITIONS ];

  struct pt3_pattern patterns[ PT3_PATTERNS ];
  struct pt3_sample samples[ PT3_SAMPLES ];
  struct pt3_ornament ornaments[ PT3_ORNAMENTS ];

  /* Track cursor */
  unsigned int position, pattern, line, quirk, tempo;
  int loops;

  /* Player state */
  struct pt3_channel channels[3];
  unsigned int envelope_base;
  struct pt3_slider envelope_slider;
  unsigned int noise_base;
  unsigned int noise_addon;

  /* TurboSound in one module (the mode byte of the header is not 0x20):
     the second AY plays pattern `ts_base' - 1 - p where the first plays
     pattern p, with the same cursor. `mirror' is the player of the second
     AY: a copy of this module sharing its data, with its own player state;
     NULL for one AY */
  unsigned int ts_base;
  struct pt3_module *mirror;
  int shared;			/* the data belongs to another module */
};


/* The tables of the version and note table of the module */
void pt3_model_tables( pt3_module *m, unsigned int version,
                       unsigned int note_table );

/* A cell command */
void pt3_model_add_command( struct pt3_cell *cell, int type, int p1, int p2,
                            int p3 );
struct pt3_command *pt3_model_find_command( struct pt3_cell *cell,
                                            int type );

/* A Vortex Tracker II text module ("[Module]"...); NULL if it is not one
   that can be played (pt3text.c) */
int pt3text_check( const unsigned char *data, size_t length );
pt3_module *pt3text_load( const unsigned char *data, size_t length );

#endif			/* #ifndef FUSE_PT3MODEL_H */
