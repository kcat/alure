#ifndef ALURE_MAIN_H
#define ALURE_MAIN_H

#include "alure2.h"

#include <system_error>


#ifdef __GNUC__
#define LIKELY(x) __builtin_expect(static_cast<bool>(x), true)
#define UNLIKELY(x) __builtin_expect(static_cast<bool>(x), false)
#else
#define LIKELY(x) static_cast<bool>(x)
#define UNLIKELY(x) static_cast<bool>(x)
#endif

#define DECL_THUNK0(ret, C, Name, cv) \
ret C::Name() cv { return pImpl->Name(); }
#define DECL_THUNK1(ret, C, Name, cv, T1)                                     \
ret C::Name(T1 a) cv                                                          \
{                                                                             \
    using _t1 = T1;                                                           \
    return pImpl->Name(std::forward<_t1>(a));                                 \
}
#define DECL_THUNK2(ret, C, Name, cv, T1, T2)                                 \
ret C::Name(T1 a, T2 b) cv                                                    \
{                                                                             \
    using _t1 = T1; using _t2 = T2;                                           \
    return pImpl->Name(std::forward<_t1>(a), std::forward<_t2>(b));           \
}
#define DECL_THUNK3(ret, C, Name, cv, T1, T2, T3)                             \
ret C::Name(T1 a, T2 b, T3 c) cv                                              \
{                                                                             \
    using _t1 = T1; using _t2 = T2; using _t3 = T3;                           \
    return pImpl->Name(std::forward<_t1>(a), std::forward<_t2>(b),            \
                       std::forward<_t3>(c));                                 \
}
#define DECL_THUNK6(ret, C, Name, cv, T1, T2, T3, T4, T5, T6)                 \
ret C::Name(T1 a, T2 b, T3 c, T4 d, T5 e, T6 f) cv                            \
{                                                                             \
    using _t1 = T1; using _t2 = T2; using _t3 = T3;                           \
    using _t4 = T4; using _t5 = T5; using _t6 = T6;                           \
    return pImpl->Name(std::forward<_t1>(a), std::forward<_t2>(b),            \
                       std::forward<_t3>(c), std::forward<_t4>(d),            \
                       std::forward<_t5>(e), std::forward<_t6>(f));           \
}


namespace alure {

template<typename T>
inline std::future_status GetFutureState(const SharedFuture<T> &future)
{ return future.wait_for(std::chrono::seconds::zero()); }

template<size_t N>
struct Bitfield {
private:
    std::array<uint8_t,(N+7)/8> mElems;

public:
    bool operator[](size_t i) const { return mElems[i/8] & (1<<(i%8)); }

    void clear() { std::fill(mElems.begin(), mElems.end(), 0); }
    void set(size_t i) { mElems[i/8] |= 1<<(i%8); }
};


class alc_category : public std::error_category {
    alc_category() noexcept { }

public:
    static alc_category sSingleton;

    const char *name() const noexcept override final { return "alc_category"; }
    std::error_condition default_error_condition(int code) const noexcept override final
    { return std::error_condition(code, *this); }

    bool equivalent(int code, const std::error_condition &condition) const noexcept override final
    { return default_error_condition(code) == condition; }
    bool equivalent(const std::error_code &code, int condition) const noexcept override final
    { return *this == code.category() && code.value() == condition; }

    std::string message(int condition) const override final;
};
template<typename T>
inline std::system_error alc_error(int code, T&& what)
{ return std::system_error(code, alc_category::sSingleton, std::forward<T>(what)); }
inline std::system_error alc_error(int code)
{ return std::system_error(code, alc_category::sSingleton); }

class al_category : public std::error_category {
    al_category() noexcept { }

public:
    static al_category sSingleton;

    const char *name() const noexcept override final { return "al_category"; }
    std::error_condition default_error_condition(int code) const noexcept override final
    { return std::error_condition(code, *this); }

    bool equivalent(int code, const std::error_condition &condition) const noexcept override final
    { return default_error_condition(code) == condition; }
    bool equivalent(const std::error_code &code, int condition) const noexcept override final
    { return *this == code.category() && code.value() == condition; }

    std::string message(int condition) const override final;
};
template<typename T>
inline std::system_error al_error(int code, T&& what)
{ return std::system_error(code, al_category::sSingleton, std::forward<T>(what)); }
inline std::system_error al_error(int code)
{ return std::system_error(code, al_category::sSingleton); }

} // namespace alure

#endif /* ALURE_MAIN_H */
