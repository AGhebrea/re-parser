#include <parser.h>
#include <cclog.h>

int main()
{
    ccLog_setLogLevel(ccLogLevels_Info);
    parse("./data/sample1.cfg");
    return 0;
}