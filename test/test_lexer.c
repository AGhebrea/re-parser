#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "../src/include/lexer.h"
#include <cclog_macros.h>

char getCharAt(FILE* f, size_t index)
{
    size_t pos = ftell(f);
    fseek(f, index, SEEK_SET);
    char c = fgetc(f);
    fseek(f, pos, SEEK_SET);

    return c;
}

bool isLexerWrong(lexer_t* lexer)
{
    if(lexer->lexerEOF)
        return false;

    char lexerChar = nextChar(lexer);
    char actualChar = getCharAt(lexer->file, lexer->charCount - 1);

    rollBackBy(lexer, 1);

    return lexerChar != actualChar;
}

void assertLexersEqual(lexer_t* la, lexer_t* lb)
{
    bool laWrong = false;
    bool lbWrong = false;
    bool ok = true;

    if(la->charCount != lb->charCount)
        ok = false;
    if(strncmp(la->buffer, lb->buffer, la->charCount) != 0)
        ok = false;
    if(la->index != lb->index)
        ok = false;
    if(la->fence != lb->fence)
        ok = false;
    if(la->bufferSize != lb->bufferSize)
        ok = false;
    if(la->fileEOF != lb->fileEOF)
        ok = false;
    if(la->lexerEOF != lb->lexerEOF)
        ok = false;
    if(la->file->_offset != lb->file->_offset)
        ok = false;

    if(ok == false){
        laWrong = isLexerWrong(la);
        lbWrong = isLexerWrong(lb);
        if(laWrong)
            ccLogError("Lexer A returned wrong character");
        if(lbWrong)
            ccLogError("Lexer B returned wrong character");
        assert(laWrong == false && lbWrong == false);
    }
}

void asserBlockIndex(lexer_t* lexer, size_t fileSize, int index)
{
    if(!lexer->lexerEOF)
        if(index == 0)
            assert(lexer->index >= 0 && lexer->index < 0x800);
        else
            assert(lexer->index >= 0x800 && lexer->index < 0x1000);
    else
        assert(lexer->index > 0 && lexer->index <= fileSize);
}

size_t getFileSize(char* filename)
{
    struct stat statb;
    int fd = open(filename, O_RDONLY);
    fstat(fd, &statb);
    return (size_t)statb.st_size;
}

int nextCharParallel(lexer_t* la, lexer_t* lb, size_t amount)
{
    char a, b;

    for(size_t i = 0; i < amount; ++i){
        if(la->lexerEOF)
            break;
        a = nextChar(la);
        b = nextChar(lb);
        if(a != b){
            assert(a == getCharAt(la->file, la->charCount - 1));
            assert(b == getCharAt(lb->file, lb->charCount - 1));
            assert(a == b);
        }
        assertLexersEqual(la, lb);
    }

    return 0;
}

void test_duplicateChars(char* filename)
{
    lexer_t lexer;
    char a, b;

    ctor_lexer(&lexer, filename);
    while(true){
        if(lexer.lexerEOF == true)
            break;
        ccLogDebug("Reading char");
        a = nextChar(&lexer);
        rollBackBy(&lexer, 1);
        b = nextChar(&lexer);
        assert(a == b);
        // assert(isLexerWrong(&lexer) == false);
        assert(a == getCharAt(lexer.file, lexer.charCount - 1));
    }

    dtor_lexer(&lexer);
}

int rollBackParallel(lexer_t* la, lexer_t* lb, size_t amount)
{
    for(size_t i = 0; i < amount; ++i)
        rollBackBy(la, 1);
    rollBackBy(lb, amount);
    assertLexersEqual(la, lb);

    return 0;
}

int test_rollBack(char* filename)
{
    int status = 0;
    int tries = 1000;
    int cases = 3;
    size_t fileSize;
    size_t rollBackSize;
    size_t readAmountSize;
    int choice;
    lexer_t la, lb;

    fileSize = getFileSize(filename);

    test_duplicateChars(filename);

    // print the seed or something.
    // maybe store it in a file
    // srand(time(NULL));

    while(tries){
        ctor_lexer(&la, filename);
        ctor_lexer(&lb, filename);

        // choice = rand() % cases;
        choice = 0;
        switch(choice){
        /* rollback outside of cache. */
        case 0:
            /* if there's not enough data to do the test we just run the simple case */
            if(fileSize < la.bufferCap * 2 + 1)
                goto CASE_1;
            /* from first buffer roll back outside of cache */
            readAmountSize = la.bufferCap + (rand() % (la.bufferCap >> 1));
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            rollBackSize = la.bufferCap;
            if(rollBackParallel(&la, &lb, rollBackSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            /* from second buffer roll back outside of cache */
            readAmountSize = la.bufferCap >> 1;
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 1);
            rollBackSize = la.bufferCap;
            if(rollBackParallel(&la, &lb, rollBackSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 1);
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 1);
            break;
        /* rollback into cache. */
        case 1:
CASE_1:
            if(fileSize < (la.bufferCap >> 1) * 3 + 1)
                goto CASE_2;
            /* from first buffer roll back into cache */
            readAmountSize = la.bufferCap - 1;
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            rollBackSize = rand() % (la.bufferCap >> 1);
            if(rollBackSize == 0)
                rollBackSize = 1;
            if(rollBackParallel(&la, &lb, rollBackSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 1);
            if(nextCharParallel(&la, &lb,  rollBackSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            /* from second buffer roll back into cache */
            readAmountSize = (la.bufferCap >> 1);
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 1);
            rollBackSize = rand() % (la.bufferCap >> 1);
            if(rollBackSize == 0)
                rollBackSize = 1;
            if(rollBackParallel(&la, &lb, rollBackSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            if(nextCharParallel(&la, &lb,  rollBackSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 1);
            break;
        /* rollback into buffer. */
        case 2:
CASE_2:
            /* first we roll back into first buffer */
            readAmountSize = (rand() % (la.bufferCap >> 1));
            if(readAmountSize == 0)
                readAmountSize = 1;
            rollBackSize = rand() % readAmountSize;
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            if(rollBackParallel(&la, &lb, rollBackSize))
                goto FAIL;
            if(nextCharParallel(&la, &lb,  rollBackSize))
                goto FAIL;
            /* read up to second buffer */
            readAmountSize = (la.bufferCap >> 1) - readAmountSize;
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            if(!la.lexerEOF)
                assert(la.index == 0x800);
            else
                assert(la.index == fileSize);
            assert(la.fence == 0x0);
            assert(lb.fence == 0x0);
            /* then we roll back into second buffer */
            readAmountSize = (rand() % ((la.bufferCap >> 1) - 1));
            if(readAmountSize == 0)
                readAmountSize = 1;
            rollBackSize = rand() % readAmountSize;
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 1);
            if(rollBackParallel(&la, &lb, rollBackSize))
                goto FAIL;
            if(nextCharParallel(&la, &lb,  rollBackSize))
                goto FAIL;
        default:
            break;
        }

        dtor_lexer(&la);
        dtor_lexer(&lb);
        tries--;
    }

EXIT:
    return status;
FAIL:
    status = 1;
    goto EXIT;
}

int test_lexer_main()
{
    int status = 0;

    /* todo: if needed we can emulate file and generate random data in memory */
    /* file with less than half buffercap data */
    // ccLogDebug("test_rollBack(\"./test/data/sample_1.txt\");");
    // status |= test_rollBack("./test/data/sample_1.txt");
    // /* file with less than buffercap data but more than half of buffercap */
    // ccLogDebug("test_rollBack(\"./test/data/sample_2.txt\");");
    // status |= test_rollBack("./test/data/sample_2.txt");
    // /* file with more than buffercap data */
    ccLogDebug("test_rollBack(\"./test/data/sample_3.txt\");");
    status |= test_rollBack("./test/data/sample_3.txt");
    /* file with much more buffercap data */
    // ccLogDebug("test_rollBack(\"./test/data/sample_4.txt\");");
    // status |= test_rollBack("./test/data/sample_4.txt");

    return status;
}