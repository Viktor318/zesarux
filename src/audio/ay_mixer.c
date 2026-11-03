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
   Mezclador del AY (como el del fork de Fuse): modo estereo, volumen y panorama de cada canal.
   Son ajustes generales del chip (tambien los oyen los juegos). Un archivo .mix junto a una cancion
   los guarda para esa cancion; cargado, se usa hasta una cancion sin el, y no cambia los ajustes
   guardados del usuario.

   Un .mix es texto, un ajuste por linea ("; " empieza un comentario), el mismo formato que en Fuse:
     stereo=ACB
     place_a=Left       (el modo Custom: Left, Centre o Right)
     volume_a=100       (0-100)
     pan_a=-100         (-100: izquierda, 0: centro, 100: derecha)
   y lo mismo para los canales B y C. La cancion n (desde 1) de un archivo con varias puede tener
   uno propio: Cancion.ay -> Cancion.3.mix, que se usa antes que Cancion.mix
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ay_mixer.h"
#include "ay38912.h"
#include "cpu.h"
#include "debug.h"
#include "utils.h"

#define AY_MIXER_FOLDER "zesarux_mix"

z80_bit ay_mixer_auto_load={1};

struct s_ay_mixer_values {
    int stereo;
    int custom[3];
    int volume[3];
    int pan[3];
};

//Ajustes del usuario mientras se usan los de un .mix
static struct s_ay_mixer_values ay_mixer_user;
static int ay_mixer_temporary=0;
static char ay_mixer_loaded[256]="";

static const char *ay_mixer_place_names[3]={"Left","Centre","Right"};

static void ay_mixer_current(struct s_ay_mixer_values *m)
{
    int canal;

    m->stereo=ay3_stereo_mode;
    m->custom[0]=ay3_custom_stereo_A;
    m->custom[1]=ay3_custom_stereo_B;
    m->custom[2]=ay3_custom_stereo_C;
    for (canal=0;canal<3;canal++) {
        m->volume[canal]=ay_mixer_volume[canal];
        m->pan[canal]=ay_mixer_pan[canal];
    }
}

static void ay_mixer_use(const struct s_ay_mixer_values *m)
{
    int canal;

    ay3_stereo_mode=m->stereo;
    ay3_custom_stereo_A=m->custom[0];
    ay3_custom_stereo_B=m->custom[1];
    ay3_custom_stereo_C=m->custom[2];
    for (canal=0;canal<3;canal++) {
        ay_mixer_volume[canal]=m->volume[canal];
        ay_mixer_pan[canal]=m->pan[canal];
    }
}

void ay_mixer_set_stereo(int modo)
{
    if (modo<0 || modo>=AY_STEREO_TOTAL_MODES) modo=0;
    ay3_stereo_mode=modo;
    ay_mixer_pans_reset();
}

void ay_mixer_reset(void)
{
    int canal;

    for (canal=0;canal<3;canal++) {
        ay_mixer_volume[canal]=100;
        ay_mixer_mute[canal]=0;
    }
    ay_mixer_pans_reset();
}

char *ay_mixer_loaded_name(void)
{
    return ay_mixer_loaded;
}

void ay_mixer_user_values(int *stereo,int custom[3],int volume[3],int pan[3])
{
    struct s_ay_mixer_values m;
    int canal;

    if (ay_mixer_temporary) m=ay_mixer_user;
    else ay_mixer_current(&m);

    *stereo=m.stereo;
    for (canal=0;canal<3;canal++) {
        custom[canal]=m.custom[canal];
        volume[canal]=m.volume[canal];
        pan[canal]=m.pan[canal];
    }
}


/* ------------------------------------------------------------------------
   Archivos .mix
   ------------------------------------------------------------------------ */

//Modo estereo por su nombre (sin distinguir mayusculas). -1 si no es ninguno
static int ay_mixer_stereo_by_name(char *nombre)
{
    int modo;

    for (modo=0;modo<AY_STEREO_TOTAL_MODES;modo++) {
        const char *n=ay_mixer_stereo_name(modo);
        int i;
        for (i=0;n[i] && nombre[i];i++) {
            if (letra_mayuscula(n[i])!=letra_mayuscula(nombre[i])) break;
        }
        if (!n[i] && !nombre[i]) return modo;
    }

    //Fuse llama "None" al mono
    if (!strcasecmp(nombre,"None")) return 0;

    return -1;
}

static int ay_mixer_place_by_name(char *nombre,int defecto)
{
    int i;
    for (i=0;i<3;i++) {
        if (!strcasecmp(nombre,ay_mixer_place_names[i])) return i;
    }
    //Tambien con la ortografia americana
    if (!strcasecmp(nombre,"Center")) return 1;
    return defecto;
}

//Carpeta de mezclas de ZEsarUX (en el directorio del usuario), con separador al final. Retorna 0 si no hay
static int ay_mixer_folder(char *destino)
{
    char carpeta[PATH_MAX];

    if (!util_get_file_in_home_dir(carpeta,AY_MIXER_FOLDER)) return 0;

#ifdef MINGW
    sprintf(destino,"%s\\",carpeta);
#else
    sprintf(destino,"%s/",carpeta);
#endif
    return 1;
}

static void ay_mixer_create_folder(void)
{
    char carpeta[PATH_MAX];

    if (!util_get_file_in_home_dir(carpeta,AY_MIXER_FOLDER)) return;

#ifdef MINGW
    mkdir(carpeta);
#else
    mkdir(carpeta,0755);
#endif
}

//Carpeta de path (con el separador final) y su nombre sin la extension
static void ay_mixer_split(char *path,char *carpeta,char *base)
{
    char *nombre=path;
    char *p;
    char *punto=NULL;

    for (p=path;*p;p++) {
        if (*p=='/' || *p=='\\') nombre=p+1;
    }
    for (p=nombre;*p;p++) {
        if (*p=='.') punto=p;
    }

    int largo_carpeta=nombre-path;
    strncpy(carpeta,path,largo_carpeta);
    carpeta[largo_carpeta]=0;

    int largo_base=(punto!=NULL ? punto-nombre : (int)strlen(nombre));
    if (largo_base>250) largo_base=250;
    strncpy(base,nombre,largo_base);
    base[largo_base]=0;
}

static int ay_mixer_folder_writable(char *carpeta)
{
    char prueba[PATH_MAX];
    FILE *f;

    snprintf(prueba,sizeof(prueba),"%s.zesarux-mix-test",carpeta);
    f=fopen(prueba,"w");
    if (f==NULL) return 0;
    fclose(f);
    remove(prueba);
    return 1;
}

void ay_mixer_default_path(char *path,int song,int songs,char *destino)
{
    char carpeta[PATH_MAX],base[256],mezclas[PATH_MAX];

    //Sin cancion: un archivo general en la carpeta de mezclas
    if (path==NULL || !path[0]) {
        if (!ay_mixer_folder(mezclas)) strcpy(mezclas,"");
        sprintf(destino,"%smixer.mix",mezclas);
        return;
    }

    ay_mixer_split(path,carpeta,base);

    //La de la cancion, en archivos con varias
    char nombre[300];
    if (songs>1) sprintf(nombre,"%s.%d.mix",base,song);
    else sprintf(nombre,"%s.mix",base);

    if (!ay_mixer_folder_writable(carpeta) && ay_mixer_folder(mezclas)) {
        ay_mixer_create_folder();
        sprintf(destino,"%s%s",mezclas,nombre);
        return;
    }

    sprintf(destino,"%s%s",carpeta,nombre);
}

int ay_mixer_save(char *filename)
{
    struct s_ay_mixer_values m;
    FILE *f;
    int canal;

    ay_mixer_current(&m);

    f=fopen(filename,"w");
    if (f==NULL) {
        debug_printf(VERBOSE_ERR,"Can not write file %s",filename);
        return 1;
    }

    fprintf(f,"; ZEsarUX AY mixer\n");
    fprintf(f,"stereo=%s\n",ay_mixer_stereo_name(m.stereo));
    for (canal=0;canal<3;canal++) {
        int lugar=m.custom[canal];
        if (lugar<0 || lugar>2) lugar=1;
        fprintf(f,"place_%c=%s\n",'a'+canal,ay_mixer_place_names[lugar]);
        fprintf(f,"volume_%c=%d\n",'a'+canal,m.volume[canal]);
        fprintf(f,"pan_%c=%d\n",'a'+canal,ay_mixer_channel_pan(canal));
    }

    if (fclose(f)) {
        debug_printf(VERBOSE_ERR,"Can not write file %s",filename);
        return 1;
    }

    //Lo que suena ahora es este archivo
    if (ay_mixer_temporary) {
        char nombre[PATH_MAX];
        util_get_file_no_directory(filename,nombre);
        snprintf(ay_mixer_loaded,sizeof(ay_mixer_loaded),"%s",nombre);
    }

    return 0;
}

//Leer un .mix sobre los ajustes de m. Retorna 0 si ok
static int ay_mixer_read(char *filename,struct s_ay_mixer_values *m)
{
    char linea[256],clave[64],valor[64];
    FILE *f=fopen(filename,"r");

    if (f==NULL) return 1;

    while (fgets(linea,sizeof(linea),f)!=NULL) {
        int canal,numero;
        int largo;

        if (sscanf(linea," %63[^= \t] = %63s",clave,valor)!=2) continue;
        if (clave[0]==';') continue;

        if (!strcmp(clave,"stereo")) {
            int modo=ay_mixer_stereo_by_name(valor);
            if (modo>=0) m->stereo=modo;
            continue;
        }

        largo=strlen(clave);
        if (largo<3) continue;
        canal=clave[largo-1]-'a';
        if (canal<0 || canal>2 || clave[largo-2]!='_') continue;
        clave[largo-2]=0;
        numero=atoi(valor);

        if (!strcmp(clave,"place")) m->custom[canal]=ay_mixer_place_by_name(valor,m->custom[canal]);
        else if (!strcmp(clave,"volume") && numero>=0 && numero<=100) m->volume[canal]=numero;
        else if (!strcmp(clave,"pan") && numero>=-100 && numero<=100) m->pan[canal]=numero;
    }

    fclose(f);
    return 0;
}

int ay_mixer_load(char *filename)
{
    struct s_ay_mixer_values m;

    ay_mixer_current(&m);
    if (ay_mixer_read(filename,&m)) {
        debug_printf(VERBOSE_ERR,"Can not read file %s",filename);
        return 1;
    }

    //Cargado a mano: pasan a ser los ajustes del usuario
    ay_mixer_temporary=0;
    ay_mixer_use(&m);

    char nombre[PATH_MAX];
    util_get_file_no_directory(filename,nombre);
    snprintf(ay_mixer_loaded,sizeof(ay_mixer_loaded),"%s",nombre);

    return 0;
}

//El .mix de una cancion: el de la propia cancion, luego el del archivo; junto a la cancion, luego en la carpeta de mezclas
static int ay_mixer_find(char *path,int song,int songs,char *encontrado)
{
    char carpeta[PATH_MAX],base[256],mezclas[PATH_MAX];
    char *carpetas[2];
    int i,propio;
    int total_carpetas=1;

    ay_mixer_split(path,carpeta,base);
    carpetas[0]=carpeta;
    if (ay_mixer_folder(mezclas)) {
        carpetas[1]=mezclas;
        total_carpetas=2;
    }

    for (i=0;i<total_carpetas;i++) {
        for (propio=(songs>1);propio>=0;propio--) {
            if (propio) sprintf(encontrado,"%s%s.%d.mix",carpetas[i],base,song);
            else sprintf(encontrado,"%s%s.mix",carpetas[i],base);

            if (si_existe_archivo(encontrado)) return 1;
        }
    }

    return 0;
}

void ay_mixer_restore_user(void)
{
    if (ay_mixer_temporary) {
        ay_mixer_use(&ay_mixer_user);
        ay_mixer_temporary=0;
    }
    ay_mixer_loaded[0]=0;
}

void ay_mixer_song_started(char *path,int song,int songs)
{
    char encontrado[PATH_MAX];
    struct s_ay_mixer_values m;

    if (!ay_mixer_auto_load.v || path==NULL || !path[0] || !ay_mixer_find(path,song,songs,encontrado)) {
        //Sin .mix: los ajustes del usuario
        ay_mixer_restore_user();
        return;
    }

    //Desde los ajustes del usuario, no desde los de otra cancion
    if (!ay_mixer_temporary) {
        ay_mixer_current(&ay_mixer_user);
        ay_mixer_temporary=1;
    }
    m=ay_mixer_user;

    if (ay_mixer_read(encontrado,&m)) {
        ay_mixer_restore_user();
        return;
    }

    ay_mixer_use(&m);

    char nombre[PATH_MAX];
    util_get_file_no_directory(encontrado,nombre);
    snprintf(ay_mixer_loaded,sizeof(ay_mixer_loaded),"%s",nombre);

    debug_printf(VERBOSE_INFO,"AY mixer settings loaded from %s",encontrado);
}
