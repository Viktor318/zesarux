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

#ifndef AY_THREAD_PLAYER_H
#define AY_THREAD_PLAYER_H

//Reproductor de archivos AY con un Z80 propio, en el hilo de audio.
//La musica no depende del emulador ni de los menus

extern int ay_tp_start(unsigned char *data,int size,int song);
extern void ay_tp_stop(void);
extern void ay_tp_set_paused(int paused);
extern int ay_tp_active(void);
//Frames (1/50 s) que ya se han escuchado
extern long ay_tp_frames_played(void);
//Registro del AY de la cancion que suena
extern unsigned char ay_tp_register(int reg);
//Desde el hilo de audio: mezclar la musica con el sonido del emulador
extern void ay_tp_mix(short *destino,int frames);

#endif
