#ifndef FRYSTL_DEFINES_H
#define FRYSTL_DEFINES_H

#ifdef FRYSTL_DEBUG
#define FRYSTL_ASSERT(assertion) if (!(assertion)) AssertFailed(#assertion,__FILE__,__LINE__)
#define FRYSTL_ASSERT2(assertion,description)\
    if (!(assertion)) AssertFailed(#assertion " " #description,__FILE__,__LINE__)

#include <cstdio>       // stderr
#include <csignal>      // raise, SIGABRT

namespace frystl {
    static void AssertFailed(const char* assertion, const char* file, unsigned line)
    {
        fprintf(stderr, "Assertion \"%s\" failed in %s at line %u.\n", 
            assertion, file, line);
        raise(SIGABRT);
    }
}

#else       // FRYSTL_DEBUG
#define FRYSTL_ASSERT(assertion)    // empty
#define FRYSTL_ASSERT2(assertion,description)  // empty
#endif      // FRYSTL_DEBUG

#include <type_traits>          // enable_if, is_convertible
#include <iterator>             // iterator_traits, input_iterator_tag

namespace frystl {
    
    // Stolen from gcc stl:
    template<typename InIter>
    using RequireInputIter = typename
    std::enable_if<std::is_convertible<
        typename std::iterator_traits<InIter>::iterator_category,
        std::input_iterator_tag>::value>::type;

    // Return the quotient num/denom rounded up
    static size_t Ceiling(size_t num, size_t denom)
    {
        return (num+denom-1)/denom;
    }
    template <class value_type, class... Args>
    void Construct(value_type* where, Args&&... args)
    {
        new ((void*)where) value_type(std::forward<Args>(args)...);
    }
    template <class value_type>
    void Destroy(value_type * x)
    {
        x->~value_type();
    }
}

#endif  // ndef FRYSTL_DEFINES_H
