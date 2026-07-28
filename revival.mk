# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Adam Vadala-Roth

SHELL := /bin/bash

.PHONY: stage1 audit baseline clean-revival help

help:
	@printf '%s\n' \
	  'Pfhorge Revival targets:' \
	  '  make -f revival.mk audit          Static source/project audit' \
	  '  make -f revival.mk baseline       macOS Xcode baseline build' \
	  '  make -f revival.mk stage1         Audit, then baseline build' \
	  '  make -f revival.mk clean-revival  Remove generated reports'

audit:
	@python3 scripts/revival/source_audit.py --root . --output-dir RevivalArtifacts

baseline:
	@scripts/revival/baseline_build.sh

stage1: audit baseline

clean-revival:
	@find RevivalArtifacts -mindepth 1 ! -name .gitkeep -delete
