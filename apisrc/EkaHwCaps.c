
#include "EkaDev.h"
#include "EkaHwExpectedVersion.h"
#include "EkaHwCaps.h"
#include "EkaEpm.h"
#include "smartnic.h"

#include "ctls.h"
#include "eka.h"
#include <sys/ioctl.h>
#include <stdio.h>


EkaHwCaps::EkaHwCaps(EkaDev* _dev) {
  dev = _dev;

  SN_DeviceId DeviceId = SN_OpenDevice(NULL, NULL);
  if (DeviceId == NULL) on_error("failed on SN_OpenDevice");

  uint words2read = sizeof(hwCaps) / 8 + !!(sizeof(hwCaps) % 8);
  uint64_t srcAddr = HwCapabilitiesAddr / 8;
  uint64_t* dstAddr = (uint64_t*)&hwCaps;
  for (uint w = 0; w < words2read; w++)
    SN_ReadUserLogicRegister(DeviceId, srcAddr++, dstAddr++);

  int fd = SN_GetFileDescriptor(DeviceId);
  eka_ioctl_t __attribute__ ((aligned(0x1000))) state = {};
  state.cmd = EKA_VERSION;
  
  auto releaseStr = new char[EKA_RELEASE_STRING_LEN];
  if (releaseStr == NULL) on_error("releaseStr == NULL");
  memset(releaseStr,0,EKA_RELEASE_STRING_LEN);
  state.paramA = (uint64_t)releaseStr;


  auto buildTimeStr = new char[EKA_BUILD_TIME_STRING_LEN];
  if (buildTimeStr == NULL) on_error("buildTimeStr == NULL");
  memset(buildTimeStr,0,EKA_BUILD_TIME_STRING_LEN);
  state.paramB = (uint64_t)buildTimeStr;
  
  state.paramC = (uint64_t)(new uint64_t);
  if (state.paramC == 0) on_error("state.paramC == NULL");
  
  ioctl(fd,SMARTNIC_EKALINE_DATA,&state);
  SN_CloseDevice(DeviceId);

  snDriverVerNum = *(uint64_t*)state.paramC;

  print2buf();
  idx += sprintf(&buf[idx],"Ekaline SN Driver Version\t\t= %ju\n",snDriverVerNum);
  idx += sprintf(&buf[idx],"EKALINE2 LIB GIT:\t\t\t= 0x%s\n",LIBEKA_GIT_VER);
  idx += sprintf(&buf[idx],"EKALINE2 LIB BUILD TIME:\t\t= %s @ %s\n",__DATE__,__TIME__);

  idx += sprintf(&buf[idx],"EKA_SN_DRIVER_BUILD_TIME:\t\t= %s\n",  (char*)state.paramA);
  idx += sprintf(&buf[idx],"EKA_SN_DRIVER_RELEASE_NOTE:\t\t= %s\n",(char*)state.paramB);
  //  idx += sprintf(&buf[idx],"EKA_RELEASE:\t\t\t\t= %ju\n",          *(uint64_t*)state.paramC);
  if (idx > bufSize) on_error("idx %u > bufSize %u",idx,bufSize);
}

void EkaHwCaps::print2buf() {
  idx += sprintf(&buf[idx],"\n");
  idx += sprintf(&buf[idx],"hwCaps.core.bitmap_md_cores\t\t= 0x%jx\n",     (uint64_t)(hwCaps.core.bitmap_md_cores));  
  idx += sprintf(&buf[idx],"hwCaps.core.bitmap_tcp_cores\t\t= 0x%jx\n",    (uint64_t)(hwCaps.core.bitmap_tcp_cores));  
  idx += sprintf(&buf[idx],"hwCaps.core.tcp_sessions_percore\t= %ju\n",    (uint64_t)(hwCaps.core.tcp_sessions_percore));
  idx += sprintf(&buf[idx],"hwCaps.epm.max_threads\t\t\t= %ju\n",          (uint64_t)(hwCaps.epm.max_threads));
  idx += sprintf(&buf[idx],"hwCaps.epm.heap_total_bytes\t\t= %ju\n",       (uint64_t)(hwCaps.epm.heap_total_bytes));
  idx += sprintf(&buf[idx],"hwCaps.epm.data_template_total_bytes\t= %ju\n",(uint64_t)(hwCaps.epm.data_template_total_bytes));
  idx += sprintf(&buf[idx],"hwCaps.epm.tcpcs_numof_templates\t= %ju\n",    (uint64_t)(hwCaps.epm.tcpcs_numof_templates));
  idx += sprintf(&buf[idx],"hwCaps.epm.numof_actions\t\t= %ju\n",          (uint64_t)(hwCaps.epm.numof_actions));
  idx += sprintf(&buf[idx],"hwCaps.epm.numof_regions\t\t= %ju\n",          (uint64_t)(hwCaps.epm.numof_regions));  
  idx += sprintf(&buf[idx],"hwCaps.entity.numof_entities\t\t= %ju\n",      (uint64_t)(hwCaps.entity.numof_entities));
  idx += sprintf(&buf[idx],"hwCaps.scratchpad.size\t\t\t= %ju\n",          (uint64_t)(hwCaps.scratchpad.size));  
  idx += sprintf(&buf[idx],"hwCaps.version.hwcaps\t\t\t= %ju\n",           (uint64_t)(hwCaps.version.hwcaps));
  idx += sprintf(&buf[idx],"hwCaps.version.strategy\t\t\t= %ju\n",         (uint64_t)(hwCaps.version.strategy));
  idx += sprintf(&buf[idx],"hwCaps.version.parser\t\t\t= %ju (%s)\n",      (uint64_t)(hwCaps.version.parser),EKA_FEED2STRING (hwCaps.version.parser));
  idx += sprintf(&buf[idx],"hwCaps.version.hwparser\t\t\t= %ju\n",          (uint64_t)(hwCaps.version.hwparser));
  idx += sprintf(&buf[idx],"hwCaps.version.sniffer\t\t\t= %ju\n",          (uint64_t)(hwCaps.version.sniffer));
  idx += sprintf(&buf[idx],"hwCaps.version.dma\t\t\t= %ju\n",              (uint64_t)(hwCaps.version.dma));
  idx += sprintf(&buf[idx],"hwCaps.version.epm\t\t\t= %ju\n",              (uint64_t)(hwCaps.version.epm));
  idx += sprintf(&buf[idx],"hwCaps.version.build_seed\t\t= %jx\n",         (uint64_t)(hwCaps.version.build_seed));
  idx += sprintf(&buf[idx],"hwCaps.version.build_date\t\t= %02jx/%02jx/20%02jx %02jx:%02jx\n",
		 (uint64_t)(hwCaps.version.build_date_day),
		 (uint64_t)(hwCaps.version.build_date_month),
		 (uint64_t)(hwCaps.version.build_date_year),
		 (uint64_t)(hwCaps.version.build_time_min),
		 (uint64_t)(hwCaps.version.build_time_sec));
  idx += sprintf(&buf[idx],"hwCaps.version.ekaline_git\t\t= 0x%08jx\n",    (uint64_t)(hwCaps.version.ekaline_git));
  idx += sprintf(&buf[idx],"hwCaps.version.silicom_git\t\t= 0x%08jx\n",    (uint64_t)(hwCaps.version.silicom_git));
  if (idx > bufSize) on_error("idx %u > bufSize %u",idx,bufSize);
}

void EkaHwCaps::print() {
  if (dev == NULL) on_error("dev == NULL");
  EKA_LOG("%s",buf);
}

void EkaHwCaps::printStdout() {
  printf ("%s",buf);
}


bool EkaHwCaps::check() {
 if (hwCaps.version.hwparser != EKA_EXPECTED_HWPARSER_VERSION) 
    on_error("hwCaps.version.hwparser 0x%x != EKA_EXPECTED_HWPARSER_VERSION 0x%x",
	     hwCaps.version.hwparser,EKA_EXPECTED_HWPARSER_VERSION);
  
  if (hwCaps.version.epm != EKA_EXPECTED_EPM_VERSION) 
    on_error("hwCaps.version.epm %x != EKA_EXPECTED_EPM_VERSION %x",
	     hwCaps.version.epm,EKA_EXPECTED_EPM_VERSION);

  if (hwCaps.version.dma != EKA_EXPECTED_DMA_VERSION) 
    on_error("hwCaps.version.dma %x != EKA_EXPECTED_DMA_VERSION %x",
	     hwCaps.version.dma,EKA_EXPECTED_DMA_VERSION);

  if (snDriverVerNum != 0 && snDriverVerNum != EKA_EXPECTED_SN_DRIVER_VERSION)
    on_error("snDriverVerNum 0x%jx != EKA_EXPECTED_SN_DRIVER_VERSION 0x%x",
	     snDriverVerNum,EKA_EXPECTED_SN_DRIVER_VERSION);

  if (hwCaps.core.tcp_sessions_percore < EKA_MAX_TCP_SESSIONS_PER_CORE)
    on_error("hwCaps.core.tcp_sessions_percore %d < EKA_MAX_TCP_SESSIONS_PER_CORE %d",
	     hwCaps.core.tcp_sessions_percore, EKA_MAX_TCP_SESSIONS_PER_CORE);
    
  if (hwCaps.epm.max_threads < EkaDev::MAX_CTX_THREADS)
    on_error("hwCaps.epm.max_threads %d < EkaDev::MAX_CTX_THREADS %d",
	     hwCaps.epm.max_threads, EkaDev::MAX_CTX_THREADS);
    
  if (hwCaps.epm.heap_total_bytes < EkaEpm::MaxHeap)
    on_error("hwCaps.epm.heap_total_bytes %d < EkaEpm::MaxHeap %ju",
	     hwCaps.epm.heap_total_bytes, EkaEpm::MaxHeap);
    
  if (hwCaps.epm.numof_actions < EkaEpm::MaxActions)
    on_error("hwCaps.epm.numof_actions %d < EkaEpm::MaxActions %d",
	     hwCaps.epm.numof_actions, EkaEpm::MaxActions);
  
    
  return true;
}
