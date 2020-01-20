#ifndef _EXC_SOCKET_H
#define _EXC_SOCKET_H

/* ***NOTICE***
 * all received buffer sizes (as well as the parameter value in the function
 * call should not be less than 128 (128B buffer).
 */

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "exc_socket_def.h"

/* #ifdef __cplusplus */
/* extern "C" */
/* { */
/* #endif */

//int exc_init(uint8_t core, struct exc_init_attrs *attrs);
//eka_dev_t*  exc_init(); // calling eka_open_dev() : Initializing the HW
int exc_init(uint8_t unused_core, struct exc_init_attrs *unused_attrs);

//void exc_destroy(uint8_t core);
void exc_destroy(uint8_t core);           //Obsolete function. Doing nothing

//int exc_socket(uint8_t core, int domain, int type, int protocol);
int exc_socket(uint8_t core, int domain, int type, int protocol);

//session_id_t exc_connect(int socket, const struct sockaddr *address, socklen_t addrlen);
uint16_t exc_connect(int socket, const struct sockaddr *dst, socklen_t addrlen);

//ssize_t exc_send(session_id_t global_session_id, void *buffer, size_t size);
ssize_t exc_send(uint16_t sess_id, void *buffer,  size_t size);/* mutexed to handle single session at a time */

//ssize_t exc_recv(session_id_t global_session_id, void *buffer, size_t size);
ssize_t exc_recv(uint16_t sess_id, void *buffer, size_t size);//mutexed to handle single session at a time

//int exc_close(session_id_t global_session_id);
int exc_close(uint16_t global_session_id);

int exc_check_session_for_state_and_data(int global_session_id); // Obsolete

/* #ifdef __cplusplus */
/* } */
/* #endif */

#endif //_EXC_SOCKET_H
