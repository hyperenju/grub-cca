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

#include <grub/acpi.h>
#include <grub/arm64/cca.h>
#include <grub/efi/efi.h>
#include <grub/fdt.h>
#include <grub/misc.h>
#include <grub/mm.h>

/* Fast, SMC64, standard service calls; see Linux asm/rsi_smc.h.  */
#define RSI_VERSION             0xc4000190UL
#define RSI_REALM_CONFIG        0xc4000196UL
#define RSI_ABI_VERSION         0x00010000UL
#define RSI_SUCCESS             0UL
#define SMCCC_NOT_SUPPORTED     (~0UL)
#define RSI_GRANULE_SIZE        0x1000

struct realm_config
{
  grub_uint64_t ipa_width;
  grub_uint8_t reserved[RSI_GRANULE_SIZE - sizeof (grub_uint64_t)];
};

/* Only x0-x3 are results for these calls.  SMCCC preserves x8-x17.  */
static void
rsi_smc (unsigned long fid, unsigned long arg, unsigned long result[3])
{
  register unsigned long x0 asm ("x0") = fid;
  register unsigned long x1 asm ("x1") = arg;
  register unsigned long x2 asm ("x2") = 0;
  register unsigned long x3 asm ("x3") = 0;
  register unsigned long x4 asm ("x4") = 0;
  register unsigned long x5 asm ("x5") = 0;
  register unsigned long x6 asm ("x6") = 0;
  register unsigned long x7 asm ("x7") = 0;

  asm volatile ("smc #0"
                : "+r" (x0), "+r" (x1), "+r" (x2), "+r" (x3),
                  "+r" (x4), "+r" (x5), "+r" (x6), "+r" (x7)
                : : "memory", "cc");
  result[0] = x0;
  result[1] = x1;
  result[2] = x2;
}

/* Like Linux, require an advertised SMC conduit before probing RSI.
   EFI firmware may advertise PSCI through either FDT or ACPI FADT.  */
static int
rsi_has_smc_conduit (void)
{
  const void *fdt = grub_efi_get_firmware_fdt ();
  const char *method;
  struct grub_acpi_fadt *fadt;
  grub_uint32_t len;
  int node;

  if (fdt != NULL && grub_fdt_check_header_nosize (fdt) == 0)
    {
      node = grub_fdt_find_subnode (fdt, 0, "psci");
      if (node >= 0)
        {
          method = grub_fdt_get_prop (fdt, node, "method", &len);
          if (method != NULL)
            return len == sizeof ("smc") &&
                   grub_memcmp (method, "smc", sizeof ("smc")) == 0;
        }
    }

  /* Fall back to ACPI only when FDT does not provide a PSCI method.  */
  fadt = grub_acpi_find_fadt ();
  if (fadt == NULL || fadt->hdr.revision < 5 ||
      fadt->hdr.length < offsetof (struct grub_acpi_fadt, minor_revision) ||
      grub_byte_checksum (fadt, fadt->hdr.length) != 0)
    return 0;
  return (fadt->arm_boot_flags & (GRUB_ACPI_FADT_PSCI_COMPLIANT |
                                  GRUB_ACPI_FADT_PSCI_USE_HVC)) ==
         GRUB_ACPI_FADT_PSCI_COMPLIANT;
}

grub_err_t
grub_arm_cca_get_ipa_width (grub_uint64_t *ipa_width)
{
  struct realm_config *config;
  unsigned long result[3];
  unsigned long current_el;

  asm volatile ("mrs %0, CurrentEL" : "=r" (current_el));
  if (current_el != 4)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "RSI requires Realm EL1");
  if (!rsi_has_smc_conduit ())
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "RSI probe requires PSCI SMC in firmware FDT or ACPI FADT");

  rsi_smc (RSI_VERSION, RSI_ABI_VERSION, result);
  if (result[0] == SMCCC_NOT_SUPPORTED)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "RSI unavailable (not running in a Realm?)");
  /* As in Linux, success means the requested version is supported.
     x1/x2 are the supported range, not the status or the IPA width.  */
  if (result[0] != RSI_SUCCESS)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "RSI 1.0 incompatible: status 0x%lx, range %lu.%lu-%lu.%lu",
                       result[0], result[1] >> 16, result[1] & 0xffff,
                       result[2] >> 16, result[2] & 0xffff);

  config = grub_memalign (RSI_GRANULE_SIZE, sizeof (*config));
  if (config == NULL)
    return grub_errno;
  grub_memset (config, 0, sizeof (*config));
  /* Arm64 EFI uses identity mappings.  In a Realm this address is an IPA.
     Keep the whole 4 KiB granule private and allocated until SMC returns.  */
  rsi_smc (RSI_REALM_CONFIG, (grub_addr_t) config, result);
  if (result[0] == RSI_SUCCESS)
    *ipa_width = config->ipa_width;
  grub_free (config);
  if (result[0] != RSI_SUCCESS)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "RSI_REALM_CONFIG failed: status 0x%lx", result[0]);
  return GRUB_ERR_NONE;
}
