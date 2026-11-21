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

#include <stdio.h>
#include <stdlib.h>

#include "datagear.h"
#include "cpu.h"
#include "debug.h"
#include "utils.h"
#include "operaciones.h"
#include "ula.h"
#include "screen.h"
#include "tbblue.h"



//Si esta recibiendo parametros de comando.
//Si no es 0, indica cuantos parámetros le quedan por recibir
//int datagear_receiving_parameters=0;

//
// Inicio variables que hay que guardar en un snapshot
//

//Mascara de los parametros a leer. Por ejemplo si WR0 enviara los 4 parametros, tendra valor 00001111b
z80_byte datagear_mask_commands=0;

//Indice al numero de parametro leido
int datagear_command_index;
//Ejemplo, en WR0 vale 0 cuando vamos a leer el primer parametro (Port A starting address Low byte), vale 1 cuando vamos a leer Port A adress high byte

//Indica ultimo comando leido, tal cual el primer byte
//z80_byte datagear_last_command_byte;


//Indica ultimo comando leido, 0=WR0, 1=WR1, etc. Caso especial: 128+2 (130) para valor de ZXN PRESCALAR (FIXED TIME TRANSFER) de WR2 en TBBLUE

z80_byte datagear_last_command;

z80_byte datagear_port_a_start_addr_low;
z80_byte datagear_port_a_start_addr_high;

z80_byte datagear_port_b_start_addr_low;
z80_byte datagear_port_b_start_addr_high;

z80_byte datagear_block_length_low;
z80_byte datagear_block_length_high;

//Valores iniciales para recuperarlos al final de la transferencia DMA
z80_byte datagear_original_block_length_low;
z80_byte datagear_original_block_length_high;

z80_byte datagear_port_a_variable_timing_byte;
z80_byte datagear_port_b_variable_timing_byte;

//Ultimo valor recibido para los registros
z80_byte datagear_wr0;
z80_byte datagear_wr1;
z80_byte datagear_wr2;
z80_byte datagear_wr3;
z80_byte datagear_wr4;
z80_byte datagear_wr5;
z80_byte datagear_wr6;



//Si hay transferencia de dma activa
z80_bit datagear_is_dma_transfering={0};

//Valor de la DMA de TBBLUE de prescaler
z80_byte datagear_dma_tbblue_prescaler;

//Fork: estado de la DMA para la lectura del puerto (TBBlue)
int datagear_tbblue_status_atleastone=0;
int datagear_tbblue_status_endofblock=0;
z80_int datagear_tbblue_byte_counter=0;
extern z80_byte datagear_tbblue_read_mask;
static void datagear_tbblue_read_advance(int despues);
static void datagear_tbblue_enable(void);
void datagear_tbblue_read_index_status(void);


//
// Fin variables que hay que guardar en un snapshot
//


int datagear_dma_last_testados=0;


/* Fork: zxnDDMA (zxnDimensionalDMA, "blitter") del core 3.03 del ZX Spectrum Next.

   Segun docs/zxnDDMA/zxnddma.gde de la distribucion System/Next 26.11a. Son comandos extra de la zxnDMA,
   solo en TBBlue y solo en el puerto 0x6B (en el 0x0B el byte de comando se ignora y los bytes
   siguientes se leen como escrituras normales de registro, igual que en un core sin zxnDDMA):

   $D7 nn / $DB nn        bits 23:16 de la direccion del puerto A / B
   $DF mm [Xlo Xhi Y]     modos: bit 0 direcciones fisicas (SRAM, sin MMU), bit 1 2D (rectangulos),
                          bit 2 color transparente, bit 3 relleno, bit 4 destino X,Y de Layer 2
                          (siguen 3 bytes), bit 5 pantalla oculta de Layer 2 (nextreg 0x13)
   $E3 / $E7 ll hh        longitud de fila / numero de filas
   $EB / $EF ll hh        distancia entre filas (stride) del puerto A / B
   $F3 nn / $F7 nn        color transparente / valor de relleno
   $C3 lo deja todo a cero

   La transferencia, como la de la DMA normal de este archivo, no consume tiempo de la cpu
*/

#define DATAGEAR_DDMA_FLAG_PHYSICAL     0x01
#define DATAGEAR_DDMA_FLAG_2D           0x02
#define DATAGEAR_DDMA_FLAG_TRANSPARENT  0x04
#define DATAGEAR_DDMA_FLAG_FILL         0x08
#define DATAGEAR_DDMA_FLAG_LAYER2_XY    0x10
#define DATAGEAR_DDMA_FLAG_HIDDEN       0x20

//Valor de datagear_last_command mientras se reciben los parametros de un comando zxnDDMA
#define DATAGEAR_LAST_COMMAND_DDMA 131
//Fork: parametro de 0xBB (mascara de lectura)
#define DATAGEAR_LAST_COMMAND_READ_MASK 132

z80_byte datagear_ddma_flags=0;

//Bytes $D7 y $DB (valores iniciales) y los de trabajo, que carga $CF y avanzan con el acarreo de la direccion
z80_byte datagear_ddma_ext_a=0;
z80_byte datagear_ddma_ext_b=0;
z80_byte datagear_ddma_work_ext_a=0;
z80_byte datagear_ddma_work_ext_b=0;

z80_int datagear_ddma_row_length=0;
z80_int datagear_ddma_rows=0;
z80_int datagear_ddma_stride_a=0;
z80_int datagear_ddma_stride_b=0;

z80_byte datagear_ddma_transparent=0;
z80_byte datagear_ddma_fill=0;

//Rectangulo en curso: inicio de la fila actual en cada puerto (24 bits), bytes que quedan de la fila y filas que quedan
int datagear_ddma_2d_running=0;
unsigned int datagear_ddma_row_start_a=0;
unsigned int datagear_ddma_row_start_b=0;
int datagear_ddma_row_bytes_left=0;
int datagear_ddma_rows_left=0;

//Comando zxnDDMA del que se estan recibiendo parametros, indice del parametro, y parametros de $DF con X,Y
z80_byte datagear_ddma_command=0;
int datagear_ddma_param_index=0;
z80_byte datagear_ddma_xy[3];

//Direcciones iniciales de los puertos A y B (WR0, WR4, o el destino X,Y de zxnDDMA). Las variables
//datagear_port_x_start_addr son las direcciones de trabajo, que avanzan con la transferencia;
//el comando $CF (load) vuelve a cargar en ellas estas direcciones iniciales, como hace la DMA real.
//Sin esto, un programa que repite la transferencia enviando solo $CF $87 seguia desde donde acabo la anterior
z80_byte datagear_initial_port_a_low=0;
z80_byte datagear_initial_port_a_high=0;
z80_byte datagear_initial_port_b_low=0;
z80_byte datagear_initial_port_b_high=0;

//Ultimo puerto por el que se ha escrito a la DMA (0x0B o 0x6B)
z80_byte datagear_last_port_written=DATAGEAR_DMA_SECOND_PORT;

//Lo que deja a cero el comando $C3 (apartado 2.6 de la guia)
void datagear_ddma_reset(void)
{
    datagear_ddma_flags=0;
    datagear_ddma_ext_a=datagear_ddma_ext_b=0;
    datagear_ddma_row_length=datagear_ddma_rows=0;
    datagear_ddma_stride_a=datagear_ddma_stride_b=0;
    datagear_ddma_transparent=0;
    datagear_ddma_fill=0;
    datagear_ddma_2d_running=0;
}

//Si los comandos zxnDDMA estan disponibles en esta escritura
int datagear_ddma_available(void)
{
    return MACHINE_IS_TBBLUE && datagear_last_port_written==DATAGEAR_DMA_SECOND_PORT;
}

//Numero de parametros que siguen a un comando zxnDDMA (sin contar los 3 de X,Y de $DF). -1 si no es un comando zxnDDMA
int datagear_ddma_command_params(z80_byte command)
{
    switch (command) {
        case 0xD7:
        case 0xDB:
        case 0xDF:
        case 0xF3:
        case 0xF7:
            return 1;

        case 0xE3:
        case 0xE7:
        case 0xEB:
        case 0xEF:
            return 2;

        //$FB: era copiar la pantalla oculta de Layer 2; ya no hace nada y el byte se ignora
        case 0xFB:
            return 0;
    }

    return -1;
}

//Destino X,Y en Layer 2: calcula la direccion fisica del puerto B (apartado 2.5 de la guia)
void datagear_ddma_set_layer2_xy(void)
{
    z80_byte registro_banco=(datagear_ddma_flags & DATAGEAR_DDMA_FLAG_HIDDEN ? tbblue_registers[0x13] : tbblue_registers[0x12]);

    unsigned int base=0x40000+(registro_banco & 0x7F)*0x4000;

    unsigned int x_lo=datagear_ddma_xy[0];
    unsigned int x_hi=datagear_ddma_xy[1];
    unsigned int y=datagear_ddma_xy[2];

    unsigned int direccion;

    int modo=(tbblue_registers[0x70]>>4) & 3;

    //256x192: fila a fila. Solo se suma X_lo
    if (modo==0) direccion=base+y*256+x_lo;

    //320x256 y 640x256: columna a columna. La columna son 9 bits
    else direccion=base+(x_lo | ((x_hi & 1)<<8))*256+y;

    datagear_port_b_start_addr_low=datagear_initial_port_b_low=direccion & 0xFF;
    datagear_port_b_start_addr_high=datagear_initial_port_b_high=(direccion>>8) & 0xFF;
    datagear_ddma_ext_b=(direccion>>16) & 0xFF;

    //La distancia entre filas del destino pasa a ser 256, hasta $EF o $C3
    datagear_ddma_stride_b=256;
}

//Recibe un parametro del comando zxnDDMA en curso. Retorna 1 si todavia faltan parametros
int datagear_ddma_write_param(z80_byte value)
{
    int indice=datagear_ddma_param_index++;

    switch (datagear_ddma_command) {
        case 0xD7:
            datagear_ddma_ext_a=value;
        break;

        case 0xDB:
            datagear_ddma_ext_b=value;
        break;

        case 0xDF:
            if (indice==0) {
                //Cada $DF fija todos los modos a la vez
                datagear_ddma_flags=value & 0x3F;
                if (value & DATAGEAR_DDMA_FLAG_LAYER2_XY) return 1;
            }
            else {
                datagear_ddma_xy[indice-1]=value;
                if (indice<3) return 1;
                datagear_ddma_set_layer2_xy();
            }
        break;

        case 0xE3:
            if (indice==0) datagear_ddma_row_length=(datagear_ddma_row_length & 0xFF00) | value;
            else datagear_ddma_row_length=(datagear_ddma_row_length & 0x00FF) | (value<<8);
        break;

        case 0xE7:
            if (indice==0) datagear_ddma_rows=(datagear_ddma_rows & 0xFF00) | value;
            else datagear_ddma_rows=(datagear_ddma_rows & 0x00FF) | (value<<8);
        break;

        case 0xEB:
            if (indice==0) datagear_ddma_stride_a=(datagear_ddma_stride_a & 0xFF00) | value;
            else datagear_ddma_stride_a=(datagear_ddma_stride_a & 0x00FF) | (value<<8);
        break;

        case 0xEF:
            if (indice==0) datagear_ddma_stride_b=(datagear_ddma_stride_b & 0xFF00) | value;
            else datagear_ddma_stride_b=(datagear_ddma_stride_b & 0x00FF) | (value<<8);
        break;

        case 0xF3:
            datagear_ddma_transparent=value;
        break;

        case 0xF7:
            datagear_ddma_fill=value;
        break;
    }

    return (datagear_ddma_param_index<datagear_ddma_command_params(datagear_ddma_command));
}

//Direccion fisica de 22 bits (se pierden los bits 7:6 del byte extendido). Como en una placa de 2 MB,
//el bit 21 tambien se pierde: las direcciones mas altas dan la vuelta
unsigned int datagear_ddma_physical_address(z80_byte ext,z80_int address)
{
    unsigned int direccion=((ext & 0x3F)<<16) | address;
    return direccion & 0x1FFFFF;
}

//Avanza la direccion de un puerto segun su modo (WR1 o WR2). En modo fisico el acarreo pasa al byte extendido
void datagear_ddma_step_address(z80_int *address,z80_byte *ext,z80_byte wr,int physical)
{
    //Direccion fija
    if (wr & 32) return;

    if (wr & 16) {
        (*address)++;
        if (physical && *address==0) (*ext)++;
    }
    else {
        if (physical && *address==0) (*ext)--;
        (*address)--;
    }
}


//Si esta activada la emulacion de dma
z80_bit datagear_dma_emulation={0};

//Si esta desactivada la dma. Si esta desactivada, se puede acceder igualmente a todo, excepto que no ejecuta transferencias DMA
z80_bit datagear_dma_is_disabled={0};



void datagear_dma_disable(void)
{
    datagear_dma_emulation.v=0;
}

void datagear_dma_enable(void)
{
    datagear_dma_emulation.v=1;
}


void datagear_reset(void)
{
    datagear_mask_commands=0;

    datagear_wr0=datagear_wr1=datagear_wr2=datagear_wr3=datagear_wr4=datagear_wr5=datagear_wr6=0;
    datagear_is_dma_transfering.v=0;

    datagear_ddma_reset();
    datagear_ddma_work_ext_a=datagear_ddma_work_ext_b=0;

}

//Escritura en la DMA indicando el puerto: los comandos zxnDDMA solo existen en el 0x6B
void datagear_write_value_port(z80_byte puerto_l,z80_byte value)
{
    datagear_last_port_written=puerto_l;
    datagear_write_value(value);
}

/*void datagear_do_transfer(void)
{
    if (datagear_dma_is_disabled.v) return;

				z80_int transfer_length=value_8_to_16(datagear_block_length_high,datagear_block_length_low);
				z80_int transfer_port_a,transfer_port_b;


					transfer_port_a=value_8_to_16(datagear_port_a_start_addr_high,datagear_port_a_start_addr_low);
					transfer_port_b=value_8_to_16(datagear_port_b_start_addr_high,datagear_port_b_start_addr_low);


				if (datagear_wr0 & 4) printf ("Copying %d bytes from %04XH to %04XH\n",transfer_length,transfer_port_a,transfer_port_b);
                else printf ("Copying %d bytes from %04XH to %04XH\n",transfer_length,transfer_port_b,transfer_port_a);

                if (datagear_wr1 & 8) printf ("Port A I/O. not implemented yet\n");
                if (datagear_wr2 & 8) printf ("Port B I/O. not implemented yet\n");

				//while (transfer_length) {
                    z80_byte byte_leido;
                    if (datagear_wr0 & 4) {
                        byte_leido=peek_byte_no_time(transfer_port_a);
					    poke_byte_no_time(transfer_port_b,byte_leido);
                    }

                    else {
                        byte_leido=peek_byte_no_time(transfer_port_b);
					    poke_byte_no_time(transfer_port_a,byte_leido);
                    }

                    if ( (datagear_wr1 & 32) == 0 ) {
                        if (datagear_wr1 & 16) transfer_port_a++;
                        else transfer_port_a--;
                    }

                    if ( (datagear_wr2 & 32) == 0 ) {
                        if (datagear_wr2 & 16) transfer_port_b++;
                        else transfer_port_b--;
                    }

					transfer_length--;
				//}

    if (transfer_length==0) datagear_is_dma_transfering.v=0;

}
*/

void datagear_write_value(z80_byte value)
{

    //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Writing Datagear DMA port with value %02XH. PC=%02XH",value,reg_pc);

	//gestionar si estamos esperando parametros de comando
	if (datagear_mask_commands) {
        //printf("if datagear_mask_commands. datagear_last_command=%d\n",datagear_last_command);
		switch (datagear_last_command) {

            //printf("Datagear datagear_last_command: %d. value=%02XH\n",datagear_last_command,value);

			//WR0
			case 0:
				//si parametro de indice actual se salta porque mascara vale 0 en bit bajo
				while ( (datagear_mask_commands&1)==0) {
					datagear_mask_commands=datagear_mask_commands >> 1;
					datagear_command_index++;
				}

				//Aqui tenemos, en datagear_command_index, el parametro que vamos a leer ahora:
				//0=Port A starting address Low byte
				//1=Port A starting address High byte
				//2=Block length low byte
				//3=Block length high byte
				switch (datagear_command_index) {
					case 0:
						datagear_port_a_start_addr_low=value;
						datagear_initial_port_a_low=value;
						//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Setting port a start address low to %02XH",value);
					break;

					case 1:
						datagear_port_a_start_addr_high=value;
						datagear_initial_port_a_high=value;
						//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Setting port a start address high to %02XH",value);
					break;

					case 2:
						datagear_block_length_low=value;
                        datagear_original_block_length_low=value;
						//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Setting block length low to %02XH",value);
					break;

					case 3:
						datagear_block_length_high=value;
                        datagear_original_block_length_high=value;
						//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Setting block length high to %02XH",value);
					break;

				}

				datagear_mask_commands=datagear_mask_commands >> 1;
				datagear_command_index++;

			break;

			//WR1
			case 1:
				//si parametro de indice actual se salta porque mascara vale 0 en bit bajo
				while ( (datagear_mask_commands&1)==0) {
					datagear_mask_commands=datagear_mask_commands >> 1;
					datagear_command_index++;
				}

				//Aqui tenemos, en datagear_command_index, el parametro que vamos a leer ahora:
				//0=Port A variable timing byte

				switch (datagear_command_index) {
					case 0:
						datagear_port_a_variable_timing_byte=value;
						//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Setting port a variable timing byte to %02XH",value);
					break;

				}

				datagear_mask_commands=datagear_mask_commands >> 1;
				datagear_command_index++;
			break;

			//WR2
			case 2:
				//si parametro de indice actual se salta porque mascara vale 0 en bit bajo
				while ( (datagear_mask_commands&1)==0) {
					datagear_mask_commands=datagear_mask_commands >> 1;
					datagear_command_index++;
				}

				//Aqui tenemos, en datagear_command_index, el parametro que vamos a leer ahora:
				//0=Port B variable timing byte

				switch (datagear_command_index) {
					case 0:
						datagear_port_b_variable_timing_byte=value;
						//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Setting port b variable timing byte to %02XH",value);
						if (value&32 && MACHINE_IS_TBBLUE) {

							//De momento solo leo el valor del prescaler, aunque no hago nada con el

/*
TODO: Solo en Next. Si bit 5 no es 0, se leera otro parametro:
D7  D6  D5  D4  D3  D2  D1  D0  ZXN PRESCALAR (FIXED TIME TRANSFER)
#
# The ZXN PRESCALAR is a feature of the ZXN DMA implementation.
# If non-zero, a delay will be inserted after each byte is transferred
# such that the total time needed for the transfer is at least the number
# of cycles indicated by the prescalar.  This works in both the continuous
# mode and the burst mode.

# The ZXN DMA's speed matches the current CPU speed so it can operate
# at 3.5MHz, 7MHz or 14MHz.  Since the prescalar delay is a cycle count,
# the actual duration depends on the speed of the DMA.  A prescalar
# delay set to N cycles will result in a real time transfer taking N/fCPU
# seconds.  For example, if the DMA is operating at 3.5MHz and the max
# prescalar of 255 is set, the transfer time for each byte will be
# 255/3.5MHz = 72.9us.  If the DMA is used to send sampled audio, the
# sample rate would be 13.7kHz and this is the lowest sample rate possible
# using the prescalar.
#
# If the DMA is operated in burst mode, the DMA will give up any waiting
# time to the CPU so that the CPU can run while the DMA is idle.



*/

						//printf ("Will receive ZXN Prescaler\n");
                        //sleep(3);
						datagear_last_command=128+2;

						datagear_mask_commands=1;        //Realmente esto cualquier cosa diferente de 0 nos vale

						}
					break;
				}

				//Siempre que no vayamos a recibir el prescaler
				if (datagear_last_command!=130) {
					datagear_mask_commands=datagear_mask_commands >> 1;
					datagear_command_index++;
				}
			break;

			//WR3
			case 3:
			break;

			//WR4
			case 4:
				//si parametro de indice actual se salta porque mascara vale 0 en bit bajo
				while ( (datagear_mask_commands&1)==0) {
					datagear_mask_commands=datagear_mask_commands >> 1;
					datagear_command_index++;
				}

				//Aqui tenemos, en datagear_command_index, el parametro que vamos a leer ahora:
				//0=Port B starting address Low byte
				//1=Port B starting address High byte
				switch (datagear_command_index) {
					case 0:
						datagear_port_b_start_addr_low=value;
						datagear_initial_port_b_low=value;
						//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Setting port b start address low to %02X",value);
					break;

					case 1:
						datagear_port_b_start_addr_high=value;
						datagear_initial_port_b_high=value;
						//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Setting port b start address high to %02XH",value);
					break;

				}

				datagear_mask_commands=datagear_mask_commands >> 1;
				datagear_command_index++;
			break;

			//WR5
			case 5:
			break;

			//WR6
			case 6:
			break;


			//Parametros de un comando zxnDDMA
			case DATAGEAR_LAST_COMMAND_DDMA:
				if (!datagear_ddma_write_param(value)) datagear_mask_commands=0;
			break;

			case DATAGEAR_LAST_COMMAND_READ_MASK:
				//Mascara de lectura (despues de 0xBB, fork)
				datagear_tbblue_read_mask=value;
				datagear_tbblue_read_advance(-1);
				datagear_mask_commands=0;
			break;

			case 130:
				DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Reading ZXN Prescaler = %02XH",value);
				datagear_dma_tbblue_prescaler=value;
				datagear_mask_commands=0;
                //sleep(1);
			break;

		}

	}

	else {

        //printf("Datagear. tipo de comando. value=%02XH\n",value);
		//datagear_last_command_byte=value;
		datagear_command_index=0;
	//Obtener tipo de comando
	//SI WR0

	z80_byte value_mask_wr0_wr3=value&(128+2+1);
	if (value_mask_wr0_wr3==1 || value_mask_wr0_wr3==2 ||value_mask_wr0_wr3==3 ) {
		datagear_last_command=0;
		datagear_wr0=value;
        //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Set WR0=%02XH",datagear_wr0);

		//Ver bits 4,5,6,7 y longitud comando
/*
#  D7  D6  D5  D4  D3  D2  D1  D0  PORT A STARTING ADDRESS (LOW BYTE)
#       |   |   V
#  D7  D6  D5  D4  D3  D2  D1  D0  PORT A STARTING ADDRESS (HIGH BYTE)
#       |   V
#  D7  D6  D5  D4  D3  D2  D1  D0  BLOCK LENGTH (LOW BYTE)
#       V
#  D7  D6  D5  D4  D3  D2  D1  D0  BLOCK LENGTH (HIGH BYTE)
*/

		datagear_mask_commands=(value>>3)&15;

		//z80_byte transfer_type=value&3;
		/*if (transfer_type==1) printf ("Type: transfer\n");
		else if (transfer_type==2) printf ("Type: search\n");
		else if (transfer_type==3) printf ("Type: search/transfer\n");

		if (value&4) printf ("Port A -> Port B\n");
		else printf ("Port B -> Port A\n");*/


	}

	if (value_mask_wr0_wr3==128) {
		datagear_last_command=3;
		datagear_wr3=value;
        //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Set WR3=%02XH",datagear_wr3);
	}

	if (value_mask_wr0_wr3==129) {
		datagear_last_command=4;
		datagear_wr4=value;
        //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Set WR4=%02XH",datagear_wr4);

		datagear_mask_commands=(value>>2)&3;



	}

	if (value_mask_wr0_wr3==128+2+1) {
		datagear_last_command=6;
		datagear_wr6=value;
        //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Set WR6=%02XH",datagear_wr6);

		//Comandos zxnDDMA (solo TBBlue, solo puerto 0x6B)
		if (datagear_ddma_available() && datagear_ddma_command_params(value)>=0) {
			datagear_ddma_command=value;
			datagear_ddma_param_index=0;
			if (datagear_ddma_command_params(value)>0) {
				datagear_last_command=DATAGEAR_LAST_COMMAND_DDMA;
				datagear_mask_commands=1;        //Cualquier cosa diferente de 0 nos vale
			}
			return;
		}

		//Tratar todos los diferentes comandos
		switch (value) {
			case 0xC3:
				//Reset. Aqui solo lo relativo a zxnDDMA; lo demas sigue como estaba
				if (MACHINE_IS_TBBLUE) {
					datagear_ddma_reset();
					//Como dma.vhd: ciclos de 3, sin prescaler, estado a cero (fork)
					datagear_port_a_variable_timing_byte=1;
					datagear_port_b_variable_timing_byte=1;
					datagear_dma_tbblue_prescaler=0;
					datagear_tbblue_status_atleastone=0;
					datagear_tbblue_status_endofblock=0;
				}
			break;

			case 0xC7:
				//Reset de la temporizacion del puerto A (fork)
				if (MACHINE_IS_TBBLUE) datagear_port_a_variable_timing_byte=1;
			break;

			case 0xCB:
				//Reset de la temporizacion del puerto B (fork)
				if (MACHINE_IS_TBBLUE) datagear_port_b_variable_timing_byte=1;
			break;

			case 0xBF:
				//Leer el byte de estado (fork)
				datagear_tbblue_read_index_status();
			break;

			case 0xA7:
				//Iniciar la secuencia de lectura (fork)
				datagear_tbblue_read_advance(-1);
			break;

			case 0x8B:
				//Reiniciar el byte de estado (fork)
				datagear_tbblue_status_atleastone=0;
				datagear_tbblue_status_endofblock=0;
			break;

			case 0xBB:
				//Sigue la mascara de lectura (fork)
				datagear_last_command=DATAGEAR_LAST_COMMAND_READ_MASK;
				datagear_mask_commands=1;
			break;

			case 0xCF:
				//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Load starting address for both ports, clear byte counter");
				datagear_tbblue_status_endofblock=0;
				datagear_tbblue_byte_counter=0;
				//Las direcciones iniciales pasan a las direcciones de trabajo
				datagear_port_a_start_addr_low=datagear_initial_port_a_low;
				datagear_port_a_start_addr_high=datagear_initial_port_a_high;
				datagear_port_b_start_addr_low=datagear_initial_port_b_low;
				datagear_port_b_start_addr_high=datagear_initial_port_b_high;

				//zxnDDMA: los bytes $D7 y $DB pasan a las direcciones de trabajo, y el rectangulo empieza de nuevo
				datagear_ddma_work_ext_a=datagear_ddma_ext_a;
				datagear_ddma_work_ext_b=datagear_ddma_ext_b;
				datagear_ddma_2d_running=0;
			break;

			case 0xD3:
				//Continue: se mantienen las direcciones de trabajo y empiezan de nuevo los contadores
				datagear_ddma_2d_running=0;
			break;

			case 0xAB:
				//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Enable interrupts");
			break;

			case 0x87:
				//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Enable DMA");
                datagear_is_dma_transfering.v=1;
                //datagear_do_transfer();
                datagear_dma_last_testados=t_estados;
                if (MACHINE_IS_TBBLUE) datagear_tbblue_enable();

			break;

			case 0x83:
				//DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Disable DMA");
                datagear_is_dma_transfering.v=0;

			break;

			case 0xB3:
				DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Force an internal ready condition independent 'on the rdy' input");
			break;

			case 0xB7:
				DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Enable after RETI so dma requests bus only after receiving a reti");
			break;


		}


	}

	z80_byte value_mask_wr1_wr2=value&(128+4+2+1);
	if (value_mask_wr1_wr2==4) {
		datagear_last_command=1;
		datagear_wr1=value;
        //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Set WR1=%02XH",datagear_wr1);

		//Ver bits D6
        //D6 Port A variable timing byte

		datagear_mask_commands=(value>>6)&1;

	}

	if (value_mask_wr1_wr2==0) {
		datagear_last_command=2;
		datagear_wr2=value;
        //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Set WR2=%02XH",datagear_wr2);

		//Ver bits D6
        //D6 Port B variable timing byte

		datagear_mask_commands=(value>>6)&1;
	}

	z80_byte value_mask_wr5=value&(128+64+4+2+1);
	if (value_mask_wr5==128+2) {
		datagear_last_command=5;
		datagear_wr5=value;
        //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: Set WR5=%02XH",datagear_wr5);
	}

	}
}




z80_byte datagear_read_operation(z80_int address,z80_byte dma_mem_type);
void datagear_write_operation(z80_int address,z80_byte value,z80_byte dma_mem_type);

//Lectura y escritura de un puerto de la DMA con el byte extendido: memoria fisica si el modo fisico de zxnDDMA esta activo
z80_byte datagear_ddma_read_operation(z80_int address,z80_byte ext,z80_byte dma_mem_type)
{
    if (!dma_mem_type && (datagear_ddma_flags & DATAGEAR_DDMA_FLAG_PHYSICAL)) {
        return memoria_spectrum[datagear_ddma_physical_address(ext,address)];
    }

    return datagear_read_operation(address,dma_mem_type);
}

void datagear_ddma_write_operation(z80_int address,z80_byte ext,z80_byte value,z80_byte dma_mem_type)
{
    if (!dma_mem_type && (datagear_ddma_flags & DATAGEAR_DDMA_FLAG_PHYSICAL)) {
        memoria_spectrum[datagear_ddma_physical_address(ext,address)]=value;
        return;
    }

    datagear_write_operation(address,value,dma_mem_type);
}

z80_byte datagear_read_operation(z80_int address,z80_byte dma_mem_type)
{

    z80_byte byte_leido;

    if (dma_mem_type) {
        z80_int puerto_h=(address>>8)&0xFF;
        z80_int puerto_l=address & 0xFF;
        //printf("Dma read i/o port=%02X%02XH\n",puerto_h,puerto_l);
        byte_leido=lee_puerto_spectrum_no_time(puerto_h,puerto_l);
    }
    else {
        //printf("Dma read mem address=%XH\n",address);

        byte_leido=peek_byte_no_time(address);

        //temp debug
        //if (address>=0x1CE7 && address<=0x1Cf0) sleep(3);
    }

    return byte_leido;
}

void datagear_write_operation(z80_int address,z80_byte value,z80_byte dma_mem_type)
{
    if (dma_mem_type) {
        //printf("dma write i/o port=%XH\n",address);
        //if (address==0x253B) sleep(1);
		out_port_spectrum_no_time(address,value);
		//printf ("Port %04XH value %02XH\n",address,value);
		//En TBBlue el tiempo de cada byte lo cuenta datagear_tbblue_byte_cycles (fork)
		if (!MACHINE_IS_TBBLUE) t_estados +=1; //Por ejemplo ;)
	}

    else {
        //printf("dma write mem address=%XH\n",address);
		poke_byte_no_time(address,value);
	}
}


int datagear_return_resta_testados(int anterior, int actual)
{
	//screen_testados_total

	int resta=actual-anterior;

	if (resta<0) {
        resta=screen_testados_total-anterior+actual;
    }

    //printf("datagear resta testados: %d. testados %d\n",resta,t_estados);

	return resta;
}

int datagear_condicion_transferencia(z80_int transfer_length,int dma_continuous,int resta,int dmapre)
{

	//printf ("dma condition length: %d dma_cont %d resta %d dmapre %d\n",transfer_length,dma_continuous,resta,dmapre);

	//Si hay bytes a transferir
	if (transfer_length==0) return 0;

	//Si es modo continuo
	if (dma_continuous) return 1;

	//Modo burst. Permitiendo ejecutar la cpu entre medio
	if (resta>=dmapre) return 1;


	//Otro caso, retornar 0
	return 0;


}

//Transfiere bytes. max_bytes<0: como siempre (continuo todo de golpe, burst segun t-estados);
//max_bytes>=0: como mucho ese numero de bytes, sin mirar el tiempo (lo controla el que llama, fork).
//Retorna los bytes transferidos
static int datagear_handle_dma_bytes(int max_bytes)
{
        if (datagear_is_dma_transfering.v==0) return 0;
        int bytes_hechos=0;


      				z80_int transfer_length=value_8_to_16(datagear_block_length_high,datagear_block_length_low);
				z80_int transfer_port_a,transfer_port_b;


					transfer_port_a=value_8_to_16(datagear_port_a_start_addr_high,datagear_port_a_start_addr_low);
					transfer_port_b=value_8_to_16(datagear_port_b_start_addr_high,datagear_port_b_start_addr_low);


				//if (datagear_wr0 & 4) DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: DMA Transfer. Copying %d bytes from %04XH to %04XH",transfer_length,transfer_port_a,transfer_port_b);
                //else DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: DMA Transfer. Copying %d bytes from %04XH to %04XH",transfer_length,transfer_port_b,transfer_port_a);






        //zxnDDMA: modos activos (siempre 0 fuera de TBBlue, porque solo se pueden fijar alli)
        int ddma_physical=(datagear_ddma_flags & DATAGEAR_DDMA_FLAG_PHYSICAL ? 1 : 0);
        int ddma_2d=(datagear_ddma_flags & DATAGEAR_DDMA_FLAG_2D ? 1 : 0);

        if (ddma_2d) {
            if (!datagear_ddma_2d_running) {
                //Empieza un rectangulo desde las direcciones actuales. Longitud de fila o numero de filas 0 mueven un byte
                datagear_ddma_2d_running=1;
                datagear_ddma_row_start_a=(datagear_ddma_work_ext_a<<16) | transfer_port_a;
                datagear_ddma_row_start_b=(datagear_ddma_work_ext_b<<16) | transfer_port_b;
                datagear_ddma_row_bytes_left=(datagear_ddma_row_length ? datagear_ddma_row_length : 1);
                datagear_ddma_rows_left=(datagear_ddma_rows ? datagear_ddma_rows : 1);
            }

            //En 2D no se usa la longitud de bloque: quedan bytes mientras queden filas
            transfer_length=1;
        }

        int dmapre=2; //Cada 2 estados, una transferencia

		int resta=datagear_return_resta_testados(datagear_dma_last_testados,t_estados);

		//dmapre *=cpu_turbo_speed;

		int resta_antes=resta;


		//printf ("Antes transferencia: dmapre: %d datagear_dma_last_testados %d t_estados %d resta %d dmapre %d length %d\n",
		//	dmapre,datagear_dma_last_testados,t_estados,resta,dmapre,transfer_length);

		//Ver si modo continuo o modo burst
		//WR4. Bits D6 D5:
		//#       0   0 = Byte mode -> Do not use (Behaves like Continuous mode, Byte mode on Z80 DMA)
		//#       0   1 = Continuous mode
		//#       1   0 = Burst mode
		//#       1   1 = Do not use

		//Por defecto, modo continuo (todo de golpe) (dma_continuous=1). Modo burst (dma_continuous=0), permite ejecutar la cpu entre medio
		int dma_continuous=1;

        //printf("DMA WR4=%02XH\n",datagear_wr4);

		z80_byte modo_transferencia=datagear_wr4 & (64+32);

        // Burst mode nominally means the DMA lets the CPU run if either port is not ready
		if (modo_transferencia==64) dma_continuous=0;

/*
Excepción:
# The ZXN DMA can operate in either burst or continuous mode.  Continuous mode means the DMA chip
# runs to completion without allowing the CPU to run.  Burst mode nominally means the DMA lets the
# CPU run if either port is not ready.  This condition can't happen in the ZXN DMA chip except when
# operated in the special fixed time transfer mode.  In this mode, the ZXN DMA chip will let the CPU
# run while it waits for the fixed time to expire between bytes transferred.  Note that there is no
# byte transfer mode as in the Z80 DMA.
*/

		//Por tanto de momento:
		if (MACHINE_IS_TBBLUE && dma_continuous==0) {
			//Modo burst en tbblue

			//Si tiene pre escalar, se permite modo burst. Si no, no

			//printf ("Tbblue and burst mode\n");

			if ( (datagear_port_b_variable_timing_byte & 32)==0 ) {
				//printf ("burst mode not allowed on tbblue because it has no pre escalar\n");
				dma_continuous=1; //no tiene pre escalar
			}
		}


		//TODO Ver ese delay
		/*
# WR2 - Write Register Group 2
#
#  D7  D6  D5  D4  D3  D2  D1  D0  BASE REGISTER BYTE
#   0   |   |   |   |   0   0   0
#       |   |   |   |
#       |   |   |   0 = Port B is memory
#       |   |   |   1 = Port B is IO
#       |   |   |
#       |   0   0 = Port B address decrements
#       |   0   1 = Port B address increments
#       |   1   0 = Port B address is fixed
#       |   1   1 = Port B address is fixed
#       |
#       V
#  D7  D6  D5  D4  D3  D2  D1  D0  PORT B VARIABLE TIMING BYTE
#   0   0   |   0   0   0   |   |
#           |               0   0 = Cycle Length = 4
#           |               0   1 = Cycle Length = 3
#           |               1   0 = Cycle Length = 2
#           |               1   1 = Do not use
#           |
#           V
#  D7  D6  D5  D4  D3  D2  D1  D0  ZXN PRESCALAR (FIXED TIME TRANSFER)
#
# The ZXN PRESCALAR is a feature of the ZXN DMA implementation.
# If non-zero, a delay will be inserted after each byte is transferred
# such that the total time needed for the transfer is at least the number
# of cycles indicated by the prescalar.  This works in both the continuous
# mode and the burst mode.
		*/

		//TEMP hacerlo de golpe. ejemplo: dmafill
		//while (transfer_length>0) {

		//TEMP hacerlo combinando tiempo con cpu
		//while (resta>=dmapre && transfer_length>0) {

			//dma_continuous=1;



        //Si prescaler=0, no se permite ejecucion de la cpu
        //TODO: gestionar otros valores de prescaler, permitiendo ejecucion de la cpu entre dos transferencias de la dma,
        //las cuales estan separadas por un tiempo determinado por el propio prescaler
        //Atic Atac usa prescaler=0
        if (datagear_dma_tbblue_prescaler==0) dma_continuous=1;

		//if (dma_continuous) printf ("Transferencia modo continuous\n");
		//else printf ("Transferencia modo burst\n");

		while ( max_bytes<0 ? datagear_condicion_transferencia(transfer_length,dma_continuous,resta,dmapre) :
                (transfer_length>0 && bytes_hechos<max_bytes) ) {
            bytes_hechos++;

			//for (i=0;i<cpu_turbo_speed;i++) {
				//printf ("dma op ");
			           z80_byte byte_leido;

                    //zxnDDMA: con relleno no se lee, se escribe el valor de $F7;
                    //con color transparente no se escriben los bytes de ese color (las direcciones avanzan igual)
                    int ddma_fill=(datagear_ddma_flags & DATAGEAR_DDMA_FLAG_FILL);
                    int ddma_transparent=(datagear_ddma_flags & DATAGEAR_DDMA_FLAG_TRANSPARENT);

                    if (datagear_wr0 & 4) {
                        //printf("DMA READ A, WRITE B\n");
                        if (ddma_fill) byte_leido=datagear_ddma_fill;
                        else byte_leido=datagear_ddma_read_operation(transfer_port_a,datagear_ddma_work_ext_a,datagear_wr1 & 8);

                        if (!ddma_transparent || byte_leido!=datagear_ddma_transparent) {
					        datagear_ddma_write_operation(transfer_port_b,datagear_ddma_work_ext_b,byte_leido,datagear_wr2 & 8);
                        }
                    }

                    else {
                        //printf("DMA READ B, WRITE A\n");
                        if (ddma_fill) byte_leido=datagear_ddma_fill;
                        else byte_leido=datagear_ddma_read_operation(transfer_port_b,datagear_ddma_work_ext_b,datagear_wr2 & 8);

                        if (!ddma_transparent || byte_leido!=datagear_ddma_transparent) {
					        datagear_ddma_write_operation(transfer_port_a,datagear_ddma_work_ext_a,byte_leido,datagear_wr1 & 8);
                        }
                    }

                    datagear_ddma_step_address(&transfer_port_a,&datagear_ddma_work_ext_a,datagear_wr1,ddma_physical);
                    datagear_ddma_step_address(&transfer_port_b,&datagear_ddma_work_ext_b,datagear_wr2,ddma_physical);

                    if (ddma_2d) {
                        datagear_ddma_row_bytes_left--;

                        if (datagear_ddma_row_bytes_left<=0) {
                            datagear_ddma_rows_left--;

                            if (datagear_ddma_rows_left<=0) {
                                //Fin del rectangulo
                                transfer_length=0;
                                datagear_ddma_2d_running=0;
                            }

                            else {
                                //Siguiente fila: la distancia entre filas se suma al inicio de la fila (24 bits)
                                datagear_ddma_row_start_a +=datagear_ddma_stride_a;
                                datagear_ddma_row_start_b +=datagear_ddma_stride_b;

                                transfer_port_a=datagear_ddma_row_start_a & 0xFFFF;
                                transfer_port_b=datagear_ddma_row_start_b & 0xFFFF;

                                //Fuera del modo fisico la direccion da la vuelta dentro de los 64K
                                if (ddma_physical) {
                                    datagear_ddma_work_ext_a=(datagear_ddma_row_start_a>>16) & 0xFF;
                                    datagear_ddma_work_ext_b=(datagear_ddma_row_start_b>>16) & 0xFF;
                                }

                                datagear_ddma_row_bytes_left=(datagear_ddma_row_length ? datagear_ddma_row_length : 1);
                            }
                        }
                    }

					else transfer_length--;

			//}

			datagear_dma_last_testados +=dmapre;

			//Ajustar a total t-estados
			//printf ("pre ajuste %d\n",datagear_dma_last_testados);
			datagear_dma_last_testados %=screen_testados_total;
			//printf ("post ajuste %d\n",datagear_dma_last_testados);

			resta=datagear_return_resta_testados(datagear_dma_last_testados,t_estados);

			//printf ("En transferencia: dmapre: %6d datagear_dma_last_testados %6d t_estados %6d resta %6d\n",	dmapre,datagear_dma_last_testados,t_estados,resta);

		//si da la vuelta
			if (resta<resta_antes) {
				//provocar fin
				resta=0;
			}


			resta_antes=resta;

		}


        //Guardar valores contadores

        //En 2D la longitud de bloque no se usa ni se toca
        if (!ddma_2d) {
            datagear_block_length_low=value_16_to_8l(transfer_length);
            datagear_block_length_high=value_16_to_8h(transfer_length);
        }

        datagear_port_a_start_addr_low=value_16_to_8l(transfer_port_a);
        datagear_port_a_start_addr_high=value_16_to_8h(transfer_port_a);

        datagear_port_b_start_addr_low=value_16_to_8l(transfer_port_b);
        datagear_port_b_start_addr_high=value_16_to_8h(transfer_port_b);


				//}

    if (transfer_length==0) {
        datagear_is_dma_transfering.v=0;
        //DBG_PRINT_DMA VERBOSE_DEBUG,"DMA: End of transfer");
        //Y reseteamos contador de bytes inicial si conviene
        //printf("WR5: %02XH\n",datagear_wr5);

        //TODO: Por alguna razón Atic Atac de Next necesita este restart pero no parece activar ese bit 0x20
        //De todas maneras recargar la longitud de la transferencia no creo que sea un problema en otros juegos que lo usen,
        //porque lo habitual es cargar todos los registros. Y el que no cargue la cantidad de bytes, ya tendrá autorestart aunque no haya activado este bit
        //if (datagear_wr5 & 0x20) {
            datagear_block_length_low=datagear_original_block_length_low;
            datagear_block_length_high=datagear_original_block_length_high;
        //}
    }

	//printf ("length: %d\n",transfer_length);

    //Estado para la lectura de la DMA (fork)
    if (bytes_hechos) {
        datagear_tbblue_status_atleastone=1;
        datagear_tbblue_byte_counter +=bytes_hechos;
    }
    if (transfer_length==0) datagear_tbblue_status_endofblock=1;

    return bytes_hechos;
}

void datagear_handle_dma(void)
{
    if (datagear_is_dma_transfering.v==0) return;

    //En TBBlue con tiempo real de la DMA: lo gestiona datagear_tbblue_handle_dma (fork)
    if (MACHINE_IS_TBBLUE) return;

    datagear_handle_dma_bytes(-1);
}


/* Fork: tiempos de la DMA de la ZX Spectrum Next (zxnDMA), medidos en una KS3 con los cores
   3.02.04 y 3.03.03 (src/tests/next/dmatest) y en el sentido de dma.vhd:
   - cada byte tarda la longitud de ciclo del puerto de origen (lectura) mas la del de destino
     (escritura), 2, 3 o 4 ciclos de CPU. A 28 MHz una lectura de memoria de 2 ciclos tarda 3
     (espera de la SRAM)
   - con prescaler, entre el inicio de un byte y el del siguiente pasan al menos prescaler*32 ciclos
     de 28 MHz, sea cual sea la velocidad de la CPU (875 kHz / prescaler bytes por segundo)
   - modo continuo (o burst sin prescaler): la CPU esta parada hasta el final del bloque. Con
     prescaler, tras cada espera se pierden 3 ciclos mas
   - modo burst con prescaler: durante la espera la CPU sigue. Al acabar la espera, la DMA toma el
     bus al final de la instruccion en curso, y tarda 3 ciclos mas el byte
*/

int datagear_tbblue_wait=0;               //t-estados que faltan para el siguiente byte
static int datagear_tbblue_last_testados=0;

static int datagear_tbblue_cycle_length(z80_byte timing)
{
    switch (timing & 3) {
        case 1: return 3;
        case 2: return 2;
        default: return 4;
    }
}

//t-estados de CPU de un byte (lectura + escritura)
static int datagear_tbblue_byte_cycles(void)
{
    z80_byte timing_origen,timing_destino;
    int origen_es_memoria;

    if (datagear_wr0 & 4) {
        //A -> B
        timing_origen=datagear_port_a_variable_timing_byte;
        timing_destino=datagear_port_b_variable_timing_byte;
        origen_es_memoria=((datagear_wr1 & 8)==0);
    }
    else {
        timing_origen=datagear_port_b_variable_timing_byte;
        timing_destino=datagear_port_a_variable_timing_byte;
        origen_es_memoria=((datagear_wr2 & 8)==0);
    }

    int lectura=datagear_tbblue_cycle_length(timing_origen);
    if (cpu_turbo_speed>=8 && origen_es_memoria && lectura<3) lectura=3;

    return lectura+datagear_tbblue_cycle_length(timing_destino);
}

//t-estados de CPU de la ventana del prescaler (0 si no hay prescaler)
static int datagear_tbblue_window(void)
{
    return datagear_dma_tbblue_prescaler*4*cpu_turbo_speed;
}

static int datagear_tbblue_burst_mode(void)
{
    return (datagear_wr4 & (64+32))==64;
}

//Si la DMA tiene el bus y la CPU no puede ejecutar instrucciones
int datagear_tbblue_cpu_stalled(void)
{
    if (!MACHINE_IS_TBBLUE) return 0;
    if (datagear_dma_emulation.v==0 || datagear_dma_is_disabled.v) return 0;
    if (datagear_is_dma_transfering.v==0) return 0;

    //En burst con prescaler la CPU sigue durante la espera
    if (datagear_tbblue_burst_mode() && datagear_dma_tbblue_prescaler) return 0;

    return 1;
}

//Un paso con la CPU parada (en vez de una instruccion): avanza t-estados y transfiere.
//Pasos cortos, para que la ULA, el copper, las interrupciones raster, etc. sigan a su ritmo
void datagear_tbblue_stall_step(void)
{
    if (datagear_tbblue_wait>0) {
        int avance=(datagear_tbblue_wait<8 ? datagear_tbblue_wait : 8);
        t_estados +=avance;
        datagear_tbblue_wait -=avance;
        datagear_tbblue_last_testados=t_estados;
        return;
    }

    int ventana=datagear_tbblue_window();
    int ciclos=datagear_tbblue_byte_cycles();
    int gastado=0;

    do {
        if (datagear_handle_dma_bytes(1)==0) break;
        gastado +=ciclos;
        if (ventana) {
            datagear_tbblue_wait=(ventana>ciclos ? ventana-ciclos : 0)+3;
            break;
        }
    } while (gastado<16 && datagear_is_dma_transfering.v);

    if (!gastado) gastado=4;
    t_estados +=gastado;
    datagear_tbblue_last_testados=t_estados;
}

//Despues de cada instruccion: modo burst con prescaler (la CPU sigue entre bytes)
void datagear_tbblue_handle_dma(void)
{
    if (datagear_is_dma_transfering.v==0) return;
    if (datagear_tbblue_cpu_stalled()) return;

    int pasado=datagear_return_resta_testados(datagear_tbblue_last_testados,t_estados);
    datagear_tbblue_last_testados=t_estados;
    datagear_tbblue_wait -=pasado;
    if (datagear_tbblue_wait>0) return;

    //Se acabo la espera: la DMA toma el bus al final de esta instruccion. El Z80 real cede el bus al
    //final de un ciclo M (como mucho unos 4 t-estados), no de la instruccion: si la espera acabo antes,
    //la ventana del byte siguiente se adelanta lo que la instruccion se paso de esos 4 t-estados
    int tarde=-datagear_tbblue_wait;
    int adelanto=(tarde>4 ? tarde-4 : 0);
    int ciclos=datagear_tbblue_byte_cycles();
    if (datagear_handle_dma_bytes(1)) {
        t_estados +=3+ciclos;
        int ventana=datagear_tbblue_window();
        datagear_tbblue_wait=(ventana>ciclos ? ventana-ciclos : 0)-adelanto;
    }
    datagear_tbblue_last_testados=t_estados;
}

//Al habilitar la DMA (0x87)
static void datagear_tbblue_enable(void)
{
    datagear_tbblue_wait=0;
    datagear_tbblue_last_testados=t_estados;
}

/* Lectura del puerto de la DMA (fork), como dma.vhd / JNEXT: segun la mascara de lectura,
   estado, contador (bytes transferidos) y direcciones actuales de los puertos A y B.
   Estado: "00" & endofblock_n & "1101" & atleastone */
z80_byte datagear_tbblue_read_mask=0x7F;
static int datagear_tbblue_read_index=0;

void datagear_tbblue_read_index_status(void)
{
    datagear_tbblue_read_index=0;
}

static void datagear_tbblue_read_advance(int despues)
{
    int i;
    for (i=1;i<=7;i++) {
        int bit=(despues+i)%7;
        if (datagear_tbblue_read_mask & (1<<bit)) {
            datagear_tbblue_read_index=bit;
            return;
        }
    }
    datagear_tbblue_read_index=0;
}

z80_byte datagear_read_port(void)
{
    if (!MACHINE_IS_TBBLUE) return 0;

    z80_byte valor=0;
    z80_int puerto_a=value_8_to_16(datagear_port_a_start_addr_high,datagear_port_a_start_addr_low);
    z80_int puerto_b=value_8_to_16(datagear_port_b_start_addr_high,datagear_port_b_start_addr_low);

    switch (datagear_tbblue_read_index) {
        case 0:
            valor=(datagear_tbblue_status_endofblock ? 0 : 0x20) | 0x1A | (datagear_tbblue_status_atleastone ? 1 : 0);
        break;
        case 1: valor=value_16_to_8l(datagear_tbblue_byte_counter); break;
        case 2: valor=value_16_to_8h(datagear_tbblue_byte_counter); break;
        case 3: valor=value_16_to_8l(puerto_a); break;
        case 4: valor=value_16_to_8h(puerto_a); break;
        case 5: valor=value_16_to_8l(puerto_b); break;
        case 6: valor=value_16_to_8h(puerto_b); break;
    }

    datagear_tbblue_read_advance(datagear_tbblue_read_index);
    return valor;
}