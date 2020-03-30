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

/**
 * @file util.c
 */
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/timer.h>
#include <linux/crc32.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/sockios.h>
#include <linux/wireless.h>
#include <asm/byteorder.h>
#include <asm/ioctls.h>

#include "main.h"
#include "eka.h" // added by ekaline

#define EXTENDED_HEXDUMP
#if defined EXTENDED_HEXDUMP
 // #include <ctype.h>  <- This file is not available in kernel space
 // Simplest possible way of determining printable characters
#define isprint(c) ((c >= 0x1f) && (c < 0x7f))
#endif

#include "main.h"
#include "util.h"

void hexdump(const device_context_t * pDevExt, const char * what, const void * pMemory, size_t memoryLength)
{
    int i = 0, length = 0;
#if defined EXTENDED_HEXDUMP
    char printable[17];
    char msg[100];
    const unsigned char * buf = pMemory;
    uint64_t address = (uint64_t)pMemory;

    if (what != NULL)
    {
        PRINTK("%s\n", what);
    }

    memset(printable, 0, 17);

    length = snprintf(msg, sizeof(msg), "%llX: ", address);
    for (i = 0; i < memoryLength; i++)
    {
        if (i && !(i % 16))
        {
            length += snprintf(&msg[length], sizeof(msg) - length, "  %s\n", printable);
            PRINTK("%s", msg);
            length = 0;
            address += 16;
            length += snprintf(&msg[length], sizeof(msg) - length, "%llX: ", address);
            memset(printable, 0, 17);
        }
        length += snprintf(&msg[length], sizeof(msg) - length, "%02x ", (unsigned char)buf[i]);
        printable[i % 16] = isprint(buf[i]) ? buf[i] : '.';
    }
    for (; i % 16; i++)
    {
        length += snprintf(&msg[length], sizeof(msg) - length, "   ");
    }
    length += snprintf(&msg[length], sizeof(msg) - length, "  %s\n", printable);
    PRINTK("%s", msg);

#else
    for (i = 0; i < len; i++)
    {
        if (i && !(i % 16))
        {
            length += snprintf(&msg[length], sizeof(msg) - length, "\n");
            PRINTK("%s", msg);
        }
        length += snprintf(&msg[length], sizeof(msg) - length, "%02x ", (unsigned char)buf[i]);
    }
    length += snprintf(&msg[length], sizeof(msg) - length, "\n");
    PRINTK("%s\n", msg);
#endif
}

int macDump(char * msg, int msgSize, const uint8_t macAddress[MAC_ADDR_LEN])
{
    return snprintf(msg, msgSize, "Mac addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
        macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
}

// ipAddress in host byte order
int ipDump(char * msg, int msgSize, uint32_t ipAddress)
{
    return snprintf(msg, msgSize, "%u.%u.%u.%u", ipAddress >> 24, (ipAddress >> 16) & 0xFF, (ipAddress >> 8) & 0xFF, ipAddress & 0xFF);
}

const char * _LogIP(char buffer[IP_BUF_LEN], uint32_t bufferSize, uint32_t ip)
{
    ipDump(buffer, bufferSize, ip);
    return buffer;
}

const char * _LogIPs(char buffer[IP_BUF_LEN], uint32_t bufferSize, struct sockaddr * pSockAddr)
{
    struct sockaddr_in * pSockAddrIn = (struct sockaddr_in *)pSockAddr;
    if (pSockAddrIn->sin_family == AF_INET)
    {
        return _LogIP(buffer, bufferSize, pSockAddrIn->sin_addr.s_addr);
    }
    return "NOT AF_INET!";
}

const char * _LogMAC(char buffer[MAC_BUF_LEN], uint32_t bufferSize, const uint8_t macAddress[MAC_ADDR_LEN])
{
    snprintf(buffer, bufferSize, "%02x:%02x:%02x:%02x:%02x:%02x",
        macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    return buffer;
}

const char * LogInt64(char buffer[MAC_BUF_LEN], uint32_t bufferSize, int64_t value)
{
    snprintf(buffer, bufferSize, "%lld", value);
    return buffer;
}
const char * LogIntHdr(const char * header, char buffer[MAC_BUF_LEN], uint32_t bufferSize, int64_t value)
{
    int header_length = strlen(header);
    strcpy(buffer, header);
    snprintf(&buffer[header_length], bufferSize - header_length, "%lld", value);
    return buffer;
}

const char * IoCtlCmd(const device_context_t * pDevExt, unsigned int operation)
{
    switch (operation)
    {
        case SC_IOCTL_LINK_STATUS:              return "SC_IOCTL_LINK_STATUS";
        case SC_IOCTL_ALLOC_CHANNEL:            return "SC_IOCTL_ALLOC_CHANNEL";
        case SC_IOCTL_DEALLOC_CHANNEL:          return "SC_IOCTL_DEALLOC_CHANNEL";
        case SC_IOCTL_GET_BUCKET_HWADDR:        return "SC_IOCTL_GET_BUCKET_HWADDR";
        case SC_IOCTL_SET_LOCAL_ADDR:           return "SC_IOCTL_SET_LOCAL_ADDR";
        case SC_IOCTL_GET_LOCAL_ADDR:           return "SC_IOCTL_GET_LOCAL_ADDR";
        case SC_IOCTL_GET_NO_BUCKETS:           return "SC_IOCTL_GET_NO_BUCKETS";
        case SC_IOCTL_SET_READMARK:             return "SC_IOCTL_SET_READMARK";
        case SC_IOCTL_GET_RECV_HWADDR:          return "SC_IOCTL_GET_RECV_HWADDR";
        case SC_IOCTL_EMPTY:                    return "SC_IOCTL_EMPTY";
        //case SC_IOCTL_MEMORY_MAPPINGS:        return "";
        case SC_IOCTL_READ_TEMPERATURE:         return "SC_IOCTL_READ_TEMPERATURE";
        case SC_IOCTL_MISC:                     return "SC_IOCTL_MISC";
        case SC_IOCTL_GET_STAT:                 return "SC_IOCTL_GET_STAT";
        case SC_IOCTL_FPGA_RESET:               return "SC_IOCTL_FPGA_RESET";
        case SC_IOCTL_TIMERMODE:                return "SC_IOCTL_TIMERMODE";
        case SC_IOCTL_USERSPACE_RX:             return "SC_IOCTL_USERSPACE_RX";
        case SC_IOCTL_USERSPACE_TX:             return "SC_IOCTL_USERSPACE_TX";
        case SC_IOCTL_RESERVE_FIFO:             return "SC_IOCTL_RESERVE_FIFO";
        //case SC_IOCTL_ALLOC_DMABUF:             return "SC_IOCTL_ALLOC_DMABUF";
        //case SC_IOCTL_DEALLOC_DMABUF:           return "SC_IOCTL_DEALLOC_DMABUF";
        case SC_IOCTL_ACL_CMD:                  return "SC_IOCTL_ACL_CMD";
        case SC_IOCTL_GET_DRIVER_VERSION:       return "SC_IOCTL_GET_DRIVER_VERSION";
        case SC_IOCTL_CONNECT:                  return "SC_IOCTL_CONNECT";
        case SC_IOCTL_DISCONNECT:               return "SC_IOCTL_DISCONNECT";
        case SC_IOCTL_ABORT_ETLM_CHANNEL:       return "SC_IOCTL_ABORT_ETLM_CHANNEL";
        case SC_IOCTL_ARP:                      return "SC_IOCTL_ARP";
        case SC_IOCTL_LISTEN:                   return "SC_IOCTL_LISTEN";
        case SC_IOCTL_IGMPINFO:                 return "SC_IOCTL_IGMPINFO";
        case SC_IOCTL_GET_TCP_STATUS:           return "SC_IOCTL_GET_TCP_STATUS";
        case SC_IOCTL_LAST_ACK_NO:              return "SC_IOCTL_LAST_ACK_NO";
        case SC_IOCTL_ETLM_DATA:                return "SC_IOCTL_ETLM_DATA";
     case SC_IOCTL_EKALINE_DATA:        return "SC_IOCTL_EKALINE_DATA"; // added by ekaline

        default:
            {
                static char unknown[20];

                DecodeIoCtlCmd(pDevExt, operation);
                snprintf(unknown, sizeof(unknown), " *** UNKNOWN: %u", operation);
                return unknown;
            }
    }
}

void LogAllIoCtlCmds(const device_context_t * pDevExt)
{
    PRINTK("SC_IOCTL_LINK_STATUS        = %lu\n", SC_IOCTL_LINK_STATUS);
    PRINTK("SC_IOCTL_ALLOC_CHANNEL      = %lu\n", SC_IOCTL_ALLOC_CHANNEL);
    PRINTK("SC_IOCTL_DEALLOC_CHANNEL    = %u\n", SC_IOCTL_DEALLOC_CHANNEL);
    PRINTK("SC_IOCTL_GET_BUCKET_HWADDR  = %lu\n", SC_IOCTL_GET_BUCKET_HWADDR);
    PRINTK("SC_IOCTL_SET_LOCAL_ADDR     = %lu\n", SC_IOCTL_SET_LOCAL_ADDR);
    PRINTK("SC_IOCTL_GET_LOCAL_ADDR     = %lu\n", SC_IOCTL_GET_LOCAL_ADDR);
    PRINTK("SC_IOCTL_GET_NO_BUCKETS     = %lu\n", SC_IOCTL_GET_NO_BUCKETS);
    PRINTK("SC_IOCTL_SET_READMARK       = %lu\n", SC_IOCTL_SET_READMARK);
    PRINTK("SC_IOCTL_GET_RECV_HWADDR    = %lu\n", SC_IOCTL_GET_RECV_HWADDR);
        //case SC_IOCTL_???:                    return "SC_IOCTL_???";
        //case SC_IOCTL_MEMORY_MAPPINGS:        return "";
    PRINTK("SC_IOCTL_READ_TEMPERATURE   = %lu\n", SC_IOCTL_READ_TEMPERATURE);
    PRINTK("SC_IOCTL_MISC               = %lu\n", SC_IOCTL_MISC);
    PRINTK("SC_IOCTL_GET_STAT           = %lu\n", SC_IOCTL_GET_STAT);
    //PRINTK("SC_IOCTL_PING               = %lu\n", SC_IOCTL_PING);
    PRINTK("SC_IOCTL_TIMERMODE          = %lu\n", SC_IOCTL_TIMERMODE);
    PRINTK("SC_IOCTL_USERSPACE_RX       = %u\n", SC_IOCTL_USERSPACE_RX);
    PRINTK("SC_IOCTL_USERSPACE_TX       = %u\n", SC_IOCTL_USERSPACE_TX);
    PRINTK("SC_IOCTL_RESERVE_FIFO       = %lu\n", SC_IOCTL_RESERVE_FIFO);
        //case SC_IOCTL_ALLOC_DMABUF:             return "SC_IOCTL_ALLOC_DMABUF";
        //case SC_IOCTL_DEALLOC_DMABUF:           return "SC_IOCTL_DEALLOC_DMABUF";
    PRINTK("SC_IOCTL_ACL_CMD            = %lu\n", SC_IOCTL_ACL_CMD);
    PRINTK("SC_IOCTL_GET_DRIVER_VERSION = %lu\n", SC_IOCTL_GET_DRIVER_VERSION);
    PRINTK("SC_IOCTL_GET_SYSTEM_INFO    = %lu\n", SC_IOCTL_GET_SYSTEM_INFO);
    PRINTK("SC_IOCTL_DMA_INFO           = %lu\n", SC_IOCTL_DMA_INFO);
    PRINTK("SC_IOCTL_MEMORY_ALLOC_FREE  = %lu\n", SC_IOCTL_MEMORY_ALLOC_FREE);
    PRINTK("SC_IOCTL_REFERENCE_COUNT    = %lu\n", SC_IOCTL_REFERENCE_COUNT);

    PRINTK("SC_IOCTL_CONNECT            = %lu\n", SC_IOCTL_CONNECT);
    PRINTK("SC_IOCTL_DISCONNECT         = %lu\n", SC_IOCTL_DISCONNECT);
    PRINTK("SC_IOCTL_ABORT_ETLM_CHANNEL = %u\n", SC_IOCTL_ABORT_ETLM_CHANNEL);
    PRINTK("SC_IOCTL_ARP                = %lu\n", SC_IOCTL_ARP);
    PRINTK("SC_IOCTL_LISTEN             = %lu\n", SC_IOCTL_LISTEN);
    PRINTK("SC_IOCTL_IGMPINFO           = %lu\n", SC_IOCTL_IGMPINFO);
    PRINTK("SC_IOCTL_GET_TCP_STATUS     = %lu\n", SC_IOCTL_GET_TCP_STATUS);
    PRINTK("SC_IOCTL_LAST_ACK_NO        = %lu\n", SC_IOCTL_LAST_ACK_NO);
    PRINTK("SC_IOCTL_ETLM_DATA          = %lu\n", SC_IOCTL_ETLM_DATA);
    PRINTK("SC_IOCT_EKALINE_DATA = %lu\n", SC_IOCTL_EKALINE_DATA); // added by ekaline

}

#if 0

// From https://elixir.free-electrons.com/linux/v4.10.17/source/include/uapi/asm-generic/ioctl.h#L79

#define _IOC(dir,type,nr,size)			\
	((unsigned int)				\
	 (((dir)  << _IOC_DIRSHIFT) |		\
	  ((type) << _IOC_TYPESHIFT) |		\
	  ((nr)   << _IOC_NRSHIFT) |		\
	  ((size) << _IOC_SIZESHIFT)))

/* used to create numbers */
#define _IO(type,nr)		_IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)	_IOC(_IOC_READ,(type),(nr),sizeof(size))
#define _IOW(type,nr,size)	_IOC(_IOC_WRITE,(type),(nr),sizeof(size))
#define _IOWR(type,nr,size)	_IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))
#endif

void DecodeIoCtlCmd(const device_context_t * pDevExt, unsigned int op)
{
    PRINTK_ERR("ioctl op = %u, SC_IOCTL_MISC = %lu\n", op, SC_IOCTL_MISC);
    PRINTK_ERR("_IOC_DIRBITS = %u, _IOC_SIZEBITS = %u, _IOC_TYPEBITS = %u, _IOC_NRBITS = %u\n", _IOC_DIRBITS, _IOC_SIZEBITS, _IOC_TYPEBITS, _IOC_NRBITS);
    PRINTK_ERR("_IOC_DIRSHIFT = %u, _IOC_SIZESHIFT = %u, _IOC_TYPESHIFT = %u, _IOC_NRSHIFT = %u\n", _IOC_DIRSHIFT, _IOC_SIZESHIFT, _IOC_TYPESHIFT, _IOC_NRSHIFT);
    PRINTK_ERR("dir = %u, size = %u, type = %u, nr = %u\n", _IOC_DIR(op), _IOC_SIZE(op), _IOC_TYPE(op), _IOC_NR(op));
}

const char * SocketFlags(int cmd)
{
    switch (cmd)
    {
        // https://elixir.free-electrons.com/linux/latest/source/include/uapi/linux/sockios.h#25

        /* Linux-specific socket ioctls */
        case SIOCINQ: return "SIOCINQ";
        case SIOCOUTQ: return "SIOCOUTQ";   /* output queue size (not sent + not acked) */

        /* Routing table calls. */
        case SIOCADDRT: return "SIOCADDRT";    /* add routing table entry */
        case SIOCDELRT: return "SIOCDELRT";    /* delete routing table entry */
        case SIOCRTMSG: return "SIOCRTMSG";    /* call to routing system */

        /* Socket configuration controls. */
        case SIOCGIFNAME: return "SIOCGIFNAME"; /* get iface name */
        case SIOCSIFLINK: return "SIOCSIFLINK"; /* set iface channel */
#if 0
#define SIOCGIFCONF	0x8912		/* get iface list		*/
#define SIOCGIFFLAGS	0x8913		/* get flags			*/
#define SIOCSIFFLAGS	0x8914		/* set flags			*/
#endif
        case SIOCGIFADDR: return "SIOCGIFADDR"; // 0x8915   /* get PA address */
        case SIOCSIFADDR: return "SIOCSIFADDR"; // 0x8916   /* set PA address */
#if 0
#define SIOCGIFDSTADDR	0x8917		/* get remote PA address	*/
#define SIOCSIFDSTADDR	0x8918		/* set remote PA address	*/
#define SIOCGIFBRDADDR	0x8919		/* get broadcast PA address	*/
#define SIOCSIFBRDADDR	0x891a		/* set broadcast PA address	*/
#define SIOCGIFNETMASK	0x891b		/* get network PA mask		*/
#define SIOCSIFNETMASK	0x891c		/* set network PA mask		*/
#define SIOCGIFMETRIC	0x891d		/* get metric			*/
#define SIOCSIFMETRIC	0x891e		/* set metric			*/
#define SIOCGIFMEM	0x891f		/* get memory address (BSD)	*/
#define SIOCSIFMEM	0x8920		/* set memory address (BSD)	*/
#define SIOCGIFMTU	0x8921		/* get MTU size			*/
#define SIOCSIFMTU	0x8922		/* set MTU size			*/
#define SIOCSIFNAME	0x8923		/* set interface name */
#define	SIOCSIFHWADDR	0x8924		/* set hardware address 	*/
#define SIOCGIFENCAP	0x8925		/* get/set encapsulations       */
#define SIOCSIFENCAP	0x8926		
#define SIOCGIFHWADDR	0x8927		/* Get hardware address		*/
#define SIOCGIFSLAVE	0x8929		/* Driver slaving support	*/
#define SIOCSIFSLAVE	0x8930
#define SIOCADDMULTI	0x8931		/* Multicast address lists	*/
#define SIOCDELMULTI	0x8932
#define SIOCGIFINDEX	0x8933		/* name -> if_index mapping	*/
#define SIOGIFINDEX	SIOCGIFINDEX	/* misprint compatibility :-)	*/
#define SIOCSIFPFLAGS	0x8934		/* set/get extended flags set	*/
#define SIOCGIFPFLAGS	0x8935
#define SIOCDIFADDR	0x8936		/* delete PA address		*/
#define	SIOCSIFHWBROADCAST	0x8937	/* set hardware broadcast addr	*/
#define SIOCGIFCOUNT	0x8938		/* get number of devices */

#define SIOCGIFBR	0x8940		/* Bridging support		*/
#define SIOCSIFBR	0x8941		/* Set bridging options 	*/

#define SIOCGIFTXQLEN	0x8942		/* Get the tx queue length	*/
#define SIOCSIFTXQLEN	0x8943		/* Set the tx queue length 	*/

        /* SIOCGIFDIVERT was:	0x8944		Frame diversion support */
        /* SIOCSIFDIVERT was:	0x8945		Set frame diversion options */

#define SIOCETHTOOL	0x8946		/* Ethtool interface		*/
#endif

        case SIOCGMIIPHY: return "SIOCGMIIPHY"; /* Get address of MII PHY in use. */
        case SIOCGMIIREG: return "SIOCGMIIREG"; /* Read MII PHY register. */
        case SIOCSMIIREG: return "SIOCSMIIREG"; /* Write MII PHY register. */

#if 0
#define SIOCWANDEV	0x894A		/* get/set netdev parameters	*/

#define SIOCOUTQNSD	0x894B		/* output queue size (not sent only) */

        /* ARP cache control calls. */
        /*  0x8950 - 0x8952  * obsolete calls, don't re-use */
#define SIOCDARP	0x8953		/* delete ARP table entry	*/
#define SIOCGARP	0x8954		/* get ARP table entry		*/
#define SIOCSARP	0x8955		/* set ARP table entry		*/

        /* RARP cache control calls. */
#define SIOCDRARP	0x8960		/* delete RARP table entry	*/
#define SIOCGRARP	0x8961		/* get RARP table entry		*/
#define SIOCSRARP	0x8962		/* set RARP table entry		*/

        /* Driver configuration calls */

#define SIOCGIFMAP	0x8970		/* Get device parameters	*/
#define SIOCSIFMAP	0x8971		/* Set device parameters	*/

        /* DLCI configuration calls */

#define SIOCADDDLCI	0x8980		/* Create new DLCI device	*/
#define SIOCDELDLCI	0x8981		/* Delete DLCI device		*/

#define SIOCGIFVLAN	0x8982		/* 802.1Q VLAN support		*/
#define SIOCSIFVLAN	0x8983		/* Set 802.1Q VLAN options 	*/

        /* bonding calls */

#define SIOCBONDENSLAVE	0x8990		/* enslave a device to the bond */
#define SIOCBONDRELEASE 0x8991		/* release a slave from the bond*/
#define SIOCBONDSETHWADDR      0x8992	/* set the hw addr of the bond  */
#define SIOCBONDSLAVEINFOQUERY 0x8993   /* rtn info about slave state   */
#define SIOCBONDINFOQUERY      0x8994	/* rtn info about bond state    */
#define SIOCBONDCHANGEACTIVE   0x8995   /* update to a new active slave */

        /* bridge calls */
#define SIOCBRADDBR     0x89a0		/* create new bridge device     */
#define SIOCBRDELBR     0x89a1		/* remove bridge device         */
#define SIOCBRADDIF	0x89a2		/* add interface to bridge      */
#define SIOCBRDELIF	0x89a3		/* remove interface from bridge */

        /* hardware time stamping: parameters in linux/net_tstamp.h */
#define SIOCSHWTSTAMP   0x89b0

        /* Device private ioctl calls */

        /*
        *	These 16 ioctls are available to devices via the do_ioctl() device
        *	vector. Each device should include this file and redefine these names
        *	as their own. Because these are device dependent it is a good idea
        *	_NOT_ to issue them to random objects and hope.
        *
        *	THESE IOCTLS ARE _DEPRECATED_ AND WILL DISAPPEAR IN 2.5.X -DaveM
        */

#define SIOCDEVPRIVATE	0x89F0	/* to 89FF */

        /*
        *	These 16 ioctl calls are protocol private
        */

#define SIOCPROTOPRIVATE 0x89E0 /* to 89EF */
#endif

        // https://elixir.free-electrons.com/linux/latest/source/include/uapi/linux/wireless.h#L232

        case SIOCGIWNAME:   return "SIOCGIWNAME";   // 0x8B01   /* get name == wireless protocol */

        default: return "???";
    }
}

const char * LogInt(char * buffer, size_t bufferSize, int x)
{
    snprintf(buffer, bufferSize, "%d", x);
    return buffer;
}

const char * strtok(char * s, char token)
{
    static char * next = NULL;
    const char * start;

    if (s == NULL)
    {
        if (next == NULL)
        {
            return NULL;
        }
        s = next;
    }
    start = s;

    while (*s != '\0' && *s != token)
    {
        ++s;
    }
    if (*s == token)
    {
        *s = '\0';
        next = s + 1;
        return start;
    }
    else if (*s == '\0' && s != start)
    {
        next = s;
        return start;
    }

    return NULL;
}

const char * basename(const char * fileName)
{
    const char * baseName = fileName + strlen(fileName);
    while (baseName != fileName && *baseName-- != '/');
    if (baseName == fileName)
    {
        return baseName;
    }
    return baseName + 2;
}
