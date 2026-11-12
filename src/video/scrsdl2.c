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


#include <string.h>
#include <stdio.h>
#if defined(__APPLE__)
        #include <SDL.h>
#else
	#include <SDL2/SDL.h>
#endif

#include "scrsdl2.h"
#include "common_sdl2.h"

#include "compileoptions.h"
#include "cpu.h"
#include "screen.h"
#include "charset.h"
#include "debug.h"
#include "zxvision.h"
#include "utils.h"
#include "joystick.h"
#include "cpc.h"
#include "prism.h"
#include "sam.h"
#include "ql.h"
#include "settings.h"
#include "realjoystick.h"
#include "msx.h"
#include "coleco.h"
#include "sg1000.h"
#include "sms.h"
#include "svi.h"
#include "pcw.h"
#include "baseconf.h"
#include "scorpion.h"
#include "hungarian_keyboard.h"
#include "atmturbo.h"
#include "menu_items.h"


SDL_Window *window=NULL;


//Indica que ha habido un evento de resize y queda pendiente redimensionar la ventana
//lo que sucede es que cuando se empieza a redimensionar una ventana, usando el raton,
//al recrear la ventana con el nuevo tamanyo, realmente lo que hace es cambiar el buffer de video interno de la ventana
//con el nuevo tamanyo, pero realmente el tamanyo de la ventana NO lo cambia, sino que queda siempre forzado con el tamanyo
//que estamos indicando con el raton
//Por tanto, lo que hacemos es que, activamos este indicador de redimensionar, y cuando hay un siguiente evento diferente de resize,
//la ventana se redimensiona
int scrsdl_debe_redimensionar=0;

//Ultimo tamano de ventana recibido en un evento de cambio de tamano
static int scrsdl_ventana_ancho_anterior=0;
static int scrsdl_ventana_alto_anterior=0;

unsigned char *scrsdl_pixeles;

SDL_Texture *scrsdl_texture;


SDL_Renderer *renderer;


void scrsdl_update_window_title(void)
{
    SDL_SetWindowTitle(window,get_window_title());
}


/* Ventana principal (fork): se crea una sola vez, maximizada, y su tamano solo lo cambia el usuario
   (arrastrando, maximizando, pantalla completa). Los cambios de maquina, zoom, border, footer, etc
   ya no destruyen la ventana: se recalcula la distribucion (maquina + ZX Desktop que llena el resto)
   y se reasignan la textura y los buffers. La escala del interfaz (menus, iconos, texto) es siempre la misma */

//Escala de la imagen de la maquina (ver screen_machine_scale) y tamano de la maquina sin ampliar,
//guardados al distribuir la ventana
static int scrsdl_escala=4;
static int scrsdl_maquina_ancho=0;
static int scrsdl_maquina_alto=0;

//Tamano de la imagen (textura) actual
static int scrsdl_imagen_ancho=0;
static int scrsdl_imagen_alto=0;

//Posicion de la imagen en la ventana (centrada en pantalla completa sin ZX Desktop)
static int scrsdl_imagen_x=0;
static int scrsdl_imagen_y=0;

//Tamano con el que se muestra la imagen en la ventana. Igual que el de la imagen, menos en pantalla completa
//sin ZX Desktop, donde se reduce (interpolacion lineal) para llenar la pantalla
static int scrsdl_destino_ancho=0;
static int scrsdl_destino_alto=0;

//Si la pantalla emulada estaba en una ventana (Emulated Display) al distribuir la ventana: cuando cambia,
//hay que volver a distribuirla, porque entonces la zona de la maquina no se amplia
static int scrsdl_pantalla_en_ventana=0;

static void scrsdl_libera_imagen(void)
{
    if (scrsdl_texture!=NULL) {
        SDL_DestroyTexture(scrsdl_texture);
        scrsdl_texture=NULL;
    }
    if (scrsdl_pixeles!=NULL) {
        free(scrsdl_pixeles);
        scrsdl_pixeles=NULL;
    }
}

//Distribuir la ventana segun su tamano actual y crear la imagen. Retorna 0 si ok
static int scrsdl_ajusta_a_ventana(void)
{
    int ancho_ventana,alto_ventana;

    SDL_GetWindowSize(window,&ancho_ventana,&alto_ventana);

    //Tamano con el que se ha distribuido (para detectar cambios despues)
    scrsdl_ventana_ancho_anterior=ancho_ventana;
    scrsdl_ventana_alto_anterior=alto_ventana;

    scrsdl_pantalla_en_ventana=ventana_video_output_esta_abierta();

    if (!scr_sdl_force_size.v) screen_layout_fit_window(ancho_ventana,alto_ventana);
    else screen_layout_fixed_gui_zoom();

    int ancho=screen_get_window_size_width_zoom_border_en();
    ancho +=screen_get_ext_desktop_width_zoom();

    int alto=screen_get_window_size_height_zoom_border_en();
    alto +=screen_get_ext_desktop_height_zoom();

    if (scr_sdl_force_size.v) {
        ancho=scr_sdl_force_size_width;
        alto=scr_sdl_force_size_height;
    }

    scrsdl_escala=screen_machine_scale;
    scrsdl_maquina_ancho=screen_get_emulated_display_width_zoom_border_en();
    scrsdl_maquina_alto=screen_get_emulated_display_height_zoom_border_en();

    debug_printf (VERBOSE_DEBUG,"Window %d X %d. Picture %d X %d. Machine scale %d/4",ancho_ventana,alto_ventana,ancho,alto,scrsdl_escala);

    scrsdl_libera_imagen();

    //Pantalla completa sin ZX Desktop: reducir la imagen para que llene la pantalla, manteniendo la proporcion
    int llenar_pantalla=(ventana_fullscreen && !screen_ext_desktop_enabled && !scr_sdl_force_size.v);
    int destino_ancho=ancho;
    int destino_alto=alto;

    if (llenar_pantalla && (ancho>ancho_ventana || alto>alto_ventana)) {
        //el menor de los dos factores
        if ((long long)ancho_ventana*alto <= (long long)alto_ventana*ancho) {
            destino_ancho=ancho_ventana;
            destino_alto=(int)(((long long)alto*ancho_ventana+ancho/2)/ancho);
        }
        else {
            destino_alto=alto_ventana;
            destino_ancho=(int)(((long long)ancho*alto_ventana+alto/2)/alto);
        }
    }

    //Interpolacion lineal solo si se reduce la imagen; si no, pixeles tal cual
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,(destino_ancho!=ancho || destino_alto!=alto ? "linear" : "nearest"));

    if (scr_sdl_8bits_color.v) {
        scrsdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STATIC, ancho, alto);
    }

    else {
        scrsdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, ancho, alto);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"nearest");

    scrsdl_pixeles=calloc(ancho * alto,4);

    if (scrsdl_texture==NULL || scrsdl_pixeles==NULL) return 1;

    scrsdl_imagen_ancho=ancho;
    scrsdl_imagen_alto=alto;

    scrsdl_destino_ancho=destino_ancho;
    scrsdl_destino_alto=destino_alto;

    scrsdl_imagen_x=scrsdl_imagen_y=0;
    if (ventana_fullscreen && !screen_ext_desktop_enabled) {
        if (ancho_ventana>destino_ancho) scrsdl_imagen_x=(ancho_ventana-destino_ancho)/2;
        if (alto_ventana>destino_alto) scrsdl_imagen_y=(alto_ventana-destino_alto)/2;
    }

    scr_reallocate_layers_menu(ancho,alto);

    //Que la ventana no se pueda hacer mas pequena que la maquina emulada
    if (!ventana_fullscreen && !scr_sdl_force_size.v) {
        SDL_SetWindowMinimumSize(window,screen_get_window_size_width_zoom_border_en(),screen_get_window_size_height_zoom_border_en());
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    if (mouse_pointer_shown.v==0) SDL_ShowCursor(0);

    clear_putpixel_cache();

    modificado_border.v=1;

    screen_z88_draw_lower_screen();

    menu_init_footer();

    scr_set_pending_redraw_desktop_windows();

    return 0;
}

/* Despues de redistribuir la ventana con otro tamano (arrastrando, maximizando, entrando o saliendo de pantalla completa):
   ventanas, iconos y el Ablakvalto (process switcher) a su sitio. Antes al entrar o salir de pantalla completa no se hacia,
   y por ejemplo el Ablakvalto se quedaba fuera de la ventana, mas abajo (fork) */
static void scrsdl_despues_de_redistribuir(int desktop_ancho_antes,int desktop_alto_antes)
{
    //El ZX Desktop es mas pequeno: mover solo las ventanas que se hayan quedado fuera
    if (zxdesktop_width<desktop_ancho_antes || zxdesktop_height<desktop_alto_antes) {
        zxvision_fit_background_windows_in_screen();
    }

    //Iconos fuera de la ventana o bajo la imagen de la maquina (que puede haber crecido): reordenarlos
    zxvision_reorder_configurable_icons_if_invalid();
    zxvision_check_all_configurable_icons_positions(0);

    zxvision_reorder_configurable_icons_if_auto();

    //Ablakvalto fijado abajo a la derecha: a la nueva esquina, tambien en pantalla completa
    if (setting_process_switcher_force_left_bottom.v) process_switcher_sync_always_left_bottom_setting();

    cls_menu_overlay();

    //Redibujar las ventanas en su sitio nuevo
    scr_set_pending_redraw_desktop_windows();
}

//Guardar la posicion y el tamano de la ventana principal (sin maximizar ni pantalla completa), para el siguiente arranque
static void scrsdl_guarda_geometria_ventana(void)
{
    if (window==NULL || scr_sdl_force_size.v) return;

    Uint32 estado=SDL_GetWindowFlags(window);

    if (estado & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_MINIMIZED)) return;

    screen_main_window_maximized=(estado & SDL_WINDOW_MAXIMIZED ? 1 : 0);

    if (!screen_main_window_maximized) {
        SDL_GetWindowPosition(window,&screen_main_window_x,&screen_main_window_y);
        SDL_GetWindowSize(window,&screen_main_window_width,&screen_main_window_height);
    }

    screen_main_window_known=1;
}

//Si la ventana guardada se veria en alguna pantalla (al menos una parte de su barra de titulo)
static int scrsdl_geometria_guardada_visible(void)
{
    if (!screen_main_window_known || screen_main_window_width<=0 || screen_main_window_height<=0) return 0;

    int pantallas=SDL_GetNumVideoDisplays();
    int i;

    for (i=0;i<pantallas;i++) {
        SDL_Rect zona;
        if (SDL_GetDisplayUsableBounds(i,&zona)!=0) continue;

        //Franja de arriba de la ventana (donde esta la barra de titulo): al menos 100x20 pixeles en la pantalla
        int x0=(screen_main_window_x>zona.x ? screen_main_window_x : zona.x);
        int x1=screen_main_window_x+screen_main_window_width;
        if (x1>zona.x+zona.w) x1=zona.x+zona.w;
        int y0=(screen_main_window_y>zona.y ? screen_main_window_y : zona.y);
        int y1=screen_main_window_y+20;
        if (y1>zona.y+zona.h) y1=zona.y+zona.h;

        if (x1-x0>=100 && y1-y0>=20) return 1;
    }

    return 0;
}

int scrsdl_crea_ventana(void)
{
    //Creada ahora maximizada: su tamano aun puede no ser el definitivo, no guardarlo todavia
    int creada_maximizada=0;

    //La ventana ya existia (pantalla completa activada o desactivada, reapertura del driver)
    int ya_existia=(window!=NULL);
    int desktop_ancho_antes=zxdesktop_width;
    int desktop_alto_antes=zxdesktop_height;

    if (window==NULL) {
        Uint32 flags=SDL_WINDOW_RESIZABLE;

        //Ventana guardada del anterior arranque (fork): misma posicion y tamano, si se ve en alguna pantalla
        int usar_guardada=(!scr_sdl_force_size.v && !screen_main_window_maximized && scrsdl_geometria_guardada_visible());

        //Al empezar, maximizada (menos con un tamano forzado, en pantalla completa o con la ventana guardada)
        if (!ventana_fullscreen && !scr_sdl_force_size.v && !usar_guardada) flags |=SDL_WINDOW_MAXIMIZED;

        screen_layout_fixed_gui_zoom();

        int ancho=screen_get_window_size_width_zoom_border_en();
        ancho +=screen_get_ext_desktop_width_zoom();

        int alto=screen_get_window_size_height_zoom_border_en();
        alto +=screen_get_ext_desktop_height_zoom();

        if (scr_sdl_force_size.v) {
            ancho=scr_sdl_force_size_width;
            alto=scr_sdl_force_size_height;
        }

        //Al empezar maximizada: crearla ya del tamano de la pantalla, para que la primera distribucion
        //(antes de que llegue el aviso de maximizada) sea la definitiva y no se recoloquen iconos
        if (flags & SDL_WINDOW_MAXIMIZED) {
            SDL_Rect disponible;
            if (SDL_GetDisplayUsableBounds(0,&disponible)==0 && disponible.w>ancho && disponible.h>alto) {
                ancho=disponible.w;
                alto=disponible.h;
            }
        }

        int posicion_x=SDL_WINDOWPOS_UNDEFINED;
        int posicion_y=SDL_WINDOWPOS_UNDEFINED;

        if (usar_guardada) {
            ancho=screen_main_window_width;
            alto=screen_main_window_height;
            posicion_x=screen_main_window_x;
            posicion_y=screen_main_window_y;
        }

        debug_printf (VERBOSE_DEBUG,"Creating window %d X %d",ancho,alto );

        window=SDL_CreateWindow("ZEsarUX",posicion_x,posicion_y,ancho,alto,flags);
        if (window==NULL) return 1;

        if (flags & SDL_WINDOW_MAXIMIZED) creada_maximizada=1;

        renderer=SDL_CreateRenderer(window,-1,0);
        if (renderer==NULL) {
            SDL_DestroyWindow(window);
            window=NULL;
            return 1;
        }
    }

    //Pantalla completa: la misma distribucion, en toda la pantalla
    if (SDL_SetWindowFullscreen(window, (ventana_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)) != 0) {
        debug_printf(VERBOSE_ERR,"Can not set fullscreen mode: %s", SDL_GetError());
        ventana_fullscreen=0;
    }

    scrsdl_update_window_title();

    if (!creada_maximizada) scrsdl_guarda_geometria_ventana();

    int retorno=scrsdl_ajusta_a_ventana();

    if (!retorno && ya_existia) scrsdl_despues_de_redistribuir(desktop_ancho_antes,desktop_alto_antes);

    return retorno;

}

void scrsdl_destruye_ventana(void)
{
    if (window!=NULL) SDL_DestroyWindow(window);
    window=NULL;
}

//Putpixel RGB 8 bit
void scrsdl_putpixel_final_rgb8(int x,int y,unsigned int color_rgb)
{
    int ancho=screen_get_window_size_width_zoom_border_en();
    ancho +=screen_get_ext_desktop_width_zoom();

    //controlar limites x,y
    if (scr_sdl_force_size.v) {
        ancho=scr_sdl_force_size_width;
        int alto=scr_sdl_force_size_height;
        if (x>=ancho || y>=alto) return;
    }

    Uint8 *p = (Uint8 *)scrsdl_pixeles + (y * ancho + x);


    //Usamos color rgb 8 bits: 3 bits Red, 3 bits Green, 2 bits Blue : RRRGGGBB
    unsigned char r,g,b;
    r=(color_rgb>>16) & 0xE0; //Mascara 11100000
    g=(color_rgb>>11) & 0x1C; //Mascara 00011100
    b=(color_rgb>> 6) & 0x03; //Mascara 00000011

    *p = (r|g|b);
}

//Putpixel RGB 32 bit
void scrsdl_putpixel_final_rgb(int x,int y,unsigned int color_rgb)
{
    int ancho=screen_get_window_size_width_zoom_border_en();
    ancho +=screen_get_ext_desktop_width_zoom();

    //controlar limites x,y
    if (scr_sdl_force_size.v) {
        ancho=scr_sdl_force_size_width;
        int alto=scr_sdl_force_size_height;
        if (x>=ancho || y>=alto) return;
    }

    Uint8 *p = (Uint8 *)scrsdl_pixeles + (y * ancho + x) * 4;


    if (screen_rgb_8bit) {
        //Usamos color rgb 8 bits: 3 bits Red, 3 bits Green, 2 bits Blue : RRRGGGBB
        //Nos quedamos con los bits mas significativos de cada componente
        color_rgb &=0xE0E0C0; //11100000 11100000 11000000
    }

    //agregar alpha
    color_rgb |=0xFF000000;

    //escribir de golpe los 32 bits
    *(Uint32 *)p = color_rgb;
}

void scrsdl_putpixel_final(int x,int y,unsigned int color)
{

    unsigned int color32=spectrum_colortable[color];

    //y escribir
    scrsdl_putpixel_final_rgb(x,y,color32);


}

static void scrsdl_putpixel_uno(int x,int y,unsigned int color)
{
    if (menu_overlay_activo==0) {
        scrsdl_putpixel_final(x,y,color);
        return;
    }

    buffer_layer_machine[y*ancho_layer_menu_machine+x]=color;
    screen_putpixel_mix_layers(x,y);
}

void scrsdl_putpixel(int x,int y,unsigned int color)
{
    //Pixel de la maquina con la imagen ampliada: ocupa varios pixeles de la ventana
    if (scrsdl_escala!=4 && x<scrsdl_maquina_ancho && y<scrsdl_maquina_alto && x>=0 && y>=0) {
        int x0=x*scrsdl_escala/4;
        int x1=(x+1)*scrsdl_escala/4;
        int y0=y*scrsdl_escala/4;
        int y1=(y+1)*scrsdl_escala/4;
        int xx,yy;
        for (yy=y0;yy<y1;yy++) {
            for (xx=x0;xx<x1;xx++) scrsdl_putpixel_uno(xx,yy,color);
        }
        return;
    }

    if (menu_overlay_activo==0) {
        //Putpixel con menu cerrado
        scrsdl_putpixel_final(x,y,color);
        return;
    }

    //Metemos pixel en layer adecuado
    buffer_layer_machine[y*ancho_layer_menu_machine+x]=color;

    //Putpixel haciendo mix
    screen_putpixel_mix_layers(x,y);

}


void scrsdl_putchar_zx8081(int x,int y, z80_byte caracter)
{
    scr_putchar_zx8081_comun(x,y, caracter);
}

void scrsdl_debug_registers(void)
{

	char buffer[2048];
	print_registers(buffer);

	printf ("%s\r",buffer);
}

void scrsdl_messages_debug(char *s)
{
    printf ("%s\n",s);
    //flush de salida standard. normalmente no hace falta esto, pero si ha finalizado el driver curses, deja la salida que no hace flush
    fflush(stdout);

}

//Rutina de putchar para menu
void scrsdl_putchar_menu(int x,int y, z80_byte caracter,int tinta,int papel)
{

    z80_bit inverse;

    inverse.v=0;

    //128 y 129 corresponden a franja de menu y a letra enye minuscula
    if (caracter<32 || caracter>MAX_CHARSET_GRAPHIC) caracter='?';

    scr_putchar_menu_comun_zoom(caracter,x,y,inverse,tinta,papel,menu_gui_zoom);

}



void scrsdl_putchar_footer(int x,int y, z80_byte caracter,int tinta,int papel)
{
    scr_putchar_footer_comun_zoom(caracter,x,y,tinta,papel);
}


char *scrsdl_get_text_clipboard(int *p_longitud)
{

    char *text = SDL_GetClipboardText();


    if (text) {

        //printf("Texto: [%s]\n",cstr);

        int longitud_texto=strlen(text)+1;

        char *memoria_texto=util_malloc(longitud_texto,"Can not allocate memory for getting text from clipboard");
        strcpy(memoria_texto,text);
        SDL_free(text);

        *p_longitud=longitud_texto;
        return memoria_texto;
    }

    else {
        *p_longitud=0;
        return NULL;
    }

}



void scrsdl_set_fullscreen(void)
{

    ventana_fullscreen=1;
    scrsdl_crea_ventana();

}


void scrsdl_reset_fullscreen(void)
{

    ventana_fullscreen=0;
    scrsdl_crea_ventana();

}


void scrsdl_refresca_pantalla_zx81(void)
{

    scr_refresca_pantalla_y_border_zx8081();

}


void scrsdl_refresca_border(void)
{
    scr_refresca_border();

}


/* Suavizado de la imagen de la maquina (opcion): cuando la escala no da pixeles enteros (1,25x por ejemplo)
   los pixeles de la maquina salen de anchos distintos. Con el suavizado, la zona de la maquina se rehace
   con interpolacion bilineal desde los pixeles de la maquina (al zoom). Se hace sobre una copia, al
   enviar la imagen; los menus y ventanas que haya encima de la maquina no se tocan */

static Uint32 *scrsdl_pixeles_suave=NULL;
static int scrsdl_pixeles_suave_tamanyo=0;

//Para cada pixel de la zona de la maquina: los dos pixeles de origen (en coordenadas de la ventana) y el peso (0-256) del segundo
static int *scrsdl_suave_x0=NULL,*scrsdl_suave_x1=NULL,*scrsdl_suave_wx=NULL;
static int *scrsdl_suave_y0=NULL,*scrsdl_suave_y1=NULL,*scrsdl_suave_wy=NULL;
static int scrsdl_suave_ancho=0,scrsdl_suave_alto=0,scrsdl_suave_escala=0,scrsdl_suave_maquina_ancho=0,scrsdl_suave_maquina_alto=0;

static void scrsdl_suave_tabla(int destino,int origen,int **p0,int **p1,int **pw)
{
    free(*p0); free(*p1); free(*pw);
    *p0=malloc(destino*sizeof(int));
    *p1=malloc(destino*sizeof(int));
    *pw=malloc(destino*sizeof(int));
    if (*p0==NULL || *p1==NULL || *pw==NULL) return;

    int i;
    for (i=0;i<destino;i++) {
        //Centro del pixel de destino en coordenadas de origen, en 1/256
        int f=((2*i+1)*4*256)/(2*scrsdl_escala)-128;
        if (f<0) f=0;
        int o0=f>>8;
        int w=f&255;
        if (o0>=origen-1) { o0=origen-1; w=0; }
        int o1=o0+1;
        if (o1>=origen) o1=origen-1;
        //Pixel de origen o -> esquina de su zona ampliada en la ventana
        (*p0)[i]=o0*scrsdl_escala/4;
        (*p1)[i]=o1*scrsdl_escala/4;
        (*pw)[i]=w;
    }
}

static Uint32 *scrsdl_aplica_suavizado(int ancho,int alto)
{
    if (!screen_machine_smooth || scr_sdl_8bits_color.v || scrsdl_escala==4) return NULL;

    //Pixeles enteros: no hace falta
    if ((zoom_x*scrsdl_escala)%4==0 && (zoom_y*scrsdl_escala)%4==0) return NULL;

    int zona_ancho=scrsdl_maquina_ancho*scrsdl_escala/4;
    int zona_alto=scrsdl_maquina_alto*scrsdl_escala/4;
    if (zona_ancho>ancho) zona_ancho=ancho;
    if (zona_alto>alto) zona_alto=alto;

    if (scrsdl_pixeles_suave_tamanyo!=ancho*alto) {
        free(scrsdl_pixeles_suave);
        scrsdl_pixeles_suave=malloc(ancho*alto*4);
        scrsdl_pixeles_suave_tamanyo=(scrsdl_pixeles_suave!=NULL ? ancho*alto : 0);
        if (scrsdl_pixeles_suave==NULL) return NULL;
    }

    if (scrsdl_suave_ancho!=zona_ancho || scrsdl_suave_alto!=zona_alto || scrsdl_suave_escala!=scrsdl_escala ||
        scrsdl_suave_maquina_ancho!=scrsdl_maquina_ancho || scrsdl_suave_maquina_alto!=scrsdl_maquina_alto) {
        scrsdl_suave_tabla(zona_ancho,scrsdl_maquina_ancho,&scrsdl_suave_x0,&scrsdl_suave_x1,&scrsdl_suave_wx);
        scrsdl_suave_tabla(zona_alto,scrsdl_maquina_alto,&scrsdl_suave_y0,&scrsdl_suave_y1,&scrsdl_suave_wy);
        if (scrsdl_suave_x0==NULL || scrsdl_suave_x1==NULL || scrsdl_suave_wx==NULL ||
            scrsdl_suave_y0==NULL || scrsdl_suave_y1==NULL || scrsdl_suave_wy==NULL) return NULL;
        scrsdl_suave_ancho=zona_ancho;
        scrsdl_suave_alto=zona_alto;
        scrsdl_suave_escala=scrsdl_escala;
        scrsdl_suave_maquina_ancho=scrsdl_maquina_ancho;
        scrsdl_suave_maquina_alto=scrsdl_maquina_alto;
    }

    Uint32 *origen=(Uint32 *)scrsdl_pixeles;
    Uint32 *destino=scrsdl_pixeles_suave;

    memcpy(destino,origen,ancho*alto*4);

    int hay_menu=(menu_overlay_activo && buffer_layer_menu!=NULL && ancho_layer_menu_machine==ancho);

    int x,y;
    for (y=0;y<zona_alto;y++) {
        int wy=scrsdl_suave_wy[y];
        Uint32 *fila0=origen+scrsdl_suave_y0[y]*ancho;
        Uint32 *fila1=origen+scrsdl_suave_y1[y]*ancho;
        for (x=0;x<zona_ancho;x++) {
            int pos=y*ancho+x;

            //Menus y ventanas encima de la maquina: tal cual
            if (hay_menu && buffer_layer_menu[pos]!=SCREEN_LAYER_TRANSPARENT_MENU) continue;

            int wx=scrsdl_suave_wx[x];
            Uint32 a=fila0[scrsdl_suave_x0[x]];
            Uint32 b=fila0[scrsdl_suave_x1[x]];
            Uint32 c=fila1[scrsdl_suave_x0[x]];
            Uint32 d=fila1[scrsdl_suave_x1[x]];

            if (a==b && a==c && a==d) {
                destino[pos]=a;
                continue;
            }

            Uint32 resultado=0xFF000000;
            int desplazamiento;
            for (desplazamiento=0;desplazamiento<24;desplazamiento+=8) {
                int ca=(a>>desplazamiento)&255,cb=(b>>desplazamiento)&255,cc=(c>>desplazamiento)&255,cd=(d>>desplazamiento)&255;
                int arriba=ca*(256-wx)+cb*wx;
                int abajo=cc*(256-wx)+cd*wx;
                int valor=(arriba*(256-wy)+abajo*wy)>>16;
                resultado |=((Uint32)valor)<<desplazamiento;
            }
            destino[pos]=resultado;
        }
    }

    return destino;
}

void scrsdl_refresca_pantalla_solo_driver(void)
{

    int ancho=screen_get_window_size_width_zoom_border_en();
    ancho +=screen_get_ext_desktop_width_zoom();

    //controlar limites x,y
    if (scr_sdl_force_size.v) {
        ancho=scr_sdl_force_size_width;
    }

    if (scr_sdl_8bits_color.v) {
        SDL_UpdateTexture(scrsdl_texture, NULL, scrsdl_pixeles, ancho);
    }
    else {
        Uint32 *suave=NULL;
        if (!scr_sdl_force_size.v && ancho==scrsdl_imagen_ancho) suave=scrsdl_aplica_suavizado(scrsdl_imagen_ancho,scrsdl_imagen_alto);
        SDL_UpdateTexture(scrsdl_texture, NULL, (suave!=NULL ? (void *)suave : (void *)scrsdl_pixeles), ancho * 4);
    }
    SDL_RenderClear(renderer);
    //La imagen a tamano real arriba a la izquierda (sin estirarla): lo que sobra de la ventana queda negro
    SDL_Rect destino={scrsdl_imagen_x,scrsdl_imagen_y,scrsdl_destino_ancho,scrsdl_destino_alto};
    SDL_RenderCopy(renderer, scrsdl_texture, NULL, &destino);
    SDL_RenderPresent(renderer);
}

//Common routine for a graphical driver
void scrsdl_refresca_pantalla(void)
{


    if (sem_screen_refresh_reallocate_layers) {
        //printf ("--Screen layers are being reallocated. return\n");
        //debug_exec_show_backtrace();
        return;
    }

    sem_screen_refresh_reallocate_layers=1;

    scr_driver_redraw_desktop_windows();

    if (MACHINE_IS_ZX8081) {


            scrsdl_refresca_pantalla_zx81();
    }

    else if (MACHINE_IS_PRISM) {
            screen_prism_refresca_pantalla();
    }

    else if (MACHINE_IS_TBBLUE) {
            screen_tbblue_refresca_pantalla();
    }


    else if (MACHINE_IS_SPECTRUM) {

		if (MACHINE_IS_TSCONF)	screen_tsconf_refresca_pantalla();

        else if (MACHINE_IS_BASECONF)	{
            baseconf_refresca_pantalla();
        }

        else if (MACHINE_IS_SCORPION_GMX)	{
            scorpiongmx_refresca_pantalla();
        }

        else if (MACHINE_IS_ATM_TURBO)	{
            atmturbo_refresca_pantalla();
        }


		  else { //Spectrum no TSConf

                //modo clasico. sin rainbow
                if (rainbow_enabled.v==0) {
                        if (border_enabled.v) {
                                //ver si hay que refrescar border
                                if (modificado_border.v)
                                {
                                        scrsdl_refresca_border();
                                        modificado_border.v=0;
                                }

                        }

                        scr_refresca_pantalla_comun();
                }

                else {
                //modo rainbow - real video
                        scr_refresca_pantalla_rainbow_comun_spectrum();
                }
		}
    }

    else if (MACHINE_IS_Z88) {
            screen_z88_refresca_pantalla();
    }


    else if (MACHINE_IS_ACE) {
            scr_refresca_pantalla_y_border_ace();
    }

	else if (MACHINE_IS_CPC) {
                scr_refresca_pantalla_y_border_cpc();
    }

    else if (MACHINE_IS_PCW) {
            scr_refresca_pantalla_y_border_pcw();
    }

    else if (MACHINE_IS_SAM) {
            scr_refresca_pantalla_y_border_sam();
    }

    else if (MACHINE_IS_QL) {
            scr_refresca_pantalla_y_border_ql();
    }

    else if (MACHINE_IS_MK14) {
            scr_refresca_pantalla_y_border_mk14();
    }

	else if (MACHINE_IS_MSX) {
		scr_refresca_pantalla_y_border_msx();
	}

	else if (MACHINE_IS_SVI) {
		scr_refresca_pantalla_y_border_svi();
	}

	else if (MACHINE_IS_COLECO) {
		scr_refresca_pantalla_y_border_coleco();
	}

	else if (MACHINE_IS_SG1000) {
		scr_refresca_pantalla_y_border_sg1000();
	}


	else if (MACHINE_IS_SMS) {
		scr_refresca_pantalla_y_border_sms();
	}


	screen_render_menu_overlay_if_active();


    //Escribir footer
    draw_middle_footer();



    scrsdl_refresca_pantalla_solo_driver();



    sem_screen_refresh_reallocate_layers=0;


}


void scrsdl_end(void)
{
    //Si se vuelve a abrir enseguida (cambio de maquina, border, footer...), la ventana se queda
    if (screen_end_keep_window && window!=NULL) {
        debug_printf (VERBOSE_INFO,"SDL2 video driver: keeping the window to be reused");
        return;
    }

    debug_printf (VERBOSE_INFO,"Closing SDL2 video driver");
    scrsdl_libera_imagen();
    scrsdl_inicializado.v=0;
    scrsdl_destruye_ventana(); //Nota: Cuando se cambia de resolucion en maquina (por ejemplo de spectrum a prism),
    //se llama a _end y luego se vuelve a inicializar el driver
    //en el caso de video sdl, si audio tambien es sdl, al llamar a commonsdl_end no se finaliza driver sdl, dado que esta usando el audio
    //Si se iniciase con otro driver de audio, si se finalizaria el driver
    //Por tanto, hay que destruir explicitamente la ventana en caso de cambios de resolucion y con driver audio sdl
    //Parece que esto solo afecta a windows. En mac y linux, al reinicializar el driver de video, desaparece la ventana anterior
    commonsdl_end();
}

z80_byte scrsdl_lee_puerto(z80_byte puerto_h,z80_byte puerto_l)
{

    //Para evitar warnings al compilar de "unused parameter"
    puerto_h=puerto_l;
    puerto_l=puerto_h;

	return 255;
}


//Teclas de Z88 asociadas a cada tecla del teclado fisico. Lo siguiente en teclado tipo Z88
int scrsdl_keymap_z88_cpc_minus;                //Tecla a la derecha del 0
int scrsdl_keymap_z88_cpc_equal;                //Tecla a la derecha de la anterior
int scrsdl_keymap_z88_cpc_backslash;            //Tecla a la derecha de la anterior (mapeado en teclado spanish en izquierda del 1)
int scrsdl_keymap_z88_cpc_bracket_left;         //Tecla a la derecha de la P
int scrsdl_keymap_z88_cpc_bracket_right;        //Tecla a la derecha de la anterior
int scrsdl_keymap_z88_cpc_semicolon;            //Tecla a la derecha de la L
int scrsdl_keymap_z88_cpc_apostrophe;           //Tecla a la derecha de la anterior
int scrsdl_keymap_z88_cpc_pound;                //Tecla a la derecha de la anterior
int scrsdl_keymap_z88_cpc_comma;                //Tecla a la derecha de la M
int scrsdl_keymap_z88_cpc_period;               //Tecla a la derecha de la anterior
int scrsdl_keymap_z88_cpc_slash;                //Tecla a la derecha de la anterior

int scrsdl_keymap_z88_cpc_circunflejo;          //Solo en CPC. Tecla a la derecha del 0. Misma que scrsdl_keymap_z88_cpc_equal
int scrsdl_keymap_z88_cpc_colon;                //Solo en CPC. Tecla a la derecha de la L. Misma que scrsdl_keymap_z88_cpc_semicolon
int scrsdl_keymap_z88_cpc_arroba;               //Solo en CPC. Tecla a la derecha de la P. Misma que scrsdl_keymap_z88_cpc_bracket_right

int scrsdl_keymap_z88_cpc_leftz;                //Tecla a la izquierda de la Z

//valores para estas teclas que vienen con compose, valores que me invento
//#define SDLK_LEFTBRACKET (16384+34)
//#define SDLK_TECLA_QUOTE (16384+48)


void scrsdl_z88_cpc_load_keymap(void)
{
    debug_printf (VERBOSE_INFO,"Loading keymap");

    #define SDLK_TECLA_SEMICOLON 241
    #define SDLK_TECLA_BACKSLASH 186
    #define SDLK_TECLA_EQUAL 161
    #define SDLK_TECLA_POUND 231

    //valor inventado, tecla a la derecha de la enye:
    #define SDLK_TECLA_INVENTADA_D_ENYE 16384+39

        //Teclas se ubican en misma disposicion fisica del Z88, excepto:
        //libra~ -> spanish: cedilla (misma ubicacion fisica del z88). english: acento grave (supuestamente a la izquierda del 1)
        //backslash: en english esta en fila inferior del z88. en spanish, lo ubicamos a la izquierda del 1 (ºª\)
        switch (z88_cpc_keymap_type) {

                case 1:
			if (MACHINE_IS_Z88 || MACHINE_IS_SAM || MACHINE_IS_QL || MACHINE_IS_MSX || MACHINE_IS_SVI || MACHINE_IS_PCW) {
	                        scrsdl_keymap_z88_cpc_minus=SDLK_QUOTE;
        	                scrsdl_keymap_z88_cpc_equal=SDLK_TECLA_EQUAL;
                	        scrsdl_keymap_z88_cpc_backslash=SDLK_TECLA_BACKSLASH;

	                        scrsdl_keymap_z88_cpc_bracket_left=SDLK_LEFTBRACKET;
        	                scrsdl_keymap_z88_cpc_bracket_right=SDLK_PLUS;
                	        scrsdl_keymap_z88_cpc_semicolon=SDLK_TECLA_SEMICOLON;
	                        scrsdl_keymap_z88_cpc_apostrophe=SDLK_TECLA_INVENTADA_D_ENYE;
        	                scrsdl_keymap_z88_cpc_pound=SDLK_TECLA_POUND;
                	        scrsdl_keymap_z88_cpc_comma=SDLK_COMMA;
                        	scrsdl_keymap_z88_cpc_period=SDLK_PERIOD;
	                        scrsdl_keymap_z88_cpc_slash=SDLK_MINUS;
	                        scrsdl_keymap_z88_cpc_leftz=SDLK_LESS;
			}

			else if (MACHINE_IS_CPC) {
				scrsdl_keymap_z88_cpc_minus=SDLK_QUOTE;
	                        scrsdl_keymap_z88_cpc_circunflejo=SDLK_TECLA_EQUAL;

        	                scrsdl_keymap_z88_cpc_arroba=SDLK_LEFTBRACKET;
                	        scrsdl_keymap_z88_cpc_bracket_left=SDLK_PLUS;
	                        scrsdl_keymap_z88_cpc_colon=SDLK_TECLA_SEMICOLON;
        	                scrsdl_keymap_z88_cpc_semicolon=SDLK_TECLA_INVENTADA_D_ENYE;
                	        scrsdl_keymap_z88_cpc_bracket_right=SDLK_TECLA_POUND;
                        	scrsdl_keymap_z88_cpc_comma=SDLK_COMMA;
	                        scrsdl_keymap_z88_cpc_period=SDLK_PERIOD;
        	                scrsdl_keymap_z88_cpc_slash=SDLK_MINUS;

                	        scrsdl_keymap_z88_cpc_backslash=SDLK_TECLA_BACKSLASH;
	                        scrsdl_keymap_z88_cpc_leftz=SDLK_LESS;

			}

			else if (MACHINE_IS_SPECTRUM && chloe_keyboard.v) {
                                scrsdl_keymap_z88_cpc_minus=SDLK_QUOTE;
                                scrsdl_keymap_z88_cpc_equal=SDLK_TECLA_EQUAL;
                                scrsdl_keymap_z88_cpc_backslash=SDLK_TECLA_BACKSLASH;

                                scrsdl_keymap_z88_cpc_bracket_left=SDLK_LEFTBRACKET;
                                scrsdl_keymap_z88_cpc_bracket_right=SDLK_PLUS;
                                scrsdl_keymap_z88_cpc_semicolon=SDLK_TECLA_SEMICOLON;
                                scrsdl_keymap_z88_cpc_apostrophe=SDLK_TECLA_INVENTADA_D_ENYE;
                                scrsdl_keymap_z88_cpc_pound=SDLK_TECLA_POUND;
                                scrsdl_keymap_z88_cpc_comma=SDLK_COMMA;
                                scrsdl_keymap_z88_cpc_period=SDLK_PERIOD;
                                scrsdl_keymap_z88_cpc_slash=SDLK_MINUS;
	                        scrsdl_keymap_z88_cpc_leftz=SDLK_LESS;
                        }

                break;


                default:
                        //0 o default
			if (MACHINE_IS_Z88 || MACHINE_IS_SAM || MACHINE_IS_QL || MACHINE_IS_MSX || MACHINE_IS_SVI || MACHINE_IS_PCW) {
	                        scrsdl_keymap_z88_cpc_minus=SDLK_MINUS;
        	                scrsdl_keymap_z88_cpc_equal=SDLK_EQUALS;
                	        scrsdl_keymap_z88_cpc_backslash=SDLK_BACKSLASH;

                        	scrsdl_keymap_z88_cpc_bracket_left=SDLK_LEFTBRACKET;
	                        scrsdl_keymap_z88_cpc_bracket_right=SDLK_RIGHTBRACKET;
        	                scrsdl_keymap_z88_cpc_semicolon=SDLK_SEMICOLON;
                	        scrsdl_keymap_z88_cpc_apostrophe=SDLK_TECLA_INVENTADA_D_ENYE;
                        	scrsdl_keymap_z88_cpc_pound=SDLK_BACKQUOTE;
	                        scrsdl_keymap_z88_cpc_comma=SDLK_COMMA;
        	                scrsdl_keymap_z88_cpc_period=SDLK_PERIOD;
                	        scrsdl_keymap_z88_cpc_slash=SDLK_SLASH;
	                        scrsdl_keymap_z88_cpc_leftz=SDLK_LESS;
			}

			else if (MACHINE_IS_CPC) {
				scrsdl_keymap_z88_cpc_minus=SDLK_MINUS;
                                scrsdl_keymap_z88_cpc_circunflejo=SDLK_EQUALS;

                                scrsdl_keymap_z88_cpc_arroba=SDLK_LEFTBRACKET;
                                scrsdl_keymap_z88_cpc_bracket_left=SDLK_RIGHTBRACKET;
                                scrsdl_keymap_z88_cpc_colon=SDLK_SEMICOLON;
                                scrsdl_keymap_z88_cpc_semicolon=SDLK_TECLA_INVENTADA_D_ENYE;
                                scrsdl_keymap_z88_cpc_bracket_right=SDLK_BACKQUOTE;
                                scrsdl_keymap_z88_cpc_comma=SDLK_COMMA;
                                scrsdl_keymap_z88_cpc_period=SDLK_PERIOD;
                                scrsdl_keymap_z88_cpc_slash=SDLK_SLASH;

                                scrsdl_keymap_z88_cpc_backslash=SDLK_BACKSLASH;
	                        scrsdl_keymap_z88_cpc_leftz=SDLK_LESS;
			}

			else if (MACHINE_IS_SPECTRUM && chloe_keyboard.v) {
                                scrsdl_keymap_z88_cpc_minus=SDLK_MINUS;
                                scrsdl_keymap_z88_cpc_equal=SDLK_EQUALS;
                                scrsdl_keymap_z88_cpc_backslash=SDLK_BACKSLASH;

                                scrsdl_keymap_z88_cpc_bracket_left=SDLK_LEFTBRACKET;
                                scrsdl_keymap_z88_cpc_bracket_right=SDLK_RIGHTBRACKET;
                                scrsdl_keymap_z88_cpc_semicolon=SDLK_SEMICOLON;
                                scrsdl_keymap_z88_cpc_apostrophe=SDLK_TECLA_INVENTADA_D_ENYE;
                                scrsdl_keymap_z88_cpc_pound=SDLK_BACKQUOTE;
                                scrsdl_keymap_z88_cpc_comma=SDLK_COMMA;
                                scrsdl_keymap_z88_cpc_period=SDLK_PERIOD;
                                scrsdl_keymap_z88_cpc_slash=SDLK_SLASH;
	                        scrsdl_keymap_z88_cpc_leftz=SDLK_LESS;
                        }

                break;
        }
}


void scrsdl_deal_raw_keys(int pressrelease,int scancode)
{

	//printf ("scrsdl_deal_raw_keys: scancode: %d pressrelease: %d\n",scancode,pressrelease);

	switch (scancode) {
		case SDL_SCANCODE_0:
			util_set_reset_key('0',pressrelease);
		break;

		case SDL_SCANCODE_1:
			util_set_reset_key('1',pressrelease);
		break;

		case SDL_SCANCODE_2:
			util_set_reset_key('2',pressrelease);
		break;

		case SDL_SCANCODE_3:
			util_set_reset_key('3',pressrelease);
		break;

		case SDL_SCANCODE_4:
			util_set_reset_key('4',pressrelease);
		break;

		case SDL_SCANCODE_5:
			util_set_reset_key('5',pressrelease);
		break;

		case SDL_SCANCODE_6:
			util_set_reset_key('6',pressrelease);
		break;

		case SDL_SCANCODE_7:
			util_set_reset_key('7',pressrelease);
		break;

		case SDL_SCANCODE_8:
			util_set_reset_key('8',pressrelease);
		break;

		case SDL_SCANCODE_9:
			util_set_reset_key('9',pressrelease);
		break;

        case SDL_SCANCODE_Q:
                util_set_reset_key('q',pressrelease);
        break;

        case SDL_SCANCODE_W:
                util_set_reset_key('w',pressrelease);
        break;

        case SDL_SCANCODE_E:
                util_set_reset_key('e',pressrelease);
        break;

        case SDL_SCANCODE_R:
                util_set_reset_key('r',pressrelease);
        break;

        case SDL_SCANCODE_T:
                util_set_reset_key('t',pressrelease);
        break;

        case SDL_SCANCODE_Y:
                util_set_reset_key('y',pressrelease);
        break;

        case SDL_SCANCODE_U:
                util_set_reset_key('u',pressrelease);
        break;

        case SDL_SCANCODE_I:
                util_set_reset_key('i',pressrelease);
        break;

        case SDL_SCANCODE_O:
                util_set_reset_key('o',pressrelease);
        break;

        case SDL_SCANCODE_P:
                util_set_reset_key('p',pressrelease);
        break;

        case SDL_SCANCODE_A:
                util_set_reset_key('a',pressrelease);
        break;

        case SDL_SCANCODE_S:
                util_set_reset_key('s',pressrelease);
        break;

        case SDL_SCANCODE_D:
                util_set_reset_key('d',pressrelease);
        break;

        case SDL_SCANCODE_F:
                util_set_reset_key('f',pressrelease);
        break;

        case SDL_SCANCODE_G:
                util_set_reset_key('g',pressrelease);
        break;

        case SDL_SCANCODE_H:
                util_set_reset_key('h',pressrelease);
        break;

        case SDL_SCANCODE_J:
                util_set_reset_key('j',pressrelease);
        break;

        case SDL_SCANCODE_K:
                util_set_reset_key('k',pressrelease);
        break;

        case SDL_SCANCODE_L:
                util_set_reset_key('l',pressrelease);
        break;

        case SDL_SCANCODE_Z:
                util_set_reset_key('z',pressrelease);
        break;

        case SDL_SCANCODE_X:
                util_set_reset_key('x',pressrelease);
        break;

        case SDL_SCANCODE_C:
                util_set_reset_key('c',pressrelease);
        break;

        case SDL_SCANCODE_V:
                util_set_reset_key('v',pressrelease);
        break;

        case SDL_SCANCODE_B:
                util_set_reset_key('b',pressrelease);
        break;

        case SDL_SCANCODE_N:
                util_set_reset_key('n',pressrelease);
        break;

        case SDL_SCANCODE_M:
                util_set_reset_key('m',pressrelease);
        break;

        case SDL_SCANCODE_COMMA:
            util_set_reset_key(',',pressrelease);
        break;

		case SDL_SCANCODE_PERIOD:
			util_set_reset_key('.',pressrelease);
        break;

		case SDL_SCANCODE_LSHIFT:
			util_set_reset_key(UTIL_KEY_SHIFT_L,pressrelease);
		break;

        case SDL_SCANCODE_LCTRL:
			util_set_reset_key(UTIL_KEY_CONTROL_L,pressrelease);
		break;

        case SDL_SCANCODE_ESCAPE:
                util_set_reset_key(UTIL_KEY_ESC,pressrelease);
        break;

        case SDL_SCANCODE_RETURN:
                util_set_reset_key(UTIL_KEY_ENTER,pressrelease);
        break;

        case SDL_SCANCODE_SPACE:
                util_set_reset_key(UTIL_KEY_SPACE,pressrelease);
        break;

        case SDL_SCANCODE_LEFT:
                util_set_reset_key(UTIL_KEY_LEFT,pressrelease);
        break;

        case SDL_SCANCODE_RIGHT:
                util_set_reset_key(UTIL_KEY_RIGHT,pressrelease);
        break;

        case SDL_SCANCODE_DOWN:
                util_set_reset_key(UTIL_KEY_DOWN,pressrelease);
        break;

        case SDL_SCANCODE_UP:
            util_set_reset_key(UTIL_KEY_UP,pressrelease);
        break;

        //F1 pulsado
        case SDL_SCANCODE_F1:
                util_set_reset_key(UTIL_KEY_F1,pressrelease);
        break;

        //F2 pulsado
        case SDL_SCANCODE_F2:
                util_set_reset_key(UTIL_KEY_F2,pressrelease);
        break;

        //F3 pulsado
        case SDL_SCANCODE_F3:
                util_set_reset_key(UTIL_KEY_F3,pressrelease);
        break;

        //F4 pulsado
        case SDL_SCANCODE_F4:
                util_set_reset_key(UTIL_KEY_F4,pressrelease);
        break;

        //F5 pulsado
        case SDL_SCANCODE_F5:
                util_set_reset_key(UTIL_KEY_F5,pressrelease);
        break;

        //F6 pulsado
        case SDL_SCANCODE_F6:
                util_set_reset_key(UTIL_KEY_F6,pressrelease);
        break;

        //F7 pulsado
        case SDL_SCANCODE_F7:
                util_set_reset_key(UTIL_KEY_F7,pressrelease);
        break;


        //F8 pulsado. osdkeyboard
        case SDL_SCANCODE_F8:
                util_set_reset_key(UTIL_KEY_F8,pressrelease);
        break;

        //F9 pulsado. quickload
        case SDL_SCANCODE_F9:
                util_set_reset_key(UTIL_KEY_F9,pressrelease);
        break;


        //F10 pulsado
        case SDL_SCANCODE_F10:
                util_set_reset_key(UTIL_KEY_F10,pressrelease);
        break;


        //F11 pulsado
        case SDL_SCANCODE_F11:
                util_set_reset_key(UTIL_KEY_F11,pressrelease);
        break;

        //F12 pulsado
        case SDL_SCANCODE_F12:
                util_set_reset_key(UTIL_KEY_F12,pressrelease);
        break;

        //F13 pulsado
        case SDL_SCANCODE_F13:
                util_set_reset_key(UTIL_KEY_F13,pressrelease);
        break;

        //F14 pulsado
        case SDL_SCANCODE_F14:
                util_set_reset_key(UTIL_KEY_F14,pressrelease);
        break;

        //F15 pulsado
        case SDL_SCANCODE_F15:
                util_set_reset_key(UTIL_KEY_F15,pressrelease);
        break;

        case SDL_SCANCODE_DELETE:
                util_set_reset_key(UTIL_KEY_DEL,pressrelease);
        break;

        case SDL_SCANCODE_BACKSPACE:
                util_set_reset_key(UTIL_KEY_BACKSPACE,pressrelease);
        break;

        case SDL_SCANCODE_LEFTBRACKET:
                util_set_reset_key('[',pressrelease);
        break;

        case SDL_SCANCODE_RIGHTBRACKET:
                util_set_reset_key(']',pressrelease);
        break;

        case SDL_SCANCODE_MINUS:
                util_set_reset_key('-',pressrelease);
        break;

        case SDL_SCANCODE_EQUALS:
                util_set_reset_key('=',pressrelease);
        break;

        case SDL_SCANCODE_SEMICOLON:
                util_set_reset_key(';',pressrelease);
        break;

        case SDL_SCANCODE_APOSTROPHE:
                util_set_reset_key('\'',pressrelease);
        break;

        case SDL_SCANCODE_SLASH:
                util_set_reset_key('/',pressrelease);
        break;

        //En caso raw, enviamos cursore del keypad igual que cursores normales
        case SDL_SCANCODE_KP_4:
                util_set_reset_key(UTIL_KEY_LEFT,pressrelease);
        break;

        case SDL_SCANCODE_KP_6:
                util_set_reset_key(UTIL_KEY_RIGHT,pressrelease);
        break;

        case SDL_SCANCODE_KP_2:
                util_set_reset_key(UTIL_KEY_DOWN,pressrelease);
        break;

        case SDL_SCANCODE_KP_8:
                util_set_reset_key(UTIL_KEY_UP,pressrelease);
        break;

	}
}


void scrsdl_deal_keys(int pressrelease,int tecla)
{

	//printf ("scrsdl_deal_keys tecla: %d 0x%X pressrelease: %d\n",tecla,tecla,pressrelease);
	//printf ("tecla: 0x%X\n",tecla);

    //Teclas que necesitan conversion de teclado para Chloe
    int tecla_gestionada_chloe=0;
    if (MACHINE_IS_SPECTRUM && chloe_keyboard.v) {
        tecla_gestionada_chloe=1;

        if (tecla==scrsdl_keymap_z88_cpc_minus) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_MINUS,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_equal) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_EQUAL,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_backslash) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_BACKSLASH,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_bracket_left) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_BRACKET_LEFT,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_bracket_right) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_BRACKET_RIGHT,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_semicolon) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_SEMICOLON,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_apostrophe) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_APOSTROPHE,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_pound) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_POUND,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_comma) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_COMMA,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_period) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_PERIOD,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_slash) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_SLASH,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_leftz) util_set_reset_key_chloe_keymap(UTIL_KEY_CHLOE_LEFTZ,pressrelease);

        else tecla_gestionada_chloe=0;
    }


    if (tecla_gestionada_chloe) return;



    int tecla_gestionada_sam_ql=0;
    if (MACHINE_IS_SAM || MACHINE_IS_QL || MACHINE_IS_MSX || MACHINE_IS_SVI || MACHINE_IS_PCW) {
        tecla_gestionada_sam_ql=1;

        if (tecla==scrsdl_keymap_z88_cpc_minus) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_MINUS,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_equal) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_EQUAL,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_backslash) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_BACKSLASH,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_bracket_left) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_BRACKET_LEFT,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_bracket_right) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_BRACKET_RIGHT,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_semicolon) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_SEMICOLON,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_apostrophe) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_APOSTROPHE,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_pound) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_POUND,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_comma) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_COMMA,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_period) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_PERIOD,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_slash) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_SLASH,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_leftz) util_set_reset_key_common_keymap(UTIL_KEY_COMMON_KEYMAP_LEFTZ,pressrelease);


        else tecla_gestionada_sam_ql=0;
    }

    if (tecla_gestionada_sam_ql) return;



    switch (tecla) {

        case 32:
            util_set_reset_key(UTIL_KEY_SPACE,pressrelease);
        break;

        case SDLK_RETURN:
            util_set_reset_key(UTIL_KEY_ENTER,pressrelease);
        break;

        case SDLK_LSHIFT:
            joystick_possible_leftshift_key(pressrelease);
        break;

        case SDLK_RSHIFT:
            joystick_possible_rightshift_key(pressrelease);
        break;

        case SDLK_LALT:
            joystick_possible_leftalt_key(pressrelease);
        break;
        case SDLK_RALT:
            joystick_possible_rightalt_key(pressrelease);
        break;


        case SDLK_LCTRL:
            joystick_possible_leftctrl_key(pressrelease);
        break;

        case SDLK_RCTRL:
            joystick_possible_rightctrl_key(pressrelease);
        break;

        case SDLK_LGUI:
            util_set_reset_key(UTIL_KEY_WINKEY_L,pressrelease);
        break;

        case SDLK_RGUI:
            util_set_reset_key(UTIL_KEY_WINKEY_R,pressrelease);
        break;

        case SDLK_DELETE:
            util_set_reset_key(UTIL_KEY_DEL,pressrelease);
        break;

        //Teclas que generan doble pulsacion
        case SDLK_BACKSPACE:
            util_set_reset_key(UTIL_KEY_BACKSPACE,pressrelease);
        break;

        case SDLK_HOME:
            joystick_possible_home_key(pressrelease);
        break;

        case SDLK_END:
            util_set_reset_key(UTIL_KEY_END,pressrelease);
        break;

        case SDLK_LEFT:
            util_set_reset_key(UTIL_KEY_LEFT,pressrelease);
        break;

        case SDLK_RIGHT:
            util_set_reset_key(UTIL_KEY_RIGHT,pressrelease);
        break;

        case SDLK_DOWN:
            util_set_reset_key(UTIL_KEY_DOWN,pressrelease);
        break;

        case SDLK_UP:
            util_set_reset_key(UTIL_KEY_UP,pressrelease);
        break;

        case SDLK_TAB:
            joystick_possible_tab_key(pressrelease);
        break;

        case SDLK_CAPSLOCK:
            util_set_reset_key(UTIL_KEY_CAPS_LOCK,pressrelease);
        break;

        case SDLK_COMMA:
            util_set_reset_key(UTIL_KEY_COMMA,pressrelease);
        break;

        case SDLK_PERIOD:
            util_set_reset_key(UTIL_KEY_PERIOD,pressrelease);
        break;

        case SDLK_MINUS:
            util_set_reset_key(UTIL_KEY_KP_MINUS,pressrelease);
        break;

        case SDLK_PLUS:
            util_set_reset_key(UTIL_KEY_KP_PLUS,pressrelease);
        break;

        case SDLK_SLASH:
            util_set_reset_key(UTIL_KEY_KP_DIVIDE,pressrelease);
        break;

        case SDLK_ASTERISK:
        case SDLK_KP_MULTIPLY:
            util_set_reset_key(UTIL_KEY_KP_MULTIPLY,pressrelease);
        break;

        //Operaciones del teclado numerico, igual que las del teclado principal
        case SDLK_KP_MINUS:
            util_set_reset_key(UTIL_KEY_KP_MINUS,pressrelease);
        break;

        case SDLK_KP_PLUS:
            util_set_reset_key(UTIL_KEY_KP_PLUS,pressrelease);
        break;

        case SDLK_KP_DIVIDE:
            util_set_reset_key(UTIL_KEY_KP_DIVIDE,pressrelease);
        break;

        case SDLK_NUMLOCKCLEAR:
        util_set_reset_key(UTIL_KEY_KP_NUMLOCK,pressrelease);
        break;

        //F1 pulsado
        case SDLK_F1:
            util_set_reset_key(UTIL_KEY_F1,pressrelease);
        break;

        //F2 pulsado
        case SDLK_F2:
            util_set_reset_key(UTIL_KEY_F2,pressrelease);
        break;

        //F3 pulsado
        case SDLK_F3:
            util_set_reset_key(UTIL_KEY_F3,pressrelease);
        break;

        //F4 pulsado
        case SDLK_F4:
            util_set_reset_key(UTIL_KEY_F4,pressrelease);
        break;

        //F5 pulsado
        case SDLK_F5:
            util_set_reset_key(UTIL_KEY_F5,pressrelease);
        break;

        //F6 pulsado
        case SDLK_F6:
            util_set_reset_key(UTIL_KEY_F6,pressrelease);
        break;

        //F7 pulsado
        case SDLK_F7:
            util_set_reset_key(UTIL_KEY_F7,pressrelease);
        break;


        //F8 pulsado. osdkeyboard
        case SDLK_F8:
            util_set_reset_key(UTIL_KEY_F8,pressrelease);
        break;

        //F9 pulsado. quickload
        case SDLK_F9:
            util_set_reset_key(UTIL_KEY_F9,pressrelease);
        break;


        //F10 pulsado
        case SDLK_F10:
            util_set_reset_key(UTIL_KEY_F10,pressrelease);
        break;


        //F11 pulsado
        case SDLK_F11:
                util_set_reset_key(UTIL_KEY_F11,pressrelease);
        break;

        //F12 pulsado
        case SDLK_F12:
                util_set_reset_key(UTIL_KEY_F12,pressrelease);
        break;

        //F13 pulsado
        case SDLK_F13:
                util_set_reset_key(UTIL_KEY_F13,pressrelease);
        break;

        //F14 pulsado
        case SDLK_F14:
                util_set_reset_key(UTIL_KEY_F14,pressrelease);
        break;

        //F15 pulsado
        case SDLK_F15:
                util_set_reset_key(UTIL_KEY_F15,pressrelease);
        break;



        //ESC pulsado
        case SDLK_ESCAPE:
                util_set_reset_key(UTIL_KEY_ESC,pressrelease);
        break;

        //PgUp
        case SDLK_PAGEUP:
                util_set_reset_key(UTIL_KEY_PAGE_UP,pressrelease);
        break;

        //PgDn
        case SDLK_PAGEDOWN:
                util_set_reset_key(UTIL_KEY_PAGE_DOWN,pressrelease);
        break;

        //Teclas del keypad. Con Num Lock apagado, 2 4 6 8 son los cursores
        case SDLK_KP_2:
        case SDLK_KP_4:
        case SDLK_KP_6:
        case SDLK_KP_8:
            if (!(SDL_GetModState() & KMOD_NUM)) {
                if (tecla==SDLK_KP_2) util_set_reset_key(UTIL_KEY_DOWN,pressrelease);
                else if (tecla==SDLK_KP_4) util_set_reset_key(UTIL_KEY_LEFT,pressrelease);
                else if (tecla==SDLK_KP_6) util_set_reset_key(UTIL_KEY_RIGHT,pressrelease);
                else util_set_reset_key(UTIL_KEY_UP,pressrelease);
            }
            else {
                if (tecla==SDLK_KP_2) util_set_reset_key(UTIL_KEY_KP2,pressrelease);
                else if (tecla==SDLK_KP_4) util_set_reset_key(UTIL_KEY_KP4,pressrelease);
                else if (tecla==SDLK_KP_6) util_set_reset_key(UTIL_KEY_KP6,pressrelease);
                else util_set_reset_key(UTIL_KEY_KP8,pressrelease);
            }
        break;

        case SDLK_KP_0:
            util_set_reset_key(UTIL_KEY_KP0,pressrelease);
        break;

        case SDLK_KP_1:
                util_set_reset_key(UTIL_KEY_KP1,pressrelease);
        break;

        case SDLK_KP_3:
                util_set_reset_key(UTIL_KEY_KP3,pressrelease);
        break;

        case SDLK_KP_5:
                util_set_reset_key(UTIL_KEY_KP5,pressrelease);
        break;

        case SDLK_KP_7:
                util_set_reset_key(UTIL_KEY_KP7,pressrelease);
        break;

        case SDLK_KP_9:
                util_set_reset_key(UTIL_KEY_KP9,pressrelease);
        break;

        case SDLK_KP_PERIOD:
                util_set_reset_key(UTIL_KEY_KP_COMMA,pressrelease);
        break;

        case SDLK_KP_ENTER:
                util_set_reset_key(UTIL_KEY_KP_ENTER,pressrelease);
        break;



        default:
            if (tecla<256) {
                //printf ("tecla: %d\n",tecla);
                //convert_numeros_letras_puerto_teclado(tecla,pressrelease);
                util_set_reset_key(tecla,pressrelease);
            }
        break;

    }


    //Fuera del switch
//Teclas que necesitan conversion de teclado para CPC
    if (MACHINE_IS_CPC) {

        if (tecla==scrsdl_keymap_z88_cpc_minus) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_MINUS,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_circunflejo) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_CIRCUNFLEJO,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_arroba) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_ARROBA,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_bracket_left) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_BRACKET_LEFT,pressrelease);



        else if (tecla==scrsdl_keymap_z88_cpc_colon) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_COLON,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_semicolon) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_SEMICOLON,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_bracket_right) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_BRACKET_RIGHT,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_comma) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_COMMA,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_period) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_PERIOD,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_slash) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_SLASH,pressrelease);

        else if (tecla==scrsdl_keymap_z88_cpc_backslash) util_set_reset_key_cpc_keymap(UTIL_KEY_CPC_BACKSLASH,pressrelease);


    }


//Teclas que necesitan conversion de teclado para Z88
    if (!MACHINE_IS_Z88) return;

    if (tecla==scrsdl_keymap_z88_cpc_minus) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_MINUS,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_equal) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_EQUAL,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_backslash) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_BACKSLASH,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_bracket_left) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_BRACKET_LEFT,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_bracket_right) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_BRACKET_RIGHT,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_semicolon) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_SEMICOLON,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_apostrophe) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_APOSTROPHE,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_pound) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_POUND,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_comma) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_COMMA,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_period) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_PERIOD,pressrelease);

    else if (tecla==scrsdl_keymap_z88_cpc_slash) util_set_reset_key_z88_keymap(UTIL_KEY_Z88_SLASH,pressrelease);


}


void scrsdl_resize(int width,int height)
{
    //Mismo tamano: nada que hacer
    if (width==scrsdl_ventana_ancho_anterior && height==scrsdl_ventana_alto_anterior) return;
    scrsdl_ventana_ancho_anterior=width;
    scrsdl_ventana_alto_anterior=height;

    debug_printf (VERBOSE_INFO,"Window resized to %d X %d",width,height);

    int desktop_ancho_antes=zxdesktop_width;
    int desktop_alto_antes=zxdesktop_height;

    //No usar los layers mientras se reasignan
    void (*previous_function)(void);
    int menu_antes;
    previous_function=menu_overlay_function;
    menu_antes=menu_overlay_activo;
    menu_overlay_activo=0;

    scrsdl_ajusta_a_ventana();

    menu_overlay_function=previous_function;
    menu_overlay_activo=menu_antes;
    if (menu_overlay_activo) scr_init_layers_menu();

    scrsdl_despues_de_redistribuir(desktop_ancho_antes,desktop_alto_antes);
}

/* Comprobar si el tamano real de la ventana es otro que con el que se ha distribuido (fork).
   Al salir de pantalla completa (por ejemplo a una ventana maximizada) a veces no llega el aviso
   SDL_WINDOWEVENT_SIZE_CHANGED con el tamano final, y la imagen quedaba cortada por abajo */
static void scrsdl_comprueba_tamano_ventana(void)
{
    if (window==NULL || scr_sdl_force_size.v) return;

    int ancho,alto;
    SDL_GetWindowSize(window,&ancho,&alto);

    if (ancho>0 && alto>0 && (ancho!=scrsdl_ventana_ancho_anterior || alto!=scrsdl_ventana_alto_anterior)) {
        scrsdl_resize(ancho,alto);
    }

    //La pantalla emulada ha pasado a una ventana o ha vuelto a su sitio: volver a distribuir con el mismo tamano
    else if (ancho>0 && alto>0 && ventana_video_output_esta_abierta()!=scrsdl_pantalla_en_ventana) {
        debug_printf (VERBOSE_INFO,"Emulated display %s: redistributing the window",
            (scrsdl_pantalla_en_ventana ? "docked" : "on a window"));
        scrsdl_ventana_ancho_anterior=-1;
        scrsdl_resize(ancho,alto);
    }
}

int scrsdl_recibido_resize(SDL_Event *event)
{

    if (event->type==SDL_WINDOWEVENT) {
        //Cualquier cambio de tamano: arrastrando, maximizando, restaurando, pantalla completa
        if (event->window.event==SDL_WINDOWEVENT_SIZE_CHANGED) return 1;
    }

    return 0;
}


/* Windowsos billentyuzetkiosztas con el menu abierto (fork): las teclas que generan un caracter imprimible
   (letras, numeros, signos, tambien con Shift o AltGr) se pasan a los menus y campos de texto como el caracter
   que genera el sistema, no como la tecla del Spectrum. Enter, Esc, borrar, cursores, tabulador, teclas F,
   y las combinaciones con Ctrl o Alt izquierdo siguen por la gestion normal.
   Retorna 1 si el evento se ha gestionado */
static int scrsdl_hungarian_keyboard_menu_event(SDL_Event *event)
{
    if (event->type!=SDL_KEYDOWN) return 0;

    SDL_Keysym keysym=event->key.keysym;
    int sym=keysym.sym;

    //AltGr solo, en Windows (LCtrl+RAlt) o en Linux
    if (sym==SDLK_RALT || sym==SDLK_MODE) return 1;

    if (sym<=32 || sym==SDLK_DELETE || (sym & SDLK_SCANCODE_MASK)) return 0;

    int altgr=( (keysym.mod & (KMOD_RALT|KMOD_MODE)) ? 1 : 0);
    if (!altgr && (keysym.mod & (KMOD_CTRL|KMOD_LALT))) return 0;

    SDL_Event siguiente;
    if (SDL_PeepEvents(&siguiente,1,SDL_PEEKEVENT,SDL_FIRSTEVENT,SDL_LASTEVENT)==1 && siguiente.type==SDL_TEXTINPUT) {
        SDL_PeepEvents(&siguiente,1,SDL_GETEVENT,SDL_TEXTINPUT,SDL_TEXTINPUT);
        int caracter=hungarian_keyboard_menu_char(hungarian_keyboard_utf8_decode(siguiente.text.text));
        if (caracter) menu_tecleado_agregar(caracter);
        //Sin equivalente: se ignora
        return 1;
    }

    //Sin texto con AltGr: tecla muerta. Se absorbe
    if (altgr) return 1;

    return 0;
}

//Teclado hungaro: gestiona un evento de tecla usando el caracter que genera el sistema operativo
//(evento SDL_TEXTINPUT que llega justo despues del SDL_KEYDOWN).
//Retorna 1 si el evento se ha gestionado y no debe pasar por la gestion normal
static int scrsdl_hungarian_keyboard_event(SDL_Event *event)
{
    SDL_Keysym keysym=event->key.keysym;
    int keyid=keysym.scancode;

    int host_mods=0;
    if (keysym.mod & KMOD_SHIFT) host_mods |=HUNGARIAN_KEYBOARD_MOD_SHIFT;
    if (keysym.mod & KMOD_CTRL) host_mods |=HUNGARIAN_KEYBOARD_MOD_SYMBOL;

    //Mientras se envia una secuencia (simbolos del modo extendido), los modificadores del host
    //no deben tocar Caps/Symbol shift: solo se recuerdan para el final
    int sym_mod=keysym.sym;
    if (hungarian_keyboard_sequence_running() &&
        (sym_mod==SDLK_LSHIFT || sym_mod==SDLK_RSHIFT || sym_mod==SDLK_LCTRL || sym_mod==SDLK_RCTRL ||
         sym_mod==SDLK_LALT || sym_mod==SDLK_RALT || sym_mod==SDLK_MODE)) {
        hungarian_keyboard_host_mods_changed(host_mods);
        return 1;
    }

    //Shift y Ctrl del host mientras hay una tecla nuestra pulsada, o repeticiones de ellos
    //(Windows repite las teclas modificadoras mantenidas): no deben tocar los shifts de la maquina,
    //que los decide el caracter (por ejemplo Shift+punto = dos puntos, sin SHIFT en el CPC).
    //Al soltar nuestra tecla se dejan segun los modificadores del host
    if (hungarian_keyboard_is_active() &&
        (sym_mod==SDLK_LSHIFT || sym_mod==SDLK_RSHIFT || sym_mod==SDLK_LCTRL || sym_mod==SDLK_RCTRL) &&
        (hungarian_keyboard_any_key_held() || (event->type==SDL_KEYDOWN && event->key.repeat))) {
        hungarian_keyboard_host_mods_changed(host_mods);
        return 1;
    }

    //AltGr (Alt derecho): con la distribucion del sistema solo sirve para escribir caracteres
    //(por ejemplo AltGr+Q = barra invertida). No se pasa a la maquina: en el CPC el Alt es la tecla CLR,
    //y pulsarla sola hacia pitar al CPC. Al soltar si se pasa, por si se pulso con la opcion inactiva
    if (event->type==SDL_KEYDOWN && hungarian_keyboard_is_active() && (sym_mod==SDLK_RALT || sym_mod==SDLK_MODE)) {
        return 1;
    }

    //Al soltar, liberar siempre lo que hayamos pulsado nosotros, aunque ya no este activo
    if (event->type==SDL_KEYUP) return hungarian_keyboard_release(keyid,host_mods);

    //Menu abierto (fork): el caracter que genera el sistema va a los menus y campos de texto
    if (hungarian_keyboard_menu_is_active()) {
        return scrsdl_hungarian_keyboard_menu_event(event);
    }

    if (!hungarian_keyboard_is_active()) return 0;

    //Autorepeticion de una tecla nuestra
    if (hungarian_keyboard_key_is_held(keyid)) {
        //Descartar el texto de la repeticion, si viene a continuacion
        SDL_Event siguiente;
        if (SDL_PeepEvents(&siguiente,1,SDL_PEEKEVENT,SDL_FIRSTEVENT,SDL_LASTEVENT)==1 && siguiente.type==SDL_TEXTINPUT) {
            SDL_PeepEvents(&siguiente,1,SDL_GETEVENT,SDL_TEXTINPUT,SDL_TEXTINPUT);
        }
        return 1;
    }

    int sym=keysym.sym;

    //Solo teclas que pueden generar un caracter imprimible. Espacio, enter, borrar, cursores,
    //F1..F12, teclado numerico, etc siguen por la gestion normal
    //En el CPC tambien las operaciones del teclado numerico (el CPC no las tiene en su teclado
    //numerico): se envia el caracter que generan. La tecla decimal sigue siendo la tecla f. del CPC
    //(punto), como en su teclado. Sin texto (Bloq Num desactivado) siguen por la gestion normal
    int teclado_numerico_caracter=(MACHINE_IS_CPC &&
        (sym==SDLK_KP_DIVIDE || sym==SDLK_KP_MULTIPLY || sym==SDLK_KP_MINUS || sym==SDLK_KP_PLUS));

    if (!teclado_numerico_caracter && (sym<=32 || sym==SDLK_DELETE || (sym & SDLK_SCANCODE_MASK))) return 0;

    //AltGr: en Linux suele ser RALT o MODE; en Windows llega como LCtrl+RAlt
    int altgr=( (keysym.mod & (KMOD_RALT|KMOD_MODE)) ? 1 : 0);

    //Ctrl o Alt izquierdo sin AltGr: Ctrl es Symbol shift del Spectrum; Alt izquierdo solo pasa la tecla. No se genera texto
    if (!altgr && (keysym.mod & (KMOD_CTRL|KMOD_LALT))) return 0;

    SDL_Event siguiente;
    if (SDL_PeepEvents(&siguiente,1,SDL_PEEKEVENT,SDL_FIRSTEVENT,SDL_LASTEVENT)==1 && siguiente.type==SDL_TEXTINPUT) {
        SDL_PeepEvents(&siguiente,1,SDL_GETEVENT,SDL_TEXTINPUT,SDL_TEXTINPUT);
        unsigned int codepoint=hungarian_keyboard_utf8_decode(siguiente.text.text);
        return hungarian_keyboard_press(keyid,codepoint);
    }

    //Sin texto con AltGr: tecla muerta (por ejemplo AltGr+1). Se absorbe
    if (altgr) return hungarian_keyboard_press(keyid,0);

    return 0;
}

//Left Alt + number keys = Caps shift + number keys (Spectrum)
//Caps shift is pressed together with the number key and released after the number key is released
static unsigned char scrsdl_alt_caps_keys[SDL_NUM_SCANCODES];
static int scrsdl_alt_caps_total=0;
static int scrsdl_alt_caps_release_pending=0;

static void scrsdl_alt_caps_shift_event(SDL_Event *event)
{
    if (event->type!=SDL_KEYDOWN && event->type!=SDL_KEYUP) return;

    SDL_Keysym keysym=event->key.keysym;
    int scancode=keysym.scancode;
    if (scancode<0 || scancode>=SDL_NUM_SCANCODES) return;

    if (event->type==SDL_KEYDOWN) {
        //Autorepeat
        if (scrsdl_alt_caps_keys[scancode]) return;

        if (!keyboard_alt_caps_shift.v || sdl_raw_keyboard_read.v) return;
        if (!MACHINE_IS_SPECTRUM && !MACHINE_IS_ZX8081) return;

        //Only left Alt, not AltGr
        if (!(keysym.mod & KMOD_LALT) || (keysym.mod & (KMOD_RALT|KMOD_MODE))) return;

        int sym=keysym.sym;

        //Spectrum: number keys. ZX80/81: SHIFT with any key (keywords, functions, symbols)
        if (MACHINE_IS_SPECTRUM) {
            if (sym<'0' || sym>'9') return;
        }
        else {
            if (!((sym>='0' && sym<='9') || (sym>='a' && sym<='z') || sym==SDLK_SPACE || sym==SDLK_PERIOD || sym==SDLK_RETURN)) return;
        }

        scrsdl_alt_caps_keys[scancode]=1;
        scrsdl_alt_caps_total++;
        util_set_reset_key(UTIL_KEY_CAPS_SHIFT,1);
    }

    else {
        if (!scrsdl_alt_caps_keys[scancode]) return;

        scrsdl_alt_caps_keys[scancode]=0;
        scrsdl_alt_caps_total--;
        if (scrsdl_alt_caps_total<=0) {
            scrsdl_alt_caps_total=0;
            scrsdl_alt_caps_release_pending=1;
        }
    }
}

//Release Caps shift once the number key has been released (unless a Shift key is held)
static void scrsdl_alt_caps_shift_release_pending(void)
{
    if (!scrsdl_alt_caps_release_pending) return;
    scrsdl_alt_caps_release_pending=0;

    if (scrsdl_alt_caps_total) return;
    if (SDL_GetModState() & KMOD_SHIFT) return;

    util_set_reset_key(UTIL_KEY_CAPS_SHIFT,0);
}

static void scrsdl_alt_caps_shift_release_all(void)
{
    if (scrsdl_alt_caps_total) {
        int i;
        for (i=0;i<SDL_NUM_SCANCODES;i++) scrsdl_alt_caps_keys[i]=0;
        scrsdl_alt_caps_total=0;
        util_set_reset_key(UTIL_KEY_CAPS_SHIFT,0);
    }
    scrsdl_alt_caps_release_pending=0;
}

void scrsdl_actualiza_tablas_teclado(void)
{


    SDL_Event event;
    int pressrelease;

    //SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    //El teclado hungaro necesita los eventos SDL_TEXTINPUT
    if (hungarian_keyboard.v && !SDL_IsTextInputActive()) SDL_StartTextInput();


    scrsdl_comprueba_tamano_ventana();

    while( SDL_PollEvent( &event ) ){

        //Caps shift of a previous left Alt + number key
        scrsdl_alt_caps_shift_release_pending();

//printf ("evento: %d\n",event.type);

		//Si se ha dejado de redimensionar la ventana
		//if (event.type!=SDL_WINDOWEVENT_SIZE_CHANGED) {

		if (event.type==SDL_WINDOWEVENT) {
			//printf("SDL_WINDOWEVENT. event=%d\n",event.window.event);

            //Movida, cambiada de tamano, maximizada o restaurada: guardarlo para el siguiente arranque
            if (event.window.event==SDL_WINDOWEVENT_MOVED || event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED ||
                event.window.event==SDL_WINDOWEVENT_MAXIMIZED || event.window.event==SDL_WINDOWEVENT_RESTORED) {
                scrsdl_guarda_geometria_ventana();
            }

			if (event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {

                //printf("FocusOut\n");

                //Si se ha salido con alt-tab, liberar tecla alt, pues SDL no avisa de alt liberado al salir
                //de la ventana asi
                //Parece que en Linux esto es necesario, no se si con Windows o Mac con este driver
                //Pero lo pongo de la misma manera que se hace con driver xwindow y sdl1, para que sea coherente

                //De hecho lo ideal seria liberar todas teclas al perder el foco de la ventana,
                //pero por probar, de momento empezar por liberar alt
                debug_printf(VERBOSE_INFO,"Releasing alt when losing window focus (to avoid alt pressed when alt-tab key combination pressed)");

                joystick_possible_leftalt_key(0);

                //Teclas pulsadas por el teclado hungaro
                hungarian_keyboard_release_all();

                scrsdl_alt_caps_shift_release_all();

            }
			if (event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) {

				//printf("Ganamos foco\n");

			}

		}

		//Left Alt + number keys = Caps shift + number keys
		scrsdl_alt_caps_shift_event(&event);

		if ( (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) && scrsdl_hungarian_keyboard_event(&event) ) {
			//Gestionada por el teclado hungaro
		}

		else if (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) {
			if (event.type==SDL_KEYDOWN) pressrelease=1;
			if (event.type==SDL_KEYUP) pressrelease=0;



			SDL_Keysym keysym=event.key.keysym;
			SDL_Scancode scancode=keysym.scancode;

			//SDLKey sym=keysym.sym;
			int tecla=keysym.sym;

			/* temp revisar SDL2
			if (tecla==SDLK_COMPOSE) {
				if (keysym.scancode==34) tecla=SDLK_LEFTBRACKET;
				else if (keysym.scancode==48) tecla=SDLK_TECLA_QUOTE;
			}
			*/
/*
See the SDL documentation. Scancodes represent the physical position of the keys, modeled after a standard QWERTY keyboard, while Keycodes are the character obtained by pressing the key.
*/

			if (keysym.scancode==SDL_SCANCODE_APOSTROPHE) tecla=SDLK_TECLA_INVENTADA_D_ENYE;
			//printf ("tecla: %d scancode: %d\n",tecla,scancode);

			if (pressrelease) notificar_tecla_interrupcion_si_z88();

            if (sdl_raw_keyboard_read.v) scrsdl_deal_raw_keys(pressrelease,scancode);
            else scrsdl_deal_keys(pressrelease,tecla);

		}



		//resize
		//if (event.type==SDL_WINDOWEVENT_SIZE_CHANGED) {
        if (scrsdl_recibido_resize(&event)) {

            scrsdl_resize(event.window.data1, event.window.data2);

		}


		//mouse motion
		if (event.type==SDL_MOUSEMOTION) {
            //La imagen puede estar centrada (pantalla completa sin ZX Desktop)
            mouse_x=event.motion.x-scrsdl_imagen_x;
            mouse_y=event.motion.y-scrsdl_imagen_y;

            //Imagen reducida para llenar la pantalla: a coordenadas de la imagen
            if (scrsdl_destino_ancho>0 && scrsdl_destino_ancho!=scrsdl_imagen_ancho) {
                mouse_x=(int)(((long long)mouse_x*scrsdl_imagen_ancho)/scrsdl_destino_ancho);
            }
            if (scrsdl_destino_alto>0 && scrsdl_destino_alto!=scrsdl_imagen_alto) {
                mouse_y=(int)(((long long)mouse_y*scrsdl_imagen_alto)/scrsdl_destino_alto);
            }

            //printf("Mouse is at (%d,%d)\n", mouse_x, mouse_y);

            lightgun_x=mouse_x;
            lightgun_y=mouse_y;
            lightgun_x=lightgun_x/zoom_x;
            lightgun_y=lightgun_y/zoom_y;

            //En la imagen ampliada de la maquina, a las coordenadas de la maquina
            lightgun_x=lightgun_x*4/scrsdl_escala;
            lightgun_y=lightgun_y*4/scrsdl_escala;

            kempston_mouse_x=(mouse_x*4/scrsdl_escala)/zoom_x;
            kempston_mouse_y=255-(mouse_y*4/scrsdl_escala)/zoom_y;


            debug_printf (VERBOSE_PARANOID,"Mouse motion. X: %d Y:%d kempston x: %d y: %d",mouse_x,mouse_y,kempston_mouse_x,kempston_mouse_y);
		}


		//mouse button
		if (event.type==SDL_MOUSEBUTTONDOWN) {

			debug_printf (VERBOSE_PARANOID,"Mouse Button Press. x=%d y=%d", event.button.x, event.button.y);

            if ( event.button.button == SDL_BUTTON_LEFT ) {
				util_set_reset_mouse(UTIL_MOUSE_LEFT_BUTTON,1);
			}

            if ( event.button.button == SDL_BUTTON_RIGHT ) {
                                //mouse_right=1;
				util_set_reset_mouse(UTIL_MOUSE_RIGHT_BUTTON,1);
            }

            debug_printf (VERBOSE_PARANOID,"Mouse Button press. x=%d y=%d", event.button.x, event.button.y);

		}


		if (event.type==SDL_MOUSEBUTTONUP) {
            debug_printf (VERBOSE_PARANOID,"Mouse Button release. x=%d y=%d", event.button.x, event.button.y);
            if ( event.button.button == SDL_BUTTON_LEFT ) {
				//mouse_left=0;
				util_set_reset_mouse(UTIL_MOUSE_LEFT_BUTTON,0);
			}

            if ( event.button.button == SDL_BUTTON_RIGHT ) {
				//mouse_right=0;
				util_set_reset_mouse(UTIL_MOUSE_RIGHT_BUTTON,0);
			}

		}

        if (event.type == SDL_MOUSEWHEEL) {
            if (event.wheel.y > 0) {
                mouse_wheel_vertical=1;
            }
            else if (event.wheel.y < 0) {
                mouse_wheel_vertical=-1;
            }

            if (event.wheel.x > 0) {
                mouse_wheel_horizontal=-1;
            }
            else if (event.wheel.x < 0) {
                mouse_wheel_horizontal=1;
            }
        }



		if (event.type==SDL_QUIT) {
            debug_printf (VERBOSE_INFO,"Received window close event");
            //Hacemos que aparezca el menu con opcion de salir del emulador
            menu_set_menu_abierto(1);
            menu_button_exit_emulator.v=1;
		}

        if (event.type==SDL_DROPFILE) {      // In case if dropped file
            //printf ("Received drag&drop file %s\n",event.drop.file);
            util_drag_drop_file(event.drop.file);

            SDL_free(event.drop.file);    // Free dropped_filedir memory
        }


	}

    //Caps shift of a left Alt + number key released in the last event
    scrsdl_alt_caps_shift_release_pending();

}

void scrsdl_detectedchar_print(z80_byte caracter)
{
        printf ("%c",caracter);
        //flush de salida standard
        fflush(stdout);

}

//Estos valores no deben ser mayores de OVERLAY_SCREEN_MAX_WIDTH y OVERLAY_SCREEN_MAX_HEIGTH
int scrsdl_get_menu_width(void)
{
    //int max=screen_get_emulated_display_width_no_zoom_border_en()/menu_char_width/menu_gui_zoom;

    int max=screen_get_machine_area_width_no_zoom();

    max +=screen_get_ext_desktop_width_no_zoom();

    max=max/menu_char_width/menu_gui_zoom/screen_reduce_menu_ancho;


    if (max>OVERLAY_SCREEN_MAX_WIDTH) max=OVERLAY_SCREEN_MAX_WIDTH;

    //printf ("max x: %d %d\n",max,screen_get_emulated_display_width_no_zoom_border_en());

    return max;
}


int scrsdl_get_menu_height(void)
{
    int max=screen_get_machine_area_height_no_zoom();

    max +=screen_get_ext_desktop_height_no_zoom();

    max=max/menu_char_height/menu_gui_zoom/screen_reduce_menu_alto;

    if (max>OVERLAY_SCREEN_MAX_HEIGTH) max=OVERLAY_SCREEN_MAX_HEIGTH;

    //printf ("max y: %d %d\n",max,screen_get_emulated_display_height_no_zoom_border_en());
    return max;
}





int realjoystick_sdl_total_joysticks=0;
SDL_Joystick *sdl_joy;

int sdl_num_axes=0;
int sdl_num_hats=0;
int sdl_num_buttons=0;


#define SDL_JOY_MAX_BOTONS 128
#define SDL_JOY_MAX_AXES 16
#define SDL_JOY_MAX_HATS 16

int sdl_states_joy_buttons[SDL_JOY_MAX_BOTONS];
int sdl_states_joy_axes[SDL_JOY_MAX_AXES];
int sdl_states_joy_hats[SDL_JOY_MAX_HATS];


int realjoystick_sdl_init(void)
{


    debug_printf(VERBOSE_DEBUG,"Initializing real joystick. Using SDL support");

    SDL_InitSubSystem(SDL_INIT_JOYSTICK);


    realjoystick_sdl_total_joysticks=SDL_NumJoysticks();

    debug_printf(VERBOSE_DEBUG,"Total joysticks: %d",realjoystick_sdl_total_joysticks);

    if (realjoystick_sdl_total_joysticks<1) {
            return 1; //error
    }

    else {


        sdl_joy=SDL_JoystickOpen(realjoystick_index);
        if (sdl_joy) {
            debug_printf(VERBOSE_DEBUG,"Opened Joystick 0");

            sdl_num_axes=SDL_JoystickNumAxes(sdl_joy);
            sdl_num_hats=SDL_JoystickNumHats(sdl_joy);
            sdl_num_buttons=SDL_JoystickNumButtons(sdl_joy);

            debug_printf(VERBOSE_DEBUG,"Name: %s", SDL_JoystickName(sdl_joy));
            debug_printf(VERBOSE_DEBUG,"Number of Axes: %d", sdl_num_axes);
            debug_printf(VERBOSE_DEBUG,"Number of Hats: %d", sdl_num_hats);
            debug_printf(VERBOSE_DEBUG,"Number of Buttons: %d", sdl_num_buttons);
            //printf("Number of Balls: %d\n", SDL_JoystickNumBalls(sdl_joy));
            //printf("Number of Hats: %d\n",SDL_JoystickNumHats(sdl_joy));


            //Por si acaso el nombre lo truncamos
            menu_tape_settings_trunc_name((char *)SDL_JoystickName(sdl_joy),realjoystick_joy_name,REALJOYSTICK_MAX_NAME);

            realjoystick_total_axes=sdl_num_axes;
            realjoystick_total_buttons=sdl_num_buttons;

            strcpy(realjoystick_driver_name,"SDL2");


        }

        else {
            return 1; //error
        }
    }


    //Inicializar estados a 0
    int i;
    for (i=0;i<SDL_JOY_MAX_BOTONS;i++) sdl_states_joy_buttons[i]=0;
    for (i=0;i<SDL_JOY_MAX_AXES;i++) sdl_states_joy_axes[i]=0;
    for (i=0;i<SDL_JOY_MAX_HATS;i++) sdl_states_joy_hats[i]=0;


	return 0; //OK
}




//SDL_API - SDL Documentation Wiki
//http://sdl.beuc.net/sdl.wiki/SDL_API

//Para leer si ha habido un hit
int realjoystick_sdl_main_hit=0;

int realjoystick_ultimo_axis=-1;

int sdl_convert_hat_windows(int v)
{

    int nuevovalor;

    //Convertir de:
    // 1 arriba, 2 derecha, 4 abajo, 8 izquierda, y diagonales sumando bits. 0 es no movimiento
    //a
    //0,1,2,3,4,5,6,7 empezando con direccion arriba y yendo en las agujas del reloj
    switch (v)
    {
        case 1: //arriba
            nuevovalor=0;
        break;

        case 2: //derecha
            nuevovalor=2;
        break;

        case 3: //arriba+derecha
            nuevovalor=1;
        break;

        case 4: //abajo
            nuevovalor=4;
        break;

        case 6: //abajo+derecha.
            nuevovalor=3;
        break;

        case 8: //izquierda
            nuevovalor=6;
        break;

        case 9: //izquierda+arriba
            nuevovalor=7;
        break;

        case 12: //izquierda+abajo
            nuevovalor=5;
        break;


        default:
            //15 indica centro en la funcion realjoystick_common_set_hat
            nuevovalor=15;
        break;

    }


    return nuevovalor;
}

void realjoystick_sdl_main(void)
{
        //printf ("calling joy sdl main\n");

        if (realjoystick_present.v==0) return;

        //printf ("calling joy sdl main. joy is present\n");

/*
#define SDL_JOY_MAX_BOTONS 128
#define SDL_JOY_MAX_AXES 16

int sdl_states_joy_buttons[SDL_JOY_MAX_BOTONS];
int sdl_states_joy_axes[SDL_JOY_MAX_AXES];
*/

        //printf ("realjoystick SDL main\n");
        //SDL_JoystickGetButton(SDL_Joystick *joystick, int button);
        int i;
        int total_botones=sdl_num_buttons;
        if (total_botones>SDL_JOY_MAX_BOTONS) total_botones=SDL_JOY_MAX_BOTONS;

        int total_axes=sdl_num_axes;
        if (total_axes>SDL_JOY_MAX_AXES) total_axes=SDL_JOY_MAX_AXES;

        int total_hats=sdl_num_hats;
        if (total_axes>SDL_JOY_MAX_HATS) total_hats=SDL_JOY_MAX_HATS;

        for (i=0;i<total_botones;i++) {
                int valorboton=SDL_JoystickGetButton(sdl_joy, i);
                //printf ("boton %d: %d\n",i,valorboton);

                //Si cambia estado anterior
                if (valorboton!=sdl_states_joy_buttons[i]) {
                        debug_printf (VERBOSE_DEBUG,"SDL Joystick: Sending state change, button: %d value: %d",i,valorboton);
                        realjoystick_common_set_event(i,REALJOYSTICK_INPUT_EVENT_BUTTON,valorboton,valorboton);
                        realjoystick_hit=1;
                        menu_info_joystick_last_raw_value=valorboton;
                }

                sdl_states_joy_buttons[i]=valorboton;

        }

        for (i=0;i<total_axes;i++) {
                int valoraxis=SDL_JoystickGetAxis(sdl_joy, i);
                //printf ("axes %d: %d\n",i,valoraxis);

                int valorfinalaxis;



                //Parametro de autocalibrado para valores 0
                if (valoraxis>-realjoystick_autocalibrate_value && valoraxis<realjoystick_autocalibrate_value) valorfinalaxis=0;
                else if (valoraxis<=-realjoystick_autocalibrate_value) valorfinalaxis=-1;
                else valorfinalaxis=+1;
/*
*en test joystick, sdl hará que last raw value se actualice al leer siempre que axis coincida con último last axis leído
Dicho valor de axis se sobreescribira si se pulsa otro axis
El funcionamiento será que se verá actualizado continuamente en test joystick cuando pase el umbral de calibrado.
A partir de entonces se verá continuo hasta que se pulse otro axis. Y vuelta a empezar
*/


                // No se que sentido tiene esto. Esta sobreescribiendo siempre valor, aunque no se pulse axis. Y eso sobreescribe valor anterior de boton pulsado por ejemplo
                /*
                if (realjoystick_ultimo_axis==i) {
                        //printf ("guardar para test joystick axis boton %d valor %d\n",i,valoraxis);
                        menu_info_joystick_last_raw_value=valoraxis;
                }
                */

                if (valoraxis!=sdl_states_joy_axes[i]) {
                        //printf ("Enviar cambio estado axis %d : %d\n",i,valorfinalaxis);
                        debug_printf (VERBOSE_DEBUG,"SDL Joystick: Sending state change, axis: %d value: %d",i,valorfinalaxis);
                        realjoystick_common_set_event(i,REALJOYSTICK_INPUT_EVENT_AXIS,valorfinalaxis,valoraxis);
                        realjoystick_hit=1;
                        menu_info_joystick_last_raw_value=valoraxis;
                        realjoystick_ultimo_axis=i;
                }
                sdl_states_joy_axes[i]=valoraxis;
        }



        for (i=0;i<total_hats;i++) {
                int valoraxis=SDL_JoystickGetHat(sdl_joy, i);
                //printf ("axes %d: %d\n",i,valoraxis);

                int valorfinalaxis;


/*
*en test joystick, sdl hará que last raw value se actualice al leer siempre que axis coincida con último last axis leído
Dicho valor de axis se sobreescribira si se pulsa otro axis
El funcionamiento será que se verá actualizado continuamente en test joystick cuando pase el umbral de calibrado.
A partir de entonces se verá continuo hasta que se pulse otro axis. Y vuelta a empezar
*/

                valorfinalaxis=valoraxis;


//por alguna razon caprichosa, en Windows estos valores no se reciben igual
#ifdef MINGW
                valorfinalaxis=sdl_convert_hat_windows(valorfinalaxis);
#endif

                if (valorfinalaxis!=sdl_states_joy_hats[i]) {
                        //printf ("Enviar cambio estado hat %d : %d\n",i,valorfinalaxis);

                        debug_printf (VERBOSE_DEBUG,"SDL Joystick: Sending state change, hat: %d value: %d",i,valorfinalaxis);


                        realjoystick_common_set_hat(i,valorfinalaxis);

                }


                sdl_states_joy_hats[i]=valorfinalaxis;
        }

}


/*
int realjoystick_sdl_hit(void)
{



        if (realjoystick_present.v==0) return 0;

        //Suponemos que no ha habido hit
        realjoystick_sdl_main_hit=0;

        //Y llamamos a main
        realjoystick_sdl_main();

	return realjoystick_sdl_main_hit;
}*/




int scrsdl_init (void) {

	debug_printf (VERBOSE_INFO,"Init SDL2 Video Driver");

    //Ventana de tamano libre: el zoom cambia la escala de la imagen de la maquina
    screen_free_window_layout=1;

    //Esto tiene que ir al principio de inicializar driver para leer correctamente el tamaño de ventana
    screen_este_driver_permite_ext_desktop=1;


    //Inicializaciones necesarias
    scr_putpixel=scrsdl_putpixel;
    scr_putpixel_final=scrsdl_putpixel_final;

    if (scr_sdl_8bits_color.v) {
        scr_putpixel_final_rgb=scrsdl_putpixel_final_rgb8;
    }
    else {
        scr_putpixel_final_rgb=scrsdl_putpixel_final_rgb;
    }

    scr_get_menu_width=scrsdl_get_menu_width;
    scr_get_menu_height=scrsdl_get_menu_height;
    //scr_driver_can_ext_desktop=scrsdl_driver_can_ext_desktop;

    scr_putchar_zx8081=scrsdl_putchar_zx8081;
    scr_debug_registers=scrsdl_debug_registers;
    scr_messages_debug=scrsdl_messages_debug;
    scr_putchar_menu=scrsdl_putchar_menu;
    scr_putchar_footer=scrsdl_putchar_footer;
    scr_set_fullscreen=scrsdl_set_fullscreen;
    scr_reset_fullscreen=scrsdl_reset_fullscreen;
    scr_z88_cpc_load_keymap=scrsdl_z88_cpc_load_keymap;
    scr_detectedchar_print=scrsdl_detectedchar_print;
    scr_update_window_title=scrsdl_update_window_title;
    scr_get_text_clipboard=scrsdl_get_text_clipboard;
    scr_tiene_colores=1;
    screen_refresh_menu=1;


    if (!realjoystick_is_linux_native() ) {

        realjoystick_init=realjoystick_sdl_init;
        realjoystick_main=realjoystick_sdl_main;

        realjoystick_initialize_joystick();
    }


    if (commonsdl_init() != 0 ) {
		debug_printf (VERBOSE_ERR,"scrsdl_init: Error initializing driver");
        return 1;
	}

	scrsdl_inicializado.v=1;

	if (scrsdl_crea_ventana()) {
		debug_printf (VERBOSE_ERR,"scrsdl_init: Could not set video mode");
		return 1;
	}



    //Otra inicializacion necesaria
    //Esto debe estar al final, para que funcione correctamente desde menu, cuando se selecciona un driver, y no va, que pueda volver al anterior
    scr_set_driver_name("sdl");


	scr_z88_cpc_load_keymap();

    return 0;

}


