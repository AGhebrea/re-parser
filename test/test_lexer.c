#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <cclog.h>
#include "../src/include/lexer.h"
#include "cclog_macros.h"

char getCharAt(FILE* f, size_t index)
{
    assert(index != -1);
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

bool buffersEqual(lexer_t* a, lexer_t* b)
{
    bool status = false;
    if(a->charCount < a->bufferCap - 1){
        if(strncmp(a->buffer, b->buffer, a->charCount) == 0)
            status = true;
        else
            status = false;
    }else{
        if(strncmp(a->buffer, b->buffer, a->bufferCap) == 0)
            status = true;
        else
            status = false;
    }

    return status;
}

void assertLexersEqual(lexer_t* la, lexer_t* lb)
{
    bool laWrong = false;
    bool lbWrong = false;
    bool ok = true;

    if(la->charCount != lb->charCount)
        ok = false;
    if(buffersEqual(la, lb) == false)
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
            ccLogError("Lexer A (%p) returned wrong character", la);
        if(lbWrong)
            ccLogError("Lexer B (%p) returned wrong character", lb);
        assert(laWrong == false && lbWrong == false);
    }
}

/* the block ranges are wrong but the idea was good */
void asserBlockIndex(lexer_t* lexer, size_t fileSize, int index)
{
    return;

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

void test_rollBackIncrementalImpl(char* filename, size_t start, size_t rollbacksize)
{
    lexer_t lexer;
    size_t i = 0;

    ctor_lexer(&lexer, filename);
    for(i = 0; i < start; ++i){
        (void)nextChar(&lexer);
        assert(isLexerWrong(&lexer) == false);
    }
    for(size_t j = i; j > rollbacksize; j -= rollbacksize){
        assert(lexer.lexerEOF == false);
        rollBackBy(&lexer, rollbacksize);
        assert(isLexerWrong(&lexer) == false);
    }
    dtor_lexer(&lexer);
}

void test_nextChar(char *filename)
{
    size_t fileSize;
    lexer_t lexer;
    char a;
    ctor_lexer(&lexer, filename);

    fileSize = getFileSize(filename);

    for(size_t i = (lexer.bufferCap >> 1) - 1; i < fileSize; ++i){
        a = nextChar(&lexer);
        assert(a == getCharAt(lexer.file, lexer.charCount - 1));
    }
    dtor_lexer(&lexer);
}

int test_rollBackIncremental(char* filename){
    size_t rb = 1;
    size_t fileSize;
    size_t start;
    lexer_t dummy;

    ctor_lexer(&dummy, filename);
    fileSize = getFileSize(filename);

    start = fileSize - (fileSize % dummy.bufferCap) - 1;
    for(size_t i = start; i < fileSize; ++i){
        rb = 1;
        while(rb < dummy.bufferCap){
            test_rollBackIncrementalImpl(filename, i, rb);
            rb = rb << 1;
        }
    }
    dtor_lexer(&dummy);

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
        a = nextChar(&lexer);
        rollBackBy(&lexer, 1);
        b = nextChar(&lexer);
        assert(a == b);
        assert(isLexerWrong(&lexer) == false);
        assert(a == getCharAt(lexer.file, lexer.charCount - 1));
    }

    dtor_lexer(&lexer);
}

int rollBackParallel(lexer_t* la, lexer_t* lb, size_t amount)
{
    int status = 0;

    for(size_t i = 0; i < amount; ++i){
        rollBackBy(la, 1);
        expectExit(status, ftell(la->file), != -1);
        if(isLexerWrong(la)){
            ccLogDebug("[i = %ld] this rollback put the lexer in a wrong state", i);
            exit(1);
        }
    }
    rollBackBy(lb, amount);
    assertLexersEqual(la, lb);

    return 0;
}

/* these tests still have some issues in the sense that we don't always roll into 
 * the correct buffers but the lexer should handle whatever case + the more robust
 * test is test_rollBackIncremental() */
int test_rollBackRandom(char* filename)
{
    int status = 0;
    int tries = 10000;
    int cases = 3;
    size_t fileSize;
    size_t rollBackSize;
    size_t readAmountSize;
    size_t randSeed;
    int choice;
    lexer_t la, lb;

    fileSize = getFileSize(filename);

    test_duplicateChars(filename);

    randSeed = (size_t)time(NULL);
    // randSeed = 1750574539;
    ccLogInfo("Seed: %ld", randSeed);
    srand(randSeed);

    while(tries){
        ctor_lexer(&la, filename);
        ctor_lexer(&lb, filename);

        choice = rand() % cases;
        switch(choice){
        /* rollback outside of cache. */
        case 0:
            /* if there's not enough data to do the test we just run the simple case */
            if(fileSize < la.bufferCap * 2 + 1)
                goto CASE_1;
            /* from first buffer roll back outside of cache 
             * la.bufferCap - 1 puts the lexer right at index 0 */
            readAmountSize = (la.bufferCap - 1) + (rand() % ((la.bufferCap >> 1) - 1));
            if(nextCharParallel(&la, &lb, readAmountSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            rollBackSize = la.bufferCap;
            if(rollBackParallel(&la, &lb, rollBackSize))
                goto FAIL;
            asserBlockIndex(&la, fileSize, 0);
            if(nextCharParallel(&la, &lb, rollBackSize))
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
            if(nextCharParallel(&la, &lb, rollBackSize))
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

    ccLog_setLogLevel(ccLogLevels_Info);

    ccLogInfo("test_nextChar(\"./test/data/sample_3.txt\");");
    test_nextChar("./test/data/sample_3.txt");

    /* todo: if needed we can emulate file and generate random data in memory */
    /* file with less than half buffercap data */
    ccLogInfo("test_rollBack(\"./test/data/sample_1.txt\");");
    status |= test_rollBackRandom("./test/data/sample_1.txt");
    /* file with less than buffercap data but more than half of buffercap */
    ccLogInfo("test_rollBack(\"./test/data/sample_2.txt\");");
    status |= test_rollBackRandom("./test/data/sample_2.txt");
    /* file with more than buffercap data */
    ccLogInfo("test_rollBack(\"./test/data/sample_3.txt\");");
    status |= test_rollBackRandom("./test/data/sample_3.txt");
    /* file with much more buffercap data */
    ccLogInfo("test_rollBack(\"./test/data/sample_4.txt\");");
    status |= test_rollBackRandom("./test/data/sample_4.txt");

    ccLogInfo("test_rollBackIncremental(\"./test/data/sample_4.txt\");");
    status |= test_rollBackIncremental("./test/data/sample_4.txt");

    return status;
}