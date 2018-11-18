#ifndef DEBUG_H_INCLUDED_
#define DEBUG_H_INCLUDED_

#if !defined(DEBUG)
#define DEBUG 0
#endif

#if !DEBUG

#define DEBUG_FUNCTION_CALL()
#define DEBUG_FUNCTION_PRINT(...)
#define DEBUG_FUNCTION_PRINTLN(...)

#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)

#else

#define DEBUG_FUNCTION_CALL() DebugFunction(__FUNCTION__)
#define DEBUG_FUNCTION_PRINT(...) do { DebugFunction::Print(__FUNCTION__); debug_port.print(__VA_ARGS__); } while(0)
#define DEBUG_FUNCTION_PRINTLN(...) do { DebugFunction::Print(__FUNCTION__); debug_port.println(__VA_ARGS__); } while(0)

#define DEBUG_PRINT(...) do { debug_port.print(__VA_ARGS__); } while(0)
#define DEBUG_PRINTLN(...) do { debug_port.println(__VA_ARGS__); } while(0)

class DebugFunction {
    const char* _f;
  public:
    static inline void Print(const char* f) {
      debug_port.print(f);
      debug_port.print("(): ");
    }
    DebugFunction(const char* f) : _f(f) {
      Print(f);
      debug_port.print("Entry\r\n");
    }
    ~DebugFunction() {
      Print(_f);
      debug_port.print("Leave\r\n");
    }
};
#endif

#endif // DEBUG_H_INCLUDED_
