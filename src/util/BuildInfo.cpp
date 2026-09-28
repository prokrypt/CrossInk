#include "BuildInfo.h"

#include <AppVersion.h>

#include <cstring>

#ifndef CROSSINK_VERSION
#define CROSSINK_VERSION "dev"
#endif

#ifndef CROSSINK_GIT_SHA
#define CROSSINK_GIT_SHA "unknown"
#endif

#ifndef CROSSINK_GIT_DIRTY
#define CROSSINK_GIT_DIRTY "unknown"
#endif

#ifndef CROSSINK_GIT_BRANCH
#define CROSSINK_GIT_BRANCH "unknown"
#endif

#ifndef CROSSINK_GIT_BRANCH_SHORT
#define CROSSINK_GIT_BRANCH_SHORT CROSSINK_GIT_BRANCH
#endif

#ifndef CROSSINK_BUILD_NUMBER
#define CROSSINK_BUILD_NUMBER ""
#endif

#ifndef CROSSINK_BUILD_TIME
#define CROSSINK_BUILD_TIME "unknown"
#endif

namespace BuildInfo {
const char* gitBranch() { return CROSSINK_GIT_BRANCH; }
const char* shortBranch() { return CROSSINK_GIT_BRANCH_SHORT; }
const char* buildNumber() { return CROSSINK_BUILD_NUMBER; }
const char* buildTime() { return CROSSINK_BUILD_TIME; }
}  // namespace BuildInfo

namespace AppVersion {
const char* version() { return CROSSINK_VERSION; }
const char* versionLabel() { return "CrossInk " CROSSINK_VERSION; }
const char* userAgent() { return "CrossInk-ESP32-" CROSSINK_VERSION; }
const char* gitSha() { return CROSSINK_GIT_SHA; }
bool gitDirty() { return std::strcmp(CROSSINK_GIT_DIRTY, "1") == 0; }
const char* gitDirtyFlag() { return CROSSINK_GIT_DIRTY; }
}  // namespace AppVersion
