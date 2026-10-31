/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsSPARCAJIT
 *
 * @brief This header file provides interfaces used by the interrupt support
 *   implementation.
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

#ifndef LIBBSP_SPARC_AJIT_BSP_IRQIMPL_H
#define LIBBSP_SPARC_AJIT_BSP_IRQIMPL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup RTEMSBSPsSPARCAJIT
 *
 * @{
 */

/*
 *  AJIT interrupt handling notes
 *  -----------------------------
 *  1. Trap dispatch flows: trap 0x10+N -> _SPARC_Interrupt_dispatch(N) ->
 *     bsp_interrupt_handler_dispatch_unchecked(N) -> registered handler.
 *
 *  2. After servicing the peripheral, every AJIT ISR MUST call
 *     ajit_interrupt_ack() (defined in <ajit.h>) before returning.
 *     The AJIT TIC stays in IRC_INTERRUPTING until toggled, so without
 *     the ack the same trap fires again immediately on rett.
 *
 *  3. rtems_interrupt_raise() / BSP_Force_interrupt() are no-ops on AJIT:
 *     the TIC has no software-force register.  Tests requiring forced
 *     interrupts (e.g. tm27) must use the SPARC `ta` software-trap path.
 */

static inline uint32_t bsp_irq_fixup( uint32_t irq )
{
  return irq;
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* LIBBSP_SPARC_AJIT_BSP_IRQIMPL_H */
