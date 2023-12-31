//#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <inttypes.h>

#include "smartnic.h"

uint64_t htoi(char s[]);
int my_strlen(char s[]);

void die(char *msg)
{
    perror(msg);
    exit(1);
}

#define BITS_PER_LONG 64
#define BIT(x) (1UL << (x))
#define MMAP_FLAG_BAR_SHIFT (BITS_PER_LONG - 1)
#define MMAP_FLAG_DMA_SHIFT (BITS_PER_LONG - 2)
#define MMAP_BAR_NUM_SHIFT  (BITS_PER_LONG - 5)
#define MMAP_FLAG_BAR       BIT(MMAP_FLAG_BAR_SHIFT)

#define MAX_BAR_SIZE (1<<25)
#define FREQUENCY 161

#define ADDR_CTX_DUMP_SECID   0xf0750
#define ADDR_CTX_BY_HASH_BASE 0x82000

typedef uint32_t fixed4_t; //scale = 10000

struct max_sizes {
  uint8_t bid_max : 4;
  uint8_t ask_max : 4;
}__attribute__ ((packed));

struct sec_ctx {
  uint16_t    min_price;
  uint16_t    max_price;
  struct max_sizes sizes;
    uint8_t        ver_num;
    uint8_t        lower_bytes_of_sec_id;
} __attribute__ ((packed));

SN_DeviceId DeviceId;

uint64_t reg_read (uint32_t addr)
{
    uint64_t value = -1;

    SN_ReadUserLogicRegister (DeviceId, addr/8, &value);

    return value;

}

void reg_write (uint32_t addr, uint64_t value)
{

    SN_WriteUserLogicRegister(DeviceId, addr/8, value);

}

     

int my_strlen(char s[]) {
    int i = 0;
    while(s[i] != '\0')
        i++;
    return i;
}

uint64_t htoi(char s[]) {
    int i, j;
    uint64_t z;
    i = z = 0;
    j = my_strlen(s) - 1;

    if(j < 2) return z;
    if (s[i++] != '0') return z;
    if (s[i] != 'X' && s[i] != 'x') return z;

    //Reset i to represent position
    i = 0;
    //Convert hexidecimal to integer
    for(i = 0; s[j] != 'x' && s[j] != 'X'; j--, i++) {
        if(s[j] >= '0' && s[j] <= '9')
            z =  z + (s[j] - '0') * pow(16, i);
        else if(s[j] >= 'a' && s[j] <= 'f')
            z = z + ((s[j] - 'a') + 10) * pow(16, i);
        else if(s[j] >= 'A' && s[j] <= 'F')
            z = z + ((s[j] - 'A') + 10) * pow(16,i);
        else
            continue;
    }
    return z;
}

int main(int argc, char *argv[])
{

  setlocale(LC_NUMERIC, "");

  int found;
  uint64_t user_sec_id;

  if ( argc < 2 )
    {
      printf("Wrong usage, need to supply at least one security ID in hex or dec format");    
      return 1;
    }

    DeviceId = SN_OpenDevice(NULL, NULL);
    if (DeviceId == NULL) {
        fprintf( stderr, "Cannot open FiberBlaze device. Is driver loaded?\n" );
        exit( -1 );
    }

  for( int sec = 1; sec<argc ; sec++){

    if (argv[sec][1] == 'x' || argv[sec][1] == 'X')
      user_sec_id = htoi(argv[sec]);
    else
      user_sec_id = atoi(argv[sec]);

    // configure secid
    reg_write(ADDR_CTX_DUMP_SECID,user_sec_id);
    
    
    uint8_t buffer[24] = { 0 };
    
    // dump ctx
    for( int a = 0; a >=0 ; a--){
      uint64_t var_ctx_current = reg_read(ADDR_CTX_BY_HASH_BASE+a*8);
      if (var_ctx_current == 0x00000bad00000bad) {
	printf("Subscription for secid %ju (0x%jx) was not found, no CTX to dump\n", user_sec_id, user_sec_id);    
	found = 0;
	//	return 1;
      } else {
	((uint64_t *)buffer)[a] = var_ctx_current;
	found = 1;
      }
      //       printf("_%016lx", var_ctx_current);
    }
    //    printf("\n");
 
    if (found) {
      printf("SecID %ju (0x%jx) is 0x", user_sec_id, user_sec_id);    
      for( int a = 7; a >=0 ; a--){
	printf("%02x", buffer[a]);
      }

      struct sec_ctx *c = (struct sec_ctx*)buffer;
      printf(" :: ");

      printf("bid_min_price: %d (0x%x) | "
	     "bid_max_size: %d (0x%x) | "
	     "ask_max_price: %d (0x%x) | "
	     "ask_max_size: %d (0x%x) | "
	     "version_num: %d | "
	     "security_id_low: 0x%08x (%d) ",
	     c->min_price, c->min_price, 
	     c->sizes.bid_max, c->sizes.bid_max,
	     c->max_price, c->max_price,
	     c->sizes.ask_max, c->sizes.ask_max, 
	     c->ver_num,
	     c->lower_bytes_of_sec_id, c->lower_bytes_of_sec_id);


      printf("\n");
    }
  }

  SN_CloseDevice(DeviceId);

  return 0;
}
