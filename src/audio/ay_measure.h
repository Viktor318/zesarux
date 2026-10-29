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

//Medir sin hilo (para pruebas). Retorna la duracion en frames (0: no encontrada)
extern long ay_measure_song(unsigned char *data,int size,int song,long *loop,volatile int *cancel);

#endif
