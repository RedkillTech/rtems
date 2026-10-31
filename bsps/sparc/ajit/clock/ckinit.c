/* SPDX-License-Identifier: BSD-2-Clause */

/*
 *  Clock driver for the AJIT processor's on-chip countdown timer.
 *
 *  The AJIT timer (Ch. 8.2 of the reference manual) is a single 32-bit
 *  register at 0xFFFF3100:
 *    [31:1]  max-timer-count
 *    [0]     enable bit
 *
 *  When enabled, it counts down from max-timer-count to 0 and asserts
 *  interrupt level 10 (TIMER_IRL = 0xA).  To re-arm the timer, the CPU
 *  must write the register again with enable=1.
 *
 *  The timer is configured to fire every
 *  rtems_configuration_get_microseconds_per_tick() microseconds.  At
 *  CLOCK_SPEED MHz, one microsecond equals CLOCK_SPEED cycles.
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
#include <bsp/sparc-counter.h>
#include <rtems/irq-extension.h>
#include <rtems/sysinit.h>
#include <rtems/timecounter.h>
#if defined(RTEMS_SMP)
#include <rtems/score/percpu.h>
#endif

extern int CLOCK_SPEED;

/*
 *  Timecounter source: SPARC ASR30 (AjitReferenceManual Ch. 2.2).
 *
 *  ASR30:31 is a 64-bit free-running counter running at the processor
 *  clock.  The low 32 bits are read through _CPU_Counter_read() and the
 *  timecounter frequency is set to CLOCK_SPEED * 1e6, so RTEMS converts
 *  counter ticks back to time correctly at cycle resolution.
 */
static uint32_t ajit_us_per_tick;

/* Cycles the count-down timer is (re)loaded with for one tick. */
static uint32_t ajit_tc_reload;

static struct timecounter ajit_tc;

static uint32_t ajit_tc_get_timecount( struct timecounter *tc )
{
  (void) tc;
  return (uint32_t) _CPU_Counter_read();
}

static void ajit_clock_init( void )
{
  struct timecounter *tc;

  tc = &ajit_tc;
  tc->tc_get_timecount = ajit_tc_get_timecount;
  tc->tc_counter_mask = 0xffffffff;
  tc->tc_frequency = _CPU_Counter_frequency();
  tc->tc_quality = RTEMS_TIMECOUNTER_QUALITY_CLOCK_DRIVER;
  rtems_timecounter_install( tc );

  /*
   *  Start the count-down timer and unmask the timer IRQ on the TIC
   *  only AFTER clockimpl.h has installed the Clock_isr handler (it
   *  calls Clock_driver_support_install_isr before initialize_hardware).
   *  Doing this in ajit_clock_initialize_early (RTEMS_SYSINIT_CPU_COUNTER)
   *  would leave a window between unmask and handler install during which
   *  a timer tick is delivered as a spurious interrupt.
   */
  ajit_timer_start( ajit_tc_reload );
  AJIT_Unmask_interrupt( AJIT_INTERRUPT_TIMER );
}

uint32_t _CPU_Counter_frequency( void )
{
  return (uint32_t) CLOCK_SPEED * 1000000U;
}

/*
 *  Re-arm the AJIT countdown timer for the next tick.
 *  Also acknowledge the interrupt by cycling the TIC.
 *
 *  AJIT has a single shared count-down timer.  Under SMP every thread receives
 *  the tick (each acknowledges it on its own TIC), but only the boot processor
 *  re-arms the one timer -- otherwise the secondaries would keep resetting the
 *  shared count-down and skew the tick period.
 */
static void ajit_clock_at_tick( void )
{
  ajit_interrupt_ack();
#if defined(RTEMS_SMP)
  if ( _Per_CPU_Is_boot_processor( _Per_CPU_Get() ) )
#endif
    ajit_timer_start( ajit_tc_reload );
}

/*
 *  Under SMP every thread receives the timer interrupt and runs the clock ISR,
 *  so the default Clock_driver_timecounter_tick() (rtems_timecounter_tick() ->
 *  _Timecounter_Tick()) already does the right thing per CPU: it advances the
 *  timecounter on the boot processor only and services each processor's own
 *  watchdog chain.  No BSP override is needed here.
 */

static void ajit_clock_initialize_early( void )
{
  /*
   *  Compute reload + microseconds-per-tick before the timecounter is
   *  installed.  Hardware enable is deferred to ajit_clock_init() which
   *  runs from clockimpl.h's initialize_hardware hook (after the ISR is
   *  registered) -- see note there.
   */
  ajit_us_per_tick = rtems_configuration_get_microseconds_per_tick();

  /* Cycles per tick = us_per_tick * CLOCK_SPEED (MHz). */
  ajit_tc_reload = ajit_us_per_tick * (uint32_t) CLOCK_SPEED;
}

RTEMS_SYSINIT_ITEM(
  ajit_clock_initialize_early,
  RTEMS_SYSINIT_CPU_COUNTER,
  RTEMS_SYSINIT_ORDER_FIRST
);

/*
 *  The countdown timer uses interrupt level 10.
 */
#define CLOCK_VECTOR AJIT_TRAP_TYPE( AJIT_INTERRUPT_TIMER )

/*
 *  Install the clock ISR.  The handler passed in by clockimpl.h (Clock_isr)
 *  calls Clock_driver_support_at_tick() -- ajit_clock_at_tick() -- and
 *  rtems_timecounter_tick(), which advances RTEMS time.
 */
#define Clock_driver_support_install_isr( _new ) \
  (void) rtems_interrupt_handler_install( \
    AJIT_INTERRUPT_TIMER, \
    "Clock", \
    RTEMS_INTERRUPT_UNIQUE, \
    _new, \
    NULL \
  )

#define Clock_driver_support_set_interrupt_affinity( _online_processors ) \
  do { \
    (void) _online_processors; \
  } while (0)

#define Clock_driver_support_at_tick(arg) \
  ajit_clock_at_tick()

#define Clock_driver_support_initialize_hardware() ajit_clock_init()

#include "../../../shared/dev/clock/clockimpl.h"

/*
 *  CPU counter source: SPARC ASR30:31 (AJIT Reference Manual §2.2).
 *
 *  ASR30:31 is a 64-bit free-running counter that increments at the
 *  processor clock; writes are ignored.  The low 32 bits (ASR31) match the
 *  SPARC CPU_Counter_ticks width and advance on every cycle, wrapping
 *  cleanly under the timecounter framework's tc_counter_mask.  This provides
 *  cycle-resolution _CPU_Counter_read() used by rtems_counter_delay_*() and
 *  RTEMS profiling.
 */
static CPU_Counter_ticks ajit_counter_read_asr31( void )
{
  CPU_Counter_ticks value;
  __asm__ volatile ( "rd %%asr31, %0" : "=r" (value) );
  return value;
}

SPARC_Counter _SPARC_Counter = {
  .read_isr_disabled = ajit_counter_read_asr31,
  .read              = ajit_counter_read_asr31
};
