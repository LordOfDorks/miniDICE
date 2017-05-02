#ifndef __SOCKET_H
#define __SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
extern const char* SocketUDP;
extern const char* SocketTCP;
    
HAL_StatusTypeDef  InitializeESP8266(
    uint32_t* ip,
    uint8_t* mac,
    uint32_t timeout);

HAL_StatusTypeDef SocketOpen(
    uint32_t handle,
    const char* socketType,
    const char* hostname,
    uint32_t destinationPort,
    uint32_t timeout);
    
HAL_StatusTypeDef SocketClose(
    uint32_t handle,
    uint32_t timeout);
    
HAL_StatusTypeDef SocketSend(
    uint32_t handle,
    uint8_t* data,
    uint32_t dataSize,
    uint32_t timeout);
    
HAL_StatusTypeDef  SocketReceive(
    uint32_t handle,
    uint8_t* data,
    uint32_t dataSize,
    uint32_t timeout);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
