#pragma once

// Build identity. The version, commit and dirty flag change with every commit,
// so scripts/git_branch.py defines them only for src/util/BuildInfo.cpp, which
// implements these accessors. A global define would change every compile
// command and force a full rebuild after each commit.
namespace AppVersion {
const char* version();       // e.g. "1.6.0-x4-pro"
const char* versionLabel();  // "CrossInk <version>"
const char* userAgent();     // "CrossInk-ESP32-<version>"
const char* gitSha();        // short commit hash, or "unknown"
bool gitDirty();             // tracked files were modified when built
const char* gitDirtyFlag();  // "1", "0" or "unknown"
}  // namespace AppVersion

// PlatformIO normally supplies these through build_flags/extra_scripts. Keep
// fallbacks here so editor indexers and simulator-like tools still parse files.
#ifndef CROSSINK_PIOENV
#define CROSSINK_PIOENV "unknown"
#endif

#ifndef CROSSINK_BUILD_ENV
#define CROSSINK_BUILD_ENV "unknown"
#endif

#ifndef CROSSINK_FIRMWARE_DEVICE_TYPE
#define CROSSINK_FIRMWARE_DEVICE_TYPE "unknown"
#endif
