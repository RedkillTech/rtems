/* SPDX-License-Identifier: BSD-2-Clause */

/**
 *  @file
 *
 *  @ingroup RTEMSBSPsSPARCAJIT
 *
 *  @brief Benchmark Timer support for the AJIT BSP.
 *
 *  Benchmark timing uses the SPARC CPU counter (cpuclock cycles), which
 *  this BSP wires to AJIT's free-running 64-bit counter in ASR[31:30]
 *  (Reference Manual §2.2).  See bsps/sparc/ajit/clock/ckinit.c for the
 *  _SPARC_Counter initialization that backs _CPU_Counter_read().
 *
 *  Returned units are microseconds, obtained by dividing the elapsed cycle
 *  count by CLOCK_SPEED (MHz).
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
#include <rtems/btimer.h>
#include <rtems/score/cpu.h>

bool benchmark_timer_find_average_overhead;

static CPU_Counter_ticks benchmark_timer_start_value;

#define AVG_OVERHEAD     12  /* cycles overhead for start/stop */
#define LEAST_VALID      13

void benchmark_timer_initialize( void )
{
  benchmark_timer_start_value = _CPU_Counter_read();
}

benchmark_timer_t benchmark_timer_read( void )
{
  CPU_Counter_ticks now = _CPU_Counter_read();
  uint32_t          cycles = (uint32_t) ( now - benchmark_timer_start_value );
  uint32_t          us;

  /* Cycles -> microseconds at CLOCK_SPEED MHz. */
  us = cycles / (uint32_t) CLOCK_SPEED;

  if ( benchmark_timer_find_average_overhead ) {
    return us;
  }

  if ( us < LEAST_VALID ) {
    return 0;
  }
  return us - AVG_OVERHEAD;
}

void benchmark_timer_disable_subtracting_average_overhead( bool find_flag )
{
  benchmark_timer_find_average_overhead = find_flag;
}
