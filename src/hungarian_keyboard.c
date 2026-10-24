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
   Teclado hungaro para Spectrum

   En un teclado hungaro (QWERTZ) los simbolos estan en sitios distintos que en un
   teclado ingles, y hay teclas con vocales acentuadas (o, u, o con doble acento, etc).
   En lugar de mapear posiciones fisicas, aqui se recibe el caracter final que genera
   el sistema operativo (por ejemplo Shift+2 = comilla doble) y se pulsa la combinacion
   del Spectrum que genera ese mismo caracter (Symbol shift + P).

   - Letras minusculas y numeros: tecla tal cual
   - Letras mayusculas: Caps shift + letra
   - Simbolos: Symbol shift + tecla correspondiente
   - Vocales acentuadas hungaras: se convierten a la vocal sin acento
   - Simbolos del modo extendido ([ ] { } \ | ~ y copyright): se envia una secuencia temporizada:
     Caps shift + Symbol shift (modo E), soltar, Symbol shift + tecla, soltar.
     Funciona igual en el editor de 48K y en el de 128K
   - Caracteres que no existen en el Spectrum: se ignoran (no se pulsa nada)

   Cada tecla del host se sigue por su identificador (scancode) para poder liberar
   exactamente lo que se pulso, aunque al soltar ya no se genere ningun caracter.
*/

#include <stdio.h>

#include "hungarian_keyboard.h"
#include "cpu.h"
#include "utils.h"
#include "debug.h"
#include "zxvision.h"
#include "ql_i8049.h"

z80_bit hungarian_keyboard={0};

//Left Alt + number keys = Caps shift + number (Spectrum). Enabled by default
z80_bit keyboard_alt_caps_shift={1};

struct s_hungarian_keyboard_tecla {
    //Si esta pulsada (gestionada por nosotros)
    int pulsada;

    //Tecla base del Spectrum ('a'-'z', '0'-'9'). 0 si el caracter no tiene equivalente
    z80_byte tecla_base;

    int necesita_caps;
    int necesita_symbol;
    //QL: CTRL (accented letters)
    int necesita_ctrl;

    //Orden de pulsacion, para saber cual es la ultima tecla aun pulsada
    unsigned int orden;
};

static struct s_hungarian_keyboard_tecla hungarian_keyboard_teclas[HUNGARIAN_KEYBOARD_MAX_KEYS];

static unsigned int hungarian_keyboard_contador_orden=0;

//Ultimos modificadores del host recibidos, para restaurar los shifts al acabar una secuencia
static int hungarian_keyboard_ultimos_host_mods=0;


//Secuencias temporizadas (cola de pulsaciones que se envian frame a frame)
//Se usan para los simbolos del modo extendido, y para cualquier tecla que se pulse
//mientras se esta enviando una secuencia, para no alterar el orden

//Duracion en frames (1/50 s) de cada paso: pulsado y soltado
#define HUNGARIAN_KEYBOARD_FRAMES_PASO 3

#define HUNGARIAN_KEYBOARD_COLA_MAX 32

struct s_hungarian_keyboard_secuencia {
    z80_byte tecla_base;
    int necesita_caps;
    int necesita_symbol;
    int necesita_ctrl;
    int modo_extendido;
};

static struct s_hungarian_keyboard_secuencia hungarian_keyboard_cola[HUNGARIAN_KEYBOARD_COLA_MAX];
static int hungarian_keyboard_cola_inicio=0;
static int hungarian_keyboard_cola_total=0;

//Paso actual de la secuencia en curso:
//0: pulsar Caps+Symbol (modo E)  1: soltar  2: pulsar tecla  3: soltar
static int hungarian_keyboard_cola_paso=0;
static int hungarian_keyboard_cola_frames=0;


//Tabla de simbolos: caracter -> tecla que se pulsa junto con Symbol shift
struct s_hungarian_keyboard_simbolo {
    unsigned int caracter;
    z80_byte tecla_base;
};

static const struct s_hungarian_keyboard_simbolo hungarian_keyboard_simbolos[]={
    {'!','1'}, {'@','2'}, {'#','3'}, {'$','4'}, {'%','5'},
    {'&','6'}, {'\'','7'}, {'(','8'}, {')','9'}, {'_','0'},
    {'<','r'}, {'>','t'}, {';','o'}, {'"','p'},
    {'^','h'}, {'-','j'}, {'+','k'}, {'=','l'},
    {':','z'}, {0xA3,'x'} /* libra */, {'?','c'}, {'/','v'}, {'*','b'},
    {',','n'}, {'.','m'},
    //Signos de multiplicar y dividir (AltGr+u acentuada / AltGr+o doble acento)
    {0xD7,'b'}, {0xF7,'v'},
    {0,0}
};


//Simbolos del modo extendido: caracter -> tecla que se pulsa con Symbol shift estando en modo E
static const struct s_hungarian_keyboard_simbolo hungarian_keyboard_simbolos_extendido[]={
    {'[','y'}, {']','u'}, {'{','f'}, {'}','g'},
    {'\\','d'}, {'|','s'}, {'~','a'}, {0xA9,'p'} /* copyright */,
    {0,0}
};


//ZX81 (and clones): character -> key pressed together with SHIFT
static const struct s_hungarian_keyboard_simbolo hungarian_keyboard_simbolos_zx81[]={
    {'$','u'}, {'(','i'}, {')','o'}, {'*','b'}, {'?','c'}, {'^','h'} /* ** */,
    {'-','j'}, {'+','k'}, {'=','l'}, {';','x'}, {'"','p'}, {'<','n'},
    {'>','m'}, {'/','v'}, {':','z'}, {',','.'}, {0xA3,' '} /* libra */,
    {0xD7,'b'}, {0xF7,'v'},
    {0,0}
};

//ZX80 (and clones): character -> key pressed together with SHIFT
static const struct s_hungarian_keyboard_simbolo hungarian_keyboard_simbolos_zx80[]={
    {'$','u'}, {'(','i'}, {')','o'}, {'*','p'}, {'?','c'}, {'^','h'} /* ** */,
    {'-','j'}, {'+','k'}, {'=','l'}, {';','x'}, {'"','y'}, {'<','n'},
    {'>','m'}, {'/','v'}, {':','z'}, {',','.'}, {0xA3,' '} /* libra */,
    {0xD7,'p'}, {0xF7,'v'},
    {0,0}
};

//ZX80/ZX81 characters typed without shift (apart from letters and numbers)
static const struct s_hungarian_keyboard_simbolo hungarian_keyboard_simbolos_zx8081_sin_shift[]={
    {'.','.'},
    {0,0}
};

//QL: character -> key and modifiers
struct s_hungarian_keyboard_simbolo_ql {
    unsigned int caracter;
    z80_byte tecla_base;  //letter, number, or the unshifted character of the key
    int shift;
    int ctrl;
};

static const struct s_hungarian_keyboard_simbolo_ql hungarian_keyboard_simbolos_ql[]={
    //Symbols on the number keys
    {'!','1',1,0}, {'@','2',1,0}, {'#','3',1,0}, {'$','4',1,0}, {'%','5',1,0},
    {'^','6',1,0}, {'&','7',1,0}, {'*','8',1,0}, {'(','9',1,0}, {')','0',1,0},
    //Symbol keys
    {'-','-',0,0}, {'_','-',1,0}, {'=','=',0,0}, {'+','=',1,0},
    {0xA3,'`',0,0} /* libra */, {'~','`',1,0},
    {'[','[',0,0}, {'{','[',1,0}, {']',']',0,0}, {'}',']',1,0},
    {';',';',0,0}, {':',';',1,0}, {'\'','\'',0,0}, {'"','\'',1,0},
    {'\\','\\',0,0}, {'|','\\',1,0},
    {',',',',0,0}, {'<',',',1,0}, {'.','.',0,0}, {'>','.',1,0},
    {'/','/',0,0}, {'?','/',1,0},
    //Characters 128-191 of the QL: CTRL (+SHIFT) + key
    {0xE1,',',0,1} /* a acute */, {0xE9,'3',1,1} /* e acute */, {0xED,'3',0,1} /* i acute */,
    {0xF3,'6',0,1} /* o acute */, {0xF6,'4',1,1} /* o diaeresis */, {0xFA,'9',0,1} /* u acute */,
    {0xFC,'\'',0,1} /* u diaeresis */,
    {0x151,'8',0,1} /* o double acute -> o circumflex */, {0x171,';',0,1} /* u double acute -> u circumflex */,
    {0xC9,'c',1,1} /* E acute */, {0xD6,'d',1,1} /* O diaeresis */, {0xDC,'g',1,1} /* U diaeresis */,
    {0x150,'d',1,1} /* O double acute -> O diaeresis */, {0x170,'g',1,1} /* U double acute -> U diaeresis */,
    {0xC4,'2',1,1} /* A diaeresis */,
    {'`','/',1,1},
    {0,0,0,0}
};


//Convierte las letras acentuadas (hungaras y alguna mas de AltGr) a su letra sin acento
static unsigned int hungarian_keyboard_quitar_acento(unsigned int c)
{
    switch (c) {
        case 0xE1: case 0xE4:                         return 'a'; // á ä
        case 0xC1: case 0xC4:                         return 'A'; // Á Ä
        case 0xE9:                                    return 'e'; // é
        case 0xC9:                                    return 'E'; // É
        case 0xED:                                    return 'i'; // í
        case 0xCD:                                    return 'I'; // Í
        case 0xF3: case 0xF6: case 0x151:             return 'o'; // ó ö ő
        case 0xD3: case 0xD6: case 0x150:             return 'O'; // Ó Ö Ő
        case 0xFA: case 0xFC: case 0x171:             return 'u'; // ú ü ű
        case 0xDA: case 0xDC: case 0x170:             return 'U'; // Ú Ü Ű
    }

    return c;
}


//Retorna 1 si el caracter tiene equivalente en el Spectrum, y rellena la tecla base y los shifts
int hungarian_keyboard_char_to_spectrum(unsigned int codepoint,z80_byte *tecla_base,int *necesita_caps,int *necesita_symbol)
{
    unsigned int c=hungarian_keyboard_quitar_acento(codepoint);

    *necesita_caps=0;
    *necesita_symbol=0;

    if (c>='a' && c<='z') {
        *tecla_base=c;
        return 1;
    }

    if (c>='A' && c<='Z') {
        *tecla_base=c+('a'-'A');
        *necesita_caps=1;
        return 1;
    }

    if (c>='0' && c<='9') {
        *tecla_base=c;
        return 1;
    }

    int i;
    for (i=0;hungarian_keyboard_simbolos[i].caracter;i++) {
        if (hungarian_keyboard_simbolos[i].caracter==c) {
            *tecla_base=hungarian_keyboard_simbolos[i].tecla_base;
            *necesita_symbol=1;
            return 1;
        }
    }

    return 0;
}


//Machine families supported by the Windows keyboard layout mode
#define HK_FAMILIA_NINGUNA  0
#define HK_FAMILIA_SPECTRUM 1
#define HK_FAMILIA_ZX81     2
#define HK_FAMILIA_ZX80     3
#define HK_FAMILIA_QL       4

static int hungarian_keyboard_familia(void)
{
    if (MACHINE_IS_SPECTRUM) return HK_FAMILIA_SPECTRUM;
    if (MACHINE_IS_ZX81_TYPE) return HK_FAMILIA_ZX81;
    if (MACHINE_IS_ZX80_TYPE) return HK_FAMILIA_ZX80;
    if (MACHINE_IS_QL) return HK_FAMILIA_QL;
    return HK_FAMILIA_NINGUNA;
}

//If the Windows keyboard layout mode can be used on the current machine
int hungarian_keyboard_machine_supported(void)
{
    return (hungarian_keyboard_familia()!=HK_FAMILIA_NINGUNA);
}

//ZX80/ZX81: no lower case letters, and SHIFT+letter gives keywords. So letters are always sent without SHIFT
static int hungarian_keyboard_char_to_zx8081(unsigned int codepoint,const struct s_hungarian_keyboard_simbolo *tabla,
    z80_byte *tecla_base,int *necesita_shift)
{
    unsigned int c=hungarian_keyboard_quitar_acento(codepoint);

    *necesita_shift=0;

    if (c>='a' && c<='z') { *tecla_base=c; return 1; }
    if (c>='A' && c<='Z') { *tecla_base=c+('a'-'A'); return 1; }
    if (c>='0' && c<='9') { *tecla_base=c; return 1; }

    int i;
    for (i=0;hungarian_keyboard_simbolos_zx8081_sin_shift[i].caracter;i++) {
        if (hungarian_keyboard_simbolos_zx8081_sin_shift[i].caracter==c) {
            *tecla_base=hungarian_keyboard_simbolos_zx8081_sin_shift[i].tecla_base;
            return 1;
        }
    }

    for (i=0;tabla[i].caracter;i++) {
        if (tabla[i].caracter==c) {
            *tecla_base=tabla[i].tecla_base;
            *necesita_shift=1;
            return 1;
        }
    }

    return 0;
}

static int hungarian_keyboard_char_to_ql(unsigned int codepoint,z80_byte *tecla_base,int *necesita_shift,int *necesita_ctrl)
{
    *necesita_shift=0;
    *necesita_ctrl=0;

    int i;
    //First the table: it has the accented letters that the QL has
    for (i=0;hungarian_keyboard_simbolos_ql[i].caracter;i++) {
        if (hungarian_keyboard_simbolos_ql[i].caracter==codepoint) {
            *tecla_base=hungarian_keyboard_simbolos_ql[i].tecla_base;
            *necesita_shift=hungarian_keyboard_simbolos_ql[i].shift;
            *necesita_ctrl=hungarian_keyboard_simbolos_ql[i].ctrl;
            return 1;
        }
    }

    unsigned int c=hungarian_keyboard_quitar_acento(codepoint);

    if (c>='a' && c<='z') { *tecla_base=c; return 1; }
    if (c>='A' && c<='Z') { *tecla_base=c+('a'-'A'); *necesita_shift=1; return 1; }
    if (c>='0' && c<='9') { *tecla_base=c; return 1; }

    return 0;
}

//Character to key of the current machine. caps = Caps shift (Spectrum) or SHIFT (ZX80/81, QL),
//symbol = Symbol shift (Spectrum only), ctrl = CTRL (QL only)
static int hungarian_keyboard_char_to_key(unsigned int codepoint,z80_byte *tecla_base,int *necesita_caps,int *necesita_symbol,int *necesita_ctrl)
{
    *necesita_caps=0;
    *necesita_symbol=0;
    *necesita_ctrl=0;

    switch (hungarian_keyboard_familia()) {
        case HK_FAMILIA_SPECTRUM:
            return hungarian_keyboard_char_to_spectrum(codepoint,tecla_base,necesita_caps,necesita_symbol);

        case HK_FAMILIA_ZX81:
            return hungarian_keyboard_char_to_zx8081(codepoint,hungarian_keyboard_simbolos_zx81,tecla_base,necesita_caps);

        case HK_FAMILIA_ZX80:
            return hungarian_keyboard_char_to_zx8081(codepoint,hungarian_keyboard_simbolos_zx80,tecla_base,necesita_caps);

        case HK_FAMILIA_QL:
            return hungarian_keyboard_char_to_ql(codepoint,tecla_base,necesita_caps,necesita_ctrl);
    }

    return 0;
}

//Press or release a key of the current machine (letter, number or symbol key)
static void hungarian_keyboard_pulsar_tecla(z80_byte tecla,int pressrelease)
{
    if ((tecla>='a' && tecla<='z') || (tecla>='0' && tecla<='9')) {
        convert_numeros_letras_puerto_teclado(tecla,pressrelease);
        return;
    }

    if (MACHINE_IS_ZX8081) {
        //puerto_32766: B N M . Space
        z80_byte mascara=0;
        if (tecla=='.') mascara=2;
        else if (tecla==' ') mascara=1;

        if (mascara) {
            if (pressrelease) puerto_32766 &=255-mascara;
            else puerto_32766 |=mascara;
        }
        return;
    }

    if (MACHINE_IS_QL) {
        //Unshifted character of the key: the QL table presses only that key
        ascii_to_keyboard_port_set_clear(tecla,pressrelease);
        return;
    }
}


//Retorna la tecla del modo extendido para el caracter, o 0 si no es un simbolo del modo extendido
static z80_byte hungarian_keyboard_tecla_extendido(unsigned int codepoint)
{
    if (!MACHINE_IS_SPECTRUM) return 0;

    int i;
    for (i=0;hungarian_keyboard_simbolos_extendido[i].caracter;i++) {
        if (hungarian_keyboard_simbolos_extendido[i].caracter==codepoint) return hungarian_keyboard_simbolos_extendido[i].tecla_base;
    }

    return 0;
}


//Decodifica el primer caracter de una cadena UTF-8. Retorna 0 si no es valida o esta vacia
unsigned int hungarian_keyboard_utf8_decode(const char *texto)
{
    const unsigned char *s=(const unsigned char *)texto;

    if (s==NULL || s[0]==0) return 0;

    if (s[0]<0x80) return s[0];

    if ((s[0]&0xE0)==0xC0) {
        if ((s[1]&0xC0)!=0x80) return 0;
        return ((s[0]&0x1F)<<6) | (s[1]&0x3F);
    }

    if ((s[0]&0xF0)==0xE0) {
        if ((s[1]&0xC0)!=0x80 || (s[2]&0xC0)!=0x80) return 0;
        return ((s[0]&0x0F)<<12) | ((s[1]&0x3F)<<6) | (s[2]&0x3F);
    }

    if ((s[0]&0xF8)==0xF0) {
        if ((s[1]&0xC0)!=0x80 || (s[2]&0xC0)!=0x80 || (s[3]&0xC0)!=0x80) return 0;
        return ((s[0]&0x07)<<18) | ((s[1]&0x3F)<<12) | ((s[2]&0x3F)<<6) | (s[3]&0x3F);
    }

    return 0;
}


//Si esta habilitado y aplica a la situacion actual
int hungarian_keyboard_is_active(void)
{
    if (hungarian_keyboard.v==0) return 0;

    if (!hungarian_keyboard_machine_supported()) return 0;

    //Con el menu abierto, las teclas las gestiona el menu como siempre
    if (menu_abierto) return 0;

    //Chloe tiene su propio teclado completo, y en modo raw se leen posiciones fisicas
    if (chloe_keyboard.v) return 0;
    if (sdl_raw_keyboard_read.v) return 0;

    return 1;
}


static int hungarian_keyboard_keyid_valido(int keyid)
{
    return (keyid>=0 && keyid<HUNGARIAN_KEYBOARD_MAX_KEYS);
}


int hungarian_keyboard_key_is_held(int keyid)
{
    if (!hungarian_keyboard_keyid_valido(keyid)) return 0;

    return hungarian_keyboard_teclas[keyid].pulsada;
}


//Caps shift on Spectrum, SHIFT on ZX80/81 (same key position) and on QL
static void hungarian_keyboard_set_caps(int pulsar)
{
    if (MACHINE_IS_QL) {
        if (pulsar) ql_keyboard_table[7] &=255-1;
        else ql_keyboard_table[7] |=1;
        return;
    }

    if (pulsar) puerto_65278 &=255-1;
    else puerto_65278 |=1;
}

//CTRL on QL
static void hungarian_keyboard_set_ctrl(int pulsar)
{
    if (!MACHINE_IS_QL) return;

    if (pulsar) ql_keyboard_table[7] &=255-2;
    else ql_keyboard_table[7] |=2;
}


static void hungarian_keyboard_set_symbol(int pulsar)
{
    //Only the Spectrum has Symbol shift (on ZX80/81 set_symshift would press SHIFT)
    if (!MACHINE_IS_SPECTRUM) return;

    if (pulsar) set_symshift();
    else clear_symshift();
}


//Deja Caps shift y Symbol shift como corresponda: segun la ultima tecla nuestra aun pulsada,
//o si no queda ninguna, segun los modificadores del host (como lo haria la gestion normal)
static void hungarian_keyboard_restaurar_shifts(int host_mods)
{
    int i;
    int ultima=-1;

    hungarian_keyboard_ultimos_host_mods=host_mods;

    //Mientras se envia una secuencia, los shifts los controla la secuencia
    if (hungarian_keyboard_cola_total) return;

    for (i=0;i<HUNGARIAN_KEYBOARD_MAX_KEYS;i++) {
        struct s_hungarian_keyboard_tecla *t=&hungarian_keyboard_teclas[i];
        if (t->pulsada && t->tecla_base) {
            if (ultima<0 || t->orden>hungarian_keyboard_teclas[ultima].orden) ultima=i;
        }
    }

    if (ultima>=0) {
        hungarian_keyboard_set_caps(hungarian_keyboard_teclas[ultima].necesita_caps);
        hungarian_keyboard_set_symbol(hungarian_keyboard_teclas[ultima].necesita_symbol);
        hungarian_keyboard_set_ctrl(hungarian_keyboard_teclas[ultima].necesita_ctrl);
    }
    else {
        hungarian_keyboard_set_caps(host_mods & HUNGARIAN_KEYBOARD_MOD_SHIFT);
        hungarian_keyboard_set_symbol(host_mods & HUNGARIAN_KEYBOARD_MOD_SYMBOL);
        //Host Ctrl is CTRL on QL
        hungarian_keyboard_set_ctrl(host_mods & HUNGARIAN_KEYBOARD_MOD_SYMBOL);
    }
}


static void hungarian_keyboard_cola_agregar(z80_byte tecla_base,int necesita_caps,int necesita_symbol,int necesita_ctrl,int modo_extendido)
{
    if (hungarian_keyboard_cola_total>=HUNGARIAN_KEYBOARD_COLA_MAX) {
        debug_printf(VERBOSE_DEBUG,"Hungarian keyboard: sequence queue full. Key ignored");
        return;
    }

    int indice=(hungarian_keyboard_cola_inicio+hungarian_keyboard_cola_total) % HUNGARIAN_KEYBOARD_COLA_MAX;

    hungarian_keyboard_cola[indice].tecla_base=tecla_base;
    hungarian_keyboard_cola[indice].necesita_caps=necesita_caps;
    hungarian_keyboard_cola[indice].necesita_symbol=necesita_symbol;
    hungarian_keyboard_cola[indice].necesita_ctrl=necesita_ctrl;
    hungarian_keyboard_cola[indice].modo_extendido=modo_extendido;

    if (hungarian_keyboard_cola_total==0) {
        //Empieza una secuencia nueva: soltar lo que tengamos pulsado para que no interfiera
        int i;
        for (i=0;i<HUNGARIAN_KEYBOARD_MAX_KEYS;i++) {
            struct s_hungarian_keyboard_tecla *t=&hungarian_keyboard_teclas[i];
            if (t->pulsada && t->tecla_base) {
                hungarian_keyboard_pulsar_tecla(t->tecla_base,0);
                //Sigue pulsada en el host, pero ya no la mantenemos en el Spectrum
                t->tecla_base=0;
            }
        }

        hungarian_keyboard_cola_paso=(modo_extendido ? 0 : 2);
        hungarian_keyboard_cola_frames=0;
    }

    hungarian_keyboard_cola_total++;
}


//Si se esta enviando una secuencia
int hungarian_keyboard_sequence_running(void)
{
    return (hungarian_keyboard_cola_total ? 1 : 0);
}


//Cambio de modificadores del host (Shift, Ctrl, Alt) mientras se envia una secuencia:
//solo se recuerdan, para aplicarlos al terminar
void hungarian_keyboard_host_mods_changed(int host_mods)
{
    hungarian_keyboard_ultimos_host_mods=host_mods;
}


//Llamado cada 20 ms (50 Hz). Avanza la secuencia en curso
void hungarian_keyboard_timer(void)
{
    if (hungarian_keyboard_cola_total==0) return;

    //Si ya no aplica (menu abierto, opcion desactivada...), descartar todo
    if (!hungarian_keyboard_is_active()) {
        hungarian_keyboard_release_all();
        return;
    }

    struct s_hungarian_keyboard_secuencia *e=&hungarian_keyboard_cola[hungarian_keyboard_cola_inicio];

    //Aplicar el estado de las teclas del paso actual. Se repite en cada frame por si
    //algo externo (por ejemplo soltar Alt en el host) ha cambiado los shifts
    {
        switch (hungarian_keyboard_cola_paso) {
            case 0:
                hungarian_keyboard_set_caps(1);
                hungarian_keyboard_set_symbol(1);
                hungarian_keyboard_set_ctrl(0);
            break;

            case 1:
            case 3:
                hungarian_keyboard_set_caps(0);
                hungarian_keyboard_set_symbol(0);
                hungarian_keyboard_set_ctrl(0);
                if (hungarian_keyboard_cola_paso==3 && hungarian_keyboard_cola_frames==0) hungarian_keyboard_pulsar_tecla(e->tecla_base,0);
            break;

            case 2:
                hungarian_keyboard_set_caps(e->necesita_caps);
                hungarian_keyboard_set_symbol(e->necesita_symbol);
                hungarian_keyboard_set_ctrl(e->necesita_ctrl);
                hungarian_keyboard_pulsar_tecla(e->tecla_base,1);
            break;
        }
    }

    hungarian_keyboard_cola_frames++;
    if (hungarian_keyboard_cola_frames<HUNGARIAN_KEYBOARD_FRAMES_PASO) return;

    //Paso siguiente
    hungarian_keyboard_cola_frames=0;
    hungarian_keyboard_cola_paso++;

    if (hungarian_keyboard_cola_paso<=3) return;

    //Fin de esta entrada
    hungarian_keyboard_cola_inicio=(hungarian_keyboard_cola_inicio+1) % HUNGARIAN_KEYBOARD_COLA_MAX;
    hungarian_keyboard_cola_total--;

    if (hungarian_keyboard_cola_total) {
        hungarian_keyboard_cola_paso=(hungarian_keyboard_cola[hungarian_keyboard_cola_inicio].modo_extendido ? 0 : 2);
    }
    else {
        //Cola vacia: dejar los shifts segun lo que tenga pulsado el host
        hungarian_keyboard_restaurar_shifts(hungarian_keyboard_ultimos_host_mods);
    }
}


//Pulsar una tecla del host que ha generado el caracter codepoint (0 = ninguno, por ejemplo tecla muerta)
//Retorna 1 si se ha gestionado (y por tanto no debe pasar por la gestion normal)
int hungarian_keyboard_press(int keyid,unsigned int codepoint)
{
    if (!hungarian_keyboard_keyid_valido(keyid)) return 0;

    struct s_hungarian_keyboard_tecla *t=&hungarian_keyboard_teclas[keyid];

    //Autorepeticion del host: ya esta pulsada
    if (t->pulsada) return 1;

    z80_byte tecla_base=0;
    int necesita_caps=0;
    int necesita_symbol=0;
    int necesita_ctrl=0;

    //Simbolos del modo extendido: siempre mediante secuencia
    z80_byte tecla_extendido=hungarian_keyboard_tecla_extendido(codepoint);
    if (tecla_extendido) {
        debug_printf(VERBOSE_DEBUG,"Hungarian keyboard: U+%04X -> extended mode + symbol shift + %c",codepoint,tecla_extendido);
        t->pulsada=1;
        t->tecla_base=0;
        t->orden=++hungarian_keyboard_contador_orden;
        hungarian_keyboard_cola_agregar(tecla_extendido,0,1,0,1);
        return 1;
    }

    if (codepoint==0 || !hungarian_keyboard_char_to_key(codepoint,&tecla_base,&necesita_caps,&necesita_symbol,&necesita_ctrl)) {
        //Caracter sin equivalente: se absorbe la tecla para que no genere otra cosa
        debug_printf(VERBOSE_DEBUG,"Hungarian keyboard: character U+%04X has no Spectrum equivalent. Ignored",codepoint);
        tecla_base=0;
    }

    t->pulsada=1;
    t->tecla_base=tecla_base;
    t->necesita_caps=necesita_caps;
    t->necesita_symbol=necesita_symbol;
    t->necesita_ctrl=necesita_ctrl;
    t->orden=++hungarian_keyboard_contador_orden;

    //Si hay una secuencia en curso, esta tecla va a la cola para conservar el orden
    if (tecla_base && hungarian_keyboard_cola_total) {
        hungarian_keyboard_cola_agregar(tecla_base,necesita_caps,necesita_symbol,necesita_ctrl,0);
        t->tecla_base=0;
        return 1;
    }

    if (tecla_base) {
        debug_printf(VERBOSE_DEBUG,"Hungarian keyboard: U+%04X -> key %c caps %d symbol %d",
            codepoint,tecla_base,necesita_caps,necesita_symbol);

        //Los shifts que pide el caracter mandan sobre los modificadores del host
        hungarian_keyboard_set_caps(necesita_caps);
        hungarian_keyboard_set_symbol(necesita_symbol);
        hungarian_keyboard_set_ctrl(necesita_ctrl);
        hungarian_keyboard_pulsar_tecla(tecla_base,1);
    }

    return 1;
}


//Liberar una tecla del host. Retorna 1 si era una tecla gestionada por nosotros
int hungarian_keyboard_release(int keyid,int host_mods)
{
    if (!hungarian_keyboard_keyid_valido(keyid)) return 0;

    struct s_hungarian_keyboard_tecla *t=&hungarian_keyboard_teclas[keyid];

    if (!t->pulsada) return 0;

    t->pulsada=0;

    if (t->tecla_base) {
        hungarian_keyboard_pulsar_tecla(t->tecla_base,0);
        hungarian_keyboard_restaurar_shifts(host_mods);
    }

    t->tecla_base=0;

    return 1;
}


//Liberar todas las teclas (por ejemplo al perder el foco la ventana)
void hungarian_keyboard_release_all(void)
{
    int i;
    int alguna=0;

    //Descartar secuencias pendientes
    if (hungarian_keyboard_cola_total) {
        struct s_hungarian_keyboard_secuencia *e=&hungarian_keyboard_cola[hungarian_keyboard_cola_inicio];
        if (hungarian_keyboard_cola_paso>=2) hungarian_keyboard_pulsar_tecla(e->tecla_base,0);
        hungarian_keyboard_cola_total=0;
        hungarian_keyboard_cola_inicio=0;
        alguna=1;
    }

    for (i=0;i<HUNGARIAN_KEYBOARD_MAX_KEYS;i++) {
        struct s_hungarian_keyboard_tecla *t=&hungarian_keyboard_teclas[i];
        if (t->pulsada) {
            if (t->tecla_base) {
                hungarian_keyboard_pulsar_tecla(t->tecla_base,0);
                alguna=1;
            }
            t->pulsada=0;
            t->tecla_base=0;
        }
    }

    if (alguna) {
        hungarian_keyboard_set_caps(0);
        hungarian_keyboard_set_symbol(0);
        hungarian_keyboard_set_ctrl(0);
    }
}
