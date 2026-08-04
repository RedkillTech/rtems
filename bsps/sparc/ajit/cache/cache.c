/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsSPARCAJIT
 *
 * @brief Cache Manager support for the AJIT SPARC V8 processor.
 *
 *  AJIT (Reference Manual ch. 4) has two VIVT caches:
 *    - 32 KB instruction cache, 64-byte lines, write-through-allocate
 *    - 32 KB data cache,        64-byte lines, write-through-allocate
 *
 *  Per Manual §2 ("FLUSH - Flushes entire ICACHE and DCACHE"), the
 *  SPARC V8 `flush` instruction on AJIT invalidates BOTH caches
 *  entirely regardless of its address operand.  Because the D-cache
 *  is write-through, there is no dirty data to push to memory: a
 *  flush is purely an invalidation.
 *
 *  Consequences for the RTEMS cache manager:
 *    - Data-cache flush  (push dirty -> mem)  : no-op (write-through).
 *    - Data-cache invalidate                  : `flush` covers it.
 *    - Instruction-cache invalidate           : `flush`.
 *    - Per-range variants reduce to whole-cache invalidate because
 *      AJIT does not provide per-line invalidate.
 *
 *  Only the instruction-cache hooks are defined.  CPU_DATA_CACHE_ALIGNMENT is
 *  left undefined, so the data-cache operations are no-ops: a write-through
 *  cache holds no dirty data to flush, and the AJIT peripherals are reached
 *  through ASI 0x20 MMU-bypass accesses that never enter the data cache.
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

#include <stddef.h>

#define CPU_INSTRUCTION_CACHE_ALIGNMENT 0

#define CPU_CACHE_SUPPORT_PROVIDES_RANGE_FUNCTIONS

static inline void _CPU_cache_invalidate_entire_instruction( void )
{
  __asm__ volatile ( "flush" );
}

static inline void _CPU_cache_invalidate_instruction_range(
  const void *i_addr,
  size_t      n_bytes
)
{
  (void) i_addr;
  (void) n_bytes;
  __asm__ volatile ( "flush" );
}

static inline void _CPU_cache_freeze_instruction( void )
{
}

static inline void _CPU_cache_unfreeze_instruction( void )
{
}

static inline void _CPU_cache_enable_instruction( void )
{
  /*
   *  Caches are controlled by the MMU PTE cacheable bit, or by bit 8 of
   *  the MMU control register when the MMU is disabled (Manual §4.3).
   *  The AJIT default configuration leaves caches enabled; there is no
   *  documented per-cache enable/disable register.
   */
}

static inline void _CPU_cache_disable_instruction( void )
{
}

#include "../../../shared/cache/cacheimpl.h"
