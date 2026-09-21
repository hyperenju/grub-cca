/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2026  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef GRUB_SPCR_HEADER
#define GRUB_SPCR_HEADER 1

#include <grub/acpi.h>
#include <grub/err.h>

enum grub_spcr_uart_type
{
  GRUB_SPCR_UART_PL011
};

struct grub_spcr_console
{
  enum grub_spcr_uart_type type;
  struct grub_acpi_genaddr registers;
  unsigned int access_bits;
};

grub_err_t grub_spcr_get_console (struct grub_spcr_console *console);
grub_err_t grub_linux_earlycon_format (const struct grub_spcr_console *console,
                                       char *buffer, grub_size_t size);

#endif
