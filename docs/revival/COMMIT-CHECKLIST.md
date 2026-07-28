# Initial commit checklist

After extracting the overlay at the repository root:

```bash
git status --short
bash -n scripts/revival/bootstrap_macos.sh
python3 -m py_compile scripts/revival/parse_build_log.py scripts/revival/source_audit.py
make -f revival.mk audit
git add LICENSE NOTICE.md README.md .github docs revival.mk scripts RevivalArtifacts/.gitignore
git commit -m "build: establish Pfhorge Revival Stage 1 baseline"
```
 Review `git diff --cached` before committing and preserve all
existing upstream files and history.
