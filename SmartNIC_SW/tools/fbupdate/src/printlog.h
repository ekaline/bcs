/* 
 * File:   prn.h
 * Author: rdj
 *
 * Created on September 28, 2010, 8:42 PM
 */

#ifndef _PRN_H
#define _PRN_H

#include <stdio.h>

#include "FL_FlashLibrary.h"

#define LOG_ACP         (1<<18)
#define LOG_FILE        (1<<17)
#define LOG_SCR         (1<<16)


#define LEVEL_ALL       FL_LOG_LEVEL_ALWAYS
#define LEVEL_FATAL     FL_LOG_LEVEL_FATAL
#define LEVEL_ERROR     FL_LOG_LEVEL_ERROR
#define LEVEL_WARNING   FL_LOG_LEVEL_WARNING
#define LEVEL_INFO      FL_LOG_LEVEL_INFO
#define LEVEL_DEBUG     FL_LOG_LEVEL_DEBUG
#define LEVEL_TRACE     FL_LOG_LEVEL_TRACE

extern FL_LogLevel * pLogLevel;

extern FILE *logFile;
extern FILE *acpFile;
extern FILE *pScreen;

void prn( uint32_t mask, const char* format, ... ) __attribute__((format(printf, 2, 3)));
void SilicomInternalInitialize(FL_FlashLibraryContext * pFlashLibraryContext);

#endif  /* _PRN_H */

