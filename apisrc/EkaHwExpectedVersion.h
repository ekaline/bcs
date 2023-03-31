#ifndef _EKA_HW_EXPECTED_VERSION_H_
#define _EKA_HW_EXPECTED_VERSION_H_

// Generic EHP based P4 HW Parser
#define EKA_EXPECTED_GENERIC_PARSER_VERSION       0x1

// CME Fast Cancel HW not generic Parser
#define EKA_EXPECTED_NONGENERIC_PARSER_VERSION       0x9 //cme

#define EKA_EXPECTED_EPM_VERSION       0x7
#define EKA_EXPECTED_DMA_VERSION       0x3
#define EKA_EXPECTED_SNIFFER_VERSION   0x3
#define EKA_EXPECTED_SN_DRIVER_VERSION 0x5

// Generic P4 Strategy
//#define EKA_EXPECTED_EFC_STRATEGY      0x42

// CME Fast Cancel Strategy
//#define EKA_EXPECTED_EFC_STRATEGY      0x43

// P4 + FastCancel
//#define EKA_EXPECTED_EFC_STRATEGY      128 //original 
//#define EKA_EXPECTED_EFC_STRATEGY      129 //new arm
//#define EKA_EXPECTED_EFC_STRATEGY      130 //fast sweep strategy
#define EKA_EXPECTED_EFC_STRATEGY      131 // efcSwKeepAliveSend()

#endif
