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

  /* Disable this thread's TIC and clear its mask mirror. */
  ajit_write_interrupt_controller_control( 0 );
  AJIT_TIC_MIRROR = 0;

  /*
   * Disable the count-down timer with ajit_timer_stop(), which writes 0 and
   * leaves the enable bit clear.  ajit_timer_start(0) must not be used here:
   * it sets the enable bit with a zero reload and fires an immediate one-shot
   * level-10 pulse.  That pulse latches in every thread's interrupt controller
   * and, under SMP, is later delivered as a spurious timer interrupt when an
   * IPI raises a secondary thread's interrupt line.
   */
  ajit_timer_stop();

  /*
   *  Set the system clock rate (MHz) used for the count-down timer reload
   *  and for converting ASR31 cycle counts to time.  The value comes from
   *  the AJIT_CLOCK_SPEED_MHZ build option and is stored in the CLOCK_SPEED
   *  word reserved in bsps/sparc/shared/start/start.S, which the BSP
   *  populates before the clock driver runs.
   */
  CLOCK_SPEED = AJIT_CLOCK_SPEED_MHZ;

  ajit_uart_init();
}
