# FM1 MDX work

Use the issue-first GitHub workflow. Preserve the current working firmware and
device rollback separately. Keep this repository source-only; never commit
stock dumps, unit-specific images, chip keys, SDK binaries or commercial songs.

Run `python scripts/build.py host` for player/USB/lifecycle changes. For board
changes, also link with the pinned SDK using `python scripts/build.py firmware`.
Report host/static validation separately from bench acceptance. Physical flashing
requires explicit user authorization for the exact candidate and rollback plan.

Karaoke mute suppresses playback triggers, not sequencing or parameter writes.
The player is owned by one task; IRQs only drain PCM or service the scanner.
Do not introduce another concurrent synth owner or a flash writer into song
upload commands. Volatile USB uploads remain bounded to192KiB.
