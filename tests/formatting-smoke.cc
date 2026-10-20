#include "strfun.hh"

#include <limits>
#include <string>

int main()
{
    std::string text;

    PrintNum(text, '\'', 1234567LL);
    if(text != "1'234'567") return 1;

    PrintNum(text, ',', -1234LL);
    if(text != "-1,234") return 2;

    PrintNumFixed<1>(text, '\0', 1234.5);
    if(text != "1234.5") return 3;
    PrintNumFixed<0>(text, '\0', 1234.6);
    if(text != "1235") return 11;
    PrintNumFixed<1>(text, ',', 1234.5);
    if(text != "1,234.5") return 12;

    const std::string value{"type-safe string"};
    if(Printf("%s", value) != value) return 4;

    if(Printf("%c", 65.0) != "A") return 5;
    if(Printf("%c", -1.0) != "?") return 6;
    if(Printf("%c", std::numeric_limits<double>::quiet_NaN()) != "?") return 7;
    if(Printf("%c", std::numeric_limits<double>::infinity()) != "?") return 8;
    if(Printf("%c", 0xD800) != "?") return 9;
    if(Printf("%c", 0x110000) != "?") return 10;

    return 0;
}
