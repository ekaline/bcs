#include "EkaDev.h"
#include "EkaHwExpectedVersion.h"
#include "EkaHwCaps.h"
#include "EkaEpm.h"
#include "smartnic.h"

#include "ctls.h"
#include "eka.h"
#include <sys/ioctl.h>


EkaHwCaps::EkaHwCaps(EkaDev* _dev) {
  dev = _dev;
  if (dev != NULL) {
    copyHw2Buf(dev,&hwCaps,HwCapabilitiesAddr,sizeof(hwCaps));
    return;
  } else {
    // Stand alone run
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
    state.nif_num = 0;
    state.session_num = 0;
    ioctl(fd,SMARTNIC_EKALINE_DATA,&state);
    
    strcpy(snDriverBuildTime,  state.eka_version);
    strcpy(snDriverEkaRelease, state.eka_release);

    SN_CloseDevice(DeviceId);
  }
}

void EkaHwCaps::print() {
  EKA_LOG("hwCaps.core.bitmap_md_cores\t\t= 0x%jx",     (uint64_t)(hwCaps.core.bitmap_md_cores));
  EKA_LOG("hwCaps.core.bitmap_tcp_cores\t\t= 0x%jx",    (uint64_t)(hwCaps.core.bitmap_tcp_cores));
  EKA_LOG("hwCaps.core.tcp_sessions_percore\t\t= %ju",    (uint64_t)(hwCaps.core.tcp_sessions_percore));
  EKA_LOG("hwCaps.epm.max_threads\t\t\t= %ju",            (uint64_t)(hwCaps.epm.max_threads));
  EKA_LOG("hwCaps.epm.heap_total_bytes\t\t= %ju",       (uint64_t)(hwCaps.epm.heap_total_bytes));
  EKA_LOG("hwCaps.epm.data_template_total_bytes\t= %ju",(uint64_t)(hwCaps.epm.data_template_total_bytes));
  EKA_LOG("hwCaps.epm.tcpcs_numof_templates\t\t= %ju",    (uint64_t)(hwCaps.epm.tcpcs_numof_templates));
  EKA_LOG("hwCaps.epm.numof_actions\t\t\t= %ju",          (uint64_t)(hwCaps.epm.numof_actions));

  EKA_LOG("hwCaps.entity.numof_entities\t\t= %ju",          (uint64_t)(hwCaps.entity.numof_entities));
  EKA_LOG("hwCaps.scratchpad.size\t\t\t= %ju",          (uint64_t)(hwCaps.scratchpad.size));

  EKA_LOG("hwCaps.version.hwcaps\t\t\t= %ju",           (uint64_t)(hwCaps.version.hwcaps));
  EKA_LOG("hwCaps.version.strategy\t\t\t= %ju",           (uint64_t)(hwCaps.version.strategy));
  EKA_LOG("hwCaps.version.parser\t\t\t= %ju (%s)",           (uint64_t)(hwCaps.version.parser),EKA_FEED2STRING (hwCaps.version.parser));
  EKA_LOG("hwCaps.version.sniffer\t\t\t= %ju",              (uint64_t)(hwCaps.version.sniffer));
  EKA_LOG("hwCaps.version.dma\t\t\t= %ju",              (uint64_t)(hwCaps.version.dma));
  EKA_LOG("hwCaps.version.epm\t\t\t= %ju",              (uint64_t)(hwCaps.version.epm));
  EKA_LOG("hwCaps.version.build_seed\t\t\t= %jx",         (uint64_t)(hwCaps.version.build_seed));
  EKA_LOG("hwCaps.version.build_date\t\t\t= %02jx/%02jx/20%02jx %02jx:%02jx",
	  (uint64_t)(hwCaps.version.build_date_day),
	  (uint64_t)(hwCaps.version.build_date_month),
	  (uint64_t)(hwCaps.version.build_date_year),
	  (uint64_t)(hwCaps.version.build_time_min),
	  (uint64_t)(hwCaps.version.build_time_sec));
  EKA_LOG("hwCaps.version.ekaline_git\t\t= 0x%08jx",    (uint64_t)(hwCaps.version.ekaline_git));
  EKA_LOG("hwCaps.version.silicom_git\t\t= 0x%08jx",    (uint64_t)(hwCaps.version.silicom_git));
  EKA_LOG("API library GIT           \t\t= 0x%s",        LIBEKA_GIT_VER);

}
#define _printN(...) {printf(__VA_ARGS__); printf("\n"); }

void EkaHwCaps::printStdout() {

  _printN("hwCaps.core.bitmap_md_cores\t\t= 0x%jx",     (uint64_t)(hwCaps.core.bitmap_md_cores));
  _printN("hwCaps.core.bitmap_tcp_cores\t\t= 0x%jx",    (uint64_t)(hwCaps.core.bitmap_tcp_cores));
  _printN("hwCaps.core.tcp_sessions_percore\t= %ju",    (uint64_t)(hwCaps.core.tcp_sessions_percore));
  _printN("hwCaps.epm.max_threads\t\t\t= %ju",            (uint64_t)(hwCaps.epm.max_threads));
  _printN("hwCaps.epm.heap_total_bytes\t\t= %ju",       (uint64_t)(hwCaps.epm.heap_total_bytes));
  _printN("hwCaps.epm.data_template_total_bytes\t= %ju",(uint64_t)(hwCaps.epm.data_template_total_bytes));
  _printN("hwCaps.epm.tcpcs_numof_templates\t= %ju",    (uint64_t)(hwCaps.epm.tcpcs_numof_templates));
  _printN("hwCaps.epm.numof_actions\t\t= %ju",          (uint64_t)(hwCaps.epm.numof_actions));

  _printN("hwCaps.entity.numof_entities\t\t= %ju",    (uint64_t)(hwCaps.entity.numof_entities));
  _printN("hwCaps.scratchpad.size\t\t\t= %ju",          (uint64_t)(hwCaps.scratchpad.size));

  _printN("hwCaps.version.hwcaps\t\t\t= %ju",           (uint64_t)(hwCaps.version.hwcaps));
  _printN("hwCaps.version.strategy\t\t\t= %ju",           (uint64_t)(hwCaps.version.strategy));
  _printN("hwCaps.version.parser\t\t\t= %ju (%s)",           (uint64_t)(hwCaps.version.parser),EKA_FEED2STRING (hwCaps.version.parser));
  _printN("hwCaps.version.sniffer\t\t\t= %ju",              (uint64_t)(hwCaps.version.sniffer));
  _printN("hwCaps.version.dma\t\t\t= %ju",              (uint64_t)(hwCaps.version.dma));
  _printN("hwCaps.version.epm\t\t\t= %ju",              (uint64_t)(hwCaps.version.epm));
  _printN("hwCaps.version.build_seed\t\t= %ju",         (uint64_t)(hwCaps.version.build_seed));
  _printN("hwCaps.version.build_date\t\t= %02jx/%02jx/20%02jx %02jx:%02jx",
	  (uint64_t)(hwCaps.version.build_date_day),
	  (uint64_t)(hwCaps.version.build_date_month),
	  (uint64_t)(hwCaps.version.build_date_year),
	  (uint64_t)(hwCaps.version.build_time_min),
	  (uint64_t)(hwCaps.version.build_time_sec));
  _printN("hwCaps.version.ekaline_git\t\t= 0x%08jx",    (uint64_t)(hwCaps.version.ekaline_git));
  _printN("hwCaps.version.silicom_git\t\t= 0x%08jx",    (uint64_t)(hwCaps.version.silicom_git));
  _printN("API library GIT           \t\t= 0x%s",        LIBEKA_GIT_VER);
}
void EkaHwCaps::printDriverVer() {
  _printN("%s",snDriverBuildTime);
  _printN("%s",snDriverEkaRelease);
}
#undef _printN

bool EkaHwCaps::check() {
  if (hwCaps.version.epm != EKA_EXPECTED_EPM_VERSION) 
    on_error("hwCaps.version.epm %x != EKA_EXPECTED_EPM_VERSION %jx",
	     hwCaps.version.epm,EKA_EXPECTED_EPM_VERSION);

  if (hwCaps.version.dma != EKA_EXPECTED_DMA_VERSION) 
    on_error("hwCaps.version.dma %x != EKA_EXPECTED_DMA_VERSION %jx",
	     hwCaps.version.dma,EKA_EXPECTED_DMA_VERSION);

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

  if (hwCaps.scratchpad.size < SCRATCHPAD_SIZE)
    on_error("hwCaps.scratchpad.size %d < SCRATCHPAD_SIZE %d",
	     hwCaps.scratchpad.size, SCRATCHPAD_SIZE);

  return true;
}
