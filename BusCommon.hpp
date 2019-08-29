#pragma once

#define BUS_VERSION "0.0.1"

#include <filesystem>
#include <string>
#include <string_view>

namespace Bus {
    namespace fs = std::experimental::filesystem;
    using Name = std::string_view;
    struct Unmoveable {
        Unmoveable() = default;
        ~Unmoveable() = default;
        Unmoveable(const Unmoveable&) = delete;
        Unmoveable(Unmoveable&&) = delete;
        Unmoveable& operator=(const Unmoveable&) = delete;
        Unmoveable& operator=(Unmoveable&&) = delete;
    };

    using GUID = std::pair<uint64_t, uint64_t>;
    GUID str2GUID(const std::string& guid);
    std::string GUID2Str(GUID guid);

    class ModuleSystem;
    class ModuleInstance;
    class Reporter;
    class ModuleFunctionBase;
}  // namespace Bus
