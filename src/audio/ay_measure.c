/*
    ZEsarUX  ZX Second-Emulator And Released for UniX
    Copyright (C) 2013 Cesar Hernandez Bano

    This file is part of ZEsarUX.

    ZEsarUX is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
   Medicion de la duracion de las canciones de archivos AY (como en el fork
   de Fuse): la cancion se ejecuta en un Z80 propio, sin pantalla ni sonido,
   guardando un hash de los registros del AY de cada frame, hasta que la
   musica se repite. Asi no hace falta acelerar el emulador ni dejar de oir
   la cancion mientras se mide.

   El Z80 es chips_z80.h de Andre Weissflog (https://github.com/floooh/chips,
   licencia zlib/libpng), copiado sin cambios.
*/

#include <stdlib.h>
#include <string.h>

#include "compileoptions.h"

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

#define CHIPS_IMPL
#define CHIPS_ASSERT(x)
#include "chips_z80.h"

#include "ay_measure.h"

#define AY_MEASURE_INT_TSTATES 32

#define AY_MEASURE_MAX_FRAMES (60L*60*50)  //una hora de musica
#define AY_MEASURE_MIN_PERIOD 250          //5 segundos
#define AY_MEASURE_CONFIRM 3000            //un minuto
#define AY_MEASURE_CHECK_EVERY 250

struct ay_measure_file {
    unsigned char *data;
    int size;
};

static int ay_measure_byte(struct ay_measure_file *f,int index)
{
    if (index<0 || index>=f->size) return 0;
    return f->data[index];
}

//Valor de 16 bits big endian
static int ay_measure_word(struct ay_measure_file *f,int index)
{
    return (ay_measure_byte(f,index)<<8) | ay_measure_byte(f,index+1);
}

//Puntero relativo con signo
static int ay_measure_pointer(struct ay_measure_file *f,int index)
{
    int value=ay_measure_word(f,index);
    if (value>=32768) value-=65536;
    return index+value;
}

/* Ha vuelto a empezar la musica? El menor periodo P para el que los ultimos
   max(P, un minuto) frames son iguales a los de P frames antes; luego atras
   hasta el primer frame S de la repeticion. La cancion dura hasta S + P y
   sigue desde S; si lo que se repite es silencio, la musica termino en S
   (*loop = -1). -1: aun no */
static long ay_measure_find(unsigned int *hash,unsigned char *silent,long n,long *loop)
{
    long period,need,k,start,i;
    int all_silent;

    for (period=AY_MEASURE_MIN_PERIOD;;period++) {
        need=(period>AY_MEASURE_CONFIRM ? period : AY_MEASURE_CONFIRM);
        if (period+need>n) return -1;

        for (k=0;k<need;k++) {
            if (hash[n-1-k]!=hash[n-1-k-period]) break;
        }
        if (k<need) continue;

        start=n-need-period;
        while (start>0 && hash[start-1]==hash[start-1+period]) start--;

        all_silent=1;
        for (i=start;i<start+period;i++) {
            if (!silent[i]) {
                all_silent=0;
                break;
            }
        }

        if (all_silent) {
            *loop=-1;
            return start;
        }
        *loop=start;
        return start+period;
    }
}

//Preparar la memoria y el Z80 para la cancion (desde 1), como audio_ay_player_play_song.
//Retorna 0 si ok
int ay_measure_setup(struct ay_measure_machine *m,unsigned char *data,int size,int song)
{
    struct ay_measure_file file={data,size};
    struct ay_measure_file *f=&file;

    memset(m->ay_registers,0,sizeof(m->ay_registers));
    m->ay_selected=0;
    m->ay_chip=0;

    if (size<20 || memcmp(data,"ZXAYEMUL",8)) return 1;
    if (song<1 || song>ay_measure_byte(f,16)+1) return 1;

    //Datos de la cancion
    int song_struct=ay_measure_pointer(f,18)+(song-1)*4;
    int song_data=ay_measure_pointer(f,song_struct+2);
    int ppoints,pdata;

    if (ay_measure_byte(f,9)<=3) {
        ppoints=ay_measure_pointer(f,song_data+10);
        pdata=ay_measure_pointer(f,song_data+12);
    }
    else {
        ppoints=ay_measure_pointer(f,song_data+28);
        pdata=ay_measure_pointer(f,song_data+30);
    }

    int reg_sp=ay_measure_word(f,ppoints);
    int reg_init=ay_measure_word(f,ppoints+2);
    int reg_inter=ay_measure_word(f,ppoints+4);

    //Sin rutina init: se llama al primer bloque
    if (reg_init==0) reg_init=ay_measure_word(f,pdata);

    unsigned char *mem=m->mem;

    //Memoria: 0000-00FF RET, 0100-3FFF FFh, 4000-FFFF 0, 0038 EI
    memset(mem,0xc9,0x100);
    memset(mem+0x100,0xff,0x3f00);
    memset(mem+0x4000,0,0xc000);
    mem[0x38]=0xfb;

    //El reproductor en la direccion 0, como el de audio_ayplayer.c
    unsigned char player[18];
    memset(player,0xc9,sizeof(player));
    player[0]=0xf3;
    player[1]=0xcd;
    player[2]=reg_init & 0xff;
    player[3]=reg_init>>8;
    if (reg_inter==0) {
        player[4]=0xed; player[5]=0x5e;
        player[6]=0xfb;
        player[7]=0x76;
        player[8]=0x18; player[9]=256-6;
    }
    else {
        player[4]=0xed; player[5]=0x56;
        player[6]=0xfb;
        player[7]=0x76;
        player[8]=0xcd;
        player[9]=reg_inter & 0xff;
        player[10]=reg_inter>>8;
        player[11]=0x18; player[12]=256-9;
    }
    player[13]=0xf3; player[14]=0x18; player[15]=256-2;
    player[16]=0xfb; player[17]=0xc9;
    memcpy(mem,player,sizeof(player));

    //Bloques de la cancion
    int bloque;
    for (bloque=0;;bloque++) {
        int entry=pdata+bloque*6;
        int address=ay_measure_word(f,entry);
        int len=ay_measure_word(f,entry+2);
        int offset=ay_measure_word(f,entry+4);
        int origin,j;

        if (address==0) break;
        if (entry+6>size) break;
        if (address+len>65536) len=65536-address;

        origin=entry+4+offset;
        for (j=0;j<len;j++) {
            if (origin+j>=size) break;
            mem[(address+j) & 0xffff]=data[origin+j];
        }
    }

    //Registros: valores de arranque en frio, A y F de la cancion, SP de la cancion,
    //I=3, interrupciones desactivadas, IM 0
    z80_t *cpu=&m->cpu;
    m->pins=z80_init(cpu);
    cpu->af=cpu->bc=cpu->de=cpu->hl=cpu->ix=cpu->iy=0xffff;
    cpu->af2=cpu->bc2=cpu->de2=cpu->hl2=0xffff;
    cpu->a=ay_measure_byte(f,song_data+8);
    cpu->f=ay_measure_byte(f,song_data+9);
    cpu->sp=reg_sp;
    cpu->i=3;
    cpu->r=0;
    cpu->im=0;
    cpu->iff1=cpu->iff2=false;
    m->pins=z80_prefetch(cpu,0);

    return 0;
}

//Ejecutar un frame. Si writes no es NULL, se guardan ahi las escrituras en el
//primer chip AY con su t-estado (como mucho max_writes). Retorna cuantas
int ay_measure_run_frame(struct ay_measure_machine *m,struct ay_measure_write *writes,int max_writes)
{
    z80_t *cpu=&m->cpu;
    uint64_t pins=m->pins;
    unsigned char *mem=m->mem;
    int count=0;
    long t;

    for (t=0;t<AY_MEASURE_FRAME_TSTATES;t++) {
        if (t<AY_MEASURE_INT_TSTATES) pins|=Z80_INT;
        else pins&=~Z80_INT;

        pins=z80_tick(cpu,pins);

        if (pins & Z80_MREQ) {
            uint16_t addr=Z80_GET_ADDR(pins);
            if (pins & Z80_RD) {
                Z80_SET_DATA(pins,mem[addr]);
            }
            else if (pins & Z80_WR) {
                mem[addr]=Z80_GET_DATA(pins);
            }
        }
        else if (pins & Z80_IORQ) {
            uint16_t port=Z80_GET_ADDR(pins);
            if (pins & Z80_M1) {
                //Reconocimiento de interrupcion: bus a FFh
                Z80_SET_DATA(pins,0xff);
            }
            else if (pins & Z80_WR) {
                uint8_t value=Z80_GET_DATA(pins);
                //Puertos del AY del Spectrum: seleccion FFFDh y datos BFFDh
                if ((port & 0xc002)==0xc000) {
                    //Turbosound: FFh primer chip, FEh segundo
                    if (value>=0xf0) m->ay_chip=(value==0xff ? 0 : 1);
                    else m->ay_selected=value & 15;
                }
                else if ((port & 0xc002)==0x8000) {
                    if (m->ay_chip==0) {
                        m->ay_registers[m->ay_selected]=value;
                        if (writes!=NULL && count<max_writes) {
                            writes[count].tstates=t;
                            writes[count].reg=m->ay_selected;
                            writes[count].value=value;
                            count++;
                        }
                    }
                }
            }
            else if (pins & Z80_RD) {
                Z80_SET_DATA(pins,0xff);
            }
        }

        //Esperando con HALT a la siguiente interrupcion: saltar hasta el final del frame
        if ((pins & Z80_HALT) && t>=AY_MEASURE_INT_TSTATES) break;
    }

    m->pins=pins;
    return count;
}

long ay_measure_song(unsigned char *data,int size,int song,long *loop,volatile int *cancel)
{
    struct ay_measure_machine *m;
    unsigned int *hash;
    unsigned char *silent;
    long frames=0,length=0;
    int i;

    *loop=0;

    m=malloc(sizeof(struct ay_measure_machine));
    hash=malloc(AY_MEASURE_MAX_FRAMES*sizeof(unsigned int));
    silent=malloc(AY_MEASURE_MAX_FRAMES);
    if (m==NULL || hash==NULL || silent==NULL || ay_measure_setup(m,data,size,song)) {
        free(m);
        free(hash);
        free(silent);
        return 0;
    }

    while (frames<AY_MEASURE_MAX_FRAMES) {
        if (cancel!=NULL && *cancel) break;

        ay_measure_run_frame(m,NULL,0);

        unsigned int h=2166136261U;
        for (i=0;i<14;i++) {
            h ^=m->ay_registers[i];
            h *=16777619U;
        }
        hash[frames]=h;
        silent[frames]=!((m->ay_registers[8] | m->ay_registers[9] | m->ay_registers[10]) & 0x1f);
        frames++;

        if (frames % AY_MEASURE_CHECK_EVERY==0) {
            long found=ay_measure_find(hash,silent,frames,loop);
            if (found>=0) {
                //Solo silencio: sin duracion
                length=(found<50 ? 0 : found);
                if (!length) *loop=0;
                break;
            }
        }
    }

    free(m);
    free(hash);
    free(silent);
    return length;
}

#ifdef USE_PTHREADS

/* ------------------------------------------------------------------------
   Medicion en un hilo aparte
   ------------------------------------------------------------------------ */

static pthread_t ay_measure_thread;
static pthread_mutex_t ay_measure_mutex=PTHREAD_MUTEX_INITIALIZER;
static int ay_measure_thread_started=0;
static volatile int ay_measure_cancel=0;
static volatile int ay_measure_busy=0;

static unsigned char *ay_measure_data=NULL;
static int ay_measure_size;
static int ay_measure_song_number;
static int ay_measure_id;

//Resultado
static int ay_measure_done_id=-1;
static long ay_measure_done_frames,ay_measure_done_loop;

static void *ay_measure_thread_function(void *arg)
{
    long loop,frames;

    (void)arg;
    frames=ay_measure_song(ay_measure_data,ay_measure_size,ay_measure_song_number,&loop,&ay_measure_cancel);

    pthread_mutex_lock(&ay_measure_mutex);
    if (!ay_measure_cancel) {
        ay_measure_done_id=ay_measure_id;
        ay_measure_done_frames=frames;
        ay_measure_done_loop=loop;
    }
    ay_measure_busy=0;
    pthread_mutex_unlock(&ay_measure_mutex);

    return NULL;
}

void ay_measure_stop(void)
{
    if (!ay_measure_thread_started) return;

    ay_measure_cancel=1;
    pthread_join(ay_measure_thread,NULL);
    ay_measure_thread_started=0;
    ay_measure_busy=0;

    free(ay_measure_data);
    ay_measure_data=NULL;
}

void ay_measure_start(unsigned char *data,int size,int song,int id)
{
    ay_measure_stop();

    ay_measure_data=malloc(size);
    if (ay_measure_data==NULL) return;
    memcpy(ay_measure_data,data,size);
    ay_measure_size=size;
    ay_measure_song_number=song;
    ay_measure_id=id;
    ay_measure_cancel=0;
    ay_measure_busy=1;

    if (pthread_create(&ay_measure_thread,NULL,ay_measure_thread_function,NULL)) {
        ay_measure_busy=0;
        free(ay_measure_data);
        ay_measure_data=NULL;
        return;
    }
    ay_measure_thread_started=1;
}

int ay_measure_result(int id,long *frames,long *loop)
{
    int found=0;

    pthread_mutex_lock(&ay_measure_mutex);
    if (ay_measure_done_id==id && id>=0) {
        *frames=ay_measure_done_frames;
        *loop=ay_measure_done_loop;
        ay_measure_done_id=-1;
        found=1;
    }
    pthread_mutex_unlock(&ay_measure_mutex);

    //Terminado: liberar el hilo
    if (found && ay_measure_thread_started && !ay_measure_busy) {
        pthread_join(ay_measure_thread,NULL);
        ay_measure_thread_started=0;
        free(ay_measure_data);
        ay_measure_data=NULL;
    }

    return found;
}

int ay_measure_running(void)
{
    return ay_measure_busy;
}

#else

//Sin hilos: se mide enseguida (tarda menos de un segundo)
static int ay_measure_done_id=-1;
static long ay_measure_done_frames,ay_measure_done_loop;

void ay_measure_stop(void)
{
}

void ay_measure_start(unsigned char *data,int size,int song,int id)
{
    ay_measure_done_frames=ay_measure_song(data,size,song,&ay_measure_done_loop,NULL);
    ay_measure_done_id=id;
}

int ay_measure_result(int id,long *frames,long *loop)
{
    if (ay_measure_done_id!=id || id<0) return 0;
    *frames=ay_measure_done_frames;
    *loop=ay_measure_done_loop;
    ay_measure_done_id=-1;
    return 1;
}

int ay_measure_running(void)
{
    return 0;
}

#endif
