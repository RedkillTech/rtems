/* SPDX-License-Identifier: BSD-2-Clause */

/*
 *  AJIT-specific implementation of the generic BSP interrupt interface.
 *
 *  Based on bsps/sparc/shared/irq/irq-shared.c (Cobham Gaisler).  The AJIT
 *  TIC (AJIT Reference Manual §8.1.1) has no software-force register, so
 *  rtems_interrupt_raise() and rtems_interrupt_clear() cannot be honored:
 *  the interrupt attributes report can_raise = false and the operations
 *  return RTEMS_UNSATISFIED rather than silently succeeding.  The remaining
 *  operations (mask/unmask, pending query, is_enabled) match the shared
 *  SPARC implementation.
 */

/*
 * Copyright (C) 2026 Shaurya Rane
 * Copyright (C) 2012-2015 Cobham Gaisler
 *
 * Based on bsps/sparc/shared/irq/irq-shared.c
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
#include <bsp/irq-generic.h>
#include <rtems/score/processormaskimpl.h>

static bool is_maskable(rtems_vector_number vector)
{
  return vector != 15;
}

bool bsp_interrupt_is_valid_vector(rtems_vector_number vector)
{
  if (vector == 0) {
    return false;
  }
  return vector <= BSP_INTERRUPT_VECTOR_MAX_STD;
}

void bsp_interrupt_facility_initialize(void)
{
  /* TIC and mirror are zeroed by bsp_start(); nothing to do here. */
}

rtems_status_code bsp_interrupt_get_attributes(
  rtems_vector_number         vector,
  rtems_interrupt_attributes *attributes
)
{
  attributes->is_maskable = is_maskable(vector);
  attributes->can_enable = true;
  attributes->maybe_enable = true;
  attributes->can_disable = is_maskable(vector);
  attributes->maybe_disable = is_maskable(vector);
  /*
   *  AJIT TIC has no software-force register: raise/clear are unsupported.
   *  An interrupt becomes pending only when its peripheral source-line is
   *  asserted, and is cleared by servicing that peripheral.
   */
  attributes->can_raise = false;
  attributes->can_raise_on = false;
  attributes->can_clear = false;
  attributes->cleared_by_acknowledge = true;
  attributes->can_set_affinity = false;
  return RTEMS_SUCCESSFUL;
}

rtems_status_code bsp_interrupt_is_pending(
  rtems_vector_number vector,
  bool               *pending
)
{
  bsp_interrupt_assert(bsp_interrupt_is_valid_vector(vector));
  bsp_interrupt_assert(pending != NULL);
  *pending = BSP_Is_interrupt_pending(vector) != 0;
  return RTEMS_SUCCESSFUL;
}

rtems_status_code bsp_interrupt_raise(rtems_vector_number vector)
{
  (void) vector;
  return RTEMS_UNSATISFIED;
}

#if defined(RTEMS_SMP)
rtems_status_code bsp_interrupt_raise_on(
  rtems_vector_number vector,
  uint32_t            cpu_index
)
{
  (void) vector;
  (void) cpu_index;
  return RTEMS_UNSATISFIED;
}
#endif

rtems_status_code bsp_interrupt_clear(rtems_vector_number vector)
{
  (void) vector;
  return RTEMS_UNSATISFIED;
}

rtems_status_code bsp_interrupt_vector_is_enabled(
  rtems_vector_number vector,
  bool               *enabled
)
{
  bsp_interrupt_assert(bsp_interrupt_is_valid_vector(vector));
  bsp_interrupt_assert(enabled != NULL);
  *enabled = !BSP_Cpu_Is_interrupt_masked(vector, 0);
  return RTEMS_SUCCESSFUL;
}

rtems_status_code bsp_interrupt_vector_enable(rtems_vector_number vector)
{
  bsp_interrupt_assert(bsp_interrupt_is_valid_vector(vector));
  BSP_Cpu_Unmask_interrupt(vector, 0);
  return RTEMS_SUCCESSFUL;
}

rtems_status_code bsp_interrupt_vector_disable(rtems_vector_number vector)
{
  bsp_interrupt_assert(bsp_interrupt_is_valid_vector(vector));
  if (!is_maskable(vector)) {
    return RTEMS_UNSATISFIED;
  }
  BSP_Cpu_Mask_interrupt(vector, 0);
  return RTEMS_SUCCESSFUL;
}

rtems_status_code bsp_interrupt_set_priority(
  rtems_vector_number vector,
  uint32_t            priority
)
{
  (void) vector;
  (void) priority;
  return RTEMS_UNSATISFIED;
}

rtems_status_code bsp_interrupt_get_priority(
  rtems_vector_number vector,
  uint32_t           *priority
)
{
  (void) vector;
  (void) priority;
  return RTEMS_UNSATISFIED;
}

#if defined(RTEMS_SMP)
rtems_status_code bsp_interrupt_get_affinity(
  rtems_vector_number  vector,
  Processor_mask      *affinity
)
{
  (void) vector;
  _Processor_mask_From_index(affinity, 0);
  return RTEMS_UNSATISFIED;
}

rtems_status_code bsp_interrupt_set_affinity(
  rtems_vector_number   vector,
  const Processor_mask *affinity
)
{
  (void) vector;
  (void) affinity;
  return RTEMS_UNSATISFIED;
}
#endif
