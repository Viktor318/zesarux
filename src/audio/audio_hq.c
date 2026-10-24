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
   High quality sound output, in the way the Fuse emulator makes it.

   The classic ZEsarUX sound is one 8 bit sample per scanline (15600 Hz).
   Here the sound is made at 48000 Hz, 16 bit, band limited:
   - Every change of the beeper (port FEh) is put in at its exact time
     (in t-states) with Blip_Buffer (from Fuse, by Shay Green), so no aliasing.
   - The other sound sources (AY etc.) still come once per scanline, but are
     also put in band limited.
   - The output goes through the same filters as in Fuse: an approximation of
     the ULA output, and then the built-in speaker of the 16K/48K, or a TV
     speaker for the other machines.
   Filters adapted from Fuse (speaker_filter.c, ula_filter.c, tv_filter.c),
   Copyright (c) 2026 Fredrick Meunier, GPL 2 or later.

   The classic 8 bit buffer is still filled as before (audio file output,
   waveform window, ZENG...), only the sound card gets this output.
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

//Setting: use the high quality sound output
z80_bit audio_hq_enabled={1};

//The output is being made now (the setting is on and the machine is supported)
int audio_hq_running=0;

#define AUDIO_HQ_PI 3.14159265358979323846

//Multiplier from the 8 bit levels of ZEsarUX to 16 bit
#define AUDIO_HQ_SCALE 128

static Blip_Buffer *hq_buf_beeper=NULL;
static Blip_Buffer *hq_buf_left=NULL;
static Blip_Buffer *hq_buf_right=NULL;
static Blip_Synth *hq_synth_beeper=NULL;
static Blip_Synth *hq_synth_left=NULL;
static Blip_Synth *hq_synth_right=NULL;

static long hq_clock_rate=0;

//Last levels given, to put them again at the start of every frame
static int hq_last_beeper=0;
static int hq_last_left=0;
static int hq_last_right=0;

//Samples of one frame. 48000 Hz at 2% speed would be 48000 samples per frame
#define AUDIO_HQ_MAX_FRAME_SAMPLES 48000
static blip_sample_t hq_samples_beeper[AUDIO_HQ_MAX_FRAME_SAMPLES];
static blip_sample_t hq_samples_left[AUDIO_HQ_MAX_FRAME_SAMPLES];
static blip_sample_t hq_samples_right[AUDIO_HQ_MAX_FRAME_SAMPLES];


/* --- Filters, adapted from Fuse --- */

//ULA output: asymmetric one pole
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

//Built-in speaker of 16K/48K: second order resonant high pass, 750 Hz
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

//TV speaker: 100 Hz high pass, 10 kHz low pass. One per channel
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

/* --- Output FIFO: 16 bit stereo, read by the sound card driver --- */

//Half a second
#define AUDIO_HQ_FIFO_FRAMES (AUDIO_HQ_SAMPLE_RATE/2)

static short hq_fifo[AUDIO_HQ_FIFO_FRAMES*2];
static volatile int hq_fifo_write=0;
static volatile int hq_fifo_read=0;

//Last sample given to the sound card, repeated when there is no more sound
static short hq_fifo_last_left=0;
static short hq_fifo_last_right=0;

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
    int w=hq_fifo_write;
    int next=w+1;
    if (next==AUDIO_HQ_FIFO_FRAMES) next=0;

    //Full: drop the sample
    if (next==hq_fifo_read) return;

    hq_fifo[w*2]=left;
    hq_fifo[w*2+1]=right;
    hq_fifo_write=next;
}

//If the emulation runs a bit faster than the sound card, the FIFO grows: keep the delay low
//by dropping the oldest samples (as the classic driver empties its FIFO when it is full)
void audio_hq_fifo_limit_latency(void)
{
    int frames=audio_hq_fifo_frames();
    if (frames>AUDIO_HQ_SAMPLE_RATE/6) {
        int drop=frames-AUDIO_HQ_SAMPLE_RATE/16;
        int r=hq_fifo_read+drop;
        if (r>=AUDIO_HQ_FIFO_FRAMES) r-=AUDIO_HQ_FIFO_FRAMES;
        hq_fifo_read=r;
    }
}

//Classic 8 bit sound (machines without the high quality output): resample to the FIFO
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

//Give samples to the sound card. When the FIFO runs out, repeat the last sample
void audio_hq_fifo_get(short *destino,int frames)
{
    int i;

    for (i=0;i<frames;i++) {
        int r=hq_fifo_read;
        if (r!=hq_fifo_write) {
            hq_fifo_last_left=hq_fifo[r*2];
            hq_fifo_last_right=hq_fifo[r*2+1];
            r++;
            if (r==AUDIO_HQ_FIFO_FRAMES) r=0;
            hq_fifo_read=r;
        }
        *destino++=hq_fifo_last_left;
        *destino++=hq_fifo_last_right;
    }
}


/* --- Optional raw dump of the output, for testing --- */

static FILE *hq_dump_file=NULL;


/* --- Machine support --- */

int audio_hq_machine_supported(void)
{
    return MACHINE_IS_SPECTRUM;
}

//16K/48K have a built-in speaker. The others go to a TV
static int audio_hq_machine_has_speaker(void)
{
    return MACHINE_IS_SPECTRUM_16_48 && !MACHINE_IS_TIMEX_TC2048;
}


/* --- Init / end --- */

static long audio_hq_get_clock_rate(void)
{
    int frames_per_second=(machine_60hz.v ? 60 : 50);
    long rate=(long)screen_testados_total*frames_per_second;
    rate=rate*porcentaje_velocidad_emulador/100;
    if (rate<1) rate=1;
    return rate;
}

static Blip_Buffer *audio_hq_new_buffer(void)
{
    Blip_Buffer *b=new_Blip_Buffer();
    if (b==NULL) return NULL;

    blip_buffer_set_clock_rate(b,hq_clock_rate);
    //One second of buffer: enough even at 2% emulation speed
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

    //Start from the current levels, without a jump
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

//Call when the machine, the emulation speed or the setting changes
void audio_hq_reset(void)
{
    audio_hq_init();
}

static int hq_machine_type=-1;

//Start or stop the output when the setting or the machine changes
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


/* --- Sound input --- */

//Time inside the frame, limited to something the buffer can take
static blip_time_t audio_hq_time(int tstates)
{
    if (tstates<0) tstates=0;
    //Up to one frame and a bit later (an instruction can go beyond the end of the frame)
    if (tstates>screen_testados_total*2) tstates=screen_testados_total*2;
    return tstates;
}

//Beeper level changed at this t-state of the frame. value: the 8 bit value_beeper
void audio_hq_beeper(int tstates,int value)
{
    if (!audio_hq_running) return;

    int level=value*AUDIO_HQ_SCALE;
    hq_last_beeper=level;

    blip_synth_update(hq_synth_beeper,audio_hq_time(tstates),level);
}

//The other sound sources, once per scanline. 8 bit values, without the beeper
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

//End of a frame of the emulated machine: frame_tstates t-states long
void audio_hq_end_frame(int frame_tstates)
{
    audio_hq_check();

    if (!audio_hq_running) return;

    //Emulation speed or frame length changed
    long rate=audio_hq_get_clock_rate();
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
            //The other sources (peripherals) do not go through the speaker. Remove their DC with the TV filter
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
