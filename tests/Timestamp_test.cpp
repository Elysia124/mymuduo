#include "net/Timestamp.h"
#include "test_util.h"

#include <string>

using namespace mymuduo;

int main()
{
    quietLogger();

    Timestamp invalid = Timestamp::invalid();
    CHECK_TRUE(!invalid.valid());

    Timestamp now = Timestamp::now();
    CHECK_TRUE(now.valid());

    Timestamp later = addTime(now, 1.5);
    CHECK_TRUE(later > now);
    CHECK_EQ(later.microSecondsSinceEpoch() - now.microSecondsSinceEpoch(), 1500000);

    Timestamp same(now.microSecondsSinceEpoch());
    CHECK_TRUE(same == now);
    CHECK_TRUE(same <= now);
    CHECK_TRUE(same >= now);
    CHECK_TRUE(same != later);

    // 只做格式 smoke test，避免绑定本地时区细节。
    std::string s = Timestamp(123456789).toString();
    CHECK_TRUE(s.find("123.456789") != std::string::npos);

    std::string fs = now.toFormattedString(true);
    CHECK_TRUE(fs.size() >= 19); // YYYY-MM-DD HH:MM:SS...

    std::cout << "Timestamp_test passed\n";
    return 0;
}
