#pragma once

namespace SquarePhysics {

#ifdef NDEBUG

#define assert(expression) ((void)0)

#else

#define assert(expression) (void)(                                                       \
            (!!(expression)) ||                                                              \
            (_wassert(_CRT_WIDE(#expression), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0) \
        )

#endif

}
