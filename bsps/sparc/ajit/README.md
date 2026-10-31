ajit
====
Board Support Package for the AJIT SPARC V8 processor (IIT Bombay).

```
BSP NAME:           ajit
BOARD:              AJIT (IIT Bombay SPARC V8 SoC)
BUS:                N/A (on-chip)
CPU FAMILY:         sparc
CPU:                AJIT (SPARC V8 with SRMMU)
COPROCESSORS:       on-chip IEEE-754 FPU
MODE:               32 bit mode
```

PERIPHERALS
-----------
```
TIMERS:
  NAME:             AJIT count-down timer
  RESOLUTION:       one processor clock period
  NAME:             ASR30:31 64-bit cycle counter
  RESOLUTION:       processor clock period (read-only, free-running)
SERIAL PORTS:       1 on-chip UART
REAL-TIME CLOCK:    none
DMA:                none
```

DRIVER INFORMATION
------------------
```
CLOCK DRIVER:       AJIT count-down timer at 0xFFFF3100, interrupt level 10
CONSOLE DRIVER:     AJIT on-chip UART (polled or interrupt driven)
TIMER DRIVER:       ASR31 cycle counter (benchmark timer)
CACHE DRIVER:       SPARC flush invalidates both the I and D caches
```

STDIO
-----
```
PORT:               UART
BAUD:               115200
BITS PER CHARACTER: 8
PARITY:             none
STOP BITS:          1
```

Build options
-------------
```
AJIT_CLOCK_SPEED_MHZ    Processor clock in MHz.
CONSOLE_USE_INTERRUPTS  0 = polled console, 1 = interrupt driven (default 1).
BSP_AJIT_SMP            Enable AJIT SMP support (default on under RTEMS_SMP).
```

Memory Map
----------
```
_RAM_START  - _RAM_START + _RAM_SIZE     code, data, BSS, workspace, stack
0xFFFF0000  - 0xFFFFFFFF                 on-chip peripherals
```

The image is based at 0x40000000.  The linker symbols `_RAM_START` and
`_RAM_SIZE` can be overridden at link time (for example
`-Wl,--defsym=_RAM_START=0x40000000`) to retarget across board variants
without editing `linkcmds`.

Notes
-----
- All peripherals live in the 0xFFFF0000 region and are accessed with
  MMU-bypass loads and stores (ASI 0x20), as prescribed by the AJIT
  Reference Manual.  The caches remain enabled with the MMU off, so a plain
  pointer access to a device register could be serviced from the data cache.
- AJIT has a single count-down timer (interrupt level 10).  The 64-bit
  ASR30:31 cycle counter provides the RTEMS CPU counter and benchmark timer.
- The data cache is write-through, so the SPARC flush instruction only
  invalidates; no dirty data is pushed to RAM.
- The TIC interrupt controller has no software force-interrupt register, so
  rtems_interrupt_raise() and rtems_interrupt_clear() report that raising an
  interrupt by software is not supported.

SMP
---
The BSP supports symmetric multiprocessing on multi-core/multi-thread AJIT
configurations (Reference Manual ch. 6, up to 4 cores x 2 threads).  Build
with `RTEMS_SMP=True`; `BSP_AJIT_SMP` (default on) enables the SMP paths.

- The running thread is identified from ASR29 ([15:8] core, [7:0] thread);
  the linear processor index is 2*core + thread.
- All threads are released from reset together.  The shared SPARC `start.S`
  elects one boot CPU with an ldstub-locked counter; the remaining threads
  wait on a release flag until the boot CPU has cleared BSS, then enter
  bsp_start_on_secondary_processor().
- There is no hold register, so each secondary then waits for a per-core
  release flag that `_CPU_SMP_Start_processor()` sets only for processors
  within the configured maximum.  A surplus thread parks in that wait loop
  instead of failing on an unassigned processor.
- Scheduler IPIs use the AJIT inter-processor-interrupt registers (0xFFFF3080
  mask, 0xFFFF3084 value, 0xFFFF30C8 lock, Reference Manual sec. 8.1.2) and are
  delivered on interrupt level 11.
- Cache coherence is maintained by the coherent memory controller, so no
  software snoop-enable step is required.
- A bad trap taken on a secondary thread switches to that thread's own ISR
  stack and per-CPU control block.  The AJIT build defines `SPARC_BSP_AJIT`
  (in `abi.yml`) so the shared `cpukit/score/cpu/sparc/sparc-bad-trap.S` uses
  the ASR29 processor index rather than the boot processor's stack.
