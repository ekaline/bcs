#ifndef _MEMORY_DEFINES_H_
#define _MEMORY_DEFINES_H_

static const uint64_t REG_FIRE_ON       = 0x0000000000f007c8;
//static const uint64_t REG_WINDOW        = 0x0000000000f00058;
//static const uint64_t WRITE_TO          = 0x0000000000600000;
//static const uint64_t REG_DEBUG         = 0x0000000000f00018;
static const uint64_t DEBUG_VAL         = 0x0000000020000000;
static const uint64_t GLOBAL_MAX_SIZE   = 0x0000000000f007e0;
//static const uint64_t INJECT_MD_DATA    = 0x0000000000f007d0;
    //^^^ saved to memory only when INJECT_MD_CTX_PTR is changed.
//static const uint64_t INJECT_MD_CTX_PTR = 0x0000000000f007d8;
    //^^^ MUST be updated AFTER INJECT_MD_DATA is written (all 64B).
static const uint64_t REG_P4_GEN        = 0x0000000000f007c0;
/*
 * P4_GEN:
 *      bit 63  = always fire mode
 *      bit 1   = generate statistics report , added to regular report
 *      bit 0   = report only enable (  0 report and send
 *                                      1 report only   )
 */

static const uint64_t WATCHDOG_CONF     = 0x0000000000f00650;


/*
 * update ctx channel addresses
 */
#define CHANNEL_BASE 0x0000000000900000
//***channel buff needs to be 2^x value multiplied by 8 (=number of banks)
//#define CHANNEL_BUFF  0x40 //single buff version
#define CHANNEL_SINGLE_SIZE 0x8
//#define CHANNEL_BUFF  0x40*8 //multiple buff version sec_ctx = 64B
//#define CHANNEL_BUFF  0x20*8 //multiple buff version sec_ctx = 17B
#define CHANNEL_BUFF  CHANNEL_SINGLE_SIZE*8 //multiple buff version sec_ctx = 16B

static const uint64_t CHANNELS[16] = {
    CHANNEL_BASE + CHANNEL_BUFF*0,
    CHANNEL_BASE + CHANNEL_BUFF*1,
    CHANNEL_BASE + CHANNEL_BUFF*2,
    CHANNEL_BASE + CHANNEL_BUFF*3,
    CHANNEL_BASE + CHANNEL_BUFF*4,
    CHANNEL_BASE + CHANNEL_BUFF*5,
    CHANNEL_BASE + CHANNEL_BUFF*6,
    CHANNEL_BASE + CHANNEL_BUFF*7,
    CHANNEL_BASE + CHANNEL_BUFF*8,
    CHANNEL_BASE + CHANNEL_BUFF*9,
    CHANNEL_BASE + CHANNEL_BUFF*10,
    CHANNEL_BASE + CHANNEL_BUFF*11,
    CHANNEL_BASE + CHANNEL_BUFF*12,
    CHANNEL_BASE + CHANNEL_BUFF*13,
    CHANNEL_BASE + CHANNEL_BUFF*14,
    CHANNEL_BASE + CHANNEL_BUFF*15};

static const uint64_t CONFIRM_REG = 0x0000000000f00660;

static volatile uint8_t BUFFER_FLAG[16] = {};

#endif /* _MEMORY_DEFINES_H_ */
