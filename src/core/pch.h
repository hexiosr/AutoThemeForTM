// pch.h - 预编译头
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cstdint>
#include <cstdlib>
#include <ctime>

#define TRAFFICE_MONITOR_PLUGIN
#include "PluginInterface.h"