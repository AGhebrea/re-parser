#include <parser.h>
#include <cclog.h>

int main()
{
    ccLog_setLogLevel(ccLogLevels_Debug);
    parse("./data/sample1.cfg");
    return 0;
}