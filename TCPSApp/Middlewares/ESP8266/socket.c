#include <string.h>
#include "main.h"
#include "stm32l0xx_hal.h"
#include "socket.h"

extern UART_HandleTypeDef huart4;
#define TIMEOUTLEFT(__timeout, __starttime) ((__timeout == HAL_MAX_DELAY) ? HAL_MAX_DELAY : (__timeout - (HAL_GetTick() - __starttime)))

const char* SocketUDP = "UDP";
const char* SocketTCP = "TCP";
const char* ResponseOK = "OK\r\n";
const char* ResponseSENDOK = "SEND OK\r\n";
const char* ResponseERROR = "ERROR\r\n";
const char* ResponseALREADYCONNECTED = "ALREADY CONNECTED\r\n\r\n";

static HAL_StatusTypeDef ESP8266GetResponse(
    char* responseIn,
    uint32_t respLenIn,
    uint32_t* usedLen,
    uint32_t timeout)
{
    HAL_StatusTypeDef retVal = HAL_OK;
    uint32_t startTime = HAL_GetTick();
    char resp[256] = {0};
    char* response = (responseIn && respLenIn) ? responseIn : resp;
    uint32_t responseLen = (responseIn && respLenIn) ? respLenIn : sizeof(resp);
    uint32_t cursor = 0;
    uint32_t newCursor = 0;
    do
    {
        if((timeout != HAL_MAX_DELAY) && (startTime + timeout < HAL_GetTick())) return HAL_TIMEOUT;
        cursor = newCursor;
        fgets(&response[cursor], responseLen - cursor - 1, &__net);
        if((!strcmp(&response[cursor], ResponseOK)) || (!strcmp(&response[cursor], ResponseSENDOK)))
        {
            break;
        }
        else if(!strcmp(&response[cursor], ResponseERROR))
        {
            retVal = HAL_ERROR;
            break;
        }
        newCursor = strlen(response);
    }
    while((newCursor + 1) < responseLen);
    response[cursor] = '\0';
    if(usedLen) *usedLen = cursor;
    return retVal;
}

HAL_StatusTypeDef InitializeESP8266(
    uint32_t* ip,
    uint8_t* mac,
    uint32_t timeout)
{
    HAL_StatusTypeDef retVal = HAL_OK;
    uint32_t startTime = HAL_GetTick();

    fprintf(&__net, "ATE0\r\n");
    if((retVal = ESP8266GetResponse(NULL, 0, NULL, TIMEOUTLEFT(timeout, startTime))) != HAL_OK)
    {
        return retVal;
    }

    fprintf(&__net, "AT+CIPMUX=1\r\n");
    if((retVal = ESP8266GetResponse(NULL, 0, NULL, TIMEOUTLEFT(timeout, startTime))) != HAL_OK)
    {
        return retVal;
    }
    
    if((ip) || (mac))
    {
        char response[100];
        uint32_t responseLen = 0;
        uint32_t extracted[10] = {0};
        fprintf(&__net, "AT+CIFSR\r\n");
        if((retVal = ESP8266GetResponse(response, sizeof(response), &responseLen, TIMEOUTLEFT(timeout, startTime))) != HAL_OK)
        {
            return retVal;
        }
        if(sscanf(response, "+CIFSR:STAIP,\"%d.%d.%d.%d\"\r\n+CIFSR:STAMAC,\"%x:%x:%x:%x:%x:%x:\"\r\n",
            &extracted[0],
            &extracted[1],
            &extracted[2],
            &extracted[3],
            &extracted[4],
            &extracted[5],
            &extracted[6],
            &extracted[7],
            &extracted[8],
            &extracted[9]) != 10)
        {
            return HAL_ERROR;
        }
        if(ip) *ip = (extracted[0] << 24) | (extracted[1] << 16) | (extracted[2] << 8) | extracted[3];
        if(mac) for(uint32_t n = 0; n < 6; n++) mac[n] = (uint8_t)(extracted[n + 4] & 0x000000ff);
    }

    return retVal;
}

HAL_StatusTypeDef SocketOpen(
    uint32_t handle,
    const char* socketType,
    const char* hostname,
    uint32_t destinationPort,
    uint32_t timeout)
{
    HAL_StatusTypeDef retVal = HAL_BUSY;
    uint32_t startTime = HAL_GetTick();

    do
    {
        char response[128];
        uint32_t responseLen;
        fprintf(&__net, "AT+CIPSTART=%d,\"%s\",\"%s\",%d\r\n", handle, socketType, hostname, destinationPort);
        if((retVal = ESP8266GetResponse(response, sizeof(response), &responseLen, TIMEOUTLEFT(timeout, startTime))) != HAL_OK)
        {
            if((!strcmp(response, ResponseALREADYCONNECTED)) &&
               ((retVal = SocketClose(handle, TIMEOUTLEFT(timeout, startTime))) == HAL_OK))
            {
                retVal = HAL_BUSY;
                continue; // Let's try again
            }
            goto Cleanup;
        }
    }
    while(retVal != HAL_OK);

Cleanup:
    return retVal;
}

HAL_StatusTypeDef SocketClose(
    uint32_t handle,
    uint32_t timeout)
{
    HAL_StatusTypeDef retVal = HAL_OK;
    uint32_t startTime = HAL_GetTick();

    fprintf(&__net, "AT+CIPCLOSE=%d\r\n", handle);
    if((retVal = ESP8266GetResponse(NULL, 0, NULL, TIMEOUTLEFT(timeout, startTime))) != HAL_OK)
    {
        retVal = HAL_ERROR;
        goto Cleanup;
    }

Cleanup:
    return retVal;
}

HAL_StatusTypeDef SocketSend(
    uint32_t handle,
    uint8_t* data,
    uint32_t dataSize,
    uint32_t timeout)
{
    HAL_StatusTypeDef retVal = HAL_OK;
    uint32_t startTime = HAL_GetTick();
    
    fprintf(&__net, "AT+CIPSEND=%d,%d\r\n", handle, dataSize);
    if((retVal = ESP8266GetResponse(NULL, 0, NULL, TIMEOUTLEFT(timeout, startTime))) != HAL_OK)
    {
        retVal = HAL_ERROR;
        goto Cleanup;
    }

    if((fgetc(&__net) != '>') || (fgetc(&__net) != ' '))
    {
        retVal = HAL_ERROR;
        goto Cleanup;
    }

    fwrite(data, 1, dataSize, &__net);

    if((retVal = ESP8266GetResponse(NULL, 0, NULL, TIMEOUTLEFT(timeout, startTime))) != HAL_OK)
    {
        retVal = HAL_ERROR;
        goto Cleanup;
    }
Cleanup:
    return retVal;
}

HAL_StatusTypeDef  SocketReceive(
    uint32_t handle,
    uint8_t* data,
    uint32_t dataSize,
    uint32_t timeout)
{
    HAL_StatusTypeDef retVal = HAL_OK;
    uint32_t startTime = HAL_GetTick();

    uint32_t cursor = 0;
    while(cursor < dataSize)
    {
        char hdr[20];
        uint32_t index = 0;
        uint32_t hdl = 0;
        uint32_t recLen = 0;

        if((timeout != HAL_MAX_DELAY) && (startTime + timeout < HAL_GetTick())) return HAL_TIMEOUT;

        while((hdr[index++] = (char)fgetc(&__net)) != ':');
        hdr[index] = '\0';
        if((sscanf(&hdr[2], "+IPD,%d,%d:", &hdl, &recLen) != 2) || (hdl != handle))
        {
            retVal = HAL_ERROR;
            goto Cleanup;
        }
        cursor += fread(data, 1, MIN(recLen, dataSize - cursor), &__net);
    }

Cleanup:
    return retVal;
}

