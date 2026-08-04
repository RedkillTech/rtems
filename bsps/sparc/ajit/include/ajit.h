/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 * @ingroup RTEMSBSPsSPARCAJIT
 * @brief Information pertaining to the AJIT processor.
 *
 * The AJIT is a SPARC V8 implementation designed by IIT Bombay.
 *
 * Peripheral register layout is derived from:
 *   - AjitReferenceManual.pdf (Madhav P. Desai, Sept 2024)
 *   - Chapter 8: Basic peripherals available for AJIT systems
 *   - Chapter 12: FPGA prototyping platforms (VC709/KC705 memory maps)
 *
 * The AJIT system has three separate peripherals, each accessed via
 * MMU-bypass loads/stores (ASI 0x20):
 *
 *   - Interrupt controller : 0xFFFF3000 (one 32-bit TIC register per thread)
 *   - Timer (count-down)   : 0xFFFF3100 (single 32-bit control register)
 *   - Serial device        : 0xFFFF3200 control, 0xFFFF3204 TX, 0xFFFF3208 RX
 *
 * There is no contiguous "register block"; each peripheral is independent.
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

#ifndef _INCLUDE_AJIT_h
#define _INCLUDE_AJIT_h

#if defined(__ASSEMBLER__) && !defined(ASM)
  #define ASM 1
#endif

#ifndef ASM
#include <stdint.h>
#include <rtems/score/sparc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Interrupt Sources
 *
 *  Per the AJIT reference manual (Ch. 8.1, 8.2, 8.3):
 *    Timer  -> interrupt level 10 (0xA), TIMER_IRL in the manual
 *    Serial -> interrupt level 12
 *
 *  Interrupt levels 1-14 are maskable, level 15 is NMI.
 *  The TIC register interrupt vector field [31:17] encodes active sources.
 */

#define AJIT_INTERRUPT_EXTERNAL_1                2
#define AJIT_INTERRUPT_EXTERNAL_2                3
/*
 *  The on-chip serial device asserts a single interrupt at level 12
 *  (AJIT Reference Manual §8.3).  There is no separate error vector.
 */
#define AJIT_INTERRUPT_UART_RX_TX               12
#define AJIT_INTERRUPT_TIMER                    10
#define AJIT_INTERRUPT_GENERAL_PURPOSE_TIMER    10
#define AJIT_INTERRUPT_REAL_TIME_CLOCK          10
#define AJIT_INTERRUPT_WATCHDOG_TIMEOUT         15

/*
 *  AJIT memory-mapped peripheral addresses.
 *  Reference: Ch. 12.1.2 / 12.2.1 memory maps.
 *
 *  All accessed via MMU-bypass (ASI 0x20).
 */

/* Interrupt controller -- per-thread TIC registers (Ch. 8.1.1) */
#define AJIT_TIC_BASE                              0xFFFF3000U
#define AJIT_TIC_CORE0_THREAD0                     0xFFFF3000U
#define AJIT_TIC_CORE0_THREAD1                     0xFFFF3004U
#define AJIT_TIC_CORE1_THREAD0                     0xFFFF3008U
#define AJIT_TIC_CORE1_THREAD1                     0xFFFF300CU

/* IPI registers (Ch. 8.1.2) */
#define AJIT_IPI_MASK                              0xFFFF3080U
#define AJIT_IPI_VALUE                             0xFFFF3084U

/* Timer -- single 32-bit countdown register (Ch. 8.2) */
#define AJIT_TIMER_REGISTER                        0xFFFF3100U

/* Serial device (Ch. 8.3) */
#define AJIT_SERIAL_CONTROL_REGISTER               0xFFFF3200U
#define AJIT_SERIAL_TX_REGISTER                    0xFFFF3204U
#define AJIT_SERIAL_RX_REGISTER                    0xFFFF3208U
#define AJIT_SERIAL_BAUD_LIMIT_REGISTER            0xFFFF320CU
#define AJIT_SERIAL_BAUD_FREQ_REGISTER             0xFFFF3210U

/* Scratch pad (Ch. 12) */
#define AJIT_SCRATCH_PAD_BASE                      0xFFFF2C00U

/* Aliases referenced by the shared SPARC code */
#define AJIT_INTERRUPT_CONTROLLER_CONTROL_REGISTER AJIT_TIC_CORE0_THREAD0
#define AJIT_TIMER_CONTROL_REGISTER                AJIT_TIMER_REGISTER

/* MMU bypass ASI */
#define AJIT_ASI_MMU_BYPASS                        0x20

#ifndef ASM

/*
 *  Trap Types for on-chip peripherals
 */

#define AJIT_TRAP_TYPE( _source ) SPARC_INTERRUPT_SOURCE_TO_TRAP( _source )

#define AJIT_TRAP_SOURCE( _trap ) SPARC_INTERRUPT_TRAP_TO_SOURCE( _trap )

#define AJIT_Is_Trap( _trap ) SPARC_IS_INTERRUPT_TRAP( _trap )

/*
 *  ====================================================================
 *  Peripheral access helpers
 *  ====================================================================
 *
 *  All AJIT peripherals live in the 0xFFFFxxxx region and are accessed with
 *  MMU-bypass loads and stores (ASI 0x20), as prescribed by the AJIT
 *  Reference Manual: the processor keeps the caches enabled even with the MMU
 *  off, so a plain pointer access to a device register could be serviced from
 *  the data cache.  The ASI 0x20 bypass reaches the device directly.
 *
 *  AJIT_USE_ASI_BYPASS defaults to 1 and selects the ASI 0x20 lda/sta
 *  instructions.  It may be set to 0 for platforms that map the peripherals
 *  in the default address space.
 */

#ifndef AJIT_USE_ASI_BYPASS
#define AJIT_USE_ASI_BYPASS 1
#endif

static inline uint32_t ajit_load_word_bypass( uintptr_t addr )
{
#if AJIT_USE_ASI_BYPASS
  uint32_t value;
  __asm__ __volatile__(
    "lda [%1] %2, %0\n\t"
    : "=r"( value )
    : "r"( addr ), "i"( AJIT_ASI_MMU_BYPASS )
  );
  return value;
#else
  return *(volatile uint32_t *)addr;
#endif
}

static inline void ajit_store_word_bypass( uintptr_t addr, uint32_t value )
{
#if AJIT_USE_ASI_BYPASS
  __asm__ __volatile__(
    "sta %0, [%1] %2\n\t"
    :
    : "r"( value ), "r"( addr ), "i"( AJIT_ASI_MMU_BYPASS )
    : "memory"
  );
#else
  *(volatile uint32_t *)addr = value;
#endif
}

static inline uint8_t ajit_load_byte_bypass( uintptr_t addr )
{
#if AJIT_USE_ASI_BYPASS
  uint8_t value;
  __asm__ __volatile__(
    "lduba [%1] %2, %0\n\t"
    : "=r"( value )
    : "r"( addr ), "i"( AJIT_ASI_MMU_BYPASS )
  );
  return value;
#else
  return *(volatile uint8_t *)addr;
#endif
}

static inline void ajit_store_byte_bypass( uintptr_t addr, uint8_t value )
{
#if AJIT_USE_ASI_BYPASS
  __asm__ __volatile__(
    "stuba %0, [%1] %2\n\t"
    :
    : "r"( value ), "r"( addr ), "i"( AJIT_ASI_MMU_BYPASS )
    : "memory"
  );
#else
  *(volatile uint8_t *)addr = value;
#endif
}

/*
 *  ====================================================================
 *  Interrupt Controller (TIC) -- Ch. 8.1
 *  ====================================================================
 *
 *  Each TIC register (one per hardware thread) has format:
 *    [31:17] interrupt vector (which interrupts are currently active)
 *    [16]    unused
 *    [15:1]  interrupt mask (if bit is set, the interrupt is recognized)
 *    [0]     enabled
 *
 *  State machine:
 *    IRC_DISABLED -> IRC_ENABLED (write bit[0]=1)
 *    IRC_ENABLED  -> IRC_INTERRUPTING (unmasked interrupt active)
 *    IRC_INTERRUPTING -> IRC_DISABLED (write bit[0]=0)
 *
 *  To acknowledge an interrupt: write 0 to disable, then write
 *  (mask | 1) to re-enable.
 */

static inline uint32_t ajit_read_tic( uintptr_t tic_addr )
{
  return ajit_load_word_bypass( tic_addr );
}

static inline void ajit_write_tic( uintptr_t tic_addr, uint32_t value )
{
  ajit_store_word_bypass( tic_addr, value );
}

static inline uint32_t ajit_read_interrupt_controller_control( void )
{
  return ajit_load_word_bypass( AJIT_TIC_CORE0_THREAD0 );
}

static inline void ajit_write_interrupt_controller_control( uint32_t value )
{
  ajit_store_word_bypass( AJIT_TIC_CORE0_THREAD0, value );
}

/*
 *  Acknowledge a pending interrupt on the AJIT TIC.
 *
 *  AJIT TIC state machine: once an unmasked interrupt is asserted the
 *  controller stays in IRC_INTERRUPTING until it is disabled (write 0)
 *  and re-enabled (write mask|enable).  Without this toggle the trap
 *  re-fires on return.
 *
 *  Every AJIT ISR installed via rtems_interrupt_handler_install() must
 *  call this exactly once before returning, AFTER the peripheral itself
 *  has been serviced (so its source-line is de-asserted).
 */
/* ajit_interrupt_ack() is defined below, after _AJIT_TIC_Mirror is declared. */

/*
 *  Interrupt mask helpers for core 0, thread 0 (single-core BSP).
 *
 *  In the TIC register, bit position N is the mask bit for interrupt
 *  level N directly (level 10 -> bit 10, level 12 -> bit 12).  Bit 0
 *  doubles as the controller enable.  When a level's bit is SET, that
 *  interrupt is RECOGNIZED (not masked).
 *
 *  Therefore the source number is the bit position directly and
 *  AJIT_TIC_MASK_SHIFT is 0: a level is unmasked with BIT(irq).
 */

#define AJIT_TIC_ENABLED_BIT    0x00000001U
#define AJIT_TIC_MASK_SHIFT     0
#define AJIT_TIC_MASK_BITS      0x0000FFFEU  /* bits [15:1] */

/* Software mirror of the TIC register for atomic mask updates */
extern uint32_t _AJIT_TIC_Mirror;

static inline void ajit_interrupt_ack( void )
{
  ajit_write_interrupt_controller_control( 0 );
  ajit_write_interrupt_controller_control( _AJIT_TIC_Mirror );
}

#define AJIT_Clear_interrupt( _source ) \
  do { \
    uint32_t _level; \
    uint32_t _tic; \
    _level = sparc_disable_interrupts(); \
    /* Disable TIC to acknowledge, then re-enable with same mask */ \
    ajit_write_tic( AJIT_TIC_CORE0_THREAD0, 0 ); \
    _tic = _AJIT_TIC_Mirror; \
    ajit_write_tic( AJIT_TIC_CORE0_THREAD0, _tic ); \
    sparc_enable_interrupts( _level ); \
  } while (0)

#define AJIT_Force_interrupt( _source ) \
  do { \
    /* \
     * The AJIT TIC has no software-force register; the tm27 timing test \
     * raises interrupts through the SPARC PIL mechanism instead. \
     */ \
    (void)(_source); \
  } while (0)

/*
 *  TIC active-vector field is bits [31:17], 15 bits one-hot for IRQ 1..15
 *  (AJIT Reference Manual §8.1.1).  Bit 17 represents IRQ 1, so the bit
 *  position for IRQ N is (16 + N).
 */
#define AJIT_Is_interrupt_pending( _source ) \
  ((ajit_read_tic( AJIT_TIC_CORE0_THREAD0 ) >> 16) & (1U << (_source)))

#define AJIT_Is_interrupt_masked( _source ) \
  (!((_AJIT_TIC_Mirror >> AJIT_TIC_MASK_SHIFT) & (1U << (_source))))

#define AJIT_Mask_interrupt( _source ) \
  do { \
    uint32_t   _level; \
    \
    _level = sparc_disable_interrupts(); \
    _AJIT_TIC_Mirror &= ~(1U << ((_source) + AJIT_TIC_MASK_SHIFT)); \
    ajit_write_tic( AJIT_TIC_CORE0_THREAD0, _AJIT_TIC_Mirror ); \
    sparc_enable_interrupts( _level ); \
  } while (0)

#define AJIT_Unmask_interrupt( _source ) \
  do { \
    uint32_t   _level; \
    \
    _level = sparc_disable_interrupts(); \
    _AJIT_TIC_Mirror |= (1U << ((_source) + AJIT_TIC_MASK_SHIFT)); \
    _AJIT_TIC_Mirror |= AJIT_TIC_ENABLED_BIT; \
    ajit_write_tic( AJIT_TIC_CORE0_THREAD0, _AJIT_TIC_Mirror ); \
    sparc_enable_interrupts( _level ); \
  } while (0)

#define AJIT_Disable_interrupt( _source, _previous ) \
  do { \
    uint32_t   _level; \
    uint32_t   _mask = 1U << ((_source) + AJIT_TIC_MASK_SHIFT); \
    \
    _level = sparc_disable_interrupts(); \
    (_previous) = _AJIT_TIC_Mirror & _mask; \
    _AJIT_TIC_Mirror &= ~_mask; \
    ajit_write_tic( AJIT_TIC_CORE0_THREAD0, _AJIT_TIC_Mirror ); \
    sparc_enable_interrupts( _level ); \
  } while (0)

#define AJIT_Restore_interrupt( _source, _previous ) \
  do { \
    uint32_t   _level; \
    uint32_t   _mask = 1U << ((_source) + AJIT_TIC_MASK_SHIFT); \
    \
    _level = sparc_disable_interrupts(); \
    _AJIT_TIC_Mirror = (_AJIT_TIC_Mirror & ~_mask) | (_previous); \
    ajit_write_tic( AJIT_TIC_CORE0_THREAD0, _AJIT_TIC_Mirror ); \
    sparc_enable_interrupts( _level ); \
  } while (0)

/* Common BSP macros for interrupt handling on local CPU */
#define BSP_Clear_interrupt(_source) AJIT_Clear_interrupt(_source)
#define BSP_Force_interrupt(_source) AJIT_Force_interrupt(_source)
#define BSP_Clear_forced_interrupt( _source ) (void)(_source)
#define BSP_Is_interrupt_pending(_source) AJIT_Is_interrupt_pending(_source)
#define BSP_Is_interrupt_forced(_source) 0
#define BSP_Is_interrupt_masked(_source) AJIT_Is_interrupt_masked(_source)
#define BSP_Unmask_interrupt(_source) AJIT_Unmask_interrupt(_source)
#define BSP_Mask_interrupt(_source) AJIT_Mask_interrupt(_source)
#define BSP_Disable_interrupt(_source, _previous) \
        AJIT_Disable_interrupt(_source, _previous)
#define BSP_Restore_interrupt(_source, _previous) \
        AJIT_Restore_interrupt(_source, _previous)

/* Common BSP macros for interrupt handling on any CPU */
#define BSP_Cpu_Is_interrupt_masked(_source, _cpu) \
        BSP_Is_interrupt_masked(_source)
#define BSP_Cpu_Unmask_interrupt(_source, _cpu) \
        BSP_Unmask_interrupt(_source)
#define BSP_Cpu_Mask_interrupt(_source, _cpu) \
        BSP_Mask_interrupt(_source)
#define BSP_Cpu_Disable_interrupt(_source, _previous, _cpu) \
        BSP_Disable_interrupt(_source, _previous)
#define BSP_Cpu_Restore_interrupt(_source, _previous, _cpu) \
        BSP_Restore_interrupt(_source, _previous)

/*
 *  ====================================================================
 *  Timer -- Ch. 8.2 "The default count-down timer"
 *  ====================================================================
 *
 *  Single 32-bit register at 0xFFFF3100:
 *    [31:1]  max-timer-count
 *    [0]     enable bit
 *
 *  When enable is set, counts down from max-timer-count to 0, then
 *  asserts interrupt level TIMER_IRL (0xA = 10).
 *
 *  To re-arm: write the register again (disable then enable, or just
 *  enable with new count).
 */

#define AJIT_TIMER_ENABLE_BIT   0x00000001U

static inline uint32_t ajit_read_timer( void )
{
  return ajit_load_word_bypass( AJIT_TIMER_REGISTER );
}

static inline void ajit_write_timer( uint32_t value )
{
  ajit_store_word_bypass( AJIT_TIMER_REGISTER, value );
}

static inline void ajit_timer_start( uint32_t count )
{
  /* Write count in [31:1] with enable bit [0] set */
  ajit_write_timer( (count << 1) | AJIT_TIMER_ENABLE_BIT );
}

static inline void ajit_timer_stop( void )
{
  ajit_write_timer( 0 );
}

/*
 *  ====================================================================
 *  Serial device -- Ch. 8.3
 *  ====================================================================
 *
 *  Control/status register at 0xFFFF3200 (word access):
 *    bit 0 -> TX enable
 *    bit 1 -> RX enable
 *    bit 2 -> RX interrupt enable
 *    bit 3 -> TX full (read-only)
 *    bit 4 -> RX full (read-only)
 *
 *  TX register at 0xFFFF3204 (byte access)
 *  RX register at 0xFFFF3208 (byte access)
 *  Baud limit register at 0xFFFF320C (word access)
 *  Baud freq register at 0xFFFF3210 (word access)
 */

#define AJIT_UART_CONTROL_TX_ENABLE          0x00000001U
#define AJIT_UART_CONTROL_RX_ENABLE          0x00000002U
#define AJIT_UART_CONTROL_RX_INTERRUPT       0x00000004U
#define AJIT_UART_CONTROL_TX_FULL            0x00000008U
#define AJIT_UART_CONTROL_RX_FULL            0x00000010U
/* write bit 3 = soft reset */
#define AJIT_UART_CONTROL_SOFT_RESET         0x00000008U

#define AJIT_UART_BAUD_RATE                  115200U
#define AJIT_UART_CLK_FREQ                   80000000U

static inline uint32_t ajit_read_serial_control( void )
{
  return ajit_load_word_bypass( AJIT_SERIAL_CONTROL_REGISTER );
}

static inline void ajit_write_serial_control( uint32_t value )
{
  ajit_store_word_bypass( AJIT_SERIAL_CONTROL_REGISTER, value );
}

static inline void ajit_write_serial_tx( uint8_t value )
{
  ajit_store_byte_bypass( AJIT_SERIAL_TX_REGISTER, value );
}

static inline uint8_t ajit_read_serial_rx( void )
{
  return ajit_load_byte_bypass( AJIT_SERIAL_RX_REGISTER );
}

static inline void ajit_write_serial_baud_limit( uint32_t value )
{
  ajit_store_word_bypass( AJIT_SERIAL_BAUD_LIMIT_REGISTER, value );
}

static inline void ajit_write_serial_baud_freq( uint32_t value )
{
  ajit_store_word_bypass( AJIT_SERIAL_BAUD_FREQ_REGISTER, value );
}

static inline uint32_t ajit_gcd( uint32_t a, uint32_t b )
{
  while ( b != 0 ) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

static inline void ajit_uart_init( void )
{
  uint32_t baud_freq, baud_limit, g, val;

  /*
   * Enable TX, RX and the RX interrupt.  RX_INTERRUPT is enabled even for
   * the polled console because the UART only latches received characters
   * while it is set.  In polled mode the resulting interrupt line is left
   * masked at the TIC, so no interrupt is delivered.
   */
  ajit_write_serial_control(
    AJIT_UART_CONTROL_TX_ENABLE | AJIT_UART_CONTROL_RX_ENABLE
      | AJIT_UART_CONTROL_RX_INTERRUPT
  );

  /* Calculate and set baud rate parameters */
  g = ajit_gcd( 16U * AJIT_UART_BAUD_RATE, AJIT_UART_CLK_FREQ );
  if ( g != 0 ) {
    baud_freq  = ( 16U * AJIT_UART_BAUD_RATE ) / g;
    baud_limit = ( AJIT_UART_CLK_FREQ / g ) - baud_freq;
    ajit_write_serial_baud_limit( baud_limit );
    ajit_write_serial_baud_freq( baud_freq );
  }

  /* Soft reset: write 8 to control, then restore */
  val = ajit_read_serial_control();
  ajit_write_serial_control( AJIT_UART_CONTROL_SOFT_RESET );
  ajit_write_serial_control( val );
}

#endif /* !ASM */

/*
 *  Assembly-only constants
 */

#ifdef ASM

#define  AJIT_INTERRUPT_CONTROLLER_CONTROL_REGISTER_ASM  0xFFFF3000
#define  AJIT_TIMER_CONTROL_REGISTER_ASM                 0xFFFF3100
#define  AJIT_SERIAL_CONTROL_REGISTER_ASM                0xFFFF3200
#define  AJIT_SERIAL_TX_REGISTER_ASM                     0xFFFF3204
#define  AJIT_SERIAL_RX_REGISTER_ASM                     0xFFFF3208
#define  AJIT_SERIAL_BAUD_LIMIT_REGISTER_ASM             0xFFFF320C
#define  AJIT_SERIAL_BAUD_FREQ_REGISTER_ASM              0xFFFF3210

#endif /* ASM */

#ifdef __cplusplus
}
#endif

#endif /* !_INCLUDE_AJIT_h */
