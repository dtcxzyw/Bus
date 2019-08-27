#pragma once

#define BUS_VERSION "0.0.1"

#include "BusReporter.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

    class ModuleSystem;
    class ModuleInstance;

    class ModuleFunctionBase : private Unmoveable {
    private:
        ModuleInstance& mInstance;

    protected:
        fs::path modulePath();
        ModuleSystem& system();
        Reporter& reporter();
        explicit ModuleFunctionBase(ModuleInstance& instance);
        virtual ~ModuleFunctionBase() = default;
    };

    using GUID = std::pair<uint64_t, uint64_t>;
    GUID str2GUID(const std::string& guid);
    std::string GUID2Str(GUID guid);

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

    class ModuleInstance : private Unmoveable {
    protected:
        fs::path mModulePath;
        ModuleSystem& mSystem;
        explicit ModuleInstance(const fs::path& path, ModuleSystem& system);

    public:
        fs::path getModulePath() const;
        ModuleSystem& getSystem();
        virtual ModuleInfo info() const = 0;
        virtual std::vector<Name> list(Name interface) const = 0;
        virtual std::shared_ptr<ModuleFunctionBase> instantiate(Name name) = 0;
        virtual ~ModuleInstance() = default;
    };

}  // namespace Bus
