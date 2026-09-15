# Devices repository layout (APE-90)

`ApexiLabs/devices` preserves the complete firmware main history through
`5fcf27913021c5a1a6d73e133626929791264e3b`. The APE-90 reorganisation uses Git
renames; it does not incorporate dirty bench work from the old checkout.
Use `git log --follow -- <new-path>` to inspect a moved file's earlier history.

| Path | Ownership |
| --- | --- |
| `logger/firmware/` | Logger sources, headers and flash partition table |
| `dash/firmware/` | Dash sources and display headers |
| `shared/protocols/` | Single definitions of BLE UUIDs, frames, sensor faults and system events |
| `shared/libraries/` | Common configuration, board pins, battery policy and reset helpers |
| `<product>/hardware/` | Hardware revision metadata; future electronics sources |
| `<product>/mechanical/` | Future enclosure/mounting sources |
| `<product>/docs/` | Product documentation entry points |
| `docs/`, `tests/`, `scripts/` | Cross-product contracts, verification and tooling |

Run PlatformIO and scripts from the repository root. `platformio.ini` keeps the
existing six environment names and `.pio/build/<environment>` outputs. Logger
builds compile only Logger sources; Dash builds compile only its entry point.
Shared header contents, pin mappings, device identifiers, endpoints, partitions,
protocol versions, BLE records and runtime behaviour are preserved.

Local secrets now belong at `shared/libraries/AppSecrets.h` (ignored). Do not
copy secrets from old checkouts into commits or release artifacts. Existing
checkouts and their local credentials remain in place.

## Versions and artifacts

Firmware keeps the existing `vMAJOR.MINOR.PATCH` release train and Release Please
version history. There is no version reset. Release metadata adds separate
`logger-build-metadata.json` and `dash-build-metadata.json` records, each carrying
its firmware version and its product's target assets. Existing flat asset names
and aggregate metadata remain compatible with release consumers. Independent
release scheduling is a later change; this migration does not change tag syntax.

Each product's `hardware/revision.json` is independent of firmware version.
`hardware_revision: null` means no qualified custom revision is recorded. Board
and SDK settings are build targets, not proof of a manufactured PCB revision.

## Preserved bench work

The original `motorsport-data-acquisition` checkout and all linked worktrees are
retained. The Logger task confirmed no commits beyond the source cutoff. The
Dash task has uncommitted waiting-only dimming and capture tooling in
`.worktrees/data-acquisition-dash-power`; port those separately after review.
No hardware was flashed as part of APE-90.
