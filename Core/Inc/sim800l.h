#ifndef SIM800L_H
#define SIM800L_H

#include "main.h"
#include <stdint.h>

typedef enum {
    SIM800_OK = 0,
    SIM800_ERROR,
    SIM800_TIMEOUT
} SIM800_Result;

#ifdef __cplusplus
extern "C" {
#endif

SIM800_Result SIM800_Init(void);

/* Voice call */
SIM800_Result SIM800_Call(const char *number);
SIM800_Result SIM800_HangUp(void);

/* SMS in text mode */
SIM800_Result SIM800_SendSMS(const char *number, const char *text);

/* GPRS/HTTP */
SIM800_Result SIM800_SetupGPRS(const char *apn,
                               const char *user,
                               const char *pwd);

SIM800_Result SIM800_HTTPGet(const char *url,
                             char *response,
                             uint16_t maxLen);

SIM800_Result SIM800_HTTPPost(const char *url,
                              const char *contentType,
                              const char *body,
                              char *response,
                              uint16_t maxLen);




#ifdef __cplusplus
}
#endif

#endif /* SIM800L_H */
