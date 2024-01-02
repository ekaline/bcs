#ifndef _EKA_HW_EXPECTED_VERSION_H_
#define _EKA_HW_EXPECTED_VERSION_H_

// Generic EHP based P4 HW Parser
// #define EKA_EXPECTED_GENERIC_PARSER_VERSION 0x1
//#define EKA_EXPECTED_GENERIC_PARSER_VERSION 0x2 // multi
#define EKA_EXPECTED_GENERIC_PARSER_VERSION 0x3 // bc

// CME Fast Cancel HW not generic Parser
#define EKA_EXPECTED_NONGENERIC_PARSER_VERSION 0x9 // cme

// #define EKA_EXPECTED_EPM_VERSION       0x7
// #define EKA_EXPECTED_EPM_VERSION 0x8
// #define EKA_EXPECTED_EPM_VERSION 0x9
#define EKA_EXPECTED_EPM_VERSION 0xa
// #define EKA_EXPECTED_DMA_VERSION 0x3

// port replication DMA ver = 4
#define EKA_EXPECTED_DMA_VERSION 0x4

#define EKA_EXPECTED_SNIFFER_VERSION 0x3
#define EKA_EXPECTED_SN_DRIVER_VERSION 0x5

// Generic P4 Strategy
// #define EKA_EXPECTED_EFC_STRATEGY      0x42

// CME Fast Cancel Strategy
// #define EKA_EXPECTED_EFC_STRATEGY      0x43

// P4 + FastCancel
// #define EKA_EXPECTED_EFC_STRATEGY      128 //original
// #define EKA_EXPECTED_EFC_STRATEGY      129 //new arm
// #define EKA_EXPECTED_EFC_STRATEGY      130 //fast sweep
// strategy #define EKA_EXPECTED_EFC_STRATEGY      131 //
// efcSwKeepAliveSend()

// #define EKA_EXPECTED_EFC_STRATEGY 132 // Tcp sess ctx in
// SRAM
// #define EKA_EXPECTED_EFC_STRATEGY 133 // QED
// #define EKA_EXPECTED_EFC_STRATEGY 134 // multi
//#define EKA_EXPECTED_EFC_STRATEGY 135 // {core,mcgroup}->action
#define EKA_EXPECTED_EFC_STRATEGY 136 // bc

#define EKA_EXPECTED_PORT_MIRRORING 2
#define EKA_EXPECTED_SEC_CTX_READ_BY_HASH 3

#endif
