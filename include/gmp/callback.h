#ifndef GMP_CALLBACK_H
#define GMP_CALLBACK_H

#include <cupti.h>
#include <cupti_events.h>
#include <stdio.h>
#include <cassert>
#include "gmp/util.h"
#include "gmp/data_struct.h"
#include "gmp/log.h"

void CUPTIAPI getTimestampCallback(void *userdata, CUpti_CallbackDomain domain,
                                   CUpti_CallbackId cbid, const CUpti_CallbackData *cbInfo);

void CUPTIAPI getEventValueCallback(void *userdata, CUpti_CallbackDomain domain,
                                    CUpti_CallbackId cbid, const CUpti_CallbackData *cbInfo);

#endif // GMP_CALLBACK_H