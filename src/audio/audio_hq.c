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
   Salida de sonido de alta calidad, tal como la genera el emulador Fuse.

   El sonido clasico de ZEsarUX es una muestra de 8 bits por scanline (15600 Hz).
   Aqui el sonido se genera a 48000 Hz, 16 bits, con limitacion de banda:
   - Cada cambio del beeper (puerto FEh) se introduce en su instante exacto
     (en t-estados) con Blip_Buffer (de Fuse, de Shay Green), sin aliasing.
   - Las demas fuentes de sonido (AY etc.) siguen llegando una vez por scanline,
     pero tambien se introducen con limitacion de banda.
   - La salida pasa por los mismos filtros que en Fuse: una aproximacion de la
     salida de la ULA y despues el altavoz interno del 16K/48K, o el altavoz de
     un televisor en las demas maquinas.
   Filtros adaptados de Fuse (speaker_filter.c, ula_filter.c, tv_filter.c),
   Copyright (c) 2026 Fredrick Meunier, GPL 2 o posterior.

   El buffer clasico de 8 bits se sigue llenando como antes (salida a archivo,
   ventana de forma de onda, ZENG...); solo la tarjeta de sonido recibe esta salida.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "audio_hq.h"
#include "blipbuffer.h"
#include "audio.h"
#include "cpu.h"
#include "debug.h"
#include "screen.h"

//Ajuste: usar la salida de sonido de alta calidad
z80_bit audio_hq_enabled={1};

//La salida se esta generando ahora (ajuste activado y maquina soportada)
int audio_hq_running=0;

#define AUDIO_HQ_PI 3.14159265358979323846

//Multiplicador de los niveles de 8 bits de ZEsarUX a 16 bits
#define AUDIO_HQ_SCALE 128

static Blip_Buffer *hq_buf_beeper=NULL;
static Blip_Buffer *hq_buf_left=NULL;
static Blip_Buffer *hq_buf_right=NULL;
static Blip_Synth *hq_synth_beeper=NULL;
static Blip_Synth *hq_synth_left=NULL;
static Blip_Synth *hq_synth_right=NULL;

static long hq_clock_rate=0;

//Ultimos niveles recibidos
static int hq_last_beeper=0;
static int hq_last_left=0;
static int hq_last_right=0;

//Muestras de un frame. A 48000 Hz y velocidad 2% serian 48000 muestras por frame
#define AUDIO_HQ_MAX_FRAME_SAMPLES 48000
static blip_sample_t hq_samples_beeper[AUDIO_HQ_MAX_FRAME_SAMPLES];
static blip_sample_t hq_samples_left[AUDIO_HQ_MAX_FRAME_SAMPLES];
static blip_sample_t hq_samples_right[AUDIO_HQ_MAX_FRAME_SAMPLES];


/* --- Filtros, adaptados de Fuse --- */

//Salida de la ULA: filtro asimetrico de un polo
#define ULA_FILTER_RISE_TAU 36.53558495933805e-6
#define ULA_FILTER_FALL_TAU 68.86073172982108e-6

static double ula_alpha_rise,ula_alpha_fall,ula_state;
static int ula_initialised;

static double ula_filter_apply(double input)
{
    if (!ula_initialised) {
        ula_state=input;
        ula_initialised=1;
        return ula_state;
    }

    double alpha=(input>ula_state ? ula_alpha_rise : ula_alpha_fall);
    ula_state +=alpha*(input-ula_state);
    return ula_state;
}

//Altavoz interno del 16K/48K: paso alto resonante de segundo orden, 750 Hz
#define SPEAKER_FILTER_FREQUENCY 750.0
#define SPEAKER_FILTER_Q 0.70710678118654752440

static double spk_b0,spk_b1,spk_b2,spk_a1,spk_a2,spk_z1,spk_z2;
static int spk_initialised;

static double speaker_filter_apply(double input)
{
    double output;

    if (!spk_initialised) {
        spk_z1=-spk_b0*input;
        spk_z2=spk_b2*input;
        spk_initialised=1;
        return 0.0;
    }

    output=spk_b0*input+spk_z1;
    spk_z1=spk_b1*input-spk_a1*output+spk_z2;
    spk_z2=spk_b2*input-spk_a2*output;

    if (fabs(spk_z1)<1.0e-20) spk_z1=0.0;
    if (fabs(spk_z2)<1.0e-20) spk_z2=0.0;

    return output;
}

//Altavoz de televisor: paso alto 100 Hz, paso bajo 10 kHz. Uno por canal
#define TV_FILTER_HIGH_PASS_FREQUENCY 100.0
#define TV_FILTER_LOW_PASS_FREQUENCY 10000.0

struct s_hq_tv_filter {
    double previous_input,high_pass_state,low_pass_state;
    int initialised;
};

static double tv_high_pass_decay,tv_low_pass_alpha;
static struct s_hq_tv_filter tv_filters[2];

static double tv_filter_apply(struct s_hq_tv_filter *f,double input)
{
    if (!f->initialised) {
        f->previous_input=input;
        f->initialised=1;
        return 0.0;
    }

    f->high_pass_state=tv_high_pass_decay*(f->high_pass_state+input-f->previous_input);
    f->previous_input=input;
    f->low_pass_state +=tv_low_pass_alpha*(f->high_pass_state-f->low_pass_state);

    if (fabs(f->high_pass_state)<1.0e-20) f->high_pass_state=0.0;
    if (fabs(f->low_pass_state)<1.0e-20) f->low_pass_state=0.0;

    return f->low_pass_state;
}

static void audio_hq_filters_init(void)
{
    double rate=AUDIO_HQ_SAMPLE_RATE;

    ula_alpha_rise=-expm1(-1.0/(rate*ULA_FILTER_RISE_TAU));
    ula_alpha_fall=-expm1(-1.0/(rate*ULA_FILTER_FALL_TAU));
    ula_initialised=0;

    double omega=2.0*AUDIO_HQ_PI*SPEAKER_FILTER_FREQUENCY/rate;
    double alpha=sin(omega)/(2.0*SPEAKER_FILTER_Q);
    double cosine=cos(omega);
    double a0=1.0+alpha;
    spk_b0=(1.0+cosine)/(2.0*a0);
    spk_b1=-(1.0+cosine)/a0;
    spk_b2=spk_b0;
    spk_a1=-2.0*cosine/a0;
    spk_a2=(1.0-alpha)/a0;
    spk_initialised=0;

    tv_high_pass_decay=exp(-2.0*AUDIO_HQ_PI*TV_FILTER_HIGH_PASS_FREQUENCY/rate);
    tv_low_pass_alpha=-expm1(-2.0*AUDIO_HQ_PI*TV_FILTER_LOW_PASS_FREQUENCY/rate);
    memset(tv_filters,0,sizeof(tv_filters));
}


static short audio_hq_clip(double v);

/* --- FIFO de salida: 16 bits estéreo, lo lee el driver de la tarjeta de sonido --- */

//Medio segundo
#define AUDIO_HQ_FIFO_FRAMES (AUDIO_HQ_SAMPLE_RATE/2)

//Retardo objetivo del FIFO: absorbe las irregularidades del temporizador del sistema
//(en Windows los frames llegan a rafagas) y las pausas cortas, por ejemplo al abrir un menu
#define AUDIO_HQ_FIFO_TARGET (AUDIO_HQ_SAMPLE_RATE*70/1000)

//Por encima de esto se descarta audio antiguo (solo en casos extremos)
#define AUDIO_HQ_FIFO_MAX (AUDIO_HQ_SAMPLE_RATE*300/1000)

//Muestras para hacer el fundido de entrada/salida al quedarse sin sonido (5 ms)
#define AUDIO_HQ_FADE_SAMPLES (AUDIO_HQ_SAMPLE_RATE*5/1000)

static short hq_fifo[AUDIO_HQ_FIFO_FRAMES*2];
static volatile int hq_fifo_write=0;
static volatile int hq_fifo_read=0;

//Ultima muestra enviada a la tarjeta de sonido
static short hq_fifo_last_left=0;
static short hq_fifo_last_right=0;

//Hay un driver leyendo el FIFO (si no, no se llena ni se ajusta la velocidad)
int audio_hq_fifo_in_use=0;

//Esperando a que el FIFO se llene hasta el retardo objetivo (al principio o tras quedarse vacio)
static int hq_fifo_buffering=1;

//Ganancia del fundido, de 0 a AUDIO_HQ_FADE_SAMPLES
static int hq_fifo_fade=0;

int audio_hq_fifo_frames(void)
{
    int w=hq_fifo_write;
    int r=hq_fifo_read;
    if (w>=r) return w-r;
    return AUDIO_HQ_FIFO_FRAMES-r+w;
}

void audio_hq_fifo_empty(void)
{
    hq_fifo_write=hq_fifo_read;
}

static void audio_hq_fifo_put(short left,short right)
{
    if (!audio_hq_fifo_in_use) return;

    int w=hq_fifo_write;
    int next=w+1;
    if (next==AUDIO_HQ_FIFO_FRAMES) next=0;

    //Lleno: descartar la muestra
    if (next==hq_fifo_read) return;

    hq_fifo[w*2]=left;
    hq_fifo[w*2+1]=right;
    hq_fifo_write=next;
}

//Si se acumula demasiado retardo, descartar el audio mas antiguo hasta dejar el retardo objetivo
void audio_hq_fifo_limit_latency(void)
{
    int frames=audio_hq_fifo_frames();
    if (frames>AUDIO_HQ_FIFO_MAX) {
        int drop=frames-AUDIO_HQ_FIFO_TARGET;
        int r=hq_fifo_read+drop;
        if (r>=AUDIO_HQ_FIFO_FRAMES) r-=AUDIO_HQ_FIFO_FRAMES;
        hq_fifo_read=r;
    }
}

//Sonido clasico de 8 bits (maquinas sin la salida de alta calidad): remuestrear al FIFO
void audio_hq_fifo_put_classic(char *buffer,int samples,int frequency)
{
    static double position=0.0;
    double step;
    int i;

    if (frequency<=0 || samples<=0) return;
    step=(double)frequency/AUDIO_HQ_SAMPLE_RATE;

    while (position<samples-1) {
        i=(int)position;
        double frac=position-i;
        double l=buffer[i*2]*(1.0-frac)+buffer[(i+1)*2]*frac;
        double r=buffer[i*2+1]*(1.0-frac)+buffer[(i+1)*2+1]*frac;
        audio_hq_fifo_put(audio_hq_clip(l*256),audio_hq_clip(r*256));
        position+=step;
    }
    position-=samples-1;
    if (position<0) position=0;

    audio_hq_fifo_limit_latency();
}

//Entregar muestras a la tarjeta de sonido (se llama desde el hilo de audio).
//Si el FIFO se queda vacio no se repite ni se corta el sonido de golpe (eso suena a chasquido):
//se hace un fundido a silencio y se espera a tener otra vez el retardo objetivo, y entonces fundido de entrada
void audio_hq_fifo_get(short *destino,int frames)
{
    int i;

    for (i=0;i<frames;i++) {
        if (hq_fifo_buffering) {
            if (audio_hq_fifo_frames()>=AUDIO_HQ_FIFO_TARGET) hq_fifo_buffering=0;
        }

        int r=hq_fifo_read;

        if (!hq_fifo_buffering && r!=hq_fifo_write) {
            hq_fifo_last_left=hq_fifo[r*2];
            hq_fifo_last_right=hq_fifo[r*2+1];
            r++;
            if (r==AUDIO_HQ_FIFO_FRAMES) r=0;
            hq_fifo_read=r;

            if (hq_fifo_fade<AUDIO_HQ_FADE_SAMPLES) hq_fifo_fade++;
        }
        else {
            //Sin sonido: volver a llenar el FIFO y fundido a silencio de la ultima muestra
            hq_fifo_buffering=1;
            if (hq_fifo_fade>0) hq_fifo_fade--;
        }

        *destino++=(short)((int)hq_fifo_last_left*hq_fifo_fade/AUDIO_HQ_FADE_SAMPLES);
        *destino++=(short)((int)hq_fifo_last_right*hq_fifo_fade/AUDIO_HQ_FADE_SAMPLES);
    }
}


/* --- Volcado opcional de la salida en bruto, para pruebas --- */

static FILE *hq_dump_file=NULL;


/* --- Maquinas soportadas --- */

int audio_hq_machine_supported(void)
{
    return MACHINE_IS_SPECTRUM;
}

//El 16K/48K tienen altavoz interno. Las demas van a un televisor
static int audio_hq_machine_has_speaker(void)
{
    return MACHINE_IS_SPECTRUM_16_48 && !MACHINE_IS_TIMEX_TC2048;
}


/* --- Inicio / fin --- */

static long audio_hq_get_clock_rate(void)
{
    //T-estados por segundo segun el ritmo real de frames del emulador
    extern int original_timer_sleep_machine;
    double frame_us=original_timer_sleep_machine;
    if (frame_us<=0) frame_us=(machine_60hz.v ? 16666 : 20000);
    long rate=(long)(screen_testados_total*1000000.0/frame_us);
    rate=rate*porcentaje_velocidad_emulador/100;
    if (rate<1) rate=1;
    return rate;
}

static Blip_Buffer *audio_hq_new_buffer(void)
{
    Blip_Buffer *b=new_Blip_Buffer();
    if (b==NULL) return NULL;

    blip_buffer_set_clock_rate(b,hq_clock_rate);
    //Un segundo de buffer: suficiente incluso a velocidad 2%
    if (blip_buffer_set_sample_rate(b,AUDIO_HQ_SAMPLE_RATE,1000)) {
        delete_Blip_Buffer(&b);
        return NULL;
    }
    blip_buffer_set_bass_freq(b,0);
    return b;
}

static Blip_Synth *audio_hq_new_synth(Blip_Buffer *b)
{
    Blip_Synth *s=new_Blip_Synth();
    if (s==NULL) return NULL;
    blip_synth_set_volume(s,1.0);
    blip_synth_set_output(s,b);
    blip_synth_set_treble_eq(s,0.0);
    return s;
}

void audio_hq_end(void)
{
    audio_hq_running=0;

    if (hq_synth_beeper!=NULL) delete_Blip_Synth(&hq_synth_beeper);
    if (hq_synth_left!=NULL) delete_Blip_Synth(&hq_synth_left);
    if (hq_synth_right!=NULL) delete_Blip_Synth(&hq_synth_right);
    if (hq_buf_beeper!=NULL) delete_Blip_Buffer(&hq_buf_beeper);
    if (hq_buf_left!=NULL) delete_Blip_Buffer(&hq_buf_left);
    if (hq_buf_right!=NULL) delete_Blip_Buffer(&hq_buf_right);
}

void audio_hq_init(void)
{
    audio_hq_end();

    if (!audio_hq_enabled.v) return;
    if (!audio_hq_machine_supported()) return;

    hq_clock_rate=audio_hq_get_clock_rate();

    hq_buf_beeper=audio_hq_new_buffer();
    hq_buf_left=audio_hq_new_buffer();
    hq_buf_right=audio_hq_new_buffer();
    if (hq_buf_beeper==NULL || hq_buf_left==NULL || hq_buf_right==NULL) {
        debug_printf(VERBOSE_ERR,"Can not allocate memory for the high quality sound");
        audio_hq_end();
        return;
    }

    hq_synth_beeper=audio_hq_new_synth(hq_buf_beeper);
    hq_synth_left=audio_hq_new_synth(hq_buf_left);
    hq_synth_right=audio_hq_new_synth(hq_buf_right);
    if (hq_synth_beeper==NULL || hq_synth_left==NULL || hq_synth_right==NULL) {
        debug_printf(VERBOSE_ERR,"Can not allocate memory for the high quality sound");
        audio_hq_end();
        return;
    }

    //Empezar desde los niveles actuales, sin saltos
    blip_synth_set_level(hq_synth_beeper,hq_last_beeper);
    blip_synth_set_level(hq_synth_left,hq_last_left);
    blip_synth_set_level(hq_synth_right,hq_last_right);

    audio_hq_filters_init();

    if (hq_dump_file==NULL) {
        char *dump=getenv("ZESARUX_HQ_AUDIO_DUMP");
        if (dump!=NULL && dump[0]) hq_dump_file=fopen(dump,"wb");
    }

    audio_hq_running=1;

    debug_printf(VERBOSE_INFO,"High quality sound: %d Hz, clock %ld",AUDIO_HQ_SAMPLE_RATE,hq_clock_rate);
}

//Llamar cuando cambie la maquina, la velocidad de emulacion o el ajuste
void audio_hq_reset(void)
{
    audio_hq_init();
}

static int hq_machine_type=-1;

//Iniciar o detener la salida cuando cambie el ajuste o la maquina
static void audio_hq_check(void)
{
    if (audio_hq_enabled.v && audio_hq_machine_supported()) {
        if (!audio_hq_running || hq_machine_type!=current_machine_type) {
            hq_machine_type=current_machine_type;
            audio_hq_init();
        }
    }
    else {
        if (audio_hq_running) audio_hq_end();
    }
}


/* --- Entrada de sonido --- */

//Instante dentro del frame, limitado a lo que admite el buffer
static blip_time_t audio_hq_time(int tstates)
{
    if (tstates<0) tstates=0;
    //Hasta algo mas de un frame (una instruccion puede pasarse del final del frame)
    if (tstates>screen_testados_total*2) tstates=screen_testados_total*2;
    return tstates;
}

//Cambio de nivel del beeper en este t-estado del frame. value: el value_beeper de 8 bits
void audio_hq_beeper(int tstates,int value)
{
    if (!audio_hq_running) return;

    int level=value*AUDIO_HQ_SCALE;
    hq_last_beeper=level;

    blip_synth_update(hq_synth_beeper,audio_hq_time(tstates),level);
}

//Las demas fuentes de sonido, una vez por scanline. Valores de 8 bits, sin el beeper
void audio_hq_others(int tstates,int left,int right)
{
    if (!audio_hq_running) return;

    int level_left=left*AUDIO_HQ_SCALE;
    int level_right=right*AUDIO_HQ_SCALE;

    if (level_left!=hq_last_left) {
        hq_last_left=level_left;
        blip_synth_update(hq_synth_left,audio_hq_time(tstates),level_left);
    }

    if (level_right!=hq_last_right) {
        hq_last_right=level_right;
        blip_synth_update(hq_synth_right,audio_hq_time(tstates),level_right);
    }
}

static short audio_hq_clip(double v)
{
    if (v>32767) return 32767;
    if (v<-32768) return -32768;
    return (short)v;
}

//Final de un frame de la maquina emulada, de frame_tstates t-estados
void audio_hq_end_frame(int frame_tstates)
{
    audio_hq_check();

    if (!audio_hq_running) return;

    //Ajuste fino de la velocidad: el reloj del emulador y el de la tarjeta de sonido nunca van exactamente
    //igual. En vez de descartar o repetir audio, se generan un poco mas o menos muestras por frame
    //(como mucho un 2%, y cambiando muy despacio) para mantener el FIFO cerca del retardo objetivo
    static double fifo_medio=AUDIO_HQ_FIFO_TARGET;
    double ajuste=0;
    if (audio_hq_fifo_in_use) {
        fifo_medio=fifo_medio*0.98+audio_hq_fifo_frames()*0.02;
        ajuste=(fifo_medio-AUDIO_HQ_FIFO_TARGET)/AUDIO_HQ_FIFO_TARGET*0.02;
    }
    if (ajuste>0.02) ajuste=0.02;
    if (ajuste<-0.02) ajuste=-0.02;

    long rate=(long)(audio_hq_get_clock_rate()*(1.0+ajuste));
    if (rate!=hq_clock_rate) {
        hq_clock_rate=rate;
        blip_buffer_set_clock_rate(hq_buf_beeper,rate);
        blip_buffer_set_clock_rate(hq_buf_left,rate);
        blip_buffer_set_clock_rate(hq_buf_right,rate);
    }

    blip_buffer_end_frame(hq_buf_beeper,frame_tstates);
    blip_buffer_end_frame(hq_buf_left,frame_tstates);
    blip_buffer_end_frame(hq_buf_right,frame_tstates);

    long total=blip_buffer_samples_avail(hq_buf_beeper);
    if (total>AUDIO_HQ_MAX_FRAME_SAMPLES) total=AUDIO_HQ_MAX_FRAME_SAMPLES;

    long n_beeper=blip_buffer_read_samples(hq_buf_beeper,hq_samples_beeper,total,0);
    long n_left=blip_buffer_read_samples(hq_buf_left,hq_samples_left,total,0);
    long n_right=blip_buffer_read_samples(hq_buf_right,hq_samples_right,total,0);

    int speaker=audio_hq_machine_has_speaker();
    double volume=audiovolume/100.0;

    long i;
    for (i=0;i<n_beeper;i++) {
        double beeper=ula_filter_apply(hq_samples_beeper[i]);
        double left=(i<n_left ? hq_samples_left[i] : 0);
        double right=(i<n_right ? hq_samples_right[i] : 0);
        double out_left,out_right;

        if (speaker) {
            beeper=speaker_filter_apply(beeper);
            //Las demas fuentes (perifericos) no pasan por el altavoz. Quitar su componente continua con el filtro de TV
            out_left=beeper+tv_filter_apply(&tv_filters[0],left);
            out_right=beeper+tv_filter_apply(&tv_filters[1],right);
        }
        else {
            out_left=tv_filter_apply(&tv_filters[0],beeper+left);
            out_right=tv_filter_apply(&tv_filters[1],beeper+right);
        }

        short l=audio_hq_clip(out_left*volume);
        short r=audio_hq_clip(out_right*volume);

        audio_hq_fifo_put(l,r);

        if (hq_dump_file!=NULL) {
            short pair[2];
            pair[0]=l;
            pair[1]=r;
            fwrite(pair,sizeof(short),2,hq_dump_file);
        }
    }

    if (hq_dump_file!=NULL) fflush(hq_dump_file);

    audio_hq_fifo_limit_latency();
}
