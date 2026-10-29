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
   Reproductor de archivos AY independiente del emulador.

   La cancion se ejecuta en el Z80 propio de ay_measure.c (solo memoria y
   puertos del AY), frame a frame, cuando la tarjeta de sonido pide mas
   muestras (en el hilo de audio). Cada escritura en el AY se guarda con su
   t-estado y el sonido se genera con un chip AY como el de audio_hq.c
   (copiado de alli: adaptado de sound/ay_engine.c de Fuse, GPL 2 o
   posterior) con Blip_Buffer. Asi la musica no se corta aunque el emulador
   o los menus se detengan un momento, y no hace falta tomar el control de
   la maquina emulada.
*/

#include <stdlib.h>
#include <string.h>

#include "compileoptions.h"

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

#include "cpu.h"
#include "audio.h"
#include "ay38912.h"
#include "blipbuffer.h"
#include "ay_measure.h"
#include "ay_thread_player.h"
#include "audio_ayplayer.h"

#define TP_SAMPLE_RATE 48000
//T-estados por segundo de un Spectrum 48K a 50 Hz
#define TP_CLOCK_RATE (AY_MEASURE_FRAME_TSTATES*50)
#define TP_AY_FREQUENCY 1773400
#define TP_CHANNELS 3
#define TP_ENV_STEPS 16
#define TP_ENV_CONT 8
#define TP_ENV_ATTACK 4
#define TP_ENV_ALT 2
#define TP_ENV_HOLD 1
#define TP_CLOCK_DIVISOR 16
#define TP_MAX_WRITES 8000
//Muestras de un frame (con margen)
#define TP_FRAME_SAMPLES 2048
//Nivel maximo de un canal (16 bits, como en audio_hq.c)
#define TP_LEVEL_SCALE (2*24*128)

struct tp_chip {
    unsigned char registers[16];
    unsigned int tone_tick[TP_CHANNELS],tone_high[TP_CHANNELS],tone_period[TP_CHANNELS];
    unsigned int noise_period,env_period;
    unsigned int noise_tick,tone_cycles,env_cycles;
    unsigned int env_internal_tick,env_tick;
    int rng,noise_toggle,env_first,env_reverse,env_counter;
    int previous[TP_CHANNELS][2];
};

#ifdef USE_PTHREADS
static pthread_mutex_t tp_mutex=PTHREAD_MUTEX_INITIALIZER;
#define TP_LOCK pthread_mutex_lock(&tp_mutex)
#define TP_UNLOCK pthread_mutex_unlock(&tp_mutex)
#else
#define TP_LOCK
#define TP_UNLOCK
#endif

static volatile int tp_active=0;
static volatile int tp_paused=0;
static volatile long tp_samples_played=0;

static struct ay_measure_machine *tp_machine=NULL;
static struct ay_measure_write tp_writes[TP_MAX_WRITES];
static struct tp_chip tp_chip;
static unsigned int tp_levels[TP_ENV_STEPS];
static double tp_next_tick=0;

static Blip_Buffer *tp_buf[2]={NULL,NULL};
static Blip_Synth *tp_synth[TP_CHANNELS][2];

//Muestras generadas que aun no se han entregado
static blip_sample_t tp_samples[2][TP_FRAME_SAMPLES];
static int tp_samples_count=0,tp_samples_position=0;

//Filtro para quitar la componente continua (la salida del chip va de 0 al nivel)
static double tp_dc_in[2],tp_dc_out[2];

static void tp_levels_init(void)
{
    //Medidas de Matthew Westcott, como en audio_hq.c
    static const int measured_levels[TP_ENV_STEPS]={
        0x0000,0x0385,0x053D,0x0770,0x0AD7,0x0FD5,0x15B0,0x230C,
        0x2B4C,0x43C1,0x5A4B,0x732F,0x9204,0xAFF1,0xD921,0xFFFF
    };
    int i;
    for (i=0;i<TP_ENV_STEPS;i++) {
        tp_levels[i]=(measured_levels[i]*TP_LEVEL_SCALE+0x8000)/0xffff;
    }
}

static void tp_apply(struct tp_chip *c,int reg,int val)
{
    int channel;

    reg &=15;
    c->registers[reg]=val;

    if (reg<=5) {
        channel=reg>>1;
        c->tone_period[channel]=c->registers[reg & ~1] | (c->registers[reg | 1] & 15)<<8;
        if (!c->tone_period[channel]) c->tone_period[channel]=1;
        if (c->tone_tick[channel]>=c->tone_period[channel]*2) {
            c->tone_tick[channel] %=c->tone_period[channel]*2;
        }
    }
    else if (reg==6) {
        c->noise_tick=0;
        c->noise_period=c->registers[6] & 31;
    }
    else if (reg==11 || reg==12) {
        c->env_period=c->registers[11] | (c->registers[12]<<8);
    }
    else if (reg==13) {
        c->env_internal_tick=c->env_tick=c->env_cycles=0;
        c->env_first=1;
        c->env_reverse=0;
        c->env_counter=(c->registers[13] & TP_ENV_ATTACK) ? 0 : TP_ENV_STEPS-1;
    }
}

static void tp_finish_envelope_cycle(struct tp_chip *c,int shape)
{
    if (!(shape & TP_ENV_CONT)) {
        c->env_counter=0;
    }
    else if (shape & TP_ENV_HOLD) {
        if (c->env_first && (shape & TP_ENV_ALT)) {
            c->env_counter=(c->env_counter ? 0 : TP_ENV_STEPS-1);
        }
    }
    else if (shape & TP_ENV_ALT) {
        c->env_reverse=!c->env_reverse;
    }
    else {
        c->env_counter=(shape & TP_ENV_ATTACK) ? 0 : TP_ENV_STEPS-1;
    }
    c->env_first=0;
}

static void tp_advance_envelope(struct tp_chip *c,int shape)
{
    if (c->env_first || ((shape & TP_ENV_CONT) && !(shape & TP_ENV_HOLD))) {
        if (c->env_reverse) c->env_counter +=((shape & TP_ENV_ATTACK) ? -1 : 1);
        else c->env_counter +=((shape & TP_ENV_ATTACK) ? 1 : -1);
        if (c->env_counter<0) c->env_counter=0;
        if (c->env_counter>=TP_ENV_STEPS) c->env_counter=TP_ENV_STEPS-1;
    }

    c->env_internal_tick++;
    while (c->env_internal_tick>=TP_ENV_STEPS) {
        c->env_internal_tick -=TP_ENV_STEPS;
        tp_finish_envelope_cycle(c,shape);
    }
}

static int tp_clock_envelope(struct tp_chip *c,int shape)
{
    int noise_count=0;

    c->env_cycles +=TP_CLOCK_DIVISOR;
    while (c->env_cycles>=TP_CLOCK_DIVISOR) {
        c->env_cycles -=TP_CLOCK_DIVISOR;
        noise_count++;
        c->env_tick++;
        while (c->env_tick>=c->env_period) {
            c->env_tick -=c->env_period;
            tp_advance_envelope(c,shape);
            if (!c->env_period) break;
        }
    }
    return noise_count;
}

static void tp_clock_noise(struct tp_chip *c,int count)
{
    c->noise_tick +=count;
    while (c->noise_tick>=c->noise_period) {
        c->noise_tick -=c->noise_period;
        c->noise_toggle ^=(c->rng ^ (c->rng>>1)) & 1;
        c->rng=(c->rng>>1) ^ (0x12000 & -(c->rng & 1));
        if (!c->noise_period) break;
    }
}

static int tp_channel_output(struct tp_chip *c,int channel,int envelope_level,unsigned int tone_count)
{
    int volume=c->registers[8+channel];
    int output;
    //Con los filtros de canales de ZEsarUX
    int mixer=c->registers[7] | ay_filtros[0];

    if (volume & 16) {
        output=(ay_envelopes_enabled.v ? envelope_level : (int)tp_levels[15]);
    }
    else {
        output=tp_levels[volume & 15];
    }

    if (!(mixer & (1<<channel))) {
        c->tone_tick[channel] +=tone_count;
        if (c->tone_tick[channel]>=c->tone_period[channel]) {
            c->tone_tick[channel] -=c->tone_period[channel];
            c->tone_high[channel]=!c->tone_high[channel];
        }
        if (!c->tone_high[channel]) output=0;
    }

    if (!(mixer & (0x08<<channel)) && c->noise_toggle) output=0;

    if ((mixer & (0x09<<channel))==(0x09<<channel) && !ay_speech_enabled.v) output=0;

    return output;
}

//Lados de cada canal segun el modo estereo de ZEsarUX (como en audio_hq.c)
static void tp_stereo_sides(int sides[TP_CHANNELS][2])
{
    int custom[TP_CHANNELS];
    int i;

    for (i=0;i<TP_CHANNELS;i++) sides[i][0]=sides[i][1]=1;

    switch (ay3_stereo_mode) {
        case 1: sides[0][1]=0; sides[1][0]=0; break;
        case 2: sides[0][1]=0; sides[2][0]=0; break;
        case 3: sides[1][1]=0; sides[2][0]=0; break;
        case 4: sides[0][0]=0; sides[2][1]=0; break;
        case 5:
            custom[0]=ay3_custom_stereo_A;
            custom[1]=ay3_custom_stereo_B;
            custom[2]=ay3_custom_stereo_C;
            for (i=0;i<TP_CHANNELS;i++) {
                if (custom[i]==0) sides[i][1]=0;
                if (custom[i]==2) sides[i][0]=0;
            }
        break;
        default: break;
    }
}

static void tp_chip_reset(struct tp_chip *c)
{
    int i,side;

    memset(c,0,sizeof(*c));
    for (i=0;i<TP_CHANNELS;i++) {
        c->tone_period[i]=1;
        for (side=0;side<2;side++) c->previous[i][side]=0;
    }
    c->rng=1;
    c->env_first=1;
    c->env_counter=TP_ENV_STEPS-1;
}

static void tp_free_buffers(void)
{
    int channel,side;

    for (channel=0;channel<TP_CHANNELS;channel++) {
        for (side=0;side<2;side++) {
            if (tp_synth[channel][side]!=NULL) delete_Blip_Synth(&tp_synth[channel][side]);
        }
    }
    for (side=0;side<2;side++) {
        if (tp_buf[side]!=NULL) delete_Blip_Buffer(&tp_buf[side]);
    }
}

static int tp_new_buffers(void)
{
    int channel,side;

    tp_free_buffers();

    for (side=0;side<2;side++) {
        tp_buf[side]=new_Blip_Buffer();
        if (tp_buf[side]==NULL) return 1;
        blip_buffer_set_clock_rate(tp_buf[side],TP_CLOCK_RATE);
        if (blip_buffer_set_sample_rate(tp_buf[side],TP_SAMPLE_RATE,200)) return 1;
        blip_buffer_set_bass_freq(tp_buf[side],0);
    }

    for (channel=0;channel<TP_CHANNELS;channel++) {
        for (side=0;side<2;side++) {
            Blip_Synth *s=new_Blip_Synth();
            if (s==NULL) return 1;
            blip_synth_set_volume(s,1.0);
            blip_synth_set_output(s,tp_buf[side]);
            blip_synth_set_treble_eq(s,0.0);
            tp_synth[channel][side]=s;
        }
    }
    return 0;
}

//Generar un frame de la cancion
static void tp_render_frame(void)
{
    int sides[TP_CHANNELS][2];
    int count,position=0,channel,side;
    double step=(double)TP_CLOCK_DIVISOR*TP_CLOCK_RATE/TP_AY_FREQUENCY;
    double f;
    struct tp_chip *c=&tp_chip;

    count=ay_measure_run_frame(tp_machine,tp_writes,TP_MAX_WRITES);

    tp_stereo_sides(sides);

    for (f=tp_next_tick;f<AY_MEASURE_FRAME_TSTATES;f+=step) {
        int at=(int)f;

        while (position<count && tp_writes[position].tstates<=at) {
            tp_apply(c,tp_writes[position].reg,tp_writes[position].value);
            position++;
        }

        int envelope_level=tp_levels[c->env_counter];
        int noise_count=tp_clock_envelope(c,c->registers[13]);
        unsigned int tone_count;

        c->tone_cycles +=TP_CLOCK_DIVISOR;
        tone_count=c->tone_cycles>>3;
        c->tone_cycles &=7;

        for (channel=0;channel<TP_CHANNELS;channel++) {
            int output=tp_channel_output(c,channel,envelope_level,tone_count);

            for (side=0;side<2;side++) {
                int level=(sides[channel][side] ? output : 0);
                if (level!=c->previous[channel][side]) {
                    blip_synth_update(tp_synth[channel][side],at,level);
                    c->previous[channel][side]=level;
                }
            }
        }

        tp_clock_noise(c,noise_count);
    }

    //Escrituras despues del ultimo tick
    while (position<count) {
        tp_apply(c,tp_writes[position].reg,tp_writes[position].value);
        position++;
    }

    tp_next_tick=f-AY_MEASURE_FRAME_TSTATES;
    if (tp_next_tick<0) tp_next_tick=0;

    for (side=0;side<2;side++) {
        blip_buffer_end_frame(tp_buf[side],AY_MEASURE_FRAME_TSTATES);
    }

    long avail=blip_buffer_samples_avail(tp_buf[0]);
    if (avail>TP_FRAME_SAMPLES) avail=TP_FRAME_SAMPLES;
    long n0=blip_buffer_read_samples(tp_buf[0],tp_samples[0],avail,0);
    long n1=blip_buffer_read_samples(tp_buf[1],tp_samples[1],avail,0);
    tp_samples_count=(n0<n1 ? n0 : n1);
    tp_samples_position=0;
}

int ay_tp_start(unsigned char *data,int size,int song)
{
    int error=0;

    TP_LOCK;

    tp_active=0;

    if (tp_machine==NULL) tp_machine=malloc(sizeof(struct ay_measure_machine));
    if (tp_machine==NULL || ay_measure_setup(tp_machine,data,size,song) || tp_new_buffers()) {
        error=1;
    }
    else {
        tp_levels_init();
        tp_chip_reset(&tp_chip);
        tp_next_tick=0;
        tp_samples_count=tp_samples_position=0;
        tp_samples_played=0;
        tp_dc_in[0]=tp_dc_in[1]=tp_dc_out[0]=tp_dc_out[1]=0;
        tp_paused=0;
        tp_active=1;
    }

    TP_UNLOCK;
    return error;
}

void ay_tp_stop(void)
{
    TP_LOCK;
    tp_active=0;
    tp_paused=0;
    TP_UNLOCK;
}

void ay_tp_set_paused(int paused)
{
    tp_paused=paused;
}

int ay_tp_active(void)
{
    return tp_active;
}

long ay_tp_frames_played(void)
{
    return tp_samples_played*50/TP_SAMPLE_RATE;
}

unsigned char ay_tp_register(int reg)
{
    if (tp_machine==NULL || reg<0 || reg>15) return 0;
    return tp_chip.registers[reg];
}

void ay_tp_mix(short *destino,int frames)
{
    int i,side;

    //En pausa solo suena el emulador
    if (!tp_active || tp_paused) return;

    TP_LOCK;

    if (!tp_active) {
        TP_UNLOCK;
        return;
    }

    //Volumen general x volumen del reproductor, como mucho 100%
    double volume=ay_player_volume_effective()/100.0;

    for (i=0;i<frames;i++) {
        if (tp_samples_position>=tp_samples_count) tp_render_frame();

        for (side=0;side<2;side++) {
            double in=0;
            if (tp_samples_position<tp_samples_count) in=tp_samples[side][tp_samples_position];
            //Quitar la componente continua
            double out=in-tp_dc_in[side]+0.9995*tp_dc_out[side];
            tp_dc_in[side]=in;
            tp_dc_out[side]=out;

            double v=*destino+out*volume;
            if (v>32767) v=32767;
            if (v<-32768) v=-32768;
            *destino++=(short)v;
        }

        tp_samples_position++;
        tp_samples_played++;
    }

    TP_UNLOCK;
}
