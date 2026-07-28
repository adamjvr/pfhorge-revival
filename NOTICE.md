# Notices and attribution

## Original Pfhorge project

Pfhorge was created by **Joshua D. Orr**. The surviving source includes original
copyright notices beginning in 2001 and grants redistribution and modification
under GNU GPL version 2 or, at the recipient's option, any later version.

Original copyright and license notices in inherited files must remain intact.
The repository is distributed as a combined work under GPL-3.0-or-later, while
preserving the valid original notices on individual files.

## Pfhorge Revival

Revival infrastructure and new work beginning in 2026 are maintained by
**Adam Vadala-Roth** and project contributors.

New revival-owned files should use:

```text
SPDX-License-Identifier: GPL-3.0-or-later
```

A contributor should add a copyright notice only for a copyrightable
contribution they made. Existing notices must not be removed or replaced.

## Aleph One integration

Visual Mode work may adapt selected rendering and map-semantics code from:

- Project: Aleph One
- Upstream: `Aleph-One-Marathon/alephone`
- License: GNU GPL version 3 or later
- Scope: portal visibility, clipping, surface construction, texture semantics,
  transfer modes, lighting, landscapes, media, and sprite behavior

Every imported or adapted file must identify its upstream path and exact commit
in a nearby provenance comment or in
`docs/revival/ALEPH-ONE-INTEGRATION.md`.

Pfhorge Revival will not import the Aleph One application shell, networking,
gameplay, AI, HUD, audio loop, or SDL window management merely to provide an
editor preview.

## Third-party game data

Marathon scenarios, Shapes files, sounds, terminal artwork, and other game data
may have separate copyright and redistribution terms. Do not commit proprietary
Bungie assets or community content unless its license or explicit permission
allows redistribution.
