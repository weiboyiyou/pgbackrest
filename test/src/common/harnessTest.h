/***********************************************************************************************************************************
C Test Harness
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_H
#define TEST_COMMON_HARNESS_H

#include <inttypes.h>

#include "common/error.h"

// Bogus values
#define BOGUS_STR                                                   "BOGUS"

// Functions
void testAdd(int run, bool selected);
bool testBegin(const char *name);
void testComplete(void);

const char *testExe(void);
void testExeSet(const char *testExe);

const char *testPath(void);
void testPathSet(const char *testPath);

/***********************************************************************************************************************************
Convert a macro to a string -- handy for testing debug macros
***********************************************************************************************************************************/
#define MACRO_TO_STR_INNER(macro)                                                                                                  \
    #macro
#define MACRO_TO_STR(macro)                                                                                                        \
    MACRO_TO_STR_INNER(macro)

/***********************************************************************************************************************************
Maximum size of a formatted result in the TEST_RESULT macro.  Strings don't count as they are output directly, so this only applies
to the formatting of bools, ints, floats, etc.  This should be plenty of room for any of those types.
***********************************************************************************************************************************/
#define TEST_RESULT_FORMAT_SIZE                                     128

/***********************************************************************************************************************************
Test that an expected error is actually thrown and error when it isn't
***********************************************************************************************************************************/
#define TEST_ERROR(statement, errorTypeExpected, errorMessageExpected)                                                             \
{                                                                                                                                  \
    bool TEST_ERROR_catch = false;                                                                                                 \
                                                                                                                                   \
    printf("    l%04d - expect %s: %s\n", __LINE__, errorTypeName(&errorTypeExpected), errorMessageExpected);                      \
    fflush(stdout);                                                                                                                \
                                                                                                                                   \
    TRY_BEGIN()                                                                                                                    \
    {                                                                                                                              \
        statement;                                                                                                                 \
    }                                                                                                                              \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        TEST_ERROR_catch = true;                                                                                                   \
                                                                                                                                   \
        if (strcmp(errorMessage(), errorMessageExpected) != 0 || errorType() != &errorTypeExpected)                                \
            THROW_FMT(                                                                                                             \
                AssertError, "EXPECTED %s: %s\n\nBUT GOT %s: %s\n\nTHROWN AT:\n%s", errorTypeName(&errorTypeExpected),             \
                errorMessageExpected, errorName(), errorMessage(), errorStackTrace());                                             \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
                                                                                                                                   \
    if (!TEST_ERROR_catch)                                                                                                         \
        THROW_FMT(                                                                                                                 \
            AssertError, "statement '%s' returned but error %s, '%s' was expected", #statement, errorTypeName(&errorTypeExpected), \
            errorMessageExpected);                                                                                                 \
}

/***********************************************************************************************************************************
Test error with a formatted expected message
***********************************************************************************************************************************/
#define TEST_ERROR_FMT(statement, errorTypeExpected, ...)                                                                          \
{                                                                                                                                  \
    char TEST_ERROR_FMT_buffer[8192];                                                                                              \
                                                                                                                                   \
    if (snprintf(TEST_ERROR_FMT_buffer, sizeof(TEST_ERROR_FMT_buffer), __VA_ARGS__) >= (int)sizeof(TEST_ERROR_FMT_buffer))         \
        THROW_FMT(AssertError, "error message needs more than the %zu characters available", sizeof(TEST_ERROR_FMT_buffer));       \
                                                                                                                                   \
    TEST_ERROR(statement, errorTypeExpected, TEST_ERROR_FMT_buffer);                                                               \
}

/***********************************************************************************************************************************
Format the test type into the given buffer -- or return verbatim if char *
***********************************************************************************************************************************/
#define TEST_TYPE_FORMAT_VAR(value)                                                                                                \
    char value##StrBuffer[TEST_RESULT_FORMAT_SIZE + 1];                                                                            \
    char *value##Str = value##StrBuffer;

#define TEST_TYPE_FORMAT_SPRINTF(format, value)                                                                                    \
    if (snprintf((char *)value##Str, TEST_RESULT_FORMAT_SIZE + 1, format, value) > TEST_RESULT_FORMAT_SIZE)                        \
    {                                                                                                                              \
        THROW_FMT(                                                                                                                 \
            AssertError, "formatted type '%" format "' needs more than the %d characters available", TEST_RESULT_FORMAT_SIZE);     \
    }

#define TEST_TYPE_FORMAT(type, format, value)                                                                                      \
    TEST_TYPE_FORMAT_VAR(value);                                                                                                   \
    TEST_TYPE_FORMAT_SPRINTF(format, value);

#define TEST_TYPE_FORMAT_PTR(type, format, value)                                                                                  \
    TEST_TYPE_FORMAT_VAR(value);                                                                                                   \
                                                                                                                                   \
    if (value == NULL)                                                                                                             \
        value##Str = (char *)"NULL";                                                                                               \
    else if (strcmp(#type, "char *") == 0)                                                                                         \
        value##Str = (char *)value;                                                                                                \
    else                                                                                                                           \
        TEST_TYPE_FORMAT_SPRINTF(format, value);

/***********************************************************************************************************************************
Compare types
***********************************************************************************************************************************/
#define TEST_TYPE_COMPARE_STR(result, value, typeOp, valueExpected)                                                                \
    if (value != NULL && valueExpected != NULL)                                                                                    \
        result = strcmp((char *)value, (char *)valueExpected) typeOp 0;                                                            \
    else                                                                                                                           \
        result = value typeOp valueExpected;

#define TEST_TYPE_COMPARE(result, value, typeOp, valueExpected)                                                                    \
    result = value typeOp valueExpected;

/***********************************************************************************************************************************
Output information about the test
***********************************************************************************************************************************/
#define TEST_RESULT_INFO(...)                                                                                                      \
    printf("    l%04d - ", __LINE__);                                                                                              \
    printf(__VA_ARGS__);                                                                                                           \
    printf("\n");                                                                                                                  \
    fflush(stdout);

/***********************************************************************************************************************************
Test the result of a statement and make sure it matches the expected value.  This macro can test any C type given the correct
parameters.
***********************************************************************************************************************************/
#define TEST_RESULT(statement, resultExpectedValue, type, format, formatMacro, typeOp, compareMacro, ...)                          \
{                                                                                                                                  \
    /* Assign expected result to a local variable */                                                                               \
    const type TEST_RESULT_resultExpected = (type)(resultExpectedValue);                                                           \
                                                                                                                                   \
    /* Output test info */                                                                                                         \
    TEST_RESULT_INFO(__VA_ARGS__);                                                                                                 \
                                                                                                                                   \
    /* Format the expected result */                                                                                               \
    formatMacro(type, format, TEST_RESULT_resultExpected);                                                                         \
                                                                                                                                   \
    /* Try to run the statement.  Assign expected to result to silence compiler warning about unitialized var. */                  \
    type TEST_RESULT_result = (type)TEST_RESULT_resultExpected;                                                                    \
                                                                                                                                   \
    TRY_BEGIN()                                                                                                                    \
    {                                                                                                                              \
        TEST_RESULT_result = (type)(statement);                                                                                    \
    }                                                                                                                              \
    /* Catch any errors */                                                                                                         \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        /* No errors were expected so error */                                                                                     \
        THROW_FMT(                                                                                                                 \
            AssertError, "STATEMENT: %s\n\nTHREW %s: %s\n\nTHROWN AT:\n%s\n\nBUT EXPECTED RESULT:\n%s",                            \
            #statement, errorName(), errorMessage(), errorStackTrace(), TEST_RESULT_resultExpectedStr);                            \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
                                                                                                                                   \
   /* Test the type operator */                                                                                                    \
    bool TEST_RESULT_resultOp = false;                                                                                             \
    compareMacro(TEST_RESULT_resultOp, TEST_RESULT_result, typeOp, TEST_RESULT_resultExpected);                                    \
                                                                                                                                   \
    /* If type operator test was not successful */                                                                                 \
    if (!TEST_RESULT_resultOp)                                                                                                     \
    {                                                                                                                              \
        /* Format the actual result */                                                                                             \
        formatMacro(type, format, TEST_RESULT_result);                                                                             \
                                                                                                                                   \
        /* Throw error */                                                                                                          \
        THROW_FMT(                                                                                                                 \
            AssertError, "\n\nSTATEMENT: %s\n\nRESULT IS:\n%s\n\nBUT EXPECTED:\n%s\n\n",                                           \
            #statement, TEST_RESULT_resultStr, TEST_RESULT_resultExpectedStr);                                                     \
    }                                                                                                                              \
}

/***********************************************************************************************************************************
Test that a void statement returns and does not throw an error
***********************************************************************************************************************************/
#define TEST_RESULT_VOID(statement, ...)                                                                                           \
{                                                                                                                                  \
    /* Output test info */                                                                                                         \
    TEST_RESULT_INFO(__VA_ARGS__);                                                                                                 \
                                                                                                                                   \
    TRY_BEGIN()                                                                                                                    \
    {                                                                                                                              \
        statement;                                                                                                                 \
    }                                                                                                                              \
    /* Catch any errors */                                                                                                         \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        /* No errors were expected so error */                                                                                     \
        THROW_FMT(                                                                                                                 \
            AssertError, "EXPECTED VOID RESULT FROM STATEMENT: %s\n\nBUT GOT %s: %s\n\nTHROWN AT:\n%s", #statement, errorName(),   \
            errorMessage(), errorStackTrace());                                                                                    \
                                                                                                                                   \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
}

/***********************************************************************************************************************************
Test that a statement does not error and assign it to the specified variable if not
***********************************************************************************************************************************/
#define TEST_ASSIGN(lValue, statement, ...)                                                                                        \
{                                                                                                                                  \
    /* Output test info */                                                                                                         \
    TEST_RESULT_INFO(__VA_ARGS__);                                                                                                 \
                                                                                                                                   \
    TRY_BEGIN()                                                                                                                    \
    {                                                                                                                              \
        lValue = statement;                                                                                                        \
    }                                                                                                                              \
    /* Catch any errors */                                                                                                         \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        /* No errors were expected so error */                                                                                     \
        THROW_FMT(                                                                                                                 \
            AssertError, "EXPECTED ASSIGNMENT FROM STATEMENT: %s\n\nBUT GOT %s: %s\n\nTHROWN AT:\n%s", #statement, errorName(),    \
            errorMessage(), errorStackTrace());                                                                                    \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
}

/***********************************************************************************************************************************
Macros to ease the use of common data types
***********************************************************************************************************************************/
#define TEST_RESULT_BOOL_PARAM(statement, resultExpected, typeOp, ...)                                                             \
    TEST_RESULT(statement, resultExpected, bool, "%d", TEST_TYPE_FORMAT, typeOp, TEST_TYPE_COMPARE, __VA_ARGS__);
#define TEST_RESULT_BOOL(statement, resultExpected, ...)                                                                           \
    TEST_RESULT_BOOL_PARAM(statement, resultExpected, ==, __VA_ARGS__);

#define TEST_RESULT_CHAR_PARAM(statement, resultExpected, typeOp, ...)                                                             \
    TEST_RESULT(statement, resultExpected, char, "%c", TEST_TYPE_FORMAT, typeOp, TEST_TYPE_COMPARE, __VA_ARGS__);
#define TEST_RESULT_CHAR(statement, resultExpected, ...)                                                                           \
    TEST_RESULT_CHAR_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_CHAR_NE(statement, resultExpected, ...)                                                                        \
    TEST_RESULT_CHAR_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#define TEST_RESULT_DOUBLE_PARAM(statement, resultExpected, typeOp, ...)                                                           \
    TEST_RESULT(statement, resultExpected, double, "%f", TEST_TYPE_FORMAT, typeOp, TEST_TYPE_COMPARE, __VA_ARGS__);
#define TEST_RESULT_DOUBLE(statement, resultExpected, ...)                                                                         \
    TEST_RESULT_DOUBLE_PARAM(statement, resultExpected, ==, __VA_ARGS__);

#define TEST_RESULT_INT_PARAM(statement, resultExpected, typeOp, ...)                                                              \
    TEST_RESULT(statement, resultExpected, int64_t, "%" PRId64, TEST_TYPE_FORMAT, typeOp, TEST_TYPE_COMPARE, __VA_ARGS__);
#define TEST_RESULT_INT(statement, resultExpected, ...)                                                                            \
    TEST_RESULT_INT_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_INT_NE(statement, resultExpected, ...)                                                                         \
    TEST_RESULT_INT_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#define TEST_RESULT_PTR_PARAM(statement, resultExpected, typeOp, ...)                                                              \
    TEST_RESULT(statement, resultExpected, void *, "%p", TEST_TYPE_FORMAT_PTR, typeOp, TEST_TYPE_COMPARE, __VA_ARGS__);
#define TEST_RESULT_PTR(statement, resultExpected, ...)                                                                            \
    TEST_RESULT_PTR_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_PTR_NE(statement, resultExpected, ...)                                                                         \
    TEST_RESULT_PTR_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#define TEST_RESULT_SIZE_PARAM(statement, resultExpected, typeOp, ...)                                                             \
    TEST_RESULT(statement, resultExpected, size_t, "%zu", TEST_TYPE_FORMAT, typeOp, TEST_TYPE_COMPARE, __VA_ARGS__);
#define TEST_RESULT_SIZE(statement, resultExpected, ...)                                                                           \
    TEST_RESULT_SIZE_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_SIZE_NE(statement, resultExpected, ...)                                                                        \
    TEST_RESULT_SIZE_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#define TEST_RESULT_STR_PARAM(statement, resultExpected, typeOp, ...)                                                              \
    TEST_RESULT(statement, resultExpected, char *, "%s", TEST_TYPE_FORMAT_PTR, typeOp, TEST_TYPE_COMPARE_STR, __VA_ARGS__);
#define TEST_RESULT_STR(statement, resultExpected, ...)                                                                            \
    TEST_RESULT_STR_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_STR_NE(statement, resultExpected, ...)                                                                         \
    TEST_RESULT_STR_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#define TEST_RESULT_U16_HEX(statement, resultExpected, ...)                                                                        \
    TEST_RESULT(statement, resultExpected, uint16_t, "%04X", TEST_TYPE_FORMAT, ==, TEST_TYPE_COMPARE, __VA_ARGS__);

#define TEST_RESULT_UINT_PARAM(statement, resultExpected, typeOp, ...)                                                             \
    TEST_RESULT(statement, resultExpected, uint64_t, "%" PRIu64, TEST_TYPE_FORMAT, typeOp, TEST_TYPE_COMPARE, __VA_ARGS__);
#define TEST_RESULT_UINT(statement, resultExpected, ...)                                                                           \
    TEST_RESULT_UINT_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_UINT_NE(statement, resultExpected, ...)                                                                        \
    TEST_RESULT_UINT_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#endif
