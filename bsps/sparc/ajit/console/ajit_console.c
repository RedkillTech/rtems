/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @brief Console driver for the on-chip UART of the AJIT processor.
 *
 * Uses the modern rtems_termios_device_handler API (RTEMS BSP Howto,
 * "Console Driver").  Polled and interrupt-driven handlers are provided;
 * the build picks one via the CONSOLE_USE_INTERRUPTS BSP option.
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
#include <bspopts.h>
#include <rtems/console.h>
#include <rtems/termiostypes.h>

typedef struct {
  rtems_termios_device_context base;
#if CONSOLE_USE_INTERRUPTS
  rtems_termios_tty           *tty;
  rtems_interrupt_entry        irq_entry;
  volatile bool                tx_busy;
  size_t                       tx_pending;
#endif
} ajit_uart_context;

static ajit_uart_context ajit_uart_a;

/* ---------------- common helpers ---------------- */

#if !CONSOLE_USE_INTERRUPTS
static void ajit_uart_enable_tx_rx(void)
{
  uint32_t ctrl = ajit_read_serial_control();
  ctrl |= AJIT_UART_CONTROL_TX_ENABLE | AJIT_UART_CONTROL_RX_ENABLE;
  ajit_write_serial_control(ctrl);
}

static int ajit_uart_poll_read(rtems_termios_device_context *base)
{
  (void) base;

  if ((ajit_read_serial_control() & AJIT_UART_CONTROL_RX_FULL) == 0) {
    return -1;
  }
  return (int) ajit_read_serial_rx();
}

static void ajit_uart_poll_write_char(uint8_t ch)
{
  while ((ajit_read_serial_control() & AJIT_UART_CONTROL_TX_FULL) != 0) {
    /* wait for TX register to drain */
  }
  ajit_write_serial_tx(ch);
}
#endif

static bool ajit_uart_set_attributes(
  rtems_termios_device_context *base,
  const struct termios          *term
)
{
  (void) base;
  (void) term;
  /* AJIT UART has no programmable baud/parity from RTEMS; ajit_uart_init()
   * already configured 115200-8N1.  Accept silently. */
  return true;
}

/* ---------------- polled handler ---------------- */

#if !CONSOLE_USE_INTERRUPTS

static bool ajit_uart_first_open_polled(
  rtems_termios_tty             *tty,
  rtems_termios_device_context  *base,
  struct termios                *term,
  rtems_libio_open_close_args_t *args
)
{
  (void) tty;
  (void) base;
  (void) args;

  rtems_termios_set_initial_baud(tty, 115200);
  ajit_uart_enable_tx_rx();
  (void) term;
  return true;
}

static void ajit_uart_write_polled(
  rtems_termios_device_context *base,
  const char                   *buf,
  size_t                        len
)
{
  (void) base;
  for (size_t i = 0; i < len; ++i) {
    ajit_uart_poll_write_char((uint8_t) buf[i]);
  }
}

static const rtems_termios_device_handler ajit_uart_handler_polled = {
  .first_open     = ajit_uart_first_open_polled,
  .last_close     = NULL,
  .poll_read      = ajit_uart_poll_read,
  .write          = ajit_uart_write_polled,
  .set_attributes = ajit_uart_set_attributes,
  .mode           = TERMIOS_POLLED
};

#endif /* !CONSOLE_USE_INTERRUPTS */

/* ---------------- interrupt handler ---------------- */

#if CONSOLE_USE_INTERRUPTS

static void ajit_uart_isr(void *arg)
{
  ajit_uart_context *ctx = arg;
  char rxbuf[16];
  unsigned int n;
  uint32_t ctrl;

  /* Drain RX FIFO into termios. */
  n = 0;
  while (n < sizeof(rxbuf)) {
    ctrl = ajit_read_serial_control();
    if ((ctrl & AJIT_UART_CONTROL_RX_FULL) == 0) {
      break;
    }
    rxbuf[n++] = (char) ajit_read_serial_rx();
  }
  if (n > 0) {
    rtems_termios_enqueue_raw_characters(ctx->tty, rxbuf, n);
  }

  /* Dequeue TX if the TX register can accept more. */
  ctrl = ajit_read_serial_control();
  if (ctx->tx_busy && (ctrl & AJIT_UART_CONTROL_TX_FULL) == 0) {
    size_t pending = ctx->tx_pending;
    ctx->tx_busy = false;
    ctx->tx_pending = 0;
    rtems_termios_dequeue_characters(ctx->tty, (int) pending);
  }

  ajit_interrupt_ack();
}

static bool ajit_uart_first_open_intr(
  rtems_termios_tty             *tty,
  rtems_termios_device_context  *base,
  struct termios                *term,
  rtems_libio_open_close_args_t *args
)
{
  ajit_uart_context *ctx = (ajit_uart_context *) base;
  rtems_status_code  sc;

  (void) term;
  (void) args;

  ctx->tty = tty;
  ctx->tx_busy = false;
  ctx->tx_pending = 0;

  rtems_termios_set_initial_baud(tty, 115200);

  /* Enable TX, RX, and RX-interrupt in the UART. */
  ajit_write_serial_control(
    AJIT_UART_CONTROL_TX_ENABLE |
    AJIT_UART_CONTROL_RX_ENABLE |
    AJIT_UART_CONTROL_RX_INTERRUPT
  );

  rtems_interrupt_entry_initialize(
    &ctx->irq_entry, ajit_uart_isr, ctx, "AJIT UART"
  );
  sc = rtems_interrupt_entry_install(
    AJIT_INTERRUPT_UART_RX_TX, RTEMS_INTERRUPT_UNIQUE, &ctx->irq_entry
  );
  if (sc != RTEMS_SUCCESSFUL) {
    return false;
  }
  AJIT_Unmask_interrupt(AJIT_INTERRUPT_UART_RX_TX);
  return true;
}

static void ajit_uart_write_intr(
  rtems_termios_device_context *base,
  const char                   *buf,
  size_t                        len
)
{
  ajit_uart_context *ctx = (ajit_uart_context *) base;

  if (len == 0) {
    ctx->tx_busy = false;
    ctx->tx_pending = 0;
    return;
  }

  /* The AJIT UART has only a one-deep TX register and no TX-empty
   * interrupt distinct from the shared RX/TX line.  Push the bytes
   * with short polling; termios will call us back when more are ready. */
  for (size_t i = 0; i < len; ++i) {
    while ((ajit_read_serial_control() & AJIT_UART_CONTROL_TX_FULL) != 0) {
      /* wait */
    }
    ajit_write_serial_tx((uint8_t) buf[i]);
  }
  ctx->tx_busy = true;
  ctx->tx_pending = len;
}

static const rtems_termios_device_handler ajit_uart_handler_intr = {
  .first_open     = ajit_uart_first_open_intr,
  .last_close     = NULL,
  .poll_read      = NULL,
  .write          = ajit_uart_write_intr,
  .set_attributes = ajit_uart_set_attributes,
  .mode           = TERMIOS_IRQ_DRIVEN
};

#endif /* CONSOLE_USE_INTERRUPTS */

/* ---------------- driver entry point ---------------- */

rtems_device_driver console_initialize(
  rtems_device_major_number  major,
  rtems_device_minor_number  minor,
  void                      *arg
)
{
  rtems_status_code sc;

  (void) major;
  (void) minor;
  (void) arg;

  rtems_termios_initialize();

  rtems_termios_device_context_initialize(&ajit_uart_a.base, "AJIT UART");

  sc = rtems_termios_device_install(
    CONSOLE_DEVICE_NAME,
#if CONSOLE_USE_INTERRUPTS
    &ajit_uart_handler_intr,
#else
    &ajit_uart_handler_polled,
#endif
    NULL,
    &ajit_uart_a.base
  );
  return sc;
}
