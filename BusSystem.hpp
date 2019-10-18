#pragma once
#include "BusCommon.hpp"
#include <functional>
#include <map>

namespace Bus {
    class ModuleLibrary : private Unmoveable {
    public:
        virtual ~ModuleLibrary() = default;
        virtual std::shared_ptr<ModuleInstance> getInstance() = 0;
    };

    struct FunctionId final {
        GUID guid;
        Name name;
        FunctionId(GUID guid, Name name) : guid(guid), name(name) {}
    };

    void addModuleSearchPath(const fs::path& path, Reporter& reporter);

    class ModuleSystem final : private Unmoveable {
    private:
        std::shared_ptr<Reporter> mReporter;
        std::map<GUID, std::shared_ptr<ModuleLibrary>> mInstances;
        std::shared_ptr<ModuleFunctionBase> instantiateImpl(FunctionId id);
        std::pair<GUID, std::string> parse(const std::string& name,
                                           Name interfaceName);
        bool load(std::shared_ptr<ModuleLibrary> library);

    public:
        explicit ModuleSystem(std::shared_ptr<Reporter> reporter);
        ~ModuleSystem();
        Reporter& getReporter();
        bool loadModuleFile(const fs::path& path);
        bool wrapBuiltin(
            const std::function<std::shared_ptr<ModuleInstance>(ModuleSystem&)>&
                gen);
        template <typename T>
        std::shared_ptr<T> instantiate(FunctionId id) {
            return std::dynamic_pointer_cast<T>(instantiateImpl(id));
        }
        std::vector<ModuleInfo> listModules();
        std::vector<FunctionId> listFunctions(Name interfaceName);
        template <typename T>
        std::vector<FunctionId> list() {
            return listFunctions(T::getInterface());
        }
        std::shared_ptr<Bus::ModuleFunctionBase>
        instantiateByName(Name interfaceName, const std::string& name) {
            auto id = parse(name, interfaceName);
            FunctionId fid(id.first, id.second);
            return instantiateImpl(fid);
        }
        template <typename T>
        std::shared_ptr<T> instantiateByName(const std::string& name) {
            auto id = parse(name, T::getInterface());
            FunctionId fid(id.first, id.second);
            return instantiate<T>(fid);
        }
    };
}  // namespace Bus
