# First Mac test

## Prerequisites

- A Mac capable of running a currently supported Xcode release.
- Full Xcode installed, not only Command Line Tools.
- The Pfhorge source present in this repository.

## Run

```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
xcodebuild -version
./scripts/revival/bootstrap_macos.sh --no-branch
```

The baseline build is expected to fail until legacy source issues are repaired.
That failure is useful: the scripts preserve the complete transcript and create
structured reports in `RevivalArtifacts/`.

## Return for analysis

Archive the generated directory:

```bash
tar -czf Pfhorge-RevivalArtifacts-$(date +%Y%m%d).tar.gz RevivalArtifacts
```

Do not commit the generated reports unless a particular diagnostic is being
preserved as a small curated test fixture.
