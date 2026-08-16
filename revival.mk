# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

SHELL := /bin/bash

.PHONY: stage1 audit baseline preview-core-check map-intake-check map1b-check content1a-check clean-revival help content1a1-check content1a2-check tex1a-check vm4a-check tex1a2-check native-format-check format1b-check format1c-check format2a-check

help:
	@printf '%s\n' \
	  'Pfhorge Revival targets:' \
	  '  make -f revival.mk audit               Static source/project audit' \
	  '  make -f revival.mk baseline            macOS Xcode baseline build' \
	  '  make -f revival.mk stage1              Audit, then baseline build' \
	  '  make -f revival.mk preview-core-check  Compile renderer-neutral preview core' \
	  '  make -f revival.mk map-intake-check    Validate universal Marathon map intake' \
	  '  make -f revival.mk native-format-check Validate Pfhorge Native vNext package foundation' \
	  '  make -f revival.mk format1b-check       Audit semantic fields and compile canonical format core' \
	  '  make -f revival.mk format1c-check       Validate complete canonical JSON schema candidate' \
	  '  make -f revival.mk format2a-check       Validate native .pfhlev integration + Xcode build' \
	  '  make -f revival.mk map1b-check         Validate MAP-1B and the macOS baseline' \
	  '  make -f revival.mk content1a-check     Validate Content Manager and VM settings' \
	  '  make -f revival.mk content1a1-check    Validate content selection and mouse/camera polish' \
	  '  make -f revival.mk content1a2-check    Validate Shapes/texture UX, progress, and 240 Hz' \
	  '  make -f revival.mk tex1a-check        Validate classic Shapes rendering in Metal' \
	  '  make -f revival.mk vm4a-check       Validate doors, collision, live sync, and texture audit' \
	  '  make -f revival.mk tex1a2-check     Validate complete wall-side and transparent texture rendering' \
	  '  make -f revival.mk clean-revival       Remove generated reports'

audit:
	@python3 scripts/revival/source_audit.py --root . --output-dir RevivalArtifacts

baseline:
	@scripts/revival/baseline_build.sh

preview-core-check:
	@scripts/revival/validate_preview_core.sh

map-intake-check:
	@scripts/revival/validate_map_intake.sh

map1b-check:
	@scripts/revival/validate_map1b.sh

content1a-check:
	@scripts/revival/validate_content1a.sh

vm4a-check:
	@scripts/revival/validate_vm4a.sh

tex1a2-check:
	@scripts/revival/validate_tex1a2.sh

tex1a-check:
	@scripts/revival/validate_tex1a.sh
	@$(MAKE) -f revival.mk preview-core-check
	@$(MAKE) -f revival.mk content1a2-check

content1a2-check:
	@scripts/revival/validate_content1a2.sh

content1a1-check:
	@scripts/revival/validate_content1a1.sh

native-format-check:
	@scripts/revival/validate_native_format.sh

format1b-check:
	@scripts/revival/validate_format1b.sh

format1c-check:
	@scripts/revival/validate_format1c.sh

format2a-check:
	@scripts/revival/validate_format2a.sh

stage1: audit baseline preview-core-check map-intake-check

clean-revival:
	@find RevivalArtifacts -mindepth 1 ! -name .gitkeep -delete
