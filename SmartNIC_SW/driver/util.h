#ifndef UTIL_H
#define UTIL_H

/*
 ***************************************************************************
 *
 * Copyright (c) 2008-2019, Silicom Denmark A/S
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Silicom nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with a
 *  Silicom network adapter product.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

#define number_of_elements(x)     (sizeof(x) / sizeof(x[0]))

/* Format an ip address for printing. Returns the number of characters written in buf.
 * Ip address is in network byte order (like struct in_addr).
 */
inline static int util_format_ip(char* buf, int bufsz, uint32_t ip)
{
    unsigned char bytes[4];
    bytes[3] = ip & 0xFF;
    bytes[2] = (ip >> 8) & 0xFF;
    bytes[1] = (ip >> 16) & 0xFF;
    bytes[0] = (ip >> 24) & 0xFF;
    return snprintf(buf, bufsz, "%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0]);
}

// This treats 0 as NOT a power of 2
#define is_power_of_2(n)    ((n) != 0 && (((n) & ((n) - 1)) == 0))

static inline uint32_t
round_upper_power_of_2(uint32_t n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

static inline uint32_t
round_lower_power_of_2(uint32_t n)
{
    if(is_power_of_2(n))
        return n;
    return round_upper_power_of_2(n) >> 1;
}

void hexdump(const device_context_t * pDevExt, const char * what, const void * pMemory, size_t memoryLength);
int macDump(char * msg, int msgSize, const uint8_t macAddress[MAC_ADDR_LEN]);
int ipDump(char * msg, int msgSize, uint32_t ipAddress);

#define IP_BUF_LEN      16
#define MAC_BUF_LEN     (3 * MAC_ADDR_LEN)

#define IPBUF(N)            char __ipBuffer__##N[IP_BUF_LEN]; const uint32_t __ipBufferSize__##N = IP_BUF_LEN
#define MACBUF(N)           char __macBuffer__##N[MAC_BUF_LEN]; const uint32_t __macBufferSize__##N = MAC_BUF_LEN

const char * _LogIP(char buffer[IP_BUF_LEN], uint32_t bufferSize, uint32_t ip);
#define LogIP(bufferNumber, ip)     _LogIP(__ipBuffer__##bufferNumber, __ipBufferSize__##bufferNumber, ip)

const char * _LogIPs(char buffer[IP_BUF_LEN], uint32_t bufferSize, struct sockaddr * pSockAddr);
#define LogIPs(bufferNumber, pSockAddr)     _LogIPs(__ipBuffer__##bufferNumber, __ipBufferSize__##bufferNumber, pSockAddr)

const char * _LogMAC(char buffer[MAC_BUF_LEN], uint32_t bufferSize, const uint8_t macAddress[MAC_ADDR_LEN]);
#define LogMAC(bufferNumber, macAddress)    _LogMAC(__macBuffer__##bufferNumber, __macBufferSize__##bufferNumber, macAddress)

const char * LogInt64(char buffer[MAC_BUF_LEN], uint32_t bufferSize, int64_t value);
const char * LogIntHdr(const char * header, char buffer[MAC_BUF_LEN], uint32_t bufferSize, int64_t value);
const char * LogInt(char * buffer, size_t bufferSize, int x);

const char * IoCtlCmd(const device_context_t * pDevExt, unsigned int operation);
void LogAllIoCtlCmds(const device_context_t * pDevExt);
void DecodeIoCtlCmd(const device_context_t * pDevExt, unsigned int op);
const char * SocketFlags(int cmd);

const char * strtok(char * s, char token);

#endif /* UTIL_H */
