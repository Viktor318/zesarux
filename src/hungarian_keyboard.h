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

#ifndef HUNGARIAN_KEYBOARD_H
#define HUNGARIAN_KEYBOARD_H

#include "cpu.h"

//Soporte de teclado hungaro (y en general, cualquier distribucion) para Spectrum:
//el caracter que genera el sistema operativo del host se traduce a la combinacion
//de teclas del Spectrum que produce ese mismo caracter

//Modificadores del host que se pasan al pulsar/liberar
#define HUNGARIAN_KEYBOARD_MOD_SHIFT  1
#define HUNGARIAN_KEYBOARD_MOD_SYMBOL 2

//Numero maximo de identificadores de tecla (scancodes) que se pueden seguir
#define HUNGARIAN_KEYBOARD_MAX_KEYS 1024

extern z80_bit hungarian_keyboard;

//Left Alt + number keys = Caps shift + number (Spectrum)
extern z80_bit keyboard_alt_caps_shift;

extern int hungarian_keyboard_is_active(void);
extern int hungarian_keyboard_key_is_held(int keyid);
extern int hungarian_keyboard_press(int keyid,unsigned int codepoint);
extern int hungarian_keyboard_release(int keyid,int host_mods);
extern void hungarian_keyboard_release_all(void);
extern void hungarian_keyboard_timer(void);
extern int hungarian_keyboard_sequence_running(void);
extern void hungarian_keyboard_host_mods_changed(int host_mods);
extern unsigned int hungarian_keyboard_utf8_decode(const char *texto);
extern int hungarian_keyboard_char_to_spectrum(unsigned int codepoint,z80_byte *tecla_base,int *necesita_caps,int *necesita_symbol);

#endif
