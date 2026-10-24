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

#ifndef AUDIO_HQ_H
#define AUDIO_HQ_H

#include "cpu.h"

#define AUDIO_HQ_SAMPLE_RATE 48000

extern z80_bit audio_hq_enabled;
extern int audio_hq_running;

extern int audio_hq_machine_supported(void);
extern void audio_hq_init(void);
extern void audio_hq_end(void);
extern void audio_hq_reset(void);

extern void audio_hq_beeper(int tstates,int value);
extern void audio_hq_others(int tstates,int left,int right);
extern void audio_hq_end_frame(int frame_tstates);

extern int audio_hq_fifo_frames(void);
extern void audio_hq_fifo_empty(void);
extern void audio_hq_fifo_get(short *destino,int frames);
extern void audio_hq_fifo_put_classic(char *buffer,int samples,int frequency);
extern void audio_hq_fifo_limit_latency(void);

#endif
