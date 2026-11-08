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
//cpc: cancion de Amstrad CPC (AY en los puertos del PPI, Z80 a 4 MHz, AY a 1 MHz)
extern void ay_measure_start(unsigned char *data,int size,int song,int id,int cpc);

//Abandonar la medicion en curso (y esperar a que termine el hilo)
extern void ay_measure_stop(void);

//Si ha terminado la medicion id: retorna 1 y la duracion en frames
//(0: no se encontro repeticion) y desde donde sigue la musica (-1: termina)
extern int ay_measure_result(int id,long *frames,long *loop);

//Midiendo ahora?
extern int ay_measure_running(void);

//Un Spectrum 48K (o un Amstrad CPC) reducido: solo memoria, Z80 y puertos del AY
//(tambien lo usa el reproductor en hilo aparte)
#include "chips_z80.h"
#include "aymodule.h"

//t-estados de un frame
#define AY_MEASURE_FRAME_TSTATES 69888
//En el CPC: 4 MHz / 50 Hz
#define AY_MEASURE_FRAME_TSTATES_CPC 80000

#define AY_MEASURE_AY_FREQUENCY 1773400
#define AY_MEASURE_AY_FREQUENCY_CPC 1000000

struct ay_measure_machine {
    z80_t cpu;
    uint64_t pins;
    unsigned char mem[65536];
    unsigned char ay_registers[16];
    int ay_selected;
    int ay_chip;
    //Amstrad CPC: el AY se maneja con los puertos A y C del PPI 8255
    int cpc;
    unsigned char ppi_a,ppi_c;
    //t-estados de un frame y frecuencia del AY de la maquina
    long frame_tstates;
    long ay_frequency;
    //Modulo de musica (Pro Tracker 3...) en vez de un archivo AY: sus registros salen de su reproductor,
    //no del Z80 (fork). NULL: archivo AY
    aymodule *module;
};

struct ay_measure_write {
    int tstates;
    unsigned char reg,value;
};

extern int ay_measure_setup(struct ay_measure_machine *m,unsigned char *data,int size,int song,int cpc);
//Liberar lo que tenga la maquina (el modulo), antes de free() o de otro ay_measure_setup
extern void ay_measure_release(struct ay_measure_machine *m);
//Los datos son un modulo de musica (no un archivo AY)? Retorna 1 si se puede reproducir
extern int ay_measure_is_module(unsigned char *data,int size);
extern int ay_measure_run_frame(struct ay_measure_machine *m,struct ay_measure_write *writes,int max_writes);

//Cancion (desde 1) de Amstrad CPC? Se prueban unos frames como Spectrum y como CPC
#define AY_MEASURE_PROBE_FRAMES 50
extern int ay_measure_is_cpc(unsigned char *data,int size,int song);

//Medir sin hilo (para pruebas). Retorna la duracion en frames (0: no encontrada)
extern long ay_measure_song(unsigned char *data,int size,int song,int cpc,long *loop,volatile int *cancel);

#endif
