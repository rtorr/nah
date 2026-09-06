#include <algorithm>
#include <array>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <zlib.h>

// Public headers must remain usable after windows.h defines these macros.
#define min(a, b) NAH_WINDOWS_MIN_MACRO_MUST_NOT_EXPAND
#define max(a, b) NAH_WINDOWS_MAX_MACRO_MUST_NOT_EXPAND
#include <nah/nah_archive.h>
#include <nah/nah_digest.h>
#undef max
#undef min
