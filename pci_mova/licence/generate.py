"""
pci_mova.licence.generate
CLI tool for generating and inspecting HMAC-JWT MOVA licences.

Usage (issuing side — run this to generate a licence for a customer):
  python3 -m pci_mova.licence.generate --streams 4 --owner "Site A" --fp <fingerprint>
  python3 -m pci_mova.licence.generate --streams 4 --owner "Site A" --fp <fp> --ref "INV-001"
  python3 -m pci_mova.licence.generate --streams 4 --owner "Site A" --fp <fp> --days 365

Usage (deployment side — run on the target box to get hardware fingerprint):
  python3 -m pci_mova.licence.generate --show

The --fp flag lets you generate a licence for a DIFFERENT machine's fingerprint.
If omitted, the licence is generated for the current machine.
"""

import argparse
import os
import sys
import time

# Allow running from repo root
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from pci_mova.licence.licence import (
    build_fingerprint, create_licence, get_hardware_info,
    load_licence, validate, LICENCE_FILE, LICENCE_SECRET, _DEFAULT_SECRET,
)


def cmd_show():
    info = get_hardware_info()
    print()
    print("  ┌──────────────────────────────────────────────────────────┐")
    print("  │              PCI MOVA — Hardware Identity                │")
    print("  ├──────────────────────────────────────────────────────────┤")
    print(f"  │  Hostname    : {info['hostname']:<42}│")
    print(f"  │  Machine ID  : {info['machine_id']:<42}│")
    print(f"  │  MAC         : {info['mac']:<42}│")
    print(f"  │  Fingerprint : {info['fingerprint']:<42}│")
    print("  └──────────────────────────────────────────────────────────┘")

    result = validate()
    print()
    if result.valid:
        d = result.data
        issued  = time.strftime("%Y-%m-%d %H:%M", time.localtime(d.issued_at))
        expires = time.strftime("%Y-%m-%d", time.localtime(d.expires_at)) \
                  if d.expires_at else "Perpetual"
        print("  ┌──────────────────────────────────────────────────────────┐")
        print("  │              PCI MOVA — Active Licence                   │")
        print("  ├──────────────────────────────────────────────────────────┤")
        print(f"  │  Owner       : {d.owner:<42}│")
        print(f"  │  Provider    : {d.provider:<42}│")
        print(f"  │  Streams     : {d.max_streams:<42}│")
        print(f"  │  Ref         : {d.licence_ref:<42}│")
        print(f"  │  Issued      : {issued:<42}│")
        print(f"  │  Expires     : {expires:<42}│")
        print("  ├──────────────────────────────────────────────────────────┤")
        print("  │  Signature   : VALID ✓                                   │")
        print("  └──────────────────────────────────────────────────────────┘")
    else:
        print(f"  No valid licence: {result.reason}")
    print()


def cmd_generate(streams, owner, provider, ref, fp, days):
    expires_at = time.time() + days * 86400 if days else None

    if LICENCE_SECRET == _DEFAULT_SECRET.encode():
        print()
        print("  WARNING: Using default development signing key.")
        print("  Set MOVA_LICENCE_SECRET env var before issuing production licences.")
        print()

    fingerprint = fp or build_fingerprint()

    data = create_licence(
        provider    = provider,
        owner       = owner,
        max_streams = streams,
        licence_ref = ref,
        expires_at  = expires_at,
        fingerprint = fingerprint,
    )

    issued  = time.strftime("%Y-%m-%d %H:%M", time.localtime(data.issued_at))
    expires = time.strftime("%Y-%m-%d", time.localtime(data.expires_at)) \
              if data.expires_at else "Perpetual"

    with open(LICENCE_FILE) as fh:
        token = fh.read().strip()

    print()
    print("  ┌──────────────────────────────────────────────────────────┐")
    print("  │              PCI MOVA — Licence Generated                │")
    print("  ├──────────────────────────────────────────────────────────┤")
    print(f"  │  Owner       : {owner:<42}│")
    print(f"  │  Provider    : {provider:<42}│")
    print(f"  │  Streams     : {streams:<42}│")
    print(f"  │  Ref         : {ref:<42}│")
    print(f"  │  Issued      : {issued:<42}│")
    print(f"  │  Expires     : {expires:<42}│")
    print(f"  │  Target FP   : {fingerprint[:42]:<42}│")
    print("  ├──────────────────────────────────────────────────────────┤")
    print(f"  │  File        : {LICENCE_FILE:<42}│")
    print("  │  Signature   : HMAC-SHA256 ✓                             │")
    print("  └──────────────────────────────────────────────────────────┘")
    print()
    print("  JWT token (contents of licence file):")
    print(f"  {token}")
    print()


def main():
    parser = argparse.ArgumentParser(
        description="PCI MOVA HMAC-JWT licence manager",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Show hardware fingerprint + current licence status:
  python3 -m pci_mova.licence.generate --show

  # Generate licence for THIS machine (4 streams):
  python3 -m pci_mova.licence.generate -s 4 -o "Site A"

  # Generate licence for a DIFFERENT machine's fingerprint:
  python3 -m pci_mova.licence.generate -s 4 -o "Site A" --fp <fingerprint>

  # With reference number and 1-year expiry:
  python3 -m pci_mova.licence.generate -s 4 -o "Site A" --fp <fp> --ref INV-001 --days 365
        """
    )
    parser.add_argument("--show", action="store_true",
                        help="Show hardware fingerprint and current licence status")
    parser.add_argument("-s", "--streams", type=int, choices=range(1, 9), metavar="N",
                        help="Number of licensed streams (1-8)")
    parser.add_argument("-o", "--owner", default="",
                        help="Owner / site name")
    parser.add_argument("-p", "--provider", default="PCI",
                        help="Provider name (default: PCI)")
    parser.add_argument("--ref", default="",
                        help="Licence reference (invoice number etc.)")
    parser.add_argument("--fp", default="",
                        help="Hardware fingerprint of target machine (omit for this machine)")
    parser.add_argument("--days", type=int, default=0,
                        help="Expiry in days from now (0 = perpetual)")

    args = parser.parse_args()

    if args.show or not args.streams:
        cmd_show()
        if not args.streams:
            parser.print_help()
        return

    if not args.owner:
        parser.error("--owner is required when generating a licence")

    cmd_generate(
        streams  = args.streams,
        owner    = args.owner,
        provider = args.provider,
        ref      = args.ref,
        fp       = args.fp or None,
        days     = args.days,
    )


if __name__ == "__main__":
    main()
