#include <time.h>
#include <string.h>
#include "main.h"
#include "stm32l0xx_hal.h"
#include "Socket.h"

#define NTP_DEFAULT_TIMEOUT 4000
#define NTP_PORT 123
#define NTP_TIMESTAMP_DELTA 2208988800ull //Diff btw a UNIX timestamp (Starting Jan, 1st 1970) and a NTP timestamp (Starting Jan, 1st 1900)

struct NTPPacket //See RFC 4330 for Simple NTP
{
    //WARN: We are in LE! Network is BE!
    //LSb first
    unsigned mode : 3;
    unsigned vn : 3;
    unsigned li : 2;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    //32 bits header
    uint32_t rootDelay;
    uint32_t rootDispersion;
    uint32_t refId;
    uint32_t refTm_s;
    uint32_t refTm_f;
    uint32_t origTm_s;
    uint32_t origTm_f;
    uint32_t rxTm_s;
    uint32_t rxTm_f;
    uint32_t txTm_s;
    uint32_t txTm_f;
} __attribute__ ((packed));

HAL_StatusTypeDef NtpGetTime(
    const char* host,
    uint32_t timeout,
    time_t* ntpTime
    )
{
    HAL_StatusTypeDef result = HAL_OK;
    struct NTPPacket pkt ={0};
    uint32_t destTm_s = 0;
    int64_t offset = 0;

    //Create socket
    if((result = SocketOpen(0, SocketUDP, host, NTP_PORT, timeout)) != HAL_OK)
    {
        return result;
    }

    //Prepare NTP Packet:
    pkt.li = 0; //Leap Indicator : No warning
    pkt.vn = 4; //Version Number : 4
    pkt.mode = 3; //Client mode
    pkt.stratum = 0; //Not relevant here
    pkt.poll = 0; //Not significant as well
    pkt.precision = 0; //Neither this one is

    pkt.rootDelay = 0; //Or this one
    pkt.rootDispersion = 0; //Or that one
    pkt.refId = 0; //...

    pkt.refTm_s = 0;
    pkt.origTm_s = 0;
    pkt.rxTm_s = 0;
    pkt.txTm_s = SWAP_UINT32( NTP_TIMESTAMP_DELTA + time(NULL) ); //WARN: We are in LE format, network byte order is BE

    pkt.refTm_f = pkt.origTm_f = pkt.rxTm_f = pkt.txTm_f = 0;

    //Now ping the server and wait for response
    if((result = SocketSend(0, (uint8_t*)&pkt, sizeof(struct NTPPacket), timeout)) != HAL_OK)
    {
        return result;
    }

    //Read response
    if((result = SocketReceive(0, (uint8_t*)&pkt, sizeof(struct NTPPacket), timeout)) != HAL_OK)
    {
        return result;
    }

    if( pkt.stratum == 0)  //Kiss of death message : Not good !
    {
        result = HAL_ERROR;
        goto Cleanup;
    }

    //Correct Endianness 
    pkt.refTm_s = SWAP_UINT32( pkt.refTm_s );
    pkt.refTm_f = SWAP_UINT32( pkt.refTm_f );
    pkt.origTm_s = SWAP_UINT32( pkt.origTm_s );
    pkt.origTm_f = SWAP_UINT32( pkt.origTm_f );
    pkt.rxTm_s = SWAP_UINT32( pkt.rxTm_s );
    pkt.rxTm_f = SWAP_UINT32( pkt.rxTm_f );
    pkt.txTm_s = SWAP_UINT32( pkt.txTm_s );
    pkt.txTm_f = SWAP_UINT32( pkt.txTm_f );

    //Compute offset, see RFC 4330 p.13
    destTm_s = (NTP_TIMESTAMP_DELTA + time(NULL));
    offset = ( (int64_t)( pkt.rxTm_s - pkt.origTm_s ) + (int64_t) ( pkt.txTm_s - destTm_s ) ) / 2; //Avoid overflow

    //Return time
    *ntpTime = time(NULL) + offset;

Cleanup:
    SocketClose(0, timeout);
    return result;
}
