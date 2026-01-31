# Release Instructions for GhostMem

This document describes how to create new releases of GhostMem with downloadable binaries.

## Automated Release Process (Recommended)

### Prerequisites
- Push access to the repository
- All tests passing on main branch

### Steps

1. **Update Version Numbers**
   
   Edit [src/ghostmem/Version.h](src/ghostmem/Version.h):
   ```cpp
   #define GHOSTMEM_VERSION_MAJOR 0
   #define GHOSTMEM_VERSION_MINOR 9  // Update this
   #define GHOSTMEM_VERSION_PATCH 0  // And this
   ```
   
   Update [README.md](README.md):
   ```markdown
   **Version 0.9.0**  # Update this
   ```

2. **Commit Version Changes**
   ```bash
   git add src/ghostmem/Version.h README.md
   git commit -m "Bump version to 0.9.0"
   git push origin main
   ```

3. **Create and Push Tag**
   ```bash
   git tag -a v0.9.0 -m "Release version 0.9.0 - Brief description of changes"
   git push origin v0.9.0
   ```

4. **Monitor Release Workflow**
   - Go to GitHub Actions tab
   - Watch the "Release" workflow execute
   - It will build for Windows and Linux
   - Create a GitHub Release automatically

5. **Edit Release Notes (Optional)**
   - Go to the Releases page
   - Click "Edit" on the new release
   - Add detailed release notes, breaking changes, etc.
   - Publish the release

## Manual Workflow Trigger

If you need to create a release without pushing a tag:

1. Go to **GitHub Actions**
2. Select **Release** workflow
3. Click **Run workflow**
4. Enter the tag name (e.g., `v0.9.0`)
5. Click **Run workflow** button

## What Gets Released

Each release includes two packages:

### Windows Package (`ghostmem-windows-x64.zip`)
```
ghostmem-windows-x64/
├── lib/
│   ├── ghostmem_shared.dll      # Shared/dynamic library
│   ├── ghostmem_shared.lib      # Import library for DLL
│   └── ghostmem.lib             # Static library
├── include/
│   ├── GhostMemoryManager.h
│   ├── GhostAllocator.h
│   └── Version.h
├── bin/
│   └── ghostmem_demo.exe        # Demo application
└── README.md
```

### Linux Package (`ghostmem-linux-x64.tar.gz`)
```
ghostmem-linux-x64/
├── lib/
│   ├── libghostmem_shared.so    # Shared library
│   └── libghostmem.a            # Static library
├── include/
│   ├── GhostMemoryManager.h
│   ├── GhostAllocator.h
│   └── Version.h
├── bin/
│   └── ghostmem_demo            # Demo application
└── README.md
```

## Release Workflow Details

The [.github/workflows/release.yml](.github/workflows/release.yml) workflow:

1. **Triggers on**:
   - Tag pushes matching `v*` pattern
   - Manual workflow dispatch

2. **Builds**:
   - Windows x64 (using MSVC via Ninja)
   - Linux x64 (using GCC/G++)
   - Both in Release configuration

3. **Packages**:
   - Creates structured directories
   - Copies libraries, headers, demo
   - Creates .zip (Windows) and .tar.gz (Linux)

4. **Publishes**:
   - Creates GitHub Release
   - Uploads packaged artifacts
   - Auto-generates release notes from commits

## Troubleshooting

### Release Workflow Fails

1. Check GitHub Actions logs for specific errors
2. Ensure all tests pass on main branch
3. Verify CMake configuration is correct
4. Check that all files exist in expected locations

### Missing Files in Package

If headers or libraries are missing:
1. Check the workflow's "Package Artifacts" step
2. Verify paths in the workflow match actual build output
3. Ensure CMake builds all targets correctly

### Tag Already Exists

If you need to recreate a release:
```bash
# Delete tag locally
git tag -d v0.9.0

# Delete tag remotely
git push origin :refs/tags/v0.9.0

# Create new tag
git tag -a v0.9.0 -m "Release version 0.9.0"
git push origin v0.9.0
```

## Version Numbering

GhostMem follows [Semantic Versioning](https://semver.org/):

- **MAJOR**: Breaking API changes
- **MINOR**: New features, backwards compatible
- **PATCH**: Bug fixes, backwards compatible

Examples:
- `v0.9.0` → `v0.9.1`: Bug fixes
- `v0.9.0` → `v0.10.0`: New feature
- `v0.9.0` → `v1.0.0`: API breaking change or major milestone

## Testing a Release

Before announcing a release:

1. **Download** the artifacts from GitHub
2. **Extract** the packages
3. **Test on clean systems**:
   - Windows: Test DLL linking
   - Linux: Test SO linking
   - Run demo executable
4. **Verify** headers are complete
5. **Check** README is included

## Pre-Release Checklist

- [ ] Version numbers updated in code and docs
- [ ] All tests passing
- [ ] CHANGELOG updated (if exists)
- [ ] Breaking changes documented
- [ ] Known issues documented
- [ ] Build tested locally on Windows and Linux
- [ ] Demo executable runs successfully
- [ ] Git tag created with descriptive message

## Post-Release Checklist

- [ ] Release appears on GitHub Releases page
- [ ] Artifacts are downloadable
- [ ] Release notes are clear
- [ ] Announcement made (if applicable)
- [ ] Documentation updated
- [ ] Version badge updated in README (if applicable)
