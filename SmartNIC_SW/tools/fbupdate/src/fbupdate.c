#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <libgen.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "FL_FlashLibrary.h"
#include "fpgaregs.h"
#include "flash.h"
#include "sysmon.h"
#include "printlog.h"
#include "actel.h"

#ifdef WIN32
#include "win32.h"
#endif

static int
do_update(FL_FlashLibraryContext *pFlashLibraryContext);

// External arguments
char *logDir = NULL;

int main(int argc, char **argv) {
  int ret = 0;
  int argp = 0; // Parse counter

  FL_FlashLibraryContext flashLibraryContext;
  FL_Error errorCode;
  extern FL_Error AddSharedCommands(FL_FlashLibraryContext *
                                    pFlashLibraryContext);
  const FL_LogContext *pLogContext =
      &flashLibraryContext.LogContext;

  FL_InitializeFlashLibraryContext(
      &flashLibraryContext, SilicomInternalInitialize);
  FL_ExitOnError(pLogContext,
                 AddSharedCommands(&flashLibraryContext));
  FL_Parameters *pParameters =
      &flashLibraryContext.Parameters;

  // Parse arguments
  argp++; // Skip function name
  while (argc > argp) {
    if (strcmp("-d", argv[argp]) ==
        0) { // ele: conflict between fbupdate and testfpga:
             // here -d means device, in testfpga -d means
             // logDir!
      argp++;
      if (argc > argp) {
        strncpy(pParameters->DeviceName, argv[argp],
                sizeof(pParameters->DeviceName));
      }
    } else if (strcmp("--version", argv[argp]) == 0) {
      printf("fbupdate version: %u.%u.%u.%u  %u\n",
             FL_VERSION_MAJOR, FL_VERSION_MINOR,
             FL_VERSION_MAINTENANCE, __BUILD_NUMBER,
             __BUILD_DATE);
      return 0;
    } else if ((strcmp("-h", argv[argp]) == 0) ||
               (strcmp("--help", argv[argp]) == 0)) {
      printf(
          "Valid arguments are:\n"
          "--flash <file>         : Erase, flash, verify "
          "primary image.\n"
          "--flashsafe <file>     : Erase, flash, verify "
          "of failsafe image.\n"
          "--verify <file>        : Verify primary image.\n"
          "--verifysafe <file>    : Verify failsafe "
          "image.\n"
          "--erase                : Erase primary image.\n"
          "--erasesafe            : Erase failsafe image.\n"
          "--device <device>      : Set devicehandle. "
          "Default /dev/fiberblaze.\n"
          "--force-programming    : Skip device and userid "
          "check.\n"
          "--pcb                  : Print PCB version.\n"
          "--pcibootmode [<mode>] : Read or write PCI boot "
          "mode. This guaranties the card can enumerate in "
          "a wide range of server configurations.\n"
          "  sequential:          : Default. Guaranties at "
          "least one endpoint that gives access to the "
          "card even if slot does not support "
          "bifurcation.\n"
          "  simultaneous:        : Use if only 1 endpoint "
          "is discovered in sequential mode even thought "
          "bifurcation is enabled.\n"
          "  single:              : Use if only 1 endpoint "
          "is required. Bifurcation not considered. The "
          "througput is limited by one endpoint.\n"
          "--debug <level 0 -> 7> : Specify a debug output "
          "level.\n"
          "--version              : print application "
          "version.\n"
          "-c <command>\n"
          "");
      exit(0);
    } else {
      if (FL_ParseCommandLineArguments(
              argc, argv, &argp, &flashLibraryContext) !=
          FL_SUCCESS) {
        exit(EXIT_FAILURE);
      }
    }
    argp++;
  }
  if (logDir == NULL)
    logDir = "/log";

  // Print parameters to file
  argp = 0;
  prn(LOG_FILE | LEVEL_INFO, "Parameters:");
  while (argc > argp) {
    prn(LOG_FILE | LEVEL_INFO, " %s", argv[argp++]);
  }
  prn(LOG_FILE | LEVEL_INFO, "\n");

  FL_ExitOnError(pLogContext,
                 FL_OpenFlashLibrary(&flashLibraryContext));

  // Run externally defined command and exit
  if (pParameters->ExternalCommandPosition != 0) {
    ret = FL_ExecuteCommand(
        &flashLibraryContext,
        argc - pParameters->ExternalCommandPosition,
        argv + pParameters->ExternalCommandPosition);
  } else {
    ret = do_update(&flashLibraryContext);
  }

  if (logFile) {
    fclose(logFile);
  }

  errorCode = FL_CloseFlashLibrary(&flashLibraryContext);
  if (errorCode != FL_SUCCESS) {
    exit(EXIT_FAILURE);
  }

  return ret;
}

static int
do_update(FL_FlashLibraryContext *pFlashLibraryContext) {
  const FL_Parameters *pParameters =
      &pFlashLibraryContext->Parameters;
  const FL_FlashInfo *pFlashInfo =
      &pFlashLibraryContext->FlashInfo;
  const FL_FpgaInfo *pFpgaInfo =
      &pFlashLibraryContext->FpgaInfo;
  const FL_PCIeInfo *pPCIeInfo =
      &pFlashLibraryContext->PCIeInfo;
  int ret = 0;
  int error = 0;
  bool cardTypeHandled = false;

  /* initialize random seed: */
  srand(time(NULL));

  // Initialize these card types:
  switch (pFpgaInfo->CardType) {
  case FL_CARD_TYPE_ANCONA:
  case FL_CARD_TYPE_ATHEN:
  case FL_CARD_TYPE_ERCOLANO:
  case FL_CARD_TYPE_FIRENZE:
  case FL_CARD_TYPE_GENOA:
  case FL_CARD_TYPE_HERCULANEUM:
  case FL_CARD_TYPE_LATINA:
  case FL_CARD_TYPE_LIVIGNO:
  case FL_CARD_TYPE_LIVORNO:
  case FL_CARD_TYPE_LUCCA:
  case FL_CARD_TYPE_LUXG:
  case FL_CARD_TYPE_MANGO: // SF2
  case FL_CARD_TYPE_MILAN:
  case FL_CARD_TYPE_MONZA:
  case FL_CARD_TYPE_PADUA: // SF2
  case FL_CARD_TYPE_PISA:
  case FL_CARD_TYPE_TORINO:
    // actel_revision = ActelRevision(baseAddr);
    // prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Actel Revision
    // %llu.%llu.%llu.%llu\n", (actel_revision >> 24)&0xFF,
    // ((actel_revision >> 16) & 0xFF), ((actel_revision >>
    // 8) & 0xFF), (actel_revision & 0xFF));
    cardTypeHandled = true;
    break;
  case FL_CARD_TYPE_SAVONA:
  case FL_CARD_TYPE_TIVOLI:
    // prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Savona is
    // still treated as an Unknown or unhandled card type
    // %d, don't know how to initialize!",
    // pFpgaInfo->CardType);
    cardTypeHandled = true;
    break;
  case FL_CARD_TYPE_UNKNOWN:
  case FL_CARD_TYPE_RAW:
    break;
  case FL_CARD_TYPE_CARDIFF:
  case FL_CARD_TYPE_RIMINI:
    prn(LOG_SCR | LOG_FILE | LEVEL_FATAL,
        " *** Cardiff and Rimini cards not supported!\n");
    return EXIT_FAILURE;

    // TODO: remove these:
  case FL_CARD_TYPE_20XX:
  case FL_CARD_TYPE_BERGAMO:
  case FL_CARD_TYPE_COMO:
  case FL_CARD_TYPE_ESSEN:
  case FL_CARD_TYPE_MARSALA:
  case FL_CARD_TYPE_SILIC100:
    break;
  }

  if (!cardTypeHandled) {
    prn(LOG_SCR | LOG_FILE | LEVEL_FATAL,
        "Unknown or unhandled card type %d, don't know how "
        "to initialize!",
        pFpgaInfo->CardType);
  }

  prn(LOG_SCR | LOG_FILE | LEVEL_INFO,
      "FPGA version: %u.%u.%u.%u Type:0x%X Model:0x%X "
      "Date:%s\n",
      pFpgaInfo->Major, pFpgaInfo->Minor, pFpgaInfo->Sub,
      pFpgaInfo->Build, pFpgaInfo->Type, pFpgaInfo->Model,
      pFpgaInfo->BuildTimeString);

  prn(LOG_SCR | LEVEL_DEBUG, "FPGA hg info: %lu %012llx\n",
      pFpgaInfo->RawMercurialRevision >> 60,
      pFpgaInfo->RawMercurialRevision & 0xFFFFFFFFFFFFULL);

  // Dot not check status bits 0xc00, not set for virtex6
  // images
  if ((pPCIeInfo->Status & 0xFFFFFFFF00000000ULL) ==
      0xDEADBEEF00000000ULL) {
    prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG,
        "PCIe gen%u, x%u    [ OK ]\n",
        pPCIeInfo->EndPoints[0].Generation,
        pPCIeInfo->EndPoints[0].Width);
  } else if (pPCIeInfo->Status == 0xDEADBEEFDEADBEEFULL) {
    prn(LOG_SCR | LOG_FILE | LEVEL_ERROR,
        "PCIe [ FAILED ]\n");
  } else {
    prn(LOG_SCR | LOG_FILE | LEVEL_ERROR,
        "PCIe gen           [ FAILED ]\n");
    // return -99;
  }

  if (pParameters->UseForceProgramming) {
    pFlashLibraryContext->FpgaInfo.PartNumber = "";
    pFlashLibraryContext->Parameters
        .ExpectedBitFileFpgaType = -1;
  } else {
    pFlashLibraryContext->Parameters
        .ExpectedBitFileFpgaType = pFpgaInfo->Type;
  }

  if ((ret = FL_VerifyBitFiles(pFlashLibraryContext)) !=
      0) {
    return ret;
  }

  switch (pFlashInfo->Type) {
  case FL_FLASH_TYPE_20XX:
    error |= FlashFileFpgaTypes_2010_2015_2022(
        pFlashLibraryContext);
    break;

  case FL_FLASH_TYPE_PARALLEL_SPANSION:
  case FL_FLASH_TYPE_PARALLEL_MICRON:
  case FL_FLASH_TYPE_MICRON_NOR:
    if (FL_FlashEraseAndWriteAndVerify(
            pFlashLibraryContext) != FL_SUCCESS) {
      return -1;
    }
    break;

  default:
    error |= HandleCommonFpgaTypes(pFlashLibraryContext);
    return error;
  }

  return ret;
}
