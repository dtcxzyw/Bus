#include "BusModule.hpp"
#include "BusCommon.cpp"
#include "BusReporter.cpp"
#include "BusSystem.hpp"

namespace Bus {
    ModuleFunctionBase::ModuleFunctionBase(ModuleInstance& instance)
        : mInstance(instance) {}
    fs::path ModuleFunctionBase::modulePath() {
        return mInstance.getModulePath();
    }
    ModuleSystem& ModuleFunctionBase::system() {
        return mInstance.getSystem();
    }
    Reporter& ModuleFunctionBase::reporter() {
        return system().getReporter();
    }
    explicit ModuleInstance::ModuleInstance(const fs::path& path,
                                            ModuleSystem& system)
        : mModulePath(path), mSystem(system) {}
    fs::path ModuleInstance::getModulePath() const {
        return mModulePath;
    }
    ModuleSystem& ModuleInstance::getSystem() {
        return mSystem;
    }
}  // namespace Bus
