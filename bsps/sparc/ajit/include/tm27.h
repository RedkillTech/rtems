/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 * @ingroup RTEMSBSPsSPARCAJIT
 * @brief Implementations for interrupt mechanisms for Time Test 27
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

#ifndef _RTEMS_TMTEST27
#error "This is an RTEMS internal file you must not include directly."
#endif

#ifndef __tm27_h
#define __tm27_h

/*
 *  Define the interrupt mechanism for Time Test 27.
 *
 *  The AJIT TIC has no software force-interrupt register, so the test uses
 *  a trap instruction (ta) to generate a synchronous software trap, which
 *  is sufficient for the tm27 benchmark.
 */

#define TEST_INTERRUPT_SOURCE AJIT_INTERRUPT_EXTERNAL_1
#define TEST_INTERRUPT_SOURCE2 (AJIT_INTERRUPT_EXTERNAL_1+1)

#define MUST_WAIT_FOR_INTERRUPT 0

static inline void Install_tm27_vector( rtems_interrupt_handler handler )
{
  (void) rtems_interrupt_handler_install(
    TEST_INTERRUPT_SOURCE,
    "tm27 low",
    RTEMS_INTERRUPT_SHARED,
    handler,
    NULL
  );
  (void) rtems_interrupt_handler_install(
    TEST_INTERRUPT_SOURCE2,
    "tm27 high",
    RTEMS_INTERRUPT_SHARED,
    handler,
    NULL
  );
}

#define Cause_tm27_intr() \
  do { \
    __asm__ __volatile__( "ta 0x02" ::: "memory" ); \
  } while (0)

#define Clear_tm27_intr() /* nothing to clear for software trap */

#define Lower_tm27_intr() /* empty */

#endif
