#include <cstring>
#ifdef _WIN32
#define MODULE_EXPORT extern "C" __declspec(dllexport)
#else
#define MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

MODULE_EXPORT int add(int left, int right) { return left + right; }
MODULE_EXPORT double scale(double value, double factor) { return value * factor; }
MODULE_EXPORT int string_length(const char *text) { return static_cast<int>(std::strlen(text)); }
