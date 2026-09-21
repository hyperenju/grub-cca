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
#include <stddef.h>

#include <grub/spcr.h>
#include <grub/misc.h>

static grub_err_t
grub_spcr_parse (const struct grub_acpi_spcr *spcr,
                 struct grub_spcr_console *console)
{
  /* Only the header, interface type and complete GAS are needed here.  */
  if (spcr->hdr.length < offsetof (struct grub_acpi_spcr, interrupt_type))
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "SPCR table is truncated");
  if (grub_byte_checksum ((void *) spcr, spcr->hdr.length) != 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "SPCR checksum is invalid");

  switch (spcr->intf_type)
    {
    case GRUB_ACPI_SPCR_INTF_TYPE_PL011:
      console->type = GRUB_SPCR_UART_PL011;
      break;
    default:
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
                         "unsupported SPCR interface type: 0x%x", spcr->intf_type);
    }

  console->registers = spcr->base_addr;
  if (console->registers.space_id != GRUB_ACPI_GENADDR_MEM_SPACE)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "SPCR UART is not MMIO");
  if (console->registers.addr == 0 || console->registers.bit_offset != 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "unsupported SPCR address or register bit offset");

  /* GAS Access Size describes transfers, not Register Bit Width.
     Treat unspecified access size as byte access, like Linux SPCR handling.  */
  switch (console->registers.access_size)
    {
    case GRUB_ACPI_GENADDR_SIZE_LGCY:
    case GRUB_ACPI_GENADDR_SIZE_BYTE:
      console->access_bits = 8;
      break;
    case GRUB_ACPI_GENADDR_SIZE_WORD:
      console->access_bits = 16;
      break;
    case GRUB_ACPI_GENADDR_SIZE_DWORD:
      console->access_bits = 32;
      break;
    default:
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
                         "unsupported SPCR GAS access size: %u",
                         console->registers.access_size);
    }
  if (console->registers.addr & (console->access_bits / 8 - 1))
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "SPCR MMIO address is unaligned");
  return GRUB_ERR_NONE;
}

grub_err_t
grub_spcr_get_console (struct grub_spcr_console *console)
{
  const struct grub_acpi_spcr *spcr;

  if (grub_machine_acpi_get_rsdpv2 () == NULL &&
      grub_machine_acpi_get_rsdpv1 () == NULL)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "ACPI is unavailable");
  spcr = grub_acpi_find_table (GRUB_ACPI_SPCR_SIGNATURE);
  if (spcr == NULL)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "ACPI SPCR table is absent");
  return grub_spcr_parse (spcr, console);
}

/* Linux earlycon ABI: serial_core.c, earlycon.c and amba-pl011.c.
   No baud option: retain firmware configuration.  */
grub_err_t
grub_linux_earlycon_format (const struct grub_spcr_console *console,
                           char *buffer, grub_size_t size)
{
  const char *mode;
  int len;

  if (console->type != GRUB_SPCR_UART_PL011)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "unsupported earlycon UART");
  switch (console->access_bits)
    {
    case 8:
      mode = "mmio";
      break;
    case 32:
      mode = "mmio32";
      break;
    default:
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
                         "Linux PL011 earlycon requires byte or dword access");
    }
  len = grub_snprintf (buffer, size, "earlycon=pl011,%s,0x%" PRIxGRUB_UINT64_T,
                       mode, console->registers.addr);
  if (len < 0 || (grub_size_t) len >= size)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "earlycon buffer is too small");
  return GRUB_ERR_NONE;
}
