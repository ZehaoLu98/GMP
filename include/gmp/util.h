
#ifndef GMP_UTIL_H
#define GMP_UTIL_H

#include <cupti.h>

#define CUPTI_CALL(call)                                                         \
    do                                                                           \
    {                                                                            \
        CUptiResult _status = call;                                              \
        if (_status != CUPTI_SUCCESS)                                            \
        {                                                                        \
            const char *errstr;                                                  \
            cuptiGetResultString(_status, &errstr);                              \
            fprintf(stderr, "%s:%d: error: function %s failed with error %s.\n", \
                    __FILE__, __LINE__, #call, errstr);                          \
            exit(-1);                                                            \
        }                                                                        \
    } while (0)

#endif // GMP_UTIL_H