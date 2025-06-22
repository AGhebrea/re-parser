#include "./include/test_lexer.h"

int main()
{
    int status = 0;

    status |= test_lexer_main();

    return status;
}