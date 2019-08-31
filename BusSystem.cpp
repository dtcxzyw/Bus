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
            BUS_TRACE_BEGIN("BusSystem.Win32Module") {
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
                using InitCall =
                    void (*)(const fs::path& path, ModuleSystem& system,
                             std::shared_ptr<ModuleInstance>& instance);
                reinterpret_cast<InitCall>(address)(path, system, mInstance);
                if(!mInstance)
                    BUS_TRACE_THROW(std::runtime_error(
                        "Failed to init module " + path.string()));
                auto tsp = mInstance->info().thirdPartySearchPath;
                auto base = path.parent_path();
                for(auto p : tsp)
                    addModuleSearchPath(base / p.data(), mReporter);
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
    std::vector<FunctionId> ModuleSystem::listFunctions(Name interfaceName) {
        std::vector<FunctionId> funcs;
        for(auto inst : mInstances) {
            auto res = inst.second->getInstance()->list(interfaceName);
            for(auto name : res)
                funcs.emplace_back(inst.first, name);
        }
        return funcs;
    }
    Reporter& ModuleSystem::getReporter() {
        return *mReporter;
    }
    std::pair<GUID, std::string> ModuleSystem::parse(const std::string& name,
                                                     Name interfaceName) {
        size_t pos = name.find_last_of('.');
        if(pos == name.npos) {
            GUID id;
            unsigned count = 0;
            for(auto inst : mInstances) {
                auto res = inst.second->getInstance()->list(interfaceName);
                for(auto&& func : res)
                    if(func == name) {
                        ++count, id = inst.first;
                        if(count > 1)
                            break;
                    }
                if(count > 1)
                    break;
            }
            if(count == 1)
                return std::make_pair(id, name);
            if(count == 0)
                mReporter->apply(ReportLevel::Error,
                                 "No function called " + name + " [interface=" +
                                     interfaceName.data() + "]",
                                 BUS_SRCLOC("BusSystem"));
            else
                mReporter->apply(ReportLevel::Error,
                                 "One or more multiply defined function.Please "
                                 "use GUID instead of name.",
                                 BUS_SRCLOC("BusSystem"));
            return {};
        } else {
            auto pre = name.substr(0, pos);
            auto nxt = name.substr(pos + 1);
            GUID id(0, 0);
            try {
                id = str2GUID(pre);
            } catch(...) {
            }
            if(id.first == 0 && id.second == 0) {
                unsigned count = 0;
                for(auto inst : mInstances) {
                    auto res = inst.second->getInstance();
                    if(res->info().name == name) {
                        for(auto func : res->list(interfaceName))
                            if(nxt == func) {
                                ++count;
                                id = inst.first;
                            }
                    }
                    if(count > 1)
                        break;
                }
                if(count == 1)
                    return std::make_pair(id, nxt);
                if(count == 0)
                    mReporter->apply(ReportLevel::Error,
                                     "No function called " + name +
                                         " [interface=" + interfaceName.data() +
                                         "].",
                                     BUS_SRCLOC("BusSystem"));
                else
                    mReporter->apply(
                        ReportLevel::Error,
                        "One or more multiply defined function.Please "
                        "use GUID instead of name.",
                        BUS_SRCLOC("BusSystem"));
                return {};
            } else {
                auto iter = mInstances.find(id);
                if(iter == mInstances.end()) {
                    mReporter->apply(ReportLevel::Error,
                                     "No module's GUID is " + pre + ".",
                                     BUS_SRCLOC("BusSystem"));
                    return {};
                }
                for(auto func :
                    iter->second->getInstance()->list(interfaceName))
                    if(func == nxt)
                        return std::make_pair(id, nxt);
                mReporter->apply(
                    ReportLevel::Error,
                    "Module " + pre + " [name=" +
                        iter->second->getInstance()->info().name.data() +
                        "] doesn't have function called " + nxt +
                        " [interface=" + interfaceName.data() + "].",
                    BUS_SRCLOC("BusSystem"));
                return {};
            }
        }
    }
}  // namespace Bus
