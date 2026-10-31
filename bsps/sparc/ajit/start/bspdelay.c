/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsSPARCAJIT
 *
 * @brief Busy-wait delay for the AJIT BSP.
 *
 * The delay spins on AJIT's free-running 64-bit cycle counter
 * (ASR[31:30], Reference Manual 2.2), read through the SPARC
 * _CPU_Counter_read() interface.  This is the same time source the
 * benchmark timer uses (see btimer.c and clock/ckinit.c), so the delay
 * is accurate to the cycle instead of relying on a hand-counted
 * instruction loop.
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
#include <stdint.h>
#include <rtems/score/cpu.h>

extern int CLOCK_SPEED;   /* core clock in MHz */

void rtems_bsp_delay(int usecs)
{
  CPU_Counter_ticks start;
  uint32_t          target;

  if (usecs <= 0) {
    return;
  }

  /*
   * usecs microseconds == usecs * CLOCK_SPEED cycles at CLOCK_SPEED MHz.
   * Spin until that many cycles have elapsed on the free-running counter.
   */
  start  = _CPU_Counter_read();
  target = (uint32_t) usecs * (uint32_t) CLOCK_SPEED;

  while ((uint32_t) (_CPU_Counter_read() - start) < target) {
    /* busy-wait on the cycle counter */
  }
}
