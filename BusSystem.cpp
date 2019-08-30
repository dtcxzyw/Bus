#include "BusSystem.hpp"
#include "BusModule.hpp"
#include "BusReporter.hpp"
#include "Windows.h"

namespace Bus {
    static std::string winerr2String(DWORD code) {
        char buf[1024];
        if(FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, buf,
                          sizeof(buf), 0))
            return "Error Code:" + std::to_string(code) + "\nReason:" + buf;
        return "Unknown Error(Code:" + std::to_string(code) + ")";
    }

    void addModuleSearchPath(const fs::path& path, Reporter& reporter) {
        if(AddDllDirectory(path.c_str()) == 0)
            reporter.apply(ReportLevel::Error,
                           "Failed to add dll search path " + path.string() +
                               ":" + winerr2String(GetLastError()),
                           BUS_SRCLOC("BusSystem"));
    }

    static void freeMod(HMODULE module, Reporter& reporter) {
        if(module == NULL)
            return;
        if(FreeLibrary(module) != TRUE)
            reporter.apply(ReportLevel::Error, "Failed to free module.",
                           BUS_SRCLOC("BusSystem::Win32Module::ModuleHolder"));
    }

    struct ModuleHolder final : private Unmoveable {
        Reporter& reporter;
        HMODULE module;

        ModuleHolder(HMODULE module, Reporter& reporter)
            : reporter(reporter), module(module) {}
        ~ModuleHolder() {
            freeMod(module, reporter);
        }
    };

    class Win32Module : public ModuleLibrary {
    private:
        Reporter& mReporter;
        HMODULE mModule;
        std::shared_ptr<ModuleInstance> mInstance;

    public:
        explicit Win32Module(const fs::path& path, ModuleSystem& system)
            : mReporter(system.getReporter()) {
            BUS_TRACE_BEGIN("BusSystem::Win32Module") {
                ModuleHolder tmp(
                    LoadLibraryExW(path.c_str(), NULL,
                                   LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                       LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                                       LOAD_LIBRARY_SEARCH_USER_DIRS |
                                       LOAD_LIBRARY_SEARCH_SYSTEM32),
                    mReporter);
                if(tmp.module == NULL)
                    BUS_TRACE_THROW(std::runtime_error(
                        "Failed to load module " + path.string() + '\n' +
                        winerr2String(GetLastError())));
                FARPROC address = GetProcAddress(tmp.module, "busInitModule");
                if(!address)
                    BUS_TRACE_THROW(std::runtime_error(
                        "Failed to init module " + path.string() + '\n' +
                        winerr2String(GetLastError())));
                using InitCall = std::shared_ptr<Bus::ModuleInstance> (*)(
                    const Bus::fs::path& path, Bus::ModuleSystem& system);
                mInstance = reinterpret_cast<InitCall>(address)(path, system);
                if(!mInstance)
                    BUS_TRACE_THROW(std::runtime_error(
                        "Failed to init module " + path.string()));
                mModule = tmp.module;
                tmp.module = NULL;
            }
            BUS_TRACE_END();
        }
        std::shared_ptr<ModuleInstance> getInstance() override {
            return mInstance;
        }
        ~Win32Module() {
            mInstance.reset();
            freeMod(mModule, mReporter);
        }
    };

    bool ModuleSystem::loadModuleFile(const fs::path& path) {
        return load(std::make_shared<Win32Module>(path, *this));
    }

    class BuiltinWrapper final : public ModuleLibrary {
    private:
        std::shared_ptr<ModuleInstance> mInstance;

    public:
        explicit BuiltinWrapper(std::shared_ptr<ModuleInstance> instance)
            : mInstance(instance) {}
        std::shared_ptr<ModuleInstance> getInstance() override {
            return mInstance;
        }
    };

    bool ModuleSystem::wrapBuiltin(
        const std::function<std::shared_ptr<ModuleInstance>(ModuleSystem&)>&
            gen) {
        return load(std::make_shared<BuiltinWrapper>(gen(*this)));
    }

    ModuleSystem::ModuleSystem(std::shared_ptr<Reporter> reporter)
        : mReporter(reporter) {}
    bool ModuleSystem::load(std::shared_ptr<ModuleLibrary> library) {
        GUID guid = library->getInstance()->info().guid;
        auto iter = mInstances.find(guid);
        if(iter != mInstances.cend())
            return false;
        mInstances.emplace(guid, library);
        return true;
    }
    std::shared_ptr<ModuleFunctionBase>
    ModuleSystem::instantiateImpl(FunctionId id) {
        auto iter = mInstances.find(id.guid);
        if(iter != mInstances.cend())
            return iter->second->getInstance()->instantiate(id.name);
        return nullptr;
    }
    std::vector<ModuleInfo> ModuleSystem::listModules() {
        std::vector<ModuleInfo> res;
        for(auto inst : mInstances)
            res.emplace_back(inst.second->getInstance()->info());
        return res;
    }
    std::vector<FunctionId> ModuleSystem::listFunctions(Name api) {
        std::vector<FunctionId> funcs;
        for(auto inst : mInstances) {
            auto res = inst.second->getInstance()->list(api);
            funcs.insert(funcs.end(), res.begin(), res.end());
        }
        return funcs;
    }
    Reporter& ModuleSystem::getReporter() {
        return *mReporter;
    }
}  // namespace Bus
