# Pfhorge Revival license policy

## Project license

Pfhorge Revival uses **GPL-2.0-or-later**.

The repository contains the complete GNU GPL version 2 text in `LICENSE`. The
"or later" permission comes from the licensing notices in the original Pfhorge
source, which permit use under GPL version 2 or, at the recipient's option, any
later version.

## Existing source files

- Preserve original copyright notices.
- Preserve existing GPL notices.
- Do not replace an original author's copyright with a revival contributor's
  name.
- When materially modifying a file, add an accurate change notice where useful
  without obscuring the original provenance.

## New source and configuration files

Place an SPDX line near the top:

```text
SPDX-License-Identifier: GPL-2.0-or-later
```

Suitable comment syntax includes:

```c
// SPDX-License-Identifier: GPL-2.0-or-later
```

```python
# SPDX-License-Identifier: GPL-2.0-or-later
```

New documentation may include the SPDX line when convenient, but the repository
license and notices govern even where a Markdown file omits a per-file header.

## Dependencies and imported code

Before adding a dependency or copying code, record:

- upstream project and source URL
- exact version or commit
- license identifier
- whether the license is GPL-2.0-compatible
- any required notices or source-distribution obligations

Do not import code under a license incompatible with GPL version 2.

## Future clean implementation

Stages 1 through 3 directly modify or reorganize GPL-covered Pfhorge code and
remain GPL-2.0-or-later. A future independent implementation may reconsider its
license only if it is genuinely not derived from copied or translated Pfhorge
implementation code and has documented provenance supporting that conclusion.
