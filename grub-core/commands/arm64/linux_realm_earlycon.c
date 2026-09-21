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
#include <grub/arm64/cca.h>
#include <grub/command.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/spcr.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_err_t
grub_realm_earlycon (char *argument, grub_size_t size)
{
  struct grub_spcr_console console = { 0 };
  grub_uint64_t ipa_width = 0;
  grub_uint64_t shared_bit;
  grub_err_t err;

  err = grub_arm_cca_get_ipa_width (&ipa_width);
  if (err != GRUB_ERR_NONE)
    return err;
  grub_printf ("INFO: Realm IPA width: %" PRIuGRUB_UINT64_T "\n", ipa_width);
  /* Validate before either shift; all arithmetic uses a 64-bit IPA.  */
  if (ipa_width == 0 || ipa_width > 64)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid Realm IPA width: %"
                       PRIuGRUB_UINT64_T, ipa_width);
  err = grub_spcr_get_console (&console);
  if (err != GRUB_ERR_NONE)
    return err;
  if (console.type != GRUB_SPCR_UART_PL011)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "Realm earlycon requires PL011");
  grub_printf ("INFO: SPCR interface: PL011\n");
  grub_printf ("INFO: SPCR address: 0x%" PRIxGRUB_UINT64_T "\n",
               console.registers.addr);
  if (ipa_width < 64 && (console.registers.addr >> ipa_width) != 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "SPCR address exceeds Realm IPA width");
  if ((console.registers.addr & 3) != 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "PL011 address is not dword aligned");

  shared_bit = ((grub_uint64_t) 1) << (ipa_width - 1);
  console.registers.addr |= shared_bit;
  /* Use PL011's 32-bit Linux earlycon path for the shared MMIO alias.  */
  console.access_bits = 32;
  return grub_linux_earlycon_format (&console, argument, size);
}

static grub_err_t
grub_cmd_linux_realm_earlycon (grub_command_t cmd __attribute__ ((unused)),
                      int argc, char **argv)
{
  grub_command_t linux_cmd;
  char argument[80];
  char **args;
  grub_err_t err;
  int i;
  int bare = 0;
  int explicit = 0;

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "filename expected");
  for (i = 1; i < argc; i++)
    {
      if (grub_strcmp (argv[i], "earlycon") == 0)
        bare = 1;
      else if (grub_strncmp (argv[i], "earlycon=", sizeof ("earlycon=") - 1) == 0)
        explicit = 1;
    }

  /* The linux module is a dependency.  Call its handler with argv directly;
     never serialize arguments back into a GRUB command string.  */
  linux_cmd = grub_command_find ("linux");
  if (linux_cmd == NULL)
    return grub_error (GRUB_ERR_UNKNOWN_COMMAND, "linux command is unavailable");
  /* An explicit setting takes precedence, including mixed explicit/bare input.  */
  if (explicit)
    grub_printf ("INFO: explicit earlycon= argument passed unchanged; "
                 "Realm shared-address conversion skipped.\n");
  else if (!bare)
    grub_printf ("INFO: no earlycon argument; loading Linux without adding earlycon.\n");
  if (!bare || explicit)
    return linux_cmd->func (linux_cmd, argc, argv);

  err = grub_realm_earlycon (argument, sizeof (argument));
  if (err != GRUB_ERR_NONE)
    return err;
  args = grub_calloc (argc, sizeof (*args));
  if (args == NULL)
    return grub_errno;
  for (i = 0; i < argc; i++)
    args[i] = (i != 0 && grub_strcmp (argv[i], "earlycon") == 0) ? argument : argv[i];

  grub_printf ("INFO: replaced bare earlycon with %s\n", argument);

  /* linux copies the command line before returning.  The original argv and
     all unrelated argument strings remain untouched.  */
  err = linux_cmd->func (linux_cmd, argc, args);
  grub_free (args);
  return err;
}

static grub_command_t cmd;

GRUB_MOD_INIT(linux_realm_earlycon)
{
  cmd = grub_register_command ("linux_realm_earlycon", grub_cmd_linux_realm_earlycon, 0,
                               "Load Linux with Realm PL011 earlycon expansion.");
}

GRUB_MOD_FINI(linux_realm_earlycon)
{
  grub_unregister_command (cmd);
}
