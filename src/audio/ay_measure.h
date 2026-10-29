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

#ifndef AY_MEASURE_H
#define AY_MEASURE_H

//Medicion de la duracion de una cancion de un archivo AY, con un Z80 propio
//(no el del emulador), en un hilo aparte

//Empezar a medir la cancion (desde 1) de los datos de un archivo AY (se copian).
//Si ya se estaba midiendo otra, se abandona. id identifica la medicion
extern void ay_measure_start(unsigned char *data,int size,int song,int id);

//Abandonar la medicion en curso (y esperar a que termine el hilo)
extern void ay_measure_stop(void);

//Si ha terminado la medicion id: retorna 1 y la duracion en frames
//(0: no se encontro repeticion) y desde donde sigue la musica (-1: termina)
extern int ay_measure_result(int id,long *frames,long *loop);

//Midiendo ahora?
extern int ay_measure_running(void);

//Un Spectrum 48K reducido: solo memoria, Z80 y puertos del AY
//(tambien lo usa el reproductor en hilo aparte)
#include "chips_z80.h"

//t-estados de un frame
#define AY_MEASURE_FRAME_TSTATES 69888

struct ay_measure_machine {
    z80_t cpu;
    uint64_t pins;
    unsigned char mem[65536];
    unsigned char ay_registers[16];
    int ay_selected;
    int ay_chip;
};

struct ay_measure_write {
    int tstates;
    unsigned char reg,value;
};

extern int ay_measure_setup(struct ay_measure_machine *m,unsigned char *data,int size,int song);
extern int ay_measure_run_frame(struct ay_measure_machine *m,struct ay_measure_write *writes,int max_writes);

//Medir sin hilo (para pruebas). Retorna la duracion en frames (0: no encontrada)
extern long ay_measure_song(unsigned char *data,int size,int song,long *loop,volatile int *cancel);

#endif
