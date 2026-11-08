/* aymodule.c: The music formats played by register (trackers and register
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

/* ZEsarUX (fork Viktor318/zesarux): tomado del fork de Fuse (Viktor318/fuse) con
   solo los formatos Pro Tracker 3 (y Vortex Tracker II en texto) y el contenedor
   TurboSound; los demas formatos se pueden anadir despues a la lista */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aymodule.h"
#include "pt3model.h"
#include "pt3player.h"

struct aymodule {
  const aymodule_format *format;
  void *data;
};

static aymodule *load_single( const unsigned char *data, size_t length );

/* Pro Tracker 3 (also Vortex Tracker II, and its text modules) */

static void *pt3_load_module( const unsigned char *d, size_t l )
{ return pt3_load( d, l ); }
static void pt3_free_module( void *m ) { pt3_free( m ); }
static void pt3_reset_module( void *m ) { pt3_reset( m ); }
static void pt3_frame_module( void *m, unsigned char *r, unsigned int *mask )
{ pt3_frame( m, r, mask ); }
static const char *pt3_title_module( const void *m )
{ return pt3_title( m ); }
static const char *pt3_author_module( const void *m )
{ return pt3_author( m ); }
/* The module is played from the start after it */
static long pt3_length_module( const void *m, long *loop )
{ return pt3_length( (pt3_module *)m, loop ); }

static int pt3_chips_module( const void *m ) { return pt3_chips( m ); }
static void pt3_frame_chips_module( void *m, unsigned char r[][ 14 ],
                                    unsigned int mask[] )
{ pt3_frame_ts( m, r, mask ); }

static const aymodule_format pt3_format = {
  "PT3", pt3_check, pt3_load_module, pt3_free_module, pt3_reset_module,
  pt3_frame_module, pt3_title_module, pt3_author_module, NULL,
  pt3_length_module, NULL, pt3_chips_module, pt3_frame_chips_module
};


/* TurboSound: the modules of the chips, each loaded as a format of its own.
   As in ZXTune, the length and the loop are those of the first module, and
   a text is shown once if the modules agree, else joined with "/" */

typedef struct turbosound {
  int chips;
  aymodule *chip[ AYMODULE_CHIPS ];
  char title[ 256 ], author[ 256 ], comment[ 256 ];
} turbosound;

/* A footer: for each module an ID ("PT3!": three 7 bit characters and
   '!') and its size (16 bits, little endian), then "02TS" or "03TS". The
   modules are before it, one after the other, in the order of the chips.
   The offset of the footer and the number of chips; 0 if none */
static int
ts_footer( const unsigned char *data, size_t length, size_t *footer )
{
  size_t at;

  /* Usually at the end; ZXTune also finds it before other data */
  for( at = length >= 4 ? length - 4 : 0; at >= 12; at-- ) {
    int chips, i, ok = 1;
    size_t start, total = 0;

    if( data[ at + 1 ] != '2' && data[ at + 1 ] != '3' ) continue;
    if( data[ at ] != '0' || data[ at + 2 ] != 'T' || data[ at + 3 ] != 'S' )
      continue;
    chips = data[ at + 1 ] - '0';
    if( at < (size_t)chips * 6 ) continue;
    start = at - chips * 6;
    for( i = 0; i < chips && ok; i++ ) {
      const unsigned char *entry = data + start + i * 6;
      if( entry[3] != '!' || ( entry[0] & 0x80 ) || ( entry[1] & 0x80 ) ||
          ( entry[2] & 0x80 ) ) ok = 0;
      total += entry[4] | entry[5] << 8;
    }
    if( !ok || total != start || total < 256 ||
        total > (size_t)32767 * chips ) continue;
    *footer = start;
    return chips;
  }
  return 0;
}

/* A Vortex Tracker text file with a second module: where it starts */
static size_t
ts_text_second( const unsigned char *data, size_t length )
{
  size_t i;

  if( length < 8 ) return 0;
  /* After a line break, not at the start */
  for( i = 9; i + 8 <= length; i++ )
    if( ( data[ i - 1 ] == '\n' || data[ i - 1 ] == '\r' ) &&
        !memcmp( data + i, "[Module]", 8 ) )
      return i;
  return 0;
}

static int
ts_check( const unsigned char *data, size_t length )
{
  size_t footer;

  if( ts_footer( data, length, &footer ) ) return 1;
  return pt3text_check( data, length ) && ts_text_second( data, length );
}

static void ts_free( void *m );

static void
merge_text( char *dest, size_t size,
            const char *( *get )( const aymodule *module ),
            const turbosound *ts )
{
  int i, j, same;

  dest[0] = '\0';
  for( i = 0; i < ts->chips; i++ ) {
    const char *text = get( ts->chip[i] );
    if( !text[0] ) continue;
    /* Once only: a text already there (also as part of a longer one) is
       left out, a longer one starting with what is there replaces it
       ("Author" and "Author / date": "Author / date") */
    for( same = 0, j = 0; j < i && !same; j++ )
      same = !strcmp( get( ts->chip[j] ), text );
    if( same || strstr( dest, text ) ) continue;
    if( dest[0] && !strncmp( text, dest, strlen( dest ) ) ) {
      snprintf( dest, size, "%s", text );
      continue;
    }
    if( dest[0] ) strncat( dest, " / ", size - strlen( dest ) - 1 );
    strncat( dest, text, size - strlen( dest ) - 1 );
  }
}

static void *
ts_load( const unsigned char *data, size_t length )
{
  turbosound *ts = calloc( 1, sizeof( *ts ) );
  size_t footer, offset = 0, second;
  int chips, i;

  if( !ts ) return NULL;

  if( ( chips = ts_footer( data, length, &footer ) ) ) {
    for( i = 0; i < chips; i++ ) {
      const unsigned char *entry = data + footer + i * 6;
      size_t size = entry[4] | entry[5] << 8;
      ts->chip[i] = load_single( data + offset, size );
      if( !ts->chip[i] ) break;
      ts->chips++;
      offset += size;
    }
  } else if( ( second = ts_text_second( data, length ) ) ) {
    chips = 2;
    ts->chip[0] = load_single( data, second );
    if( ts->chip[0] ) {
      ts->chips++;
      ts->chip[1] = load_single( data + second, length - second );
      if( ts->chip[1] ) ts->chips++;
    }
  }

  if( !chips || ts->chips != chips ) {
    ts_free( ts );
    return NULL;
  }

  merge_text( ts->title, sizeof( ts->title ), aymodule_title, ts );
  merge_text( ts->author, sizeof( ts->author ), aymodule_author, ts );
  merge_text( ts->comment, sizeof( ts->comment ), aymodule_comment, ts );
  return ts;
}

static void
ts_free( void *m )
{
  turbosound *ts = m;
  int i;

  if( !ts ) return;
  for( i = 0; i < AYMODULE_CHIPS; i++ ) aymodule_free( ts->chip[i] );
  free( ts );
}

static void
ts_reset( void *m )
{
  turbosound *ts = m;
  int i;

  for( i = 0; i < ts->chips; i++ ) aymodule_reset( ts->chip[i] );
}

static const char *ts_title( const void *m )
{ return ( (const turbosound *)m )->title; }
static const char *ts_author( const void *m )
{ return ( (const turbosound *)m )->author; }
static const char *ts_comment( const void *m )
{ return ( (const turbosound *)m )->comment; }
static const char *ts_stereo( const void *m )
{ return aymodule_stereo( ( (const turbosound *)m )->chip[0] ); }

static long
ts_length( const void *m, long *loop )
{
  return aymodule_exact_length( ( (const turbosound *)m )->chip[0], loop );
}

static const aymodule_format ts_format = {
  "TurboSound", ts_check, ts_load, ts_free, ts_reset, NULL,
  ts_title, ts_author, ts_comment, ts_length, ts_stereo
};


static const aymodule_format * const formats[] = {
  &ts_format,
  &pt3_format,
  NULL
};


/* ------------------------------------------------------------------------
   The interface
   ------------------------------------------------------------------------ */

const aymodule_format *
aymodule_identify( const unsigned char *data, size_t length )
{
  int i;

  for( i = 0; formats[i]; i++ )
    if( formats[i]->check( data, length ) ) return formats[i];
  return NULL;
}

/* A module of one chip: the first format that loads it (a weak check may
   match more than one) */
static aymodule *
load_single( const unsigned char *data, size_t length )
{
  aymodule *m;
  int i;

  for( i = 1; formats[i]; i++ ) {
    void *loaded;

    if( !formats[i]->check( data, length ) ) continue;
    loaded = formats[i]->load( data, length );
    if( !loaded ) continue;
    m = malloc( sizeof( *m ) );
    if( !m ) {
      formats[i]->free( loaded );
      return NULL;
    }
    m->format = formats[i];
    m->data = loaded;
    return m;
  }
  return NULL;
}

aymodule *
aymodule_load( const unsigned char *data, size_t length )
{
  void *loaded;
  aymodule *m;

  /* TurboSound first: its first module alone would match a format too */
  if( ts_check( data, length ) && ( loaded = ts_load( data, length ) ) ) {
    m = malloc( sizeof( *m ) );
    if( !m ) {
      ts_free( loaded );
      return NULL;
    }
    m->format = &ts_format;
    m->data = loaded;
    return m;
  }

  return load_single( data, length );
}

void
aymodule_free( aymodule *module )
{
  if( !module ) return;
  module->format->free( module->data );
  free( module );
}

const aymodule_format *
aymodule_format_of( const aymodule *module )
{
  return module->format;
}

void
aymodule_reset( aymodule *module )
{
  module->format->reset( module->data );
}

int
aymodule_chips( const aymodule *module )
{
  int chips;

  if( module->format == &ts_format ) {
    /* A module of the pair may play two chips by itself; at most three
       chips in all */
    const turbosound *ts = module->data;
    int i;
    for( chips = 0, i = 0; i < ts->chips; i++ )
      chips += aymodule_chips( ts->chip[i] );
    return chips > AYMODULE_CHIPS ? AYMODULE_CHIPS : chips;
  }
  return module->format->chips ? module->format->chips( module->data ) : 1;
}

void
aymodule_frame( aymodule *module,
                unsigned char registers[][ AYMODULE_REGISTERS ],
                unsigned int mask[] )
{
  int i;

  if( module->format == &ts_format ) {
    turbosound *ts = module->data;
    unsigned char spare[ AYMODULE_CHIPS ][ AYMODULE_REGISTERS ];
    unsigned int spare_mask[ AYMODULE_CHIPS ];
    int chip = 0;
    for( i = 0; i < ts->chips; i++ ) {
      int n = aymodule_chips( ts->chip[i] );
      if( chip + n <= AYMODULE_CHIPS ) {
        aymodule_frame( ts->chip[i], registers + chip, mask + chip );
      } else {
        /* No room for its chips: it goes on unheard */
        aymodule_frame( ts->chip[i], spare, spare_mask );
      }
      chip += n;
    }
    return;
  }
  if( module->format->frame_chips && module->format->chips &&
      module->format->chips( module->data ) > 1 ) {
    module->format->frame_chips( module->data, registers, mask );
    return;
  }
  module->format->frame( module->data, registers[0], &mask[0] );
}

static const char *
text( const char *( *get )( const void *module ), const aymodule *module )
{
  const char *value = get ? get( module->data ) : NULL;
  return value ? value : "";
}

const char *
aymodule_title( const aymodule *module )
{
  return text( module->format->title, module );
}

const char *
aymodule_author( const aymodule *module )
{
  return text( module->format->author, module );
}

const char *
aymodule_comment( const aymodule *module )
{
  return text( module->format->comment, module );
}

const char *
aymodule_stereo( const aymodule *module )
{
  return text( module->format->stereo, module );
}

long
aymodule_exact_length( const aymodule *module, long *loop )
{
  long frames;

  *loop = -1;
  if( !module->format->exact_length ) return 0;
  frames = module->format->exact_length( module->data, loop );
  if( frames <= 0 ) {
    *loop = -1;
    return 0;
  }
  return frames;
}
