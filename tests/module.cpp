#include <cstring>
#ifdef _WIN32
#define MODULE_EXPORT extern "C" __declspec(dllexport)
#else
#define MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {
    int count = 0;
}
MODULE_EXPORT int module_add(int left, int right) { return left + right; }
MODULE_EXPORT double module_scale(int n, double factor, float offset) { return n * factor + offset; }
MODULE_EXPORT signed char module_char(signed char n) { return n; }
MODULE_EXPORT unsigned char module_byte(unsigned char n) { return n; }
MODULE_EXPORT short module_short(short n) { return n; }
MODULE_EXPORT unsigned short module_ushort(unsigned short n) { return n; }
MODULE_EXPORT unsigned int module_uint(unsigned int n) { return n; }
MODULE_EXPORT long module_long(long n) { return n; }
MODULE_EXPORT unsigned long module_ulong(unsigned long n) { return n; }
MODULE_EXPORT long long module_longlong(long long n) { return n; }
MODULE_EXPORT unsigned long long module_unsigned(unsigned long long n) { return n; }
MODULE_EXPORT float module_float(float n) { return n; }
MODULE_EXPORT long double module_extended(long double n) { return n; }
MODULE_EXPORT bool module_bool(bool n) { return !n; }
MODULE_EXPORT int module_length(const char *text) { return static_cast<int>(std::strlen(text)); }
MODULE_EXPORT void module_bump() { ++count; }
MODULE_EXPORT int module_count() { return count; }
MODULE_EXPORT int module_spare() { return 42; }
