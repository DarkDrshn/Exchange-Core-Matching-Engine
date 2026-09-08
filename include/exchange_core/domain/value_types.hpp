#pragma once

#include <cstdint>
#include <type_traits>

namespace exchange_core::domain
{
    class Price
    {
    public:
        using Storage = std::int64_t;

        constexpr explicit Price(Storage value) : value_(value) {}

        [[nodiscard]] constexpr Storage value() const
        {
            return value_;
        }

        [[nodiscard]] constexpr bool is_positive() const
        {
            return value_ > 0;
        }

        friend constexpr bool operator<(Price left, Price right)
        {
            return left.value_ < right.value_;
        }

        friend constexpr bool operator>(Price left, Price right)
        {
            return left.value_ > right.value_;
        }

        friend constexpr bool operator<=(Price left, Price right)
        {
            return left.value_ <= right.value_;
        }

        friend constexpr bool operator>=(Price left, Price right)
        {
            return left.value_ >= right.value_;
        }

    private:
        Storage value_;
    };

    class Quantity
    {
    public:
        using Storage = std::uint64_t;

        constexpr explicit Quantity(Storage value) : value_(value) {}

        [[nodiscard]] constexpr Storage value() const
        {
            return value_;
        }

        [[nodiscard]] constexpr bool is_positive() const
        {
            return value_ > 0;
        }

        constexpr Quantity &operator-=(Quantity other)
        {
            value_ -= other.value_;
            return *this;
        }

        friend constexpr bool operator<(Quantity left, Quantity right)
        {
            return left.value_ < right.value_;
        }

        friend constexpr bool operator>(Quantity left, Quantity right)
        {
            return left.value_ > right.value_;
        }

        friend constexpr bool operator==(Quantity left, Quantity right)
        {
            return left.value_ == right.value_;
        }

    private:
        Storage value_;
    };

    static_assert(std::is_integral_v<Price::Storage>);
    static_assert(std::is_signed_v<Price::Storage>);
    static_assert(sizeof(Price::Storage) == sizeof(std::int64_t));
    static_assert(std::is_integral_v<Quantity::Storage>);
    static_assert(std::is_unsigned_v<Quantity::Storage>);
    static_assert(sizeof(Quantity::Storage) == sizeof(std::uint64_t));
    static_assert(Price(1).is_positive());
    static_assert(Quantity(1).is_positive());
}
