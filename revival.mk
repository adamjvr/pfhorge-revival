# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

SHELL := /bin/bash

.PHONY: stage1 audit baseline preview-core-check map-intake-check clean-revival help

help:
	@printf '%s\n' \
	  'Pfhorge Revival targets:' \
	  '  make -f revival.mk audit               Static source/project audit' \
	  '  make -f revival.mk baseline            macOS Xcode baseline build' \
	  '  make -f revival.mk stage1              Audit, then baseline build' \
	  '  make -f revival.mk preview-core-check  Compile renderer-neutral preview core' \
	  '  make -f revival.mk map-intake-check    Validate universal Marathon map intake' \
	  '  make -f revival.mk clean-revival       Remove generated reports'

audit:
	@python3 scripts/revival/source_audit.py --root . --output-dir RevivalArtifacts

baseline:
	@scripts/revival/baseline_build.sh

preview-core-check:
	@scripts/revival/validate_preview_core.sh

map-intake-check:
	@scripts/revival/validate_map_intake.sh

stage1: audit baseline preview-core-check map-intake-check

clean-revival:
	@find RevivalArtifacts -mindepth 1 ! -name .gitkeep -delete
