#pragma once

#include <cstdint>
#include <string>
namespace mymuduo {
class Timestamp
{
public:
    Timestamp();

    explicit Timestamp(int64_t microSecondsSinceEpoch);

    static Timestamp now();

    static Timestamp invalid();

    bool valid() const { return microSecondsSinceEpoch_ > 0; }

    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
    int64_t SecondsSinceEpoch() const { return microSecondsSinceEpoch_ / kMicroSecondsPerSecond; }

    // Example: 2026-05-16 09:08:58.313460
    std::string toFormattedString(bool showMicroseconds = true) const;

    std::string toString() const;

    static constexpr int kMicroSecondsPerSecond = 1000 * 1000;

private:
    int64_t microSecondsSinceEpoch_;
};

inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline bool operator!=(Timestamp lhs, Timestamp rhs)
{
    return !(lhs == rhs);
}

inline bool operator>(Timestamp lhs, Timestamp rhs)
{
    return rhs < lhs;
}

inline bool operator<=(Timestamp lhs, Timestamp rhs)
{
    return !(rhs < lhs);
}

inline bool operator>=(Timestamp lhs, Timestamp rhs)
{
    return !(lhs < rhs);
}

inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    auto delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

}   // namespace mymuduo