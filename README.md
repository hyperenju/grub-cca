# Arm CCA Realm earlycon for GRUB

This arm64 EFI proof of concept adds `linux_realm_earlycon`, a wrapper around
GRUB's `linux` command. It uses `RSI_REALM_CONFIG` and ACPI SPCR to expand a bare
`earlycon` argument into a PL011 earlycon parameter with a Realm shared address.

## Motivation

As described in Linux's `Documentation/arch/arm64/arm-cca.rst`, Normal
World-emulated MMIO used by earlycon must be addressed in the upper half of
Realm IPA space. The VMM or boot loader must supply that address. Since the
IPA width and UART address vary between environments, this command discovers
both at boot instead of requiring users to preconfigure them:

```text
shared_address = spcr_address | (1ULL << (ipa_width - 1))
```

## Usage

In `grub.cfg`:

```grub
linux_realm_earlycon Image rw root=/dev/vda1 console=ttyAMA0 earlycon
```

Example output:

```text
INFO: Realm IPA width: 41
INFO: SPCR interface: PL011
INFO: SPCR address: 0x9000000
INFO: replaced bare earlycon with earlycon=pl011,mmio32,0x10009000000
```

Use `initrd` and `boot` as with `linux`. Only ARM PL011 is supported.

- Bare `earlycon` is expanded; failure stops kernel loading.
- Explicit `earlycon=...` takes precedence and is passed through unchanged.
- Without `earlycon`, nothing is added.

Other arguments and the normal `linux` command are unchanged.

See [INSTALL](INSTALL) for GRUB build instructions and [SECURITY](SECURITY)
for vulnerability reporting.
