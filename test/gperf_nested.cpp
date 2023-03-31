EMILUA_GPERF_DECLS_BEGIN(includes)
#include "tap_test.hpp"
#include <optional>
EMILUA_GPERF_DECLS_END(includes)

EMILUA_GPERF_DECLS_BEGIN(impl)
namespace emilua::gperf::detail {
template<class T>
auto value_or(const T* p, decltype(std::declval<T>().action) default_value)
    -> decltype(default_value)
{
    return p ? p->action : default_value;
}

template<class T>
auto make_optional(const T* p)
    -> std::optional<decltype(std::declval<T>().action)>
{
    if (p)
        return p->action;
    else
        return std::nullopt;
}
} // namespace emilua::gperf::detail
EMILUA_GPERF_DECLS_END(impl)

std::string foo(std::string_view a, std::string_view b)
{
    return EMILUA_GPERF_BEGIN(a)
        EMILUA_GPERF_PARAM(std::string (*action)(std::string_view))
        EMILUA_GPERF_DEFAULT_VALUE([](std::string_view) -> std::string {
            return "not found";
        })
        EMILUA_GPERF_PAIR("a", [](std::string_view b) -> std::string {
            return EMILUA_GPERF_BEGIN(b)
                EMILUA_GPERF_PARAM(std::string action)
                EMILUA_GPERF_DEFAULT_VALUE("not found2")
                EMILUA_GPERF_PAIR("1", "a1")
                EMILUA_GPERF_PAIR("2", "a2")
            EMILUA_GPERF_END(b);
        })
        EMILUA_GPERF_PAIR("b", [](std::string_view b) -> std::string {
            return EMILUA_GPERF_BEGIN(b)
                EMILUA_GPERF_PARAM(std::string action)
                EMILUA_GPERF_DEFAULT_VALUE("not found2")
                EMILUA_GPERF_PAIR("1", "b1")
                EMILUA_GPERF_PAIR("2", "b2")
            EMILUA_GPERF_END(b);
        })
    EMILUA_GPERF_END(a)(b);
}

int main()
{
    tap_test t;

    EMILUA_TEST_EQUAL(t, foo("a", "1"), "a1");
    EMILUA_TEST_EQUAL(t, foo("a", "2"), "a2");
    EMILUA_TEST_EQUAL(t, foo("b", "1"), "b1");
    EMILUA_TEST_EQUAL(t, foo("b", "2"), "b2");
}
