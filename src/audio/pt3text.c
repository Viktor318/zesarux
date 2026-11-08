/* pt3text.c: Reading Vortex Tracker II text modules
   Copyright (c) 2026 Viktor (GitHub: Viktor318)

   The format as ZXTune (https://zxtune.bitbucket.io/), Copyright (c)
   Vitamin/CAIG <vitamin.caig@gmail.com>, GNU LGPL version 3, reads it
   (src/formats/chiptune/aym/protracker3_vortex.cpp).

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

/* Vortex Tracker II saves its modules as text too (.vt2, .txt):

   [Module]
   Version=3.5, Title=..., Author=..., NoteTable=1, Speed=5,
   PlayOrder=0,1,L2,3 (L: the loop position), one per line

   [Ornament1]
   L-5,0,3			(the offsets in halftones; L: the loop)

   [Sample1]
   TnE +000_ +00_ E_ L	(tone, noise, envelope masks; tone offset,
				 kept or not; noise or envelope offset, kept
				 or not; volume and its slide; L: the loop)

   [Pattern0]
   ....|..|C-5 1F2F ....|--- .... ....|R-- .... ....
				(envelope base, noise base, then each
				 channel: note, sample, envelope type,
				 ornament, volume, command, its period and
				 parameter)

   Each part ends with an empty line; a second one (or another [Module]:
   the second AY of a TurboSound module) ends the module. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pt3model.h"

#define MAX_LINE 256
#define MAX_SAMPLE_LINES 64
#define MAX_ORNAMENT_LINES 255

typedef struct reader {
  const unsigned char *data;
  size_t length, position;
  char line[ MAX_LINE ];
} reader;

/* The next line, without the spaces at its ends; 0 at the end of the data */
static int
next_line( reader *r )
{
  size_t start, end, length;

  if( r->position >= r->length ) return 0;
  start = r->position;
  while( r->position < r->length && r->data[ r->position ] != '\n' )
    r->position++;
  end = r->position;
  if( r->position < r->length ) r->position++;

  while( start < end && ( r->data[ start ] == ' ' ||
                          r->data[ start ] == '\t' ) )
    start++;
  while( end > start && ( r->data[ end - 1 ] == ' ' ||
                          r->data[ end - 1 ] == '\t' ||
                          r->data[ end - 1 ] == '\r' ) )
    end--;
  length = end - start;
  if( length >= MAX_LINE ) length = MAX_LINE - 1;
  memcpy( r->line, r->data + start, length );
  r->line[ length ] = '\0';
  return 1;
}

static size_t
start_of_text( const unsigned char *data, size_t length )
{
  /* A UTF-8 byte order mark */
  return length >= 3 && data[0] == 0xef && data[1] == 0xbb &&
         data[2] == 0xbf ? 3 : 0;
}

int
pt3text_check( const unsigned char *data, size_t length )
{
  size_t start = start_of_text( data, length );
  return length >= start + 8 && !memcmp( data + start, "[Module]", 8 );
}

/* "[Name<number>]": the number, or -1 */
static int
section( const char *line, const char *name )
{
  size_t length = strlen( name );
  const char *p;
  int number = 0;

  if( line[0] != '[' || strncmp( line + 1, name, length ) ) return -1;
  p = line + 1 + length;
  if( *p < '0' || *p > '9' ) return -1;
  while( *p >= '0' && *p <= '9' ) {
    number = number * 10 + *p - '0';
    if( number > 1000 ) return -1;
    p++;
  }
  return p[0] == ']' && p[1] == '\0' ? number : -1;
}

/* A hexadecimal digit; '.' (or `zero') is 0. -1 if not one */
static int
digit( char c, int max )
{
  int value;

  if( c == '.' ) return 0;
  if( c >= '0' && c <= '9' ) value = c - '0';
  else if( c >= 'A' && c <= 'Z' ) value = c - 'A' + 10;
  else if( c >= 'a' && c <= 'z' ) value = c - 'a' + 10;
  else return -1;
  return value <= max ? value : -1;
}

/* `count' hexadecimal digits (dots are 0) */
static int
hex( const char *text, int count )
{
  int value = 0, d;

  while( count-- ) {
    d = digit( *text++, 15 );
    if( d < 0 ) return -1;
    value = value * 16 + d;
  }
  return value;
}

/* "+0A3": a sign and `count' hexadecimal digits */
static int
signed_hex( const char *text, int count, int *value )
{
  int v;

  if( text[0] != '+' && text[0] != '-' ) return 0;
  v = hex( text + 1, count );
  if( v < 0 ) return 0;
  *value = text[0] == '-' ? -v : v;
  return 1;
}

/* "1,2,L3,4": the numbers and the loop (0 without an L); the count, or -1 */
static int
looped_list( const char *text, int *values, int max, int *loop )
{
  int count = 0;

  *loop = 0;
  while( *text ) {
    char *end;
    long value;

    if( count >= max ) return -1;
    if( *text == 'L' ) {
      *loop = count;
      text++;
    }
    value = strtol( text, &end, 10 );
    if( end == text ) return -1;
    values[ count++ ] = value;
    text = end;
    if( *text == ',' ) text++;
    else if( *text ) return -1;
  }
  return count ? count : -1;
}

static int
read_header( reader *r, pt3_module *m, unsigned int *version,
             unsigned int *note_table )
{
  int positions[ PT3_MAX_POSITIONS ], count = 0, loop = 0, i;

  if( !next_line( r ) || strcmp( r->line, "[Module]" ) ) return 0;
  *version = 0;
  *note_table = 0;
  m->initial_tempo = 0;

  while( next_line( r ) && r->line[0] ) {
    char *value = strchr( r->line, '=' );
    char *name = r->line;
    size_t name_length;

    if( !value ) return 0;
    *value++ = '\0';
    name_length = strlen( name );
    while( name_length && name[ name_length - 1 ] == ' ' )
      name[ --name_length ] = '\0';
    while( *value == ' ' ) value++;

    if( !strcmp( name, "Version" ) ) {
      if( value[0] != '3' || value[1] != '.' || value[2] < '0' ||
          value[2] > '9' )
        return 0;
      *version = value[2] - '0';
    } else if( !strcmp( name, "Title" ) ) {
      snprintf( m->title, sizeof( m->title ), "%s", value );
    } else if( !strcmp( name, "Author" ) ) {
      snprintf( m->author, sizeof( m->author ), "%s", value );
    } else if( !strcmp( name, "NoteTable" ) ) {
      *note_table = atoi( value );
    } else if( !strcmp( name, "Speed" ) ) {
      m->initial_tempo = atoi( value );
    } else if( !strcmp( name, "PlayOrder" ) ) {
      count = looped_list( value, positions, PT3_MAX_POSITIONS, &loop );
      if( count < 0 ) return 0;
    }
  }

  if( !m->initial_tempo || count <= 0 ) return 0;
  for( i = 0; i < count; i++ ) {
    if( positions[i] < 0 || positions[i] >= PT3_PATTERNS ) return 0;
    m->positions[i] = positions[i];
  }
  m->position_count = count;
  m->loop_position = loop;
  return 1;
}

static int
read_ornament( reader *r, pt3_module *m, int index )
{
  struct pt3_ornament *o;
  int values[ MAX_ORNAMENT_LINES ], count, loop, i;

  if( index >= PT3_ORNAMENTS || !next_line( r ) ) return 0;
  count = looped_list( r->line, values, MAX_ORNAMENT_LINES, &loop );
  if( count < 0 ) return 0;

  o = &m->ornaments[ index ];
  free( o->lines );
  o->lines = malloc( count );
  if( !o->lines ) return 0;
  for( i = 0; i < count; i++ ) o->lines[i] = values[i];
  o->size = count;
  o->loop = loop;
  return 1;
}

/* "TnE +000_ +00_ E_ L" */
static int
sample_line( const char *text, struct pt3_sample_line *l, int *looped )
{
  char fields[5][8];
  int count = 0, value;

  while( *text && count < 5 ) {
    size_t length = strcspn( text, " " );
    if( length >= sizeof( fields[0] ) ) return 0;
    memcpy( fields[ count ], text, length );
    fields[ count++ ][ length ] = '\0';
    text += length;
    while( *text == ' ' ) text++;
  }
  if( *text || count < 4 ) return 0;
  *looped = count == 5;
  if( *looped && strcmp( fields[4], "L" ) ) return 0;

  if( strlen( fields[0] ) != 3 || strlen( fields[1] ) != 5 ||
      strlen( fields[2] ) != 4 || strlen( fields[3] ) != 2 )
    return 0;

  if( !strchr( "tT", fields[0][0] ) || !strchr( "nN", fields[0][1] ) ||
      !strchr( "eE", fields[0][2] ) )
    return 0;
  l->tone_mask = fields[0][0] == 't';
  l->noise_mask = fields[0][1] == 'n';
  l->envelope_mask = fields[0][2] == 'e';

  if( !signed_hex( fields[1], 3, &value ) ||
      !strchr( "^_", fields[1][4] ) )
    return 0;
  l->tone_offset = value;
  l->keep_tone_offset = fields[1][4] == '^';

  if( !signed_hex( fields[2], 2, &value ) ||
      !strchr( "^_", fields[2][3] ) )
    return 0;
  l->noise_or_envelope_offset = value;
  l->keep_noise_or_envelope_offset = fields[2][3] == '^';

  value = digit( fields[3][0], 15 );
  if( fields[3][0] == '.' || value < 0 || !strchr( "+-_", fields[3][1] ) )
    return 0;
  l->level = value;
  l->volume_slide = fields[3][1] == '+' ? 1 : fields[3][1] == '-' ? -1 : 0;
  return 1;
}

static int
read_sample( reader *r, pt3_module *m, int index )
{
  struct pt3_sample_line lines[ MAX_SAMPLE_LINES ];
  struct pt3_sample *s;
  int count = 0, loop = -1, looped;

  if( index >= PT3_SAMPLES ) return 0;
  while( next_line( r ) && r->line[0] ) {
    if( count >= MAX_SAMPLE_LINES ||
        !sample_line( r->line, &lines[ count ], &looped ) )
      return 0;
    if( looped && loop < 0 ) loop = count;
    count++;
  }
  if( !count ) return 0;

  s = &m->samples[ index ];
  free( s->lines );
  s->lines = malloc( count * sizeof( *s->lines ) );
  if( !s->lines ) return 0;
  memcpy( s->lines, lines, count * sizeof( *s->lines ) );
  s->size = count;
  s->loop = loop < 0 ? 0 : loop;
  return 1;
}

/* "C-5", "R--" (rest: -2), "---" (none: -1); -3 if not a note */
static int
note( const char *text )
{
  static const char * const names[12] =
    { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
  int i;

  if( !strncmp( text, "---", 3 ) ) return -1;
  if( !strncmp( text, "R--", 3 ) ) return -2;
  if( text[2] < '1' || text[2] > '8' ) return -3;
  for( i = 0; i < 12; i++ )
    if( !strncmp( text, names[i], 2 ) )
      return ( text[2] - '1' ) * 12 + i;
  return -3;
}

/* "C-5 1F2F 1102": the note, its parameters and the command of a channel
   into a cell, in the order of ZXTune's builder */
static int
channel( const char *text, unsigned int envelope_base,
         struct pt3_cell *cell, struct pt3_line *line )
{
  int value, sample, envelope, ornament, volume;
  int command, period, parameter;

  if( strlen( text ) != 13 || text[3] != ' ' || text[8] != ' ' ) return 0;

  value = note( text );
  sample = digit( text[4], 'Z' - 'A' + 10 );
  envelope = digit( text[5], 15 );
  ornament = digit( text[6], 15 );
  volume = digit( text[7], 15 );
  command = digit( text[9], 15 );
  period = digit( text[10], 15 );
  parameter = hex( text + 11, 2 );
  if( value < -2 || sample < 0 || envelope < 0 || ornament < 0 ||
      volume < 0 || command < 0 || period < 0 || parameter < 0 )
    return 0;

  /* The parameters */
  if( sample ) {
    cell->has_sample = 1;
    cell->sample = sample < PT3_SAMPLES ? sample : 0;
  }
  if( envelope ) {
    if( envelope == 15 )
      pt3_model_add_command( cell, PT3_NOENVELOPE, 0, 0, 0 );
    else
      pt3_model_add_command( cell, PT3_ENVELOPE, envelope, envelope_base,
                             0 );
    cell->has_ornament = 1;
    cell->ornament = ornament;
  } else if( ornament ) {
    cell->has_ornament = 1;
    cell->ornament = ornament;
  }
  if( volume ) {
    cell->has_volume = 1;
    cell->volume = volume;
  }

  /* The command */
  switch( command ) {
  case 1:			/* glissando up */
    pt3_model_add_command( cell, PT3_GLISS, period, parameter, 0 );
    break;
  case 2:			/* glissando down */
    pt3_model_add_command( cell, PT3_GLISS, period, -parameter, 0 );
    break;
  case 3:			/* portamento to the note */
    pt3_model_add_command( cell, PT3_GLISS_NOTE, period, parameter, 0 );
    break;
  case 4:
    pt3_model_add_command( cell, PT3_SAMPLEOFFSET, parameter, 0, 0 );
    break;
  case 5:
    pt3_model_add_command( cell, PT3_ORNAMENTOFFSET, parameter, 0, 0 );
    break;
  case 6:			/* vibrato: on and off times */
    pt3_model_add_command( cell, PT3_VIBRATE, parameter / 16,
                           parameter % 16, 0 );
    break;
  case 9:			/* envelope slide up */
    pt3_model_add_command( cell, PT3_SLIDEENV, period, parameter, 0 );
    break;
  case 10:			/* envelope slide down */
    pt3_model_add_command( cell, PT3_SLIDEENV, period, -parameter, 0 );
    break;
  case 11:			/* tempo */
    if( parameter ) line->tempo = parameter;
    break;
  }

  /* The note: with a portamento, where to slide to */
  if( value == -2 ) {
    cell->has_enabled = 1;
    cell->enabled = 0;
  } else if( value >= 0 ) {
    struct pt3_command *gliss = pt3_model_find_command( cell,
                                                        PT3_GLISS_NOTE );
    cell->has_enabled = 1;
    cell->enabled = 1;
    if( gliss ) {
      gliss->param3 = value;
    } else {
      cell->has_note = 1;
      cell->note = value;
    }
  }
  return 1;
}

/* "....|..|C-5 1F2F ....|--- .... ....|R-- .... ...." */
static int
pattern_line( const char *text, struct pt3_line *line )
{
  char fields[5][16];
  int count = 0, envelope_base, noise_base, i;

  while( count < 5 ) {
    size_t length = strcspn( text, "|" );
    if( length >= sizeof( fields[0] ) ) return 0;
    memcpy( fields[ count ], text, length );
    fields[ count++ ][ length ] = '\0';
    text += length;
    if( !*text ) break;
    text++;
  }
  if( count != 5 || *text || strlen( fields[0] ) != 4 ||
      strlen( fields[1] ) != 2 )
    return 0;

  envelope_base = hex( fields[0], 4 );
  noise_base = hex( fields[1], 2 );
  if( envelope_base < 0 || noise_base < 0 ) return 0;

  line->present = 1;
  for( i = 0; i < 3; i++ )
    if( !channel( fields[ 2 + i ], envelope_base, &line->cells[i], line ) )
      return 0;
  /* The noise base goes with the last channel, on every line */
  pt3_model_add_command( &line->cells[2], PT3_NOISEBASE, noise_base, 0, 0 );
  return 1;
}

static int
read_pattern( reader *r, pt3_module *m, int index )
{
  struct pt3_pattern *p;
  struct pt3_line *lines;
  int count = 0;

  if( index >= PT3_PATTERNS ) return 0;
  lines = calloc( PT3_MAX_PATTERN_SIZE, sizeof( *lines ) );
  if( !lines ) return 0;

  while( next_line( r ) && r->line[0] ) {
    if( count >= PT3_MAX_PATTERN_SIZE ||
        !pattern_line( r->line, &lines[ count ] ) ) {
      free( lines );
      return 0;
    }
    count++;
  }
  if( !count ) {
    free( lines );
    return 0;
  }

  p = &m->patterns[ index ];
  free( p->lines );
  p->lines = lines;
  p->size = count;
  return 1;
}

pt3_module *
pt3text_load( const unsigned char *data, size_t length )
{
  reader r;
  pt3_module *m;
  unsigned int version, note_table, i;

  if( !pt3text_check( data, length ) ) return NULL;
  r.data = data;
  r.length = length;
  r.position = start_of_text( data, length );

  m = calloc( 1, sizeof( *m ) );
  if( !m ) return NULL;
  if( !read_header( &r, m, &version, &note_table ) ) {
    pt3_free( m );
    return NULL;
  }
  pt3_model_tables( m, version, note_table );

  /* Ornaments, samples and patterns, up to an empty line or anything else
     (the second module of a TurboSound pair) */
  for( ;; ) {
    size_t start = r.position;
    int index, ok;

    if( !next_line( &r ) || !r.line[0] ) break;
    if( ( index = section( r.line, "Ornament" ) ) >= 0 ) {
      ok = read_ornament( &r, m, index );
      /* An empty line after it */
      start = r.position;
      if( ok && next_line( &r ) && r.line[0] ) r.position = start;
    } else if( ( index = section( r.line, "Sample" ) ) >= 0 ) {
      ok = read_sample( &r, m, index );
    } else if( ( index = section( r.line, "Pattern" ) ) >= 0 ) {
      ok = read_pattern( &r, m, index );
    } else {
      r.position = start;
      break;
    }
    if( !ok ) {
      pt3_free( m );
      return NULL;
    }
  }

  /* Ornament 0 is "no ornament" when not given */
  if( !m->ornaments[0].lines ) {
    m->ornaments[0].lines = calloc( 1, 1 );
    if( m->ornaments[0].lines ) m->ornaments[0].size = 1;
  }

  /* The patterns played must be there */
  for( i = 0; i < m->position_count; i++ )
    if( !m->patterns[ m->positions[i] ].lines ) {
      pt3_free( m );
      return NULL;
    }

  pt3_reset( m );
  return m;
}
