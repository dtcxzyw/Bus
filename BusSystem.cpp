#include "BusSystem.hpp"
#include "BusModule.hpp"
#include "BusReporter.hpp"
#include "Windows.h"
#ifdef BUS_MSVC_DELAYLOAD
#define DELAYIMP_INSECURE_WRITABLE_HOOKS
#pragma comment(lib, "delayimp.lib")
#include <delayimp.h>
#endif

namespace Bus {
    static std::string winerr2String(const std::string& func, DWORD code) {
        char buf[1024];
        std::string res = "Failed to call function " + func +
            "\nError Code:" + std::to_string(code) + "\nReason:";
        if(FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, buf,
                          sizeof(buf), 0))
            res += buf;
        else
            res += "Unknown";
        return res;
    }

#ifdef BUS_MSVC_DELAYLOAD
    static Reporter* pReporter;
    FARPROC WINAPI notify(unsigned dliNotify, PDelayLoadInfo pdli) {
        if(dliNotify == dliNotePreLoadLibrary) {
            if(pReporter)
                pReporter->apply(ReportLevel::Info,
                                 std::string("loading ") + pdli->szDll,
                                 BUS_SRCLOC("BusSystem.MSVCDelayLoader"));
            HMODULE res = LoadLibraryExA(pdli->szDll, NULL,
                                         LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(res == NULL && pReporter)
                pReporter->apply(
                    ReportLevel::Error,
                    winerr2String("LoadLibraryExA", GetLastError()),
                    BUS_SRCLOC("BusSystem.MSVCDelayLoader"));
            return reinterpret_cast<FARPROC>(res);
        }
        return NULL;
    }
    FARPROC WINAPI failure(unsigned dliNotify, PDelayLoadInfo pdli) {
        if(pReporter) {
            if(dliNotify == dliFailGetProc) {
                std::string prog = pdli->dlp.fImportByName ?
                    std::string(pdli->dlp.szProcName) :
                    std::to_string(pdli->dlp.dwOrdinal);
                pReporter->apply(ReportLevel::Error,
                                 "Failed to get function " + prog +
                                     "'s address in module " + pdli->szDll,
                                 BUS_SRCLOC("BusSystem.MSVCDelayLoader"));
            }
            if(dliNotify == dliFailLoadLib) {
                pReporter->apply(ReportLevel::Error,
                                 "Failed to load module " +
                                     std::string(pdli->szDll),
                                 BUS_SRCLOC("BusSystem.MSVCDelayLoader"));
            }
        }
        return NULL;
    }
#endif

    void addModuleSearchPath(const fs::path& path, Reporter& reporter) {
        if(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) == FALSE)
            reporter.apply(
                ReportLevel::Error,
                "Failed to set default dll directories.\n" +
                    winerr2String("SetDefaultDllDirectories", GetLastError()),
                BUS_SRCLOC("BusSystem"));
        if(AddDllDirectory(path.c_str()) == 0)
            reporter.apply(ReportLevel::Error,
                           "Failed to add dll search path " + path.string() +
                               "\n" +
                               winerr2String("AddDllDirectory", GetLastError()),
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
        explicit Win32Module(fs::path path, ModuleSystem& system,
                             const ExceptionHandler& handler)
            : mReporter(system.getReporter()) {
            BUS_TRACE_BEGIN("BusSystem.Win32Module") {
                path = fs::absolute(path);
                ModuleHolder tmp(
                    LoadLibraryExW(path.c_str(), NULL,
                                   LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                       LOAD_LIBRARY_SEARCH_DEFAULT_DIRS),
                    mReporter);
                if(tmp.module == NULL)
                    BUS_TRACE_THROW(std::runtime_error(
                        "Failed to load module " + path.string() + '\n' +
                        winerr2String("LoadLibraryExW", GetLastError())));
                FARPROC address = GetProcAddress(tmp.module, "busInitModule");
                if(!address)
                    BUS_TRACE_THROW(std::runtime_error(
                        "Failed to init module " + path.string() + '\n' +
                        winerr2String("GetProcAddress", GetLastError())));
                using InitCall =
                    void (*)(const fs::path& path, ModuleSystem& system,
                             std::shared_ptr<ModuleInstance>& instance);
                try {
                    reinterpret_cast<InitCall>(address)(path, system,
                                                        mInstance);
                } catch(...) {
                    handler();
                }
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
        return load(std::make_shared<Win32Module>(path, *this, mHandler));
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

    ModuleSystem::ModuleSystem(std::shared_ptr<Reporter> reporter,
                               const ExceptionHandler& handler)
        : mReporter(reporter), mHandler(handler) {
#ifdef BUS_MSVC_DELAYLOAD
        pReporter = mReporter.get();
#endif
    }
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
                    if(res->info().name == pre) {
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
    ModuleSystem::~ModuleSystem() {
#ifdef BUS_MSVC_DELAYLOAD
        pReporter = nullptr;
#endif
    }
}  // namespace Bus

#ifdef BUS_MSVC_DELAYLOAD
PfnDliHook __pfnDliNotifyHook2 = Bus::notify;
PfnDliHook __pfnDliFailureHook2 = Bus::failure;
#endif
