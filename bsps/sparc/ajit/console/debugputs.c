/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Copyright (C) 2026 Shaurya Rane
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <bsp.h>
#include <rtems/libio.h>
#include <stdlib.h>

/*
 *  console_outbyte_polled
 *
 *  This routine transmits a character using polling.
 */
static int ajit_uart_initialized = 0;

static void ajit_uart_init_once( void )
{
  if ( !ajit_uart_initialized ) {
    ajit_uart_init();
    ajit_uart_initialized = 1;
  }
}

void console_outbyte_polled(
  int  port,
  unsigned char ch
)
{
  if ( port != 0 ) {
    return;
  }

  ajit_uart_init_once();

  while ( (ajit_read_serial_control() & AJIT_UART_CONTROL_TX_FULL) != 0 ) {
    /* Wait for room in TX */
  }

  ajit_write_serial_tx( ch );
}

/*
 *  console_inbyte_nonblocking
 *
 *  This routine polls for a character.
 */
int console_inbyte_nonblocking( int port )
{
  uint32_t control;

  if ( port != 0 ) {
    rtems_fatal_error_occurred( 'D' << 8 | (port & 0xffffff) );
  }

  control = ajit_read_serial_control();
  if ( (control & AJIT_UART_CONTROL_RX_ENABLE) == 0 ) {
    ajit_write_serial_control(
      control | AJIT_UART_CONTROL_TX_ENABLE | AJIT_UART_CONTROL_RX_ENABLE
    );
    control = ajit_read_serial_control();
  }

  if ( (control & AJIT_UART_CONTROL_RX_FULL) == 0 ) {
    return -1;
  }

  return (int) ajit_read_serial_rx();
}

/*
 *  To support printk
 */

#include <rtems/bspIo.h>

static void BSP_output_char_f(char c) { console_outbyte_polled( 0, c ); }

/*
 *  Polled input for getchark(): the "press any key" prompts and the RTEMS
 *  shell read characters through BSP_poll_char.  Returns the next character
 *  from the UART, or -1 if none is waiting.
 */
static int BSP_poll_char_f( void ) { return console_inbyte_nonblocking( 0 ); }

BSP_output_char_function_type           BSP_output_char = BSP_output_char_f;
BSP_polling_getchar_function_type       BSP_poll_char = BSP_poll_char_f;
