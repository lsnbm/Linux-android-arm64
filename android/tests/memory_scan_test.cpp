#include "../jni/include/base/scan_value.h"
#include <cstdio>
#include <cstdlib>

static void require(bool result, const char *message)
{
    if (result) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
}

template <typename T> static void checkFloatingPoint()
{
    using namespace MemUtils;
    using Types::FuzzyMode;
    auto matches = [](T value, std::string_view input, FloatRounding rounding = FloatRounding::Rounded)
    {
        const auto target = ParseScanValue<T>(input);
        const auto range = ParseFloatScanRange<T>(input, rounding);
        require(target.has_value() && range.has_value(), "valid floating input");
        return Compare(value, *target, FuzzyMode::Equal, T{}, T{}, *range);
    };

    require(matches(T(3.1445), "3.1445"), "3.1445 literal");
    require(matches(T(3.145), "3.145"), "3.145 literal");
    require(matches(T(3.1454), "3.145"), "rounded three decimals");
    require(matches(T(3.14454), "3.1445"), "rounded four decimals");
    require(!matches(T(3.14458), "3.1445"), "reject outside four decimal rounding");
    require(!matches(T(3.1454), "3.1450"), "trailing zero preserves precision");
    require(matches(T(-3.1454), "-3.145"), "negative rounding");
    require(matches(T(3.1454), "3.145e0"), "scientific notation");
    require(matches(T(0.0031454), "3.145e-3"), "negative exponent");
    require(matches(T(3145.4), "3.145e3"), "positive exponent");
    require(matches(T(3.1459), "3.145", FloatRounding::Truncated), "truncated matching");
    require(!matches(T(3.1449), "3.145", FloatRounding::Truncated), "truncated lower bound");
    require(matches(T(3.1442), "3.145", FloatRounding::Extreme), "extreme matching");
    require(!matches(T(3.1438), "3.145", FloatRounding::Extreme), "extreme outside range");
    require(!matches(T(3.1454), "3.145", FloatRounding::Exact), "exact matching");
    require(matches(T(3.145), "3.145000000000000000000"), "precision beyond storage");

    const T previous = T(3.145);
    const T next = std::nextafter(previous, std::numeric_limits<T>::infinity());
    require(Compare(next, T{}, FuzzyMode::Increased, previous), "one ULP increase");
    require(Compare(previous, T{}, FuzzyMode::Decreased, next), "one ULP decrease");
    require(Compare(next, T{}, FuzzyMode::Changed, previous), "one ULP change");
    require(!Compare(next, T{}, FuzzyMode::Unchanged, previous), "one ULP is not unchanged");
    require(Compare(previous, T{}, FuzzyMode::Unchanged, previous), "unchanged value");
    require(!Compare(next, previous, FuzzyMode::Range, T{}, previous), "range has no epsilon");
    require(Compare(T(3.145), T(4), FuzzyMode::Range, T{}, T(3)), "reversed range");
    require(!matches(std::numeric_limits<T>::infinity(), "3.145"), "reject infinity");
    require(!matches(std::numeric_limits<T>::quiet_NaN(), "3.145"), "reject NaN");
    require(!ParseFloatScanRange<T>("invalid"), "reject invalid text");
    require(!ParseScanValue<T>("1e9999"), "reject overflow");
    require(ParseScanValue<T>("0").has_value(), "accept zero");
    const T tiny = std::numeric_limits<T>::denorm_min();
    require(Compare(tiny, T{}, FuzzyMode::Greater, T{}), "subnormal differs from zero");
}

int main()
{
    checkFloatingPoint<float>();
    checkFloatingPoint<double>();
    require(MemUtils::Compare(int64_t(-42), int64_t(-42), Types::FuzzyMode::Equal, int64_t{}), "signed integer equality");
    require(!MemUtils::ParseScanValue<int8_t>("128"), "signed integer overflow");
    require(MemUtils::Compare(int64_t(0xb400000000012340ULL), int64_t(0x12340), Types::FuzzyMode::Pointer, int64_t{}), "tagged pointer");
    std::puts("PASS: float/double decimal precision, refinement, ranges, integers and pointers");
}