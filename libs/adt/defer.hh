#pragma once

/* https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/ */

namespace adt
{

template<typename LAMBDA_T>
class Defer
{
    LAMBDA_T onScopeExit;

public:
    Defer(LAMBDA_T f) : onScopeExit(f) {}
    ~Defer() { onScopeExit(); }
};

} /* namespace adt */

/* create unique name with ## and __COUNTER__ */
#define ADT_DEFER_GLUE2(x, y) x##y##_
#define ADT_DEFER_GLUE1(x, y) ADT_DEFER_GLUE2(x, y)

#define ADT_DEFER(code) auto ADT_DEFER_GLUE1(_lamDefer, __COUNTER__) = adt::Defer([&] { code; })

#ifdef ADT_DEFER_LESS_TYPING
    #define defer(code) ADT_DEFER(code)
#endif
