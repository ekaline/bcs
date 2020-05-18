#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <assert.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <stdbool.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#include "eka_nw_headers.h"
#include "eka_macros.h"

struct Sec {
  int id;
  char name[21];
  struct Sec* next;
};

struct HashInd {
  int cnt;
  Sec* head;
  Sec* tail;
};


int printBuckets(HashInd* ind) {
  Sec* s = ind->head;
  TEST_LOG("printing %u entries",ind->cnt);
  for (int i = 0; i < ind->cnt; i++) {
    if (s == NULL) on_error("s = NULL");
    printf ("\t%3d: |%s|\n",i,s->name);
    s = s->next;
  }
  return 0;
}
/* --------------------------------------- */
uint32_t hash2(std::string line) {
  // 5 * 5 = 25 bits
  uint64_t hashRes = 0;
  for (int i = 0; i < 5; i++) {
    uint64_t nameLetter = (line.c_str()[i] - 'A') & 0x1F;
    hashRes |=  nameLetter << (i * 5);
  }

  // 5 bits
  char month = line.c_str()[6];
  hashRes |= ((month - 'A') & 0x1F) << (5 * 5);

  // 9,999,999 = 24 bits
  std::string priceStr = line.substr(8,7);
  uint64_t price = std::stoul(priceStr,nullptr,10) & 0xFFFFFFFF;
  hashRes |= price << 30;

  // 2 bits
  std::string yearStr = line.substr(16,2);
  uint64_t year = (std::stoi(yearStr,nullptr,10) - 20) & 0x3;
  hashRes |= year << 54;

  // 5 bits
  std::string dayStr = line.substr(18,2);
  int day = std::stoi(dayStr,nullptr,10) & 0x1F;
  hashRes |= year << 56;

  uint32_t hashPartA = (hashRes >> 0 ) & 0xFFFFF;
  uint32_t hashPartB = (hashRes >> 20) & 0xFFFFF;
  uint32_t hashPartC = (hashRes >> 40) & 0xFFFFF;
  //    printf ("line = %s, price = %s  = %u, month = %c, hash = %x\n",line.c_str(),priceStr.c_str(), price, month,hashVal);
  return hashPartA ^ hashPartB ^ hashPartC;
}

/* --------------------------------------- */
uint32_t hash1(std::string line) {
    char monthStr = line.c_str()[6];
    uint32_t month = monthStr - 'A';

    std::string dayStr = line.substr(18,2);
    int day = std::stoi(dayStr,nullptr,10);

    std::string priceStr = line.substr(10,4);
    uint32_t price = std::stoul(priceStr,nullptr,10);

    uint32_t priceHash = (price & 0x3FFF) << 6; // 14
    uint32_t monthHash = (month & 0x1F  ) << 1; // 5
    uint32_t dayHash   = (day   & 0x2   ) >> 1; // 1
    uint32_t hashVal = priceHash | monthHash | dayHash;
    //    printf ("line = %s, price = %s  = %u, month = %c, hash = %x\n",line.c_str(),priceStr.c_str(), price, month,hashVal);
    return hashVal;
}
/* --------------------------------------- */
int main(int argc, char *argv[]) {
  std::ifstream infile(argv[1]);
  if (! infile.is_open()) on_error ("cannot open %s",argv[1]);
  printf ("%s is open\n",argv[1]); 

  const uint SecArraySize = 128 * 1024;

  Sec* secArr = (Sec*) malloc(sizeof(Sec) * SecArraySize);
  if (secArr == NULL) on_error("secArr = NULL");
  memset(secArr,0,sizeof(Sec) * SecArraySize);

  const uint IndTableSize = 1024 * 1024;

  HashInd* hashIndTable = (HashInd*) malloc(sizeof(HashInd) * IndTableSize);
  if (hashIndTable == NULL) on_error("secArr = NULL");
  memset(hashIndTable,0,sizeof(HashInd) * IndTableSize);

  int i = 0;
  for (std::string line; getline(infile, line); ) {   
    uint32_t hashVal = hash2(line);

    if (hashVal >= IndTableSize) on_error("hashVal %u >= IndTableSize %u",hashVal, IndTableSize);

    memcpy(secArr[i].name,line.c_str(),20);
    
    if (hashIndTable[hashVal].cnt == 0) {
      hashIndTable[hashVal].head = &secArr[i];
    } else {
      hashIndTable[hashVal].tail->next = &secArr[i];
    }
    hashIndTable[hashVal].tail = &secArr[i];
    hashIndTable[hashVal].tail->next = NULL;
    hashIndTable[hashVal].cnt ++;
    i++;
  }
  infile.close();

  const uint MaxCollisions = 100;
  uint total = 0;
  int collision[MaxCollisions] = {};
  for (auto i = 0; i < IndTableSize; i++) {
    if (hashIndTable[i].cnt >= MaxCollisions) {
      on_warning("hashIndTable[%d].cnt = %d",i,hashIndTable[i].cnt);
    } else {
      collision[hashIndTable[i].cnt]++;
      total += hashIndTable[i].cnt;
    }
    if (hashIndTable[i].cnt == 74) 
      printBuckets(&hashIndTable[i]);
  }

  uint totalfromCollisions = 0;
  for (auto i = 0; i < MaxCollisions; i++) {
    totalfromCollisions += (collision[i] * i);
    printf ("%10u : %10u\n",i,collision[i]);
  }
  printf("total = %u, totalfromCollisions = %u\n",total,totalfromCollisions);

  return 0;
}
