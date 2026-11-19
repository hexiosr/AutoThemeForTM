// 体积优化：屏蔽 libstdc++ 自带的 __cxa_demangle（约 50 KB），
// 只影响崩溃信息里类型名的可读性，插件功能不依赖它。
#include <cstddef>

extern "C" char* __wrap___cxa_demangle(const char*, char*, size_t*, int*) {
    return nullptr;
}