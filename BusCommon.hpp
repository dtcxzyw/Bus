#pragma once

#define BUS_VERSION "0.0.1"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Bus {
    namespace fs = std::filesystem;
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

    struct ModuleInfo final {
        Name name;
        GUID guid;
        Name busVersion;
        Name version;
        Name description;
        Name copyright;
        std::vector<Name> thirdPartySearchPath;
        fs::path modulePath;
    };

}  // namespace Bus
