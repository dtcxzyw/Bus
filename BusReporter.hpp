#pragma once
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Bus {
    enum class ReportLevel { Warning, Debug, Error, Info };
    struct SourceLocation final {
        const char* module;
        const char* srcFile;
        const char* functionName;
        int line;
        SourceLocation(const char* module, const char* srcFile,
                       const char* functionName, int line)
            : srcFile(srcFile), functionName(functionName), line(line) {}
    };
#define BUS_SRCLOC(MODULE) \
    Bus::SourceLocation(MODULE, __FILE__, __FUNCTION__, __LINE__)
    using ReportFunction = std::function<void(ReportLevel, const std::string&,
                                              const SourceLocation& srcLoc)>;
    using TraceFunction = std::function<void(const SourceLocation&)>;

    class Reporter final : private Unmoveable {
    private:
        std::map<ReportLevel, std::vector<ReportFunction>> mActions;
        std::mutex mMutex;

    public:
        void addAction(ReportLevel level, const ReportFunction& function);
        void apply(ReportLevel level, const std::string& message,
                   const SourceLocation& loc) const;
    };

#define BUS_TRACE_BEGIN(MODULE)                       \
    SourceLocation _bus_srcloc_ = BUS_SRCLOC(MODULE); \
    try
#define BUS_TRACE_THROW(ex)       \
    _bus_srcloc_.line = __LINE__; \
    throw(ex)
#define BUS_TRACE_END()                       \
    catch(...) {                              \
        std::throw_with_nested(_bus_srcloc_); \
    }
}  // namespace Bus
