/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsSPARCAJIT
 *
 * @brief This file contains the shared BSP startup code for AJIT.
 *
 * Installs the BSP pre-driver hook.
 */

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
#include <bsp/bootcard.h>
#include <bsp/irq-generic.h>
#include <rtems/sysinit.h>

/*
 * Initialize shared interrupt handling, must be done after IRQ controller has
 * been found and initialized.
 */
RTEMS_SYSINIT_ITEM(
  bsp_interrupt_initialize,
  RTEMS_SYSINIT_BSP_PRE_DRIVERS,
  RTEMS_SYSINIT_ORDER_MIDDLE
);

void bsp_start(void)
{
  /*
   *  Bring the on-chip peripherals to a known state before any driver
   *  touches them.  After a warm restart the TIC mask and the count-down
   *  timer may still hold values from a previous run.
   */

  /* Disable the TIC (core 0 / thread 0) and clear its mirror. */
  ajit_write_interrupt_controller_control( 0 );
  _AJIT_TIC_Mirror = 0;

  /* Disable the count-down timer (bit 0 = 0). */
  ajit_timer_start( 0 );

  /*
   *  Set the system clock rate (MHz) that the count-down timer reload, the
   *  ASR30 cycle counter and the UART baud generator derive from.  The value
   *  is stored in the CLOCK_SPEED word reserved in
   *  bsps/sparc/shared/start/start.S and is set here before the clock driver
   *  reads it during sysinit.
   */
  CLOCK_SPEED = 80;

  ajit_uart_init();
}
