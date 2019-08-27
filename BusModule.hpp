#pragma once
#include "BusCommon.hpp"
#include <string>
#include <vector>

#define BUS_API __declspec(dllexport)
extern "C" {
BUS_API std::shared_ptr<Bus::ModuleInstance>
busInitModule(const Bus::fs::path& path, Bus::ModuleSystem& system);
}
