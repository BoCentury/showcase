// Copyright BoCentury 2020, all rights reserved.


// C++ nonsense to let me use defer
template<typename T>
struct __deferer {
    T lambda;
    __deferer(T lambda):lambda(lambda) {}
    ~__deferer() { lambda(); }
    
    __deferer(const __deferer&);
    __deferer &operator =(const __deferer&);
};

struct __deferer_helper {
    template<typename T>
        __deferer<T> operator+(T t) { return t; }
};

#define ___Stringify(a) #a
#define ____Concat(a, b) a##b
#define ___Concat(a, b) ____Concat(a, b)

#define defer          const auto  &___Concat(__defer_var_,__LINE__) = __deferer_helper() + [&]()
#define defer_by_value const auto  &___Concat(__defer_var_,__LINE__) = __deferer_helper() + [=]()

#ifdef BO_NO_INTERNAL
#define CompileTimeAssert(...)
#else
#define CompileTimeAssert(expression) typedef char ___CompileTimeAssertFailed[(expression) ? 1 : -1]
#endif
// Uncomment to test compile time assert
//CompileTimeAssert(false);


#define BoCheckType(T, size_in_bytes) CompileTimeAssert(sizeof(T) == (size_in_bytes))

typedef   signed long long s64; BoCheckType(s64, 8);
typedef   signed int       s32; BoCheckType(s32, 4);
typedef   signed short     s16; BoCheckType(s16, 2);
typedef   signed char      s8;  BoCheckType(s8,  1);

typedef unsigned long long u64; BoCheckType(s64, 8);
typedef unsigned int       u32; BoCheckType(s32, 4);
typedef unsigned short     u16; BoCheckType(s16, 2);
typedef unsigned char      u8;  BoCheckType(s8,  1);

typedef u64 umm; BoCheckType(umm, sizeof(void *));

typedef double f64; BoCheckType(f64, 8);
typedef float  f32; BoCheckType(f32, 4);
;
typedef wchar_t wchar; BoCheckType(wchar, 2);

#undef BoCheckType

#define ArrayCount(a) (sizeof(a)/sizeof((a)[0]))

//#define ForIt(a) for(auto it : (a))
#define For(i, init, op, count, inc) for (auto (i) = (init); (i) op (count); (i) += inc)
#define ForI(i, init, count)         for (auto (i) = (init); (i) <  (count); (i) += 1)
#define ForII( i, min, max)          for (auto (i) = (min);  (i) <= (max);   (i) += 1)
#define ForIIR(i, max, min)          for (auto (i) = (max);  (i) >= (min);   (i) -= 1)

//#define For(i, start, op, end, inc)  for (auto (i) = (start); (i) op (end); (i) += inc)
//#define ForI(i, start, end)          for (auto (i) = (start); (i) <  (end); (i) += 1)
//#define ForII(i, start, end)         for (auto (i) = (start); (i) <= (end); (i) += 1)
//#define ForIIR(i, start, end)        for (auto (i) = (start); (i) >= (end); (i) -= 1)


#define ForA(i, array) ForI (i, 0, ArrayCount(array))

//#define ForI(i, init, count)           For ((i), (init), < , (count),  1)
//#define ForII( i, min, max)            For ((i), (min),  <=, (max),    1)
//#define ForIIR(i, max, min)            For ((i), (max),  >=, (min),   -1)

//ForI (i, 0, s.count)
//For (i, 0, <, s.count, 1)


#define       cast(type, expr) (     static_cast<type>(expr))
#define super_cast(type, expr) (reinterpret_cast<type>(expr))

// Use these so you don't have to constantly put 'break' everywhere
#define Case    break; case
#define Default break; default
#define OrCase         case
#define OrDefault      default

//#define null 0
#define null nullptr

// 'static' is too overloaded and means too many things
#define internal      static // internal linkage (usually a function)
#define local_persist static // local variable that persists
#define global        static // global variable (within the translation unit)

global const u8  U8_MIN  = 0;
global const u8  U8_MAX  = 0xff;
global const u16 U16_MIN = 0;
global const u16 U16_MAX = 0xffff;
global const u32 U32_MIN = 0;
global const u32 U32_MAX = 0xffffFFFF;
global const u64 U64_MIN = 0;
global const u64 U64_MAX = 0xffffFFFFffffFFFF;

global const s8  S8_MIN  = -128;
global const s8  S8_MAX  =  127;
global const s16 S16_MIN = -32768;
global const s16 S16_MAX =  32767;
global const s32 S32_MIN = -2147483647-1;
global const s32 S32_MAX =  2147483647;
global const s64 S64_MIN = -9223372036854775807-1;
global const s64 S64_MAX =  9223372036854775807;

global const f32 F32_MIN = 1.1754943508e-38f;
global const f32 F32_MAX = 3.402823466e+38f;
global const f64 F64_MIN = 2.2250738585072014e-308;
global const f64 F64_MAX = 1.7976931348623157e+308;
