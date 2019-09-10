#pragma once
#include "BusCommon.hpp"
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace Bus {
    enum class ReportLevel { Warning, Debug, Error, Info };
    struct SourceLocation {
        const char* module;
        const char* srcFile;
        const char* functionName;
        int line;
        SourceLocation(const char* module, const char* srcFile,
                       const char* functionName, int line)
            : module(module), srcFile(srcFile), functionName(functionName),
              line(line) {}
        virtual ~SourceLocation() = default;
    };
#define BUS_SRCLOC(MODULE) \
    Bus::SourceLocation(MODULE, __FILE__, __FUNCTION__, __LINE__)
    using ReportFunction = std::function<void(ReportLevel, const std::string&,
                                              const SourceLocation& srcLoc)>;

    class Reporter final : private Unmoveable {
    private:
        std::map<ReportLevel, std::vector<ReportFunction>> mActions;
        std::mutex mMutex;

    public:
        void addAction(ReportLevel level, const ReportFunction& function);
        void apply(ReportLevel level, const std::string& message,
                   const SourceLocation& loc);
    };

#define BUS_TRACE_BEGIN(MODULE)                            \
    Bus::SourceLocation _bus_srcloc_ = BUS_SRCLOC(MODULE); \
    try
#define BUS_TRACE_BEG() BUS_TRACE_BEGIN(_bus_module_name_)
#define BUS_TRACE_THROW(ex) _bus_srcloc_.line = __LINE__, throw(ex)
#define BUS_TRACE_END()                       \
    catch(...) {                              \
        std::throw_with_nested(_bus_srcloc_); \
    }
}  // namespace Bus
