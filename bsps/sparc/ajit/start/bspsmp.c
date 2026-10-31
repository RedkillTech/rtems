/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsSPARCAJIT
 *
 * @brief Symmetric Multiprocessing (SMP) support for the AJIT BSP.
 *
 * The AJIT processor is a coherent shared-memory multi-core part (up to four
 * cores of two hardware threads each, AJIT Reference Manual ch. 6).  This file
 * implements the RTEMS SMP CPU interface on top of the AJIT facilities:
 *
 *   - per-thread identity in ASR29 (manual sec. 2.3) to determine the running
 *     processor index;
 *   - the inter-processor-interrupt (IPI) register file (manual sec. 8.1.2) to
 *     deliver scheduler IPIs between threads;
 *   - the all-cores-released-at-reset boot model: every core enters the shared
 *     start.S, one wins the boot-CPU election and the rest are funneled here
 *     through bsp_start_on_secondary_processor().
 *
 * Cache coherence is maintained by the AJIT coherent memory controller
 * (snoop-invalidate, manual ch. 6), so no software snoop-enable step is
 * required.
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

#include <rtems/score/smpimpl.h>
#include <rtems/score/assert.h>
#include <rtems/irq-extension.h>
#include <rtems/sysinit.h>
#include <bsp.h>
#include <bsp/bootcard.h>
#include <bsp/irq.h>
#include <ajit.h>

/*
 * Number of cores that checked in during the early boot of shared start.S.
 * Each thread atomically increments this counter under the boot lock before
 * the boot CPU clears BSS, so by the time _CPU_SMP_Initialize() runs on the
 * boot CPU every present thread has been counted.  Defined in start.S.
 */
extern uint32_t _AJIT_CPU_Online_count;

/*
 * Per-core release gate.  Every AJIT thread is released from reset together
 * and reaches start.S on its own, so each secondary waits in
 * bsp_start_on_secondary_processor() until the boot CPU sets its flag from
 * _CPU_SMP_Start_processor().  RTEMS calls that only for processor indices
 * below the configured maximum, so a thread beyond the configured count parks
 * here rather than failing on an unassigned processor.  The memory system is
 * coherent, so a volatile flag with a compiler barrier suffices.
 */
static volatile uint32_t _AJIT_Secondary_go[ AJIT_MAX_PROCESSORS ];

/*
 * Map from dense RTEMS processor index -> physical thread number (2*core+thread).
 * The IPI mask/value register bits are indexed by the physical thread slot, not
 * by the dense RTEMS index, and the two differ whenever a core has fewer than the
 * full two threads enabled (e.g. a 1-thread-per-core bitstream: core1/thread0 is
 * RTEMS index 1 but IPI bit 2).  Each thread self-registers its entry before it
 * can be targeted by an IPI: the boot CPU in _CPU_SMP_Initialize(), each secondary
 * at the top of bsp_start_on_secondary_processor() (before it enables interrupts
 * or enters multitasking).  Coherent memory makes the stores visible to senders.
 */
static uint32_t _AJIT_Ipi_phys_thread[ AJIT_MAX_PROCESSORS ];

/*
 * Read the linear processor index from ASR29.
 *
 *   ASR29 = [31:16] identification tag
 *           [15:8]  core id
 *           [7:0]   thread id
 *
 * The TIC register map and RTEMS both expect a linear index, which the manual
 * defines (sec. 8.1.1) as 2*core + thread for the two-thread-per-core parts.
 */
uint32_t _CPU_SMP_Get_current_processor( void )
{
  uint32_t asr29;

  __asm__ __volatile__( "rd %%asr29, %0" : "=r"( asr29 ) );

  return ( ( asr29 >> 8 ) & 0xFFU ) * AJIT_THREADS_PER_CORE
       + ( asr29 & 0xFFU );
}

uint32_t _CPU_SMP_Initialize( void )
{
  uint32_t count = _AJIT_CPU_Online_count;

  if ( count == 0 ) {
    count = 1;
  }

  /* Boot CPU records its own dense-index -> physical-thread IPI mapping. */
  _AJIT_Ipi_phys_thread[ _CPU_SMP_Get_current_processor() ] =
    ajit_physical_thread_id();

  return count;
}

bool _CPU_SMP_Start_processor( uint32_t cpu_index )
{
  /*
   * All AJIT threads are released from reset together and are already waiting
   * in bsp_start_on_secondary_processor() for their go flag.  There is no wake
   * register to poke; setting the flag authorizes this processor to proceed.
   * RTEMS calls this only for processors within the configured maximum, so any
   * surplus thread keeps spinning and never enters multitasking.
   */
  _Assert( cpu_index < AJIT_MAX_PROCESSORS );
  _AJIT_Secondary_go[ cpu_index ] = 1;
  __asm__ __volatile__( "" ::: "memory" );

  return true;
}

void _CPU_SMP_Prepare_start_multitasking( void )
{
  /* Nothing to do. */
}

/*
 * Acknowledge an IPI on the current thread: clear this thread's bit in the IPI
 * value register.  This drops the level-11 line for the thread (manual sec.
 * 8.1.2 receiver sequence step 4).  Must be called with the IPI lock held.
 */
static void ajit_ipi_ack( uint32_t phys_thread )
{
  uint32_t value = ajit_read_ipi_value();

  ajit_write_ipi_value( value & ~( 1U << phys_thread ) );
}

void _CPU_SMP_Send_interrupt( uint32_t target_processor_index )
{
  uint32_t bit;
  uint32_t level;
  uint32_t mask;

  /*
   * IPI send sequence (AJIT Reference Manual sec. 8.1.2).  The IPI to a thread
   * is raised on the 0->1 transition of (mask AND value) for that thread.
   * Clearing the destination mask bit, setting its value bit, then setting the
   * mask bit again guarantees a rising edge regardless of the prior state.
   */
  level = sparc_disable_interrupts();
  bit = 1U << _AJIT_Ipi_phys_thread[ target_processor_index ];
  ajit_ipi_lock_acquire();

  mask = ajit_read_ipi_mask();
  ajit_write_ipi_mask( mask & ~bit );                  /* clear dst mask bit  */
  ajit_write_ipi_value( ajit_read_ipi_value() | bit ); /* set dst value bit   */
  ajit_write_ipi_mask( ( mask & ~bit ) | bit );        /* set dst mask -> IPI */

  ajit_ipi_lock_release();
  sparc_enable_interrupts( level );
}

void bsp_start_on_secondary_processor( Per_CPU_Control *cpu_self )
{
  uint32_t cpu_index = _CPU_SMP_Get_current_processor();

  /*
   * Wait until the boot CPU authorizes this processor from
   * _CPU_SMP_Start_processor().  A surplus thread (index >= configured maximum)
   * is never authorized and parks here, which keeps it out of multitasking
   * instead of fataling on an unassigned/invalid processor.
   */
  while ( _AJIT_Secondary_go[ cpu_index ] == 0 ) {
    /* spin -- coherent memory makes the boot CPU's store visible */
  }

  /*
   * Record this processor's dense-index -> physical-thread mapping before it
   * enables interrupts or enters multitasking, so any IPI later targeted at this
   * processor sets the correct (physical-slot) bit in the IPI value register.
   */
  _AJIT_Ipi_phys_thread[ cpu_index ] = ajit_physical_thread_id();

  /*
   * Enable this thread's TIC.  Each secondary needs:
   *   - the IPI level (11) for scheduler inter-processor interrupts, and
   *   - the timer level (10) so it receives the periodic tick and runs its own
   *     per-CPU watchdog chain (the boot CPU still owns advancing time and
   *     re-arming the single shared timer -- see clock/ckinit.c).
   * The console (UART) interrupt stays masked here and is owned by the boot CPU.
   * Writing through the per-CPU mirror keeps these confined to this thread.
   */
  AJIT_Unmask_interrupt( AJIT_INTERRUPT_IPI );
  AJIT_Unmask_interrupt( AJIT_INTERRUPT_TIMER );

  _SMP_Start_multitasking_on_secondary_processor( cpu_self );
}

static void ajit_inter_processor_interrupt( void *arg )
{
  (void) arg;

  ajit_ipi_lock_acquire();
  ajit_ipi_ack( ajit_physical_thread_id() );
  ajit_ipi_lock_release();
  ajit_interrupt_ack();

  _SMP_Inter_processor_interrupt_handler( _Per_CPU_Get() );
}

void _CPU_SMP_Finalize_initialization( uint32_t cpu_count )
{
  /*
   * The IPI handler is installed from a SYSINIT item below, because this
   * function runs from _SMP_Handler_initialize() at RTEMS_SYSINIT_DATA_STRUCTURES,
   * which is before the generic interrupt framework is initialized at
   * RTEMS_SYSINIT_BSP_PRE_DRIVERS.
   */
  (void) cpu_count;
}

static rtems_interrupt_entry ajit_inter_processor_interrupt_entry =
  RTEMS_INTERRUPT_ENTRY_INITIALIZER(
    ajit_inter_processor_interrupt,
    NULL,
    "IPI"
  );

/*
 * Install the IPI handler on the boot CPU and unmask level 11 in its TIC.
 * Secondary threads unmask the IPI level on their own TIC when they come up in
 * bsp_start_on_secondary_processor().  This runs after bsp_interrupt_initialize()
 * but before the boot CPU starts multitasking (and therefore before any IPI can
 * be delivered), so the handler is always in place when the first IPI arrives.
 */
static void ajit_install_inter_processor_interrupt( void )
{
  rtems_status_code sc;

  sc = rtems_interrupt_entry_install(
    AJIT_INTERRUPT_IPI,
    RTEMS_INTERRUPT_SHARED,
    &ajit_inter_processor_interrupt_entry
  );
  _Assert_Unused_variable_equals( sc, RTEMS_SUCCESSFUL );
  (void) sc;
}

RTEMS_SYSINIT_ITEM(
  ajit_install_inter_processor_interrupt,
  RTEMS_SYSINIT_BSP_PRE_DRIVERS,
  RTEMS_SYSINIT_ORDER_LAST_BUT_5
);
