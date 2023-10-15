#ifndef _EKA_PRINT_STRUCTS_H_
#define _EKA_PRINT_STRUCTS_H_

void printHwBookParams(hw_book_params_t* _ptr);

#define EKA__PRINT_HW_BOOK_PARAMS(type, name) \
  printOneField(#name,#type,sizeof(_ptr->name),_ptr->name);

void printOneField (const char* fieldName, const char* origType, uint fieldSize, depth_entry_params_array fieldVal);
void printOneField (const char* fieldName, const char* origType, uint fieldSize, depth_params_t fieldVal);
void printOneField (const char* fieldName, const char* origType, uint fieldSize, depth_entry_params_t fieldVal);
void printOneField (const char* fieldName, const char* origType, uint fieldSize, tob_params_t fieldVal);

void printOneField (const char* fieldName, const char* origType, uint fieldSize, uint64_t fieldVal) {
  printf ("%s = %ju,",fieldName, fieldVal);
}

void printOneField (const char* fieldName, const char* origType, uint fieldSize, uint32_t fieldVal) {
  printf ("%s = %u,",fieldName, fieldVal);
}

void printOneField (const char* fieldName, const char* origType, uint fieldSize, uint16_t fieldVal) {
  printf ("%s = %u,",fieldName, fieldVal);
}

void printOneField (const char* fieldName, const char* origType, uint fieldSize, const char* fieldVal) {
  printf ("%s = %s,",fieldName, fieldVal);
}

void printOneField (const char* fieldName, const char* origType, uint fieldSize, EkaTobPriceAscii fieldVal) {
  printf ("%s = %s,",fieldName, fieldVal);
}

void printOneField (const char* fieldName, const char* origType, uint fieldSize, tob_params_t fieldVal) {
  printf ("tob_params_t: \n\t");
  tob_params_t* _ptr = &fieldVal;
  tob_params_t__ITER( EKA__PRINT_HW_BOOK_PARAMS )
  printf("\n");
}

void printOneField (const char* fieldName, const char* origType, uint fieldSize, depth_entry_params_t fieldVal) {
  printf ("depth_entry_params_t: \n");
  depth_entry_params_t* _ptr = &fieldVal;
  depth_entry_params_t__ITER( EKA__PRINT_HW_BOOK_PARAMS )
  printf("\n");
}


void printOneField (const char* fieldName, const char* origType, uint fieldSize, depth_params_t fieldVal) {
  printf ("depth_params_t: \n");

  depth_params_t* _ptr = &fieldVal;
  depth_params_t__ITER( EKA__PRINT_HW_BOOK_PARAMS )
  printf("\n");
}

void printOneField (const char* fieldName, const char* origType, uint fieldSize, depth_entry_params_array fieldVal) {
  printf ("depth_entry_params_array: \n");
  for (uint i = 0; i < sizeof(depth_entry_params_array)/sizeof(depth_entry_params_t); i++) {
    depth_entry_params_t* _ptr = &fieldVal[i];
    printf ("\tdepth_entry_params_t[%u]: ",i);
    depth_entry_params_t__ITER( EKA__PRINT_HW_BOOK_PARAMS )
    printf("\n");
  }
  printf("\n");
}

void printHwBookParams(hw_book_params_t* _ptr) {
  printf ("hw_book_params_t: \n");
  hw_book_params_t__ITER( EKA__PRINT_HW_BOOK_PARAMS )
  printf("\n");
}

#endif
