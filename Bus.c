#include "Bus.h"

// Setup
#if defined(_WIN32)
#define BUS_SYSTEM_WINDOWS
#elif defined(__linux)
#define BUS_SYSTEM_LINUX
#elif defined(__APPLE__)
#define BUS_SYSTEM_APPLE
#else
#error "Unsupported system."
#endif  // defined(_WIN32)

#ifdef BUS_SYSTEM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#endif  // BUS_SYSTEM_WINDOWS
