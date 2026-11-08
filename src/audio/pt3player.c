/* pt3player.c: Pro Tracker 3 music module player
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "pt3player.h"
#include "pt3model.h"
#include "pt3tables.h"

/* ------------------------------------------------------------------------
   Reading the module
   ------------------------------------------------------------------------ */

struct pt3_reader {
  const unsigned char *data;
  size_t length;
  int error;
};

static unsigned int
read_byte( struct pt3_reader *r, size_t offset )
{
  if( offset >= r->length ) { r->error = 1; return 0; }
  return r->data[ offset ];
}

static unsigned int
read_le16( struct pt3_reader *r, size_t offset )
{
  return read_byte( r, offset ) | read_byte( r, offset + 1 ) << 8;
}

static unsigned int
read_be16( struct pt3_reader *r, size_t offset )
{
  return read_byte( r, offset ) << 8 | read_byte( r, offset + 1 );
}

static int
read_le16_signed( struct pt3_reader *r, size_t offset )
{
  return (short)read_le16( r, offset );
}

/* The size of the header with the positions (0: not a module) */
static size_t
header_size( const unsigned char *data, size_t length )
{
  size_t i;

  if( length < PT3_HEADER_SIZE + 1 ) return 0;
  for( i = PT3_HEADER_SIZE;
       i < length && i < PT3_HEADER_SIZE + PT3_MAX_POSITIONS + 1; i++ ) {
    if( data[i] == 0xff ) return i + 1;
    if( data[i] % 3 ) return 0;
  }
  return 0;
}

static int
compiled_check( const unsigned char *data, size_t length )
{
  size_t size = header_size( data, length );
  unsigned int patterns;

  if( !size || size == PT3_HEADER_SIZE + 1 ) return 0;	/* no positions */
  if( length < PT3_MIN_SIZE ) return 0;
  if( data[100] < 1 ) return 0;				/* tempo */
  patterns = data[103] | data[104] << 8;
  /* The pattern table follows the positions */
  return patterns == size;
}

int
pt3_check( const unsigned char *data, size_t length )
{
  return compiled_check( data, length ) || pt3text_check( data, length );
}

static void
copy_text( char *dest, size_t size, const unsigned char *src, size_t length )
{
  size_t i, n = 0;

  for( i = 0; i < length && n + 1 < size; i++ )
    dest[ n++ ] = src[i] >= 32 && src[i] < 127 ? (char)src[i] : ' ';
  dest[n] = '\0';
  /* Spaces at both ends go */
  while( n && dest[ n - 1 ] == ' ' ) dest[ --n ] = '\0';
  for( i = 0; dest[i] == ' '; i++ ) ;
  if( i ) memmove( dest, dest + i, n - i + 1 );
}

static void
read_texts( pt3_module *m, const unsigned char *data )
{
  char by[5];

  /* "<title, 32> by <author, 32>", or a title of all of it */
  copy_text( by, sizeof( by ), data + 62, 4 );
  if( ( by[0] == 'b' || by[0] == 'B' ) && ( by[1] == 'y' || by[1] == 'Y' ) &&
      !by[2] ) {
    copy_text( m->title, sizeof( m->title ), data + 30, 32 );
    copy_text( m->author, sizeof( m->author ), data + 66, 32 );
  } else {
    copy_text( m->title, sizeof( m->title ), data + 30, 68 );
    m->author[0] = '\0';
  }
}

static void
read_sample( pt3_module *m, struct pt3_reader *r, unsigned int index,
             size_t offset )
{
  struct pt3_sample *s = &m->samples[ index ];
  unsigned int i;

  if( !offset || offset + 2 > r->length ) return;
  s->loop = r->data[ offset ];
  s->size = r->data[ offset + 1 ];
  if( !s->size ) return;
  s->lines = calloc( s->size, sizeof( *s->lines ) );
  if( !s->lines ) { s->size = 0; return; }

  for( i = 0; i < s->size; i++ ) {
    struct pt3_sample_line *l = &s->lines[i];
    /* 64 lines of 4 bytes at most: the rest of the lines repeat them */
    size_t at = offset + 2 + ( i % 64 ) * 4;
    unsigned int b0, b1, offset_bits;

    if( at + 4 > r->length ) {
      /* Partial sample: the missing lines are silent */
      l->tone_mask = l->noise_mask = l->envelope_mask = 1;
      continue;
    }
    /* SUoooooE NkKTLLLL OOOOOOOO OOOOOOOO */
    b0 = r->data[ at ];
    b1 = r->data[ at + 1 ];
    offset_bits = ( b0 & 62 ) >> 1;
    l->envelope_mask = b0 & 1;
    l->noise_or_envelope_offset =
      (signed char)( offset_bits & 16 ? offset_bits | 0xf0 : offset_bits );
    l->volume_slide = ( b0 & 128 ) ? ( ( b0 & 64 ) ? +1 : -1 ) : 0;
    l->level = b1 & 15;
    l->tone_mask = ( b1 & 16 ) != 0;
    l->keep_noise_or_envelope_offset = ( b1 & 32 ) != 0;
    l->keep_tone_offset = ( b1 & 64 ) != 0;
    l->noise_mask = ( b1 & 128 ) != 0;
    l->tone_offset = (short)( r->data[ at + 2 ] | r->data[ at + 3 ] << 8 );
  }
  if( s->loop > s->size ) s->loop = s->size;
}

static void
read_ornament( pt3_module *m, struct pt3_reader *r, unsigned int index,
               size_t offset )
{
  struct pt3_ornament *o = &m->ornaments[ index ];
  unsigned int i;

  if( !offset || offset + 2 > r->length ) return;
  o->loop = r->data[ offset ];
  o->size = r->data[ offset + 1 ];
  if( !o->size ) return;
  o->lines = calloc( o->size, 1 );
  if( !o->lines ) { o->size = 0; return; }

  for( i = 0; i < o->size; i++ ) {
    size_t at = offset + 2 + ( i & 255 );
    o->lines[i] = at < r->length ? (signed char)r->data[ at ] : 0;
  }
  if( o->loop > o->size ) o->loop = o->size;
}

/* Parsing a pattern: where each channel's data is and how many lines it
   skips */
struct pt3_channel_cursor {
  size_t offset;
  unsigned int period;
  unsigned int counter;
};

void
pt3_model_add_command( struct pt3_cell *cell, int type, int p1, int p2,
                       int p3 )
{
  struct pt3_command *c;

  if( cell->command_count >= PT3_MAX_COMMANDS ) return;
  c = &cell->commands[ cell->command_count++ ];
  c->type = type;
  c->param1 = p1;
  c->param2 = p2;
  c->param3 = p3;
}

struct pt3_command *
pt3_model_find_command( struct pt3_cell *cell, int type )
{
  int i;

  for( i = 0; i < cell->command_count; i++ )
    if( cell->commands[i].type == type ) return &cell->commands[i];
  return NULL;
}

static void
read_channel( struct pt3_reader *r, struct pt3_channel_cursor *c,
              struct pt3_cell *cell, struct pt3_line *line )
{
  unsigned int effects[ 16 ];
  int effect_count = 0, note = -1, i;

  while( !r->error && c->offset < r->length ) {
    unsigned int cmd = read_byte( r, c->offset++ );

    if( cmd < 0x10 ) {
      if( effect_count < 16 ) effects[ effect_count++ ] = cmd;
    } else if( cmd < 0x20 || ( cmd >= 0xb2 && cmd <= 0xbf ) || cmd >= 0xf0 ) {
      int has_envelope = cmd >= 0x11 && cmd <= 0xbf;
      int has_ornament = cmd >= 0xf0;
      int has_sample = cmd < 0xb2 || cmd > 0xbf;

      if( has_envelope ) {
        unsigned int type = cmd - ( cmd >= 0xb2 ? 0xb1 : 0x10 );
        unsigned int tone = read_be16( r, c->offset );
        c->offset += 2;
        pt3_model_add_command( cell, PT3_ENVELOPE, type, tone, 0 );
      } else {
        pt3_model_add_command( cell, PT3_NOENVELOPE, 0, 0, 0 );
      }
      if( has_ornament ) {
        cell->has_ornament = 1;
        cell->ornament = cmd - 0xf0;
      }
      if( has_sample ) {
        unsigned int double_sample = read_byte( r, c->offset++ );
        int valid = double_sample < PT3_SAMPLES * 2 && !( double_sample & 1 );
        cell->has_sample = 1;
        cell->sample = valid ? double_sample / 2 : 0;
      }
    } else if( cmd < 0x40 ) {
      pt3_model_add_command( cell, PT3_NOISEBASE, cmd - 0x20, 0, 0 );
    } else if( cmd < 0x50 ) {
      cell->has_ornament = 1;
      cell->ornament = cmd - 0x40;
    } else if( cmd < 0xb0 ) {
      note = cmd - 0x50;
      break;
    } else if( cmd == 0xb0 ) {
      pt3_model_add_command( cell, PT3_NOENVELOPE, 0, 0, 0 );
    } else if( cmd == 0xb1 ) {
      c->period = ( read_byte( r, c->offset++ ) - 1 ) & 0xff;
    } else if( cmd == 0xc0 ) {
      cell->has_enabled = 1;
      cell->enabled = 0;
      break;
    } else if( cmd < 0xd0 ) {
      cell->has_volume = 1;
      cell->volume = cmd - 0xc0;
    } else if( cmd == 0xd0 ) {
      break;
    } else if( cmd < 0xf0 ) {
      cell->has_sample = 1;
      cell->sample = cmd - 0xd0;
    }
  }

  /* The parameters of the effects follow, in reverse order */
  for( i = effect_count - 1; i >= 0 && !r->error; i-- ) {
    switch( effects[i] ) {
    case 1: {			/* glissando */
      unsigned int period = read_byte( r, c->offset++ );
      int step = read_le16_signed( r, c->offset );
      c->offset += 2;
      pt3_model_add_command( cell, PT3_GLISS, period, step, 0 );
      break;
    }
    case 2: {			/* portamento to the note */
      unsigned int period = read_byte( r, c->offset++ );
      int step;
      c->offset += 2;		/* limit: not used */
      step = read_le16_signed( r, c->offset );
      c->offset += 2;
      pt3_model_add_command( cell, PT3_GLISS_NOTE, period, step, 0 );
      break;
    }
    case 3:
      pt3_model_add_command( cell, PT3_SAMPLEOFFSET, read_byte( r, c->offset++ ), 0, 0 );
      break;
    case 4:
      pt3_model_add_command( cell, PT3_ORNAMENTOFFSET, read_byte( r, c->offset++ ), 0,
                   0 );
      break;
    case 5: {
      unsigned int on = read_byte( r, c->offset++ );
      unsigned int off = read_byte( r, c->offset++ );
      pt3_model_add_command( cell, PT3_VIBRATE, on, off, 0 );
      break;
    }
    case 8: {
      unsigned int period = read_byte( r, c->offset++ );
      int step = read_le16_signed( r, c->offset );
      c->offset += 2;
      pt3_model_add_command( cell, PT3_SLIDEENV, period, step, 0 );
      break;
    }
    case 9:
      line->tempo = read_byte( r, c->offset++ );
      break;
    }
  }

  if( note >= 0 ) {
    struct pt3_command *gliss = pt3_model_find_command( cell, PT3_GLISS_NOTE );
    cell->has_enabled = 1;
    cell->enabled = 1;
    /* With a portamento the note is where to slide to */
    if( gliss ) {
      gliss->param3 = note;
    } else {
      cell->has_note = 1;
      cell->note = note;
    }
  }
}

static void
read_pattern( pt3_module *m, struct pt3_reader *r, unsigned int index,
              size_t table, size_t min_offset )
{
  struct pt3_pattern *p = &m->patterns[ index ];
  struct pt3_channel_cursor c[3];
  struct pt3_line *lines;
  unsigned int line, chan;
  size_t at = table + index * 6;

  if( p->lines ) return;	/* already read */
  memset( c, 0, sizeof( c ) );
  for( chan = 0; chan < 3; chan++ ) {
    c[ chan ].offset = read_le16( r, at + chan * 2 );
    if( r->error || !c[ chan ].offset || c[ chan ].offset < min_offset ||
        c[ chan ].offset >= r->length ) {
      r->error = 0;
      return;
    }
  }

  lines = calloc( PT3_MAX_PATTERN_SIZE, sizeof( *lines ) );
  if( !lines ) return;

  for( line = 0; line < PT3_MAX_PATTERN_SIZE; line++ ) {
    unsigned int skip = c[0].counter, has_line = 1;

    for( chan = 1; chan < 3; chan++ )
      if( c[ chan ].counter < skip ) skip = c[ chan ].counter;
    if( skip ) {
      for( chan = 0; chan < 3; chan++ ) c[ chan ].counter -= skip;
      line += skip;
    }

    /* The pattern ends where channel A has 0 */
    for( chan = 0; chan < 3; chan++ ) {
      if( c[ chan ].counter ) continue;
      if( c[ chan ].offset >= r->length ||
          ( chan == 0 && r->data[ c[ chan ].offset ] == 0x00 ) ) {
        has_line = 0;
        break;
      }
    }
    if( !has_line || line >= PT3_MAX_PATTERN_SIZE ) break;

    lines[ line ].present = 1;
    for( chan = 0; chan < 3; chan++ ) {
      if( c[ chan ].counter ) {
        c[ chan ].counter--;
        continue;
      }
      read_channel( r, &c[ chan ], &lines[ line ].cells[ chan ],
                    &lines[ line ] );
      c[ chan ].counter = c[ chan ].period;
    }
    if( r->error ) { r->error = 0; line++; break; }
  }

  p->lines = lines;
  p->size = line ? ( line > PT3_MAX_PATTERN_SIZE ? PT3_MAX_PATTERN_SIZE : line )
                 : 1;
}

void
pt3_model_tables( pt3_module *m, unsigned int version,
                  unsigned int note_table )
{
  m->version = version;
  m->volume_table = version <= 4 ? pt3_volume_33 : pt3_volume_35;
  switch( note_table ) {
  case 1: m->note_table = pt3_table_st; break;
  case 2: m->note_table = version <= 3 ? pt3_table_asm33 : pt3_table_asm34;
    break;
  case 3: m->note_table = version <= 3 ? pt3_table_real33 : pt3_table_real34;
    break;
  case 4: m->note_table = pt3_table_natural; break;
  default: m->note_table = version <= 3 ? pt3_table_pt33 : pt3_table_pt34;
    break;
  }
}

pt3_module *
pt3_load( const unsigned char *data, size_t length )
{
  struct pt3_reader reader;
  pt3_module *m;
  size_t size, table, min_offset;
  unsigned int i, max_pattern = 0, version, tone_table;

  /* Vortex Tracker text */
  if( pt3text_check( data, length ) ) return pt3text_load( data, length );

  if( length > PT3_MAX_SIZE ) length = PT3_MAX_SIZE;
  if( !compiled_check( data, length ) ) return NULL;
  size = header_size( data, length );

  m = calloc( 1, sizeof( *m ) );
  if( !m ) return NULL;

  reader.data = data;
  reader.length = length;
  reader.error = 0;

  read_texts( m, data );

  version = isdigit( data[13] ) ? (unsigned int)( data[13] - '0' ) : 6;
  tone_table = data[99];
  pt3_model_tables( m, version, tone_table );
  m->initial_tempo = data[100];
  m->loop_position = data[102];

  for( i = PT3_HEADER_SIZE; i < size - 1; i++ ) {
    unsigned int pattern = data[i] / 3;
    m->positions[ m->position_count++ ] = pattern;
    if( pattern > max_pattern ) max_pattern = pattern;
  }
  if( m->loop_position >= m->position_count ) m->loop_position = 0;

  /* TurboSound in one module: the second AY plays the patterns from the
     mode byte down, as ZXTune does when they fit in */
  if( data[98] != PT3_SINGLE_AY_MODE && data[98] > max_pattern &&
      data[98] < max_pattern * 2 && data[98] <= PT3_PATTERNS )
    m->ts_base = data[98];

  table = data[103] | data[104] << 8;
  min_offset = table + ( m->ts_base ? m->ts_base - 1 : max_pattern ) * 6;
  for( i = 0; i < m->position_count; i++ )
    if( m->positions[i] < PT3_PATTERNS ) {
      read_pattern( m, &reader, m->positions[i], table, min_offset );
      if( m->ts_base )
        read_pattern( m, &reader, m->ts_base - 1 - m->positions[i], table,
                      min_offset );
    }

  for( i = 0; i < PT3_SAMPLES; i++ )
    read_sample( m, &reader, i, read_le16( &reader, 105 + i * 2 ) );
  for( i = 0; i < PT3_ORNAMENTS; i++ )
    read_ornament( m, &reader, i, read_le16( &reader, 169 + i * 2 ) );

  /* At least one pattern must be there */
  for( i = 0; i < m->position_count; i++ )
    if( m->positions[i] < PT3_PATTERNS &&
        m->patterns[ m->positions[i] ].lines ) break;
  if( i == m->position_count ) {
    pt3_free( m );
    return NULL;
  }

  /* The player of the second AY */
  if( m->ts_base ) {
    m->mirror = malloc( sizeof( *m->mirror ) );
    if( !m->mirror ) {
      pt3_free( m );
      return NULL;
    }
    *m->mirror = *m;
    m->mirror->mirror = NULL;
    m->mirror->shared = 1;
  }

  pt3_reset( m );
  return m;
}

void
pt3_free( pt3_module *m )
{
  int i;

  if( !m ) return;
  if( m->shared ) {
    free( m );
    return;
  }
  pt3_free( m->mirror );
  for( i = 0; i < PT3_PATTERNS; i++ ) free( m->patterns[i].lines );
  for( i = 0; i < PT3_SAMPLES; i++ ) free( m->samples[i].lines );
  for( i = 0; i < PT3_ORNAMENTS; i++ ) free( m->ornaments[i].lines );
  free( m );
}

const char *
pt3_title( const pt3_module *m )
{
  return m->title;
}

const char *
pt3_author( const pt3_module *m )
{
  return m->author;
}

int
pt3_loops( const pt3_module *m )
{
  return m->loops;
}

/* ------------------------------------------------------------------------
   The track cursor
   ------------------------------------------------------------------------ */

static const struct pt3_line *
current_line( const pt3_module *m )
{
  const struct pt3_pattern *p;

  if( m->pattern >= PT3_PATTERNS ) return NULL;
  p = &m->patterns[ m->pattern ];
  if( !p->lines || m->line >= (unsigned int)p->size ) return NULL;
  return p->lines[ m->line ].present ? &p->lines[ m->line ] : NULL;
}

/* The pattern the second AY of a TurboSound module plays, or NULL */
static const struct pt3_pattern *
mirrored_pattern( const pt3_module *m )
{
  unsigned int index;

  if( !m->ts_base || m->pattern >= m->ts_base ) return NULL;
  index = m->ts_base - 1 - m->pattern;
  if( index >= PT3_PATTERNS || !m->patterns[ index ].lines ) return NULL;
  return &m->patterns[ index ];
}

static int
line_valid( const pt3_module *m )
{
  const struct pt3_pattern *mirrored = mirrored_pattern( m );

  if( m->pattern >= PT3_PATTERNS ) return 0;
  /* TurboSound: the line must be there in both patterns */
  if( mirrored && m->line >= (unsigned int)mirrored->size ) return 0;
  return m->patterns[ m->pattern ].lines &&
         m->line < (unsigned int)m->patterns[ m->pattern ].size;
}

/* A line starts: its tempo, if it sets one (TurboSound: that of the second
   AY first, as ZXTune does) */
static void
line_loaded( pt3_module *m )
{
  const struct pt3_line *line = current_line( m );
  const struct pt3_pattern *mirrored = mirrored_pattern( m );

  if( mirrored && m->line < (unsigned int)mirrored->size &&
      mirrored->lines[ m->line ].present &&
      mirrored->lines[ m->line ].tempo ) {
    m->tempo = mirrored->lines[ m->line ].tempo;
    return;
  }
  if( line && line->tempo ) m->tempo = line->tempo;
}

static void
set_position( pt3_module *m, unsigned int position )
{
  m->position = position;
  m->pattern = position < m->position_count ? m->positions[ position ] : 0;
  m->line = 0;
  m->quirk = 0;
  line_loaded( m );
}

static void
cursor_reset( pt3_module *m )
{
  m->tempo = m->initial_tempo;
  set_position( m, 0 );
}

static int
advance_line( pt3_module *m )
{
  m->line++;
  m->quirk = 0;
  line_loaded( m );
  return line_valid( m );
}

static int
next_position( pt3_module *m )
{
  set_position( m, m->position + 1 );
  return m->position < m->position_count;
}

static void
next_frame( pt3_module *m )
{
  if( ++m->quirk < m->tempo ) return;
  if( advance_line( m ) ) return;
  if( next_position( m ) ) return;

  /* The end: from the start to the loop position, so that the tempo is
     what it is there */
  cursor_reset( m );
  while( m->position < m->position_count &&
         m->position != m->loop_position ) {
    if( !advance_line( m ) ) next_position( m );
  }
  m->loops++;
}

/* Two hours at most */
#define PT3_MAX_LENGTH ( 2L * 60 * 60 * 50 )

long
pt3_length( pt3_module *m, long *loop )
{
  long frames = 0;

  *loop = 0;
  cursor_reset( m );
  m->loops = 0;
  while( m->loops == 0 && frames < PT3_MAX_LENGTH ) {
    if( m->position == m->loop_position && m->line == 0 && m->quirk == 0 &&
        !*loop )
      *loop = frames;
    next_frame( m );
    frames++;
  }
  pt3_reset( m );
  return frames;
}

/* ------------------------------------------------------------------------
   The player
   ------------------------------------------------------------------------ */

static int
slider_update( struct pt3_slider *s )
{
  if( s->counter && !--s->counter ) {
    s->value += s->delta;
    s->counter = s->period;
    return 1;
  }
  return 0;
}

static void
slider_reset( struct pt3_slider *s )
{
  s->counter = 0;
  s->value = 0;
}

static int
clamp( int value, int low, int high )
{
  return value < low ? low : value > high ? high : value;
}

static void
channel_reset( struct pt3_channel *c )
{
  memset( c, 0, sizeof( *c ) );
  c->sample = PT3_DEFAULT_SAMPLE;
  c->ornament = PT3_DEFAULT_ORNAMENT;
  c->volume = 15;
  c->sliding_target_note = PT3_NO_TARGET;
}

void
pt3_reset( pt3_module *m )
{
  int i;

  for( i = 0; i < 3; i++ ) channel_reset( &m->channels[i] );
  m->envelope_base = 0;
  memset( &m->envelope_slider, 0, sizeof( m->envelope_slider ) );
  m->noise_base = m->noise_addon = 0;
  m->loops = 0;
  cursor_reset( m );
  if( m->mirror ) pt3_reset( m->mirror );
}

static int
sliding_difference( const pt3_module *m, int from, int to )
{
  from = clamp( from, 0, PT3_NOTES - 1 );
  to = clamp( to, 0, PT3_NOTES - 1 );
  return (int)m->note_table[ to ] - (int)m->note_table[ from ];
}

static void
new_channel_state( pt3_module *m, const struct pt3_cell *src,
                   struct pt3_channel *dst, unsigned char *registers,
                   unsigned int *mask )
{
  int previous_slide = dst->tone_slider.value;
  int i;

  if( src->has_enabled ) {
    dst->position_in_sample = dst->position_in_ornament = 0;
    dst->volume_slide = dst->envelope_sliding = dst->noise_sliding = 0;
    slider_reset( &dst->tone_slider );
    dst->tone_accumulator = 0;
    dst->vibrate_counter = 0;
    dst->enabled = src->enabled;
  }
  if( src->has_note ) dst->note = src->note;
  if( src->has_sample ) dst->sample = src->sample;
  if( src->has_ornament ) {
    dst->ornament = src->ornament;
    dst->position_in_ornament = 0;
  }
  if( src->has_volume ) dst->volume = src->volume;

  for( i = 0; i < src->command_count; i++ ) {
    const struct pt3_command *cmd = &src->commands[i];

    switch( cmd->type ) {
    case PT3_GLISS:
      dst->tone_slider.period = dst->tone_slider.counter = cmd->param1;
      dst->tone_slider.delta = cmd->param2;
      dst->sliding_target_note = PT3_NO_TARGET;
      dst->vibrate_counter = 0;
      if( !dst->tone_slider.counter && m->version >= 7 )
        dst->tone_slider.counter++;
      break;
    case PT3_GLISS_NOTE:
      dst->vibrate_counter = 0;
      dst->tone_slider.period = dst->tone_slider.counter = cmd->param1;
      dst->tone_slider.delta = abs( cmd->param2 );
      dst->sliding_target_note = cmd->param3;
      dst->sliding_delta = sliding_difference( m, dst->note,
                                               dst->sliding_target_note );
      if( m->version >= 6 ) dst->tone_slider.value = previous_slide;
      /* tone up, frequency up */
      if( dst->sliding_delta - dst->tone_slider.value < 0 )
        dst->tone_slider.delta = -dst->tone_slider.delta;
      break;
    case PT3_SAMPLEOFFSET:
      dst->position_in_sample = cmd->param1;
      break;
    case PT3_ORNAMENTOFFSET:
      dst->position_in_ornament = cmd->param1;
      break;
    case PT3_VIBRATE:
      dst->vibrate_counter = dst->vibrate_on = cmd->param1;
      dst->vibrate_off = cmd->param2;
      dst->tone_slider.value = 0;
      dst->tone_slider.counter = 0;
      break;
    case PT3_SLIDEENV:
      m->envelope_slider.period = m->envelope_slider.counter = cmd->param1;
      m->envelope_slider.delta = cmd->param2;
      break;
    case PT3_ENVELOPE:
      registers[13] = cmd->param1;
      *mask |= 1 << 13;
      m->envelope_base = cmd->param2;
      dst->envelope = 1;
      slider_reset( &m->envelope_slider );
      dst->position_in_ornament = 0;
      break;
    case PT3_NOENVELOPE:
      dst->envelope = 0;
      dst->position_in_ornament = 0;
      break;
    case PT3_NOISEBASE:
      m->noise_base = cmd->param1;
      break;
    }
  }
}

static void
new_line_state( pt3_module *m, unsigned char *registers, unsigned int *mask )
{
  const struct pt3_line *line;
  int chan;

  if( m->line == 0 ) m->noise_base = 0;
  line = current_line( m );
  if( !line ) return;
  for( chan = 0; chan < 3; chan++ )
    new_channel_state( m, &line->cells[ chan ], &m->channels[ chan ],
                       registers, mask );
}

static const struct pt3_sample_line *
sample_line( const pt3_module *m, unsigned int sample, unsigned int position )
{
  static const struct pt3_sample_line silent = { 0, 0, 1, 0, 0, 1, 1, 0, 0 };
  const struct pt3_sample *s;

  if( sample >= PT3_SAMPLES ) return &silent;
  s = &m->samples[ sample ];
  return position < s->size ? &s->lines[ position ] : &silent;
}

static int
ornament_line( const pt3_module *m, unsigned int ornament,
               unsigned int position )
{
  const struct pt3_ornament *o;

  if( ornament >= PT3_ORNAMENTS ) return 0;
  o = &m->ornaments[ ornament ];
  return position < o->size ? o->lines[ position ] : 0;
}

static void
synthesize_channel( pt3_module *m, int chan, struct pt3_channel *dst,
                    unsigned char *registers, unsigned int *mask,
                    int *envelope_addon )
{
  const struct pt3_sample_line *line;
  const struct pt3_sample *sample;
  unsigned int sample_size, sample_loop, ornament_size, ornament_loop;
  int tone_addon, half_tones, tone_offset, note;
  unsigned int tone, level;

  if( !dst->enabled ) {
    registers[ 8 + chan ] = 0;
    *mask |= 1 << ( 8 + chan );
    return;
  }

  line = sample_line( m, dst->sample, dst->position_in_sample );

  /* Tone */
  tone_addon = line->tone_offset + dst->tone_accumulator;
  if( line->keep_tone_offset ) dst->tone_accumulator = tone_addon;
  half_tones = (int)dst->note +
               ornament_line( m, dst->ornament, dst->position_in_ornament );
  tone_offset = dst->tone_slider.value + tone_addon;
  note = clamp( half_tones, 0, PT3_NOTES - 1 );
  tone = ( m->note_table[ note ] + (unsigned int)tone_offset ) & 0xfff;
  registers[ 2 * chan ] = tone & 0xff;
  registers[ 2 * chan + 1 ] = tone >> 8;
  *mask |= 3 << ( 2 * chan );
  if( line->tone_mask ) registers[7] |= 1 << chan;

  /* Level */
  dst->volume_slide = clamp( dst->volume_slide + line->volume_slide, -15, 15 );
  level = m->volume_table[ dst->volume * 16 +
                           clamp( dst->volume_slide + (int)line->level, 0,
                                  15 ) ];
  registers[ 8 + chan ] = clamp( level, 0, 15 );
  *mask |= 1 << ( 8 + chan );

  /* Envelope */
  if( dst->envelope && !line->envelope_mask ) registers[ 8 + chan ] |= 0x10;

  /* Noise */
  if( line->noise_mask ) {
    int addon = line->noise_or_envelope_offset + dst->envelope_sliding;
    if( line->keep_noise_or_envelope_offset ) dst->envelope_sliding = addon;
    *envelope_addon += addon;
    registers[7] |= 8 << chan;
  } else {
    m->noise_addon = line->noise_or_envelope_offset + dst->noise_sliding;
    if( line->keep_noise_or_envelope_offset )
      dst->noise_sliding = m->noise_addon;
  }

  if( slider_update( &dst->tone_slider ) &&
      dst->sliding_target_note != PT3_NO_TARGET ) {
    if( ( dst->tone_slider.delta < 0 &&
          dst->tone_slider.value <= dst->sliding_delta ) ||
        ( dst->tone_slider.delta >= 0 &&
          dst->tone_slider.value >= dst->sliding_delta ) ) {
      /* Arrived at the target note */
      dst->note = dst->sliding_target_note;
      dst->tone_slider.value = 0;
      dst->tone_slider.counter = 0;
    }
  }

  sample = dst->sample < PT3_SAMPLES ? &m->samples[ dst->sample ] : NULL;
  sample_size = sample ? sample->size : 0;
  sample_loop = sample ? sample->loop : 0;
  if( ++dst->position_in_sample >= sample_size )
    dst->position_in_sample = sample_loop;

  ornament_size = dst->ornament < PT3_ORNAMENTS ?
                  m->ornaments[ dst->ornament ].size : 0;
  ornament_loop = dst->ornament < PT3_ORNAMENTS ?
                  m->ornaments[ dst->ornament ].loop : 0;
  if( ++dst->position_in_ornament >= ornament_size )
    dst->position_in_ornament = ornament_loop;
}

/* The registers of the current frame, without moving on */
static void
render_frame( pt3_module *m, unsigned char registers[ PT3_REGISTERS ],
              unsigned int *mask )
{
  int envelope_addon = 0, chan, period;
  unsigned int noise;

  memset( registers, 0, PT3_REGISTERS );
  *mask = 1 << 7;		/* the mixer is always written */

  if( m->quirk == 0 ) new_line_state( m, registers, mask );

  for( chan = 0; chan < 3; chan++ ) {
    struct pt3_channel *c = &m->channels[ chan ];
    synthesize_channel( m, chan, c, registers, mask, &envelope_addon );
    /* Vibrato */
    if( c->vibrate_counter > 0 && !--c->vibrate_counter ) {
      c->enabled = !c->enabled;
      c->vibrate_counter = c->enabled ? c->vibrate_on : c->vibrate_off;
    }
  }

  noise = m->noise_base + m->noise_addon;
  registers[6] = noise & 31;
  period = envelope_addon + m->envelope_slider.value + (int)m->envelope_base;
  registers[11] = (unsigned int)period & 0xff;
  registers[12] = ( (unsigned int)period >> 8 ) & 0xff;
  *mask |= 1 << 6 | 1 << 11 | 1 << 12;
  slider_update( &m->envelope_slider );
}

void
pt3_frame( pt3_module *m, unsigned char registers[ PT3_REGISTERS ],
           unsigned int *mask )
{
  render_frame( m, registers, mask );
  next_frame( m );
}

int
pt3_chips( const pt3_module *m )
{
  return m->mirror ? 2 : 1;
}

void
pt3_frame_ts( pt3_module *m, unsigned char registers[][ PT3_REGISTERS ],
              unsigned int mask[] )
{
  pt3_module *second = m->mirror;

  render_frame( m, registers[0], &mask[0] );
  if( second ) {
    /* The second AY: the same cursor, on the mirrored pattern */
    second->position = m->position;
    second->pattern = m->ts_base - 1 - m->pattern;
    second->line = m->line;
    second->quirk = m->quirk;
    second->tempo = m->tempo;
    render_frame( second, registers[1], &mask[1] );
  }
  next_frame( m );
}
