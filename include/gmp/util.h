
#ifndef GMP_UTIL_H
#define GMP_UTIL_H

#include <cupti.h>
#include <gmp/data_struct.h>

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

#define DRIVER_API_CALL(apiFunctionCall)                                            \
do                                                                                  \
{                                                                                   \
    CUresult _status = apiFunctionCall;                                             \
    if (_status != CUDA_SUCCESS)                                                    \
    {                                                                               \
        const char *pErrorString;                                                   \
        cuGetErrorString(_status, &pErrorString);                                   \
                                                                                    \
        std::cerr << "\n\nError: " << __FILE__ << ":" << __LINE__ << ": Function "  \
        << #apiFunctionCall << " failed with error(" << _status << "): "            \
        << pErrorString << ".\n\n";                                                 \
                                                                                    \
        exit(EXIT_FAILURE);                                                         \
    }                                                                               \
} while (0)

#define GMP_API_CALL(apiFunctionCall)                                            \
do                                                                                  \
{                                                                                   \
    GmpResult _status = apiFunctionCall;                                             \
    if (_status != GmpResult::SUCCESS)                                                    \
    {                                                                               \
        std::cerr << "\n\nError: " << __FILE__ << ":" << __LINE__ << ": Function "  \
        << #apiFunctionCall << ".\n\n";                                             \
                                                                                    \
        exit(EXIT_FAILURE);                                                         \
    }                                                                               \
} while (0)

#endif // GMP_UTIL_H
