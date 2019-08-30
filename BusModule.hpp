#pragma once
#include "BusCommon.hpp"
#include <memory>
#include <vector>

namespace Bus {
#define BUS_API extern "C" __declspec(dllexport)

    class ModuleFunctionBase : private Unmoveable {
    protected:
        ModuleInstance& mInstance;
        fs::path modulePath();
        ModuleSystem& system();
        Reporter& reporter();
        explicit ModuleFunctionBase(ModuleInstance& instance);
        virtual ~ModuleFunctionBase() = default;
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
        virtual std::vector<Name> list(Name interfaceName) const = 0;
        virtual std::shared_ptr<ModuleFunctionBase> instantiate(Name name) = 0;
        virtual ~ModuleInstance() = default;
    };
}  // namespace Bus
