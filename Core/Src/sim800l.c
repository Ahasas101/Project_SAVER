/*
 * sim800l.c
 *
 *  Created on: Nov 23, 2025
 *      Author: user
 */

#include "sim800l.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;     // use USART2 as requested

#define SIM800_UART         (&huart2)
#define SIM800_RX_TMP_LEN   256

/* ------------ Low-level helpers ------------ */

static void SIM800_SendString(const char *s)
{
    HAL_UART_Transmit(SIM800_UART, (uint8_t *)s, (uint16_t)strlen(s), 1000);
}

static void SIM800_SendData(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(SIM800_UART, (uint8_t *)data, len, 1000);
}

/**
 * Read from UART until "expected" is found or timeout (ms) expires.
 * This is a simple blocking implementation.
 */
static SIM800_Result SIM800_WaitFor(const char *expected, uint32_t timeout_ms)
{
    uint8_t c;
    char buf[SIM800_RX_TMP_LEN];
    uint16_t idx = 0;
    uint32_t start = HAL_GetTick();
    uint32_t per_char_timeout = 50;   // ms per character

    memset(buf, 0, sizeof(buf));

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (HAL_UART_Receive(SIM800_UART, &c, 1, per_char_timeout) == HAL_OK) {
            if (idx < (SIM800_RX_TMP_LEN - 1)) {
                buf[idx++] = (char)c;
                buf[idx]   = '\0';
            }

            if (strstr(buf, expected) != NULL) {
                return SIM800_OK;
            }
        }
        // else: no char this cycle, keep looping until global timeout
    }

    return SIM800_TIMEOUT;
}



/* Send command and wait for expected string */
static SIM800_Result SIM800_SendCmdWait(const char *cmd,
                                        const char *expected,
                                        uint32_t timeout_ms)
{
    SIM800_SendString(cmd);
    return SIM800_WaitFor(expected, timeout_ms);
}

/* ------------ Public API ------------ */

SIM800_Result SIM800_Init(void)
{
    SIM800_Result res;

    /* Allow module to boot */
    HAL_Delay(1000);

    /* Basic test */
    res = SIM800_SendCmdWait("AT\r\n", "OK", 1000);
    if (res != SIM800_OK) return res;

    /* Turn echo off */
    res = SIM800_SendCmdWait("ATE0\r\n", "OK", 1000);
    if (res != SIM800_OK) return res;


    /* SMS text mode */
    res = SIM800_SendCmdWait("AT+CMGF=1\r\n", "OK", 1000);
    if (res != SIM800_OK) return res;

    /* Optional: set SMS character set */
    SIM800_SendCmdWait("AT+CSCS=\"GSM\"\r\n", "OK", 1000);

    return SIM800_OK;
}

/* ----------- Voice Call ----------- */

SIM800_Result SIM800_Call(const char *number)
{
    char cmd[64];

    snprintf(cmd, sizeof(cmd), "ATD%s;\r\n", number);
    return SIM800_SendCmdWait(cmd, "OK", 10000);   // waits for command acceptance
}

SIM800_Result SIM800_HangUp(void)
{
    return SIM800_SendCmdWait("ATH\r\n", "OK", 3000);
}

/* ----------- SMS ----------- */

SIM800_Result SIM800_SendSMS(const char *number, const char *text)
{
    SIM800_Result res;
    char cmd[64];

    /* Ensure text mode */
    res = SIM800_SendCmdWait("AT+CMGF=1\r\n", "OK", 1000);
    if (res != SIM800_OK) return res;

    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r\n", number);
    SIM800_SendString(cmd);

    /* Wait for '>' prompt */
    res = SIM800_WaitFor(">", 3000);
    if (res != SIM800_OK) return res;

    /* Send text and Ctrl+Z */
    SIM800_SendString(text);
    uint8_t ctrlZ = 0x1A;
    SIM800_SendData(&ctrlZ, 1);

    /* Wait for message send result */
    res = SIM800_WaitFor("OK", 15000);
    return res;
}

/* ----------- GPRS + HTTP ----------- */

/**
 * Configure GPRS bearer. Call this once before HTTP GET/POST.
 * apn/user/pwd: e.g. ("internet", "", "")
 */
SIM800_Result SIM800_SetupGPRS(const char *apn,
                               const char *user,
                               const char *pwd)
{
    SIM800_Result res;
    char cmd[96];

    /* Bearer config */
    res = SIM800_SendCmdWait("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n", "OK", 2000);
    if (res != SIM800_OK) return res;

    snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"APN\",\"%s\"\r\n", apn);
    res = SIM800_SendCmdWait(cmd, "OK", 2000);
    if (res != SIM800_OK) return res;

    if (user && user[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"USER\",\"%s\"\r\n", user);
        res = SIM800_SendCmdWait(cmd, "OK", 2000);
        if (res != SIM800_OK) return res;
    }

    if (pwd && pwd[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,\"PWD\",\"%s\"\r\n", pwd);
        res = SIM800_SendCmdWait(cmd, "OK", 2000);
        if (res != SIM800_OK) return res;
    }

    /* Open bearer */
    res = SIM800_SendCmdWait("AT+SAPBR=1,1\r\n", "OK", 10000);
    if (res != SIM800_OK) return res;

    return SIM800_OK;
}

/* Read HTTP body using AT+HTTPREAD into response buffer */
static SIM800_Result SIM800_HTTPRead(char *response, uint16_t maxLen)
{
    uint8_t c;
    uint16_t idx = 0;
    uint32_t start = HAL_GetTick();
    uint32_t timeout_ms = 10000;
    uint32_t per_char_timeout = 100;

    if (response && maxLen > 0) {
        memset(response, 0, maxLen);
    }

    SIM800_SendString("AT+HTTPREAD\r\n");

    /* Wait for header "+HTTPREAD:" first (not strictly necessary but useful) */
    SIM800_WaitFor("+HTTPREAD:", 3000);

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (HAL_UART_Receive(SIM800_UART, &c, 1, per_char_timeout) == HAL_OK) {
            // detect end: "OK\r\n" will come after body; we just rely on buffer size/timeout
            if (response && idx < (maxLen - 1)) {
                response[idx++] = (char)c;
                response[idx] = '\0';
            }
        } else {
            /* no char right now, break when we see "OK" in buffer or timeout */
        }

        if (response && strstr(response, "\r\nOK\r\n") != NULL) {
            break;
        }
    }

    return SIM800_OK;
}

/* HTTP GET */
SIM800_Result SIM800_HTTPGet(const char *url,
                             char *response,
                             uint16_t maxLen)
{
    SIM800_Result res;
    char cmd[128];

    res = SIM800_SendCmdWait("AT+HTTPTERM\r\n", "OK", 1000);   // ignore error
    (void)res;

    /* Init HTTP */
    res = SIM800_SendCmdWait("AT+HTTPINIT\r\n", "OK", 2000);
    if (res != SIM800_OK) return res;

    /* Link to bearer profile 1 */
    res = SIM800_SendCmdWait("AT+HTTPPARA=\"CID\",1\r\n", "OK", 2000);
    if (res != SIM800_OK) goto http_end;

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", url);
    res = SIM800_SendCmdWait(cmd, "OK", 3000);
    if (res != SIM800_OK) goto http_end;

    /* Start GET */
    res = SIM800_SendCmdWait("AT+HTTPACTION=0\r\n", "HTTPACTION:", 15000);
    if (res != SIM800_OK) goto http_end;

    /* Read response */
    res = SIM800_HTTPRead(response, maxLen);

http_end:
    SIM800_SendCmdWait("AT+HTTPTERM\r\n", "OK", 2000);
    return res;
}

/* HTTP POST */
SIM800_Result SIM800_HTTPPost(const char *url,
                              const char *contentType,
                              const char *body,
                              char *response,
                              uint16_t maxLen)
{
    SIM800_Result res;
    char cmd[160];
    uint16_t bodyLen = (uint16_t)strlen(body);

    res = SIM800_SendCmdWait("AT+HTTPTERM\r\n", "OK", 1000);   // ignore error
    (void)res;

    /* Init HTTP */
    res = SIM800_SendCmdWait("AT+HTTPINIT\r\n", "OK", 2000);
    if (res != SIM800_OK) return res;

    /* Link to bearer profile 1 */
    res = SIM800_SendCmdWait("AT+HTTPPARA=\"CID\",1\r\n", "OK", 2000);
    if (res != SIM800_OK) goto http_post_end;

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"\r\n", url);
    res = SIM800_SendCmdWait(cmd, "OK", 3000);
    if (res != SIM800_OK) goto http_post_end;

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"CONTENT\",\"%s\"\r\n", contentType);
    res = SIM800_SendCmdWait(cmd, "OK", 3000);
    if (res != SIM800_OK) goto http_post_end;

    /* Provide body */
    snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%u,10000\r\n", bodyLen);
    res = SIM800_SendCmdWait(cmd, "DOWNLOAD", 5000);
    if (res != SIM800_OK) goto http_post_end;

    SIM800_SendString(body);
    res = SIM800_WaitFor("OK", 10000);
    if (res != SIM800_OK) goto http_post_end;

    /* Start POST */
    res = SIM800_SendCmdWait("AT+HTTPACTION=1\r\n", "HTTPACTION:", 20000);
    if (res != SIM800_OK) goto http_post_end;

    /* Read response */
    res = SIM800_HTTPRead(response, maxLen);

http_post_end:
    SIM800_SendCmdWait("AT+HTTPTERM\r\n", "OK", 2000);
    return res;
}

