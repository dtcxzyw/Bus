#include "BusReporter.hpp"

namespace Bus {
    void Reporter::addAction(ReportLevel level,
                             const ReportFunction& function) {
        mActions[level].emplace_back(function);
    }
    void Reporter::apply(ReportLevel level, const std::string& message,
                         const SourceLocation& loc) {
        std::lock_guard guard(mMutex);
        auto iter = mActions.find(level);
        if(iter == mActions.cend())
            return;
        for(const ReportFunction& func : iter->second)
            func(level, message, loc);
    }
}  // namespace Bus
