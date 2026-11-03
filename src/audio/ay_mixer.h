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

#ifndef AY_MIXER_H
#define AY_MIXER_H

#include "cpu.h"

//Mezclador del AY: cambios desde los menus (se aplican enseguida) y archivos .mix de las canciones.
//El estado esta en ay38912.c (ay3_stereo_mode, ay_mixer_volume, ay_mixer_pan, ay_mixer_mute)

//Cargar automaticamente el .mix de cada cancion del reproductor AY
extern z80_bit ay_mixer_auto_load;

//Cambiar el modo estereo: los panoramas vuelven a donde los pone el modo
extern void ay_mixer_set_stereo(int modo);

//Volumenes al 100%, panoramas donde los pone el modo, ningun canal silenciado
extern void ay_mixer_reset(void);

//Empieza una cancion (desde 1) del archivo path, con songs canciones: se carga su .mix si esta
//activada la carga automatica. Sin .mix vuelven los ajustes del usuario
extern void ay_mixer_song_started(char *path,int song,int songs);

//Vuelven los ajustes del usuario (al parar el reproductor o desactivar la carga automatica)
extern void ay_mixer_restore_user(void);

//Nombre del .mix en uso ("": ninguno)
extern char *ay_mixer_loaded_name(void);

//El .mix propuesto para una cancion (junto a ella, o en la carpeta de mezclas de ZEsarUX si alli no se puede escribir)
extern void ay_mixer_default_path(char *path,int song,int songs,char *destino);

//Guardar / cargar un .mix. Retornan 0 si ok
extern int ay_mixer_save(char *filename);
extern int ay_mixer_load(char *filename);

//Los ajustes del usuario (para guardar la configuracion): no los de un .mix cargado
extern void ay_mixer_user_values(int *stereo,int custom[3],int volume[3],int pan[3]);

#endif
