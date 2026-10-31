/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsSPARCAJIT
 *
 * @brief Global BSP definitions.
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

#ifndef LIBBSP_SPARC_AJIT_BSP_H
#define LIBBSP_SPARC_AJIT_BSP_H

/**
 * @defgroup RTEMSBSPsSPARCAJIT AJIT
 *
 * @ingroup RTEMSBSPsSPARC
 *
 * @brief AJIT Board Support Package (IIT Bombay SPARC V8 Processor).
 *
 * @{
 */

#include <bspopts.h>
#include <bsp/default-initial-extension.h>

#include <rtems.h>
#include <ajit.h>
#include <rtems/irq-extension.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_FEATURE_IRQ_EXTENSION

/*
 *  BSP provides its own Idle thread body
 */
void *bsp_idle_thread( uintptr_t ignored );
#define BSP_IDLE_TASK_BODY bsp_idle_thread

/* Constants */

/*
 *  Information placed in the linkcmds file.
 */

extern int   RAM_START;
extern int   RAM_END;
extern int   RAM_SIZE;

extern int   PROM_START;
extern int   PROM_END;
extern int   PROM_SIZE;

extern int   CLOCK_SPEED;

extern int   end;        /* last address in the program */

/* functions */

void SPARC_Clear_and_unmask_interrupt(rtems_vector_number vector);

void BSP_fatal_exit(uint32_t error);

/* Interrupt Service Routine (ISR) pointer */
typedef void (*bsp_shared_isr)(void *arg);

RTEMS_DEPRECATED static inline int BSP_shared_interrupt_register
       (
       int irq,
       const char *info,
       bsp_shared_isr isr,
       void *arg
       )
{
       return rtems_interrupt_handler_install(irq, info,
                                       RTEMS_INTERRUPT_SHARED, isr, arg);
}

RTEMS_DEPRECATED static inline int BSP_shared_interrupt_unregister
       (
       int irq,
       bsp_shared_isr isr,
       void *arg
       )
{
       return rtems_interrupt_handler_remove(irq, isr, arg);
}

RTEMS_DEPRECATED static inline void BSP_shared_interrupt_clear( int irq )
{
  (void) rtems_interrupt_clear( (rtems_vector_number) irq );
}

RTEMS_DEPRECATED static inline void BSP_shared_interrupt_unmask( int irq )
{
  (void) rtems_interrupt_vector_enable( (rtems_vector_number) irq );
}

RTEMS_DEPRECATED static inline void BSP_shared_interrupt_mask( int irq )
{
  (void) rtems_interrupt_vector_disable( (rtems_vector_number) irq );
}

/*
 *  Delay for the specified number of microseconds.
 */
void rtems_bsp_delay(int usecs);

/*
 * Prototypes for methods used across file boundaries
 */
void console_outbyte_polled(int  port, unsigned char ch);
int console_inbyte_nonblocking(int port);

#ifdef __cplusplus
}
#endif

/** @} */

#endif
