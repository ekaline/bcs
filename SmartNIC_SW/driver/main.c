/*
 ***************************************************************************
 *
 * Copyright (c) 2008-2019, Silicom Denmark A/S
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 *or without modification, are permitted provided that the
 *following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *copyright notice, this list of conditions and the
 *following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the
 *above copyright notice, this list of conditions and the
 *following disclaimer in the documentation and/or other
 *materials provided with the distribution.
 *
 * 3. Neither the name of the Silicom nor the names of its
 * contributors may be used to endorse or promote products
 *derived from this software without specific prior written
 *permission.
 *
 * 4. This software may only be redistributed and used in
 *connection with a Silicom network adapter product.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/pci.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/aer.h>
#include <linux/rtnetlink.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/sched.h>

// #include <asm/cacheflush.h> // added by ekaline
// #include <asm/mtrr.h>       // added by ekaline

#include "main.h"
#include "fpga.h"
#include "connect.h"
#include "channel.h"
#include "oob_chan.h"
#include "recv.h"
#include "igmp.h"
#include "ndev.h"
#include "intr.h"
#include "custom.h"
#include "stat.h"
#include "recv.h"
#include "acl.h"
#include "util.h"
#include "fpgadeviceids.h"
#include "eka.h" // added by ekaline

#ifdef DEBUG
#define DEBUG_BUILD " (DEBUG)"
#else
#define DEBUG_BUILD ""
#endif
#include "../include/release_version.h"

#ifdef DUMP
#include "dump.c"
#endif

#define DRIVER_AUTHOR "Esa Leskinen <ele@silicom.dk>"
#define DRIVER_DESC "Driver for Silicom " PRODUCT " cards"

// Driver load parameters:
static int input_delay = 0; ///< Quants of 6.4ns
int pad_frames = 0; ///< Per default do not pad frames. If
                    ///< different from zero then pad Tx
                    ///< frames to a minimum of 60 bytes.
// int pad_frames = 1; ///< Per default DO PAD frames. If
// different from zero then pad Tx frames to a minimum of 60
// bytes.
LogMask_t logmask =
    LOG_DETECT_COMMAND_LINE_OVERRIDE; ///< Logging mask to
                                      ///< enable logging of
                                      ///< different
                                      ///< categories.
unsigned int max_etlm_channels =
    SC_MAX_TCP_CHANNELS; ///< Maximum number of ETLM
                         ///< channels to use.
int support_ul_devices =
    false; ///< Support for /dev/ul devices
int max_lanes_per_device =
    8; ///< Number of lanes used for lane_base calculations
       ///< if probe calls leave holes in dev_data.
char *device_mapping = ""; ///< Mapping of minor device
                           ///< numbers to PCIe device names
char *interface_name =
    "feth"; /**< Base name of network interfaces. Default
               name is "feth". */
unsigned long bufsize =
    0; /**< Total size of all PRB buffers in megabytes. Zero
          value means don't use MMU even if present. */

module_param(
    input_delay, int,
    S_IRUGO |
        S_IWUSR); // S_IRUGO = User/Group/Owner (=all!) can
                  // read S_IWUSR = root can write
module_param(pad_frames, int, S_IRUGO | S_IWUSR);
module_param(logmask, ulong, S_IRUGO | S_IWUSR);
module_param(max_etlm_channels, uint, S_IRUGO);
module_param(support_ul_devices, int, S_IRUGO);
module_param(max_lanes_per_device, int, S_IRUGO);
module_param(device_mapping, charp, S_IRUGO);
module_param(interface_name, charp, S_IRUGO);
module_param(bufsize, ulong, S_IRUGO);

MODULE_PARM_DESC(input_delay,
                 "Added/subtract fixed input time delay in "
                 "quants of 6.4ns - default value = 0");
MODULE_PARM_DESC(pad_frames,
                 "Enable frame padding for standard NIC "
                 "with zeroes to minimum size of 60 bytes: "
                 "0 = disabled (default), 1 = enabled");
MODULE_PARM_DESC(logmask,
                 "Set logging mask enabling logging of "
                 "different categories");
MODULE_PARM_DESC(max_etlm_channels,
                 "Maximum number of ETLM channels actually "
                 "used. Default is 64.");
MODULE_PARM_DESC(support_ul_devices,
                 "Support /dev/ul user logic devices. 0 = "
                 "disabled (default), 1 = enabled.");
MODULE_PARM_DESC(max_lanes_per_device,
                 "Number of lanes used for lane_base "
                 "calculations if probe calls leave holes "
                 "in device_mapping. Default is 8.");
MODULE_PARM_DESC(device_mapping,
                 "Mapping of minor device numbers to PCIe "
                 "device names.");
MODULE_PARM_DESC(interface_name,
                 "Base name of network interfaces. Default "
                 "name is \"feth\".");
MODULE_PARM_DESC(
    bufsize,
    "Total size of all PRB buffers in megabytes. Zero "
    "value means don't use MMU even if present.");

static unsigned int major = 0;
static int s_NumberOfCardsFound = 0;
static int s_NumberOfTestFpgaCardsFound = 0;

#define MAX_NUMBER_OF_CARDS 20

/**
 *  Array of all device contexts allocated in this driver.
 */
static struct {
  const char *pcieDeviceName; ///< PCIe device name
  device_context_t *pDevExt;  ///< Pointer to device context
  uint16_t
      nb_netdev_used_for_lane_base; ///< Number of NIC
                                    ///< interfaces if this
                                    ///< device context has
                                    ///< been already
                                    ///< initialized,
                                    ///< otherwise maximum
                                    ///< of 16 is used
} dev_data[MAX_NUMBER_OF_CARDS] = {{NULL, NULL}};

static DECLARE_WAIT_QUEUE_HEAD(sc_wait_queue);

/* Forward declaration */
static int sc_allocate_channel(struct file *f,
                               device_context_t *pDevExt,
                               int type, uint32_t chan,
                               uint32_t phy,
                               uint8_t enableDma);
static const char *getCardName(uint32_t deviceId);

extern int panicParse(device_context_t *, int forceDecode);

typedef struct {
  unsigned int minor;
} DummyDevice;

static device_context_t *GetDevExt(unsigned minor,
                                   const char *caller) {
  {
    device_context_t *pDevExt;

    if (minor >= 16) {
      if (minor < 144) {
        // This is a user logic device /dev/ul*.
        // Find the corresponding device context minor
        // number:
        minor = (minor - 16) /
                8; // minor is now device context minor
                   // number that handles the incoming
                   // /dev/ul* device minor.
      } else {
        printk(KERN_ERR MODULE_NAME
               "Device context for minor %u is not "
               "available: too many devices"
               ", maximum number of devices supported is "
               "%lu; caller %s!\n",
               minor, number_of_elements(dev_data), caller);
        return NULL;
      }
    }
    pDevExt = dev_data[minor].pDevExt;
    if (pDevExt != NULL) {
      return pDevExt;
    }
  }

  {
    DummyDevice dummyDevice;
    DummyDevice *pDevExt = &dummyDevice;
    pDevExt->minor = minor;
    PRINTK_ERR("Device context for minor %u is not "
               "available: not allocated: caller %s!\n",
               minor, caller);
  }

  return NULL;
}

/**
 *  Print into syslog all arguments but the first one which
 * is an int minor number.
 *
 *  @brief  Used in the case when only one card is present
 * and card identifying minor number is not needed, i.e.
 *          this function discards the minor number.
 */
int printMinusOne(const char *format, ...) {
  int numberOfCharacters;

  va_list arguments;
  va_start(arguments, format);
  va_arg(arguments,
         unsigned int); // Discard 1st argument which is
                        // unsigned int minor number

  numberOfCharacters = vprintk(format, arguments);

  va_end(arguments);

  return numberOfCharacters;
}

static void LogRevision(const device_context_t *pDevExt) {
  char msg[100];
  int length;

  fpga_revision_t revision = pDevExt->rev;
  uint32_t extendedBuildNumber =
      getFpgaExtendedBuildNumber(pDevExt);

  const char *fpgaTypeName =
      IsLeonbergFpga(pDevExt->rev.parsed.type);
  fpgaTypeName = fpgaTypeName == NULL
                     ? IsTestFpga(pDevExt->rev.parsed.type)
                     : fpgaTypeName;

  length =
      snprintf(msg, sizeof(msg),
               "   Hardware Model %d, FW type 0x%x (%s), "
               "FPGA version %d.%d.%d.",
               revision.parsed.model, revision.parsed.type,
               fpgaTypeName, revision.parsed.major,
               revision.parsed.minor, revision.parsed.sub);
  if (revision.parsed.build == 0 ||
      extendedBuildNumber == 0) {
    length += snprintf(&msg[length], sizeof(msg) - length,
                       "%d", revision.parsed.build);
  } else {
    length += snprintf(&msg[length], sizeof(msg) - length,
                       "%u", extendedBuildNumber);
  }

  if (revision.parsed.pldma == 1) {
    length += snprintf(&msg[length], sizeof(msg) - length,
                       ", PLDMA");
  }

  PRINTK("%s\n", msg);
}

/**
 *  Get full file path name from Linux struct file.
 *
 *  @param  f           Pointer to Linux struct file which
 * represents an open file.
 *  @param  name        File name buffer.
 *  @param  nameSize    Size of file name buffer.
 *
 *  @return             Pointer to full file path name.
 */
static const char *
get_file_name_from_file(struct file *f, char *name,
                        size_t nameSize) {
#if LINUX_VERSION_CODE >=                                  \
    KERNEL_VERSION(                                        \
        2, 6,                                              \
        38) // Checked at
            // http://elixir.free-electrons.com/linux/v2.6.38/ident/dentry_path_raw
  return dentry_path_raw(f->f_path.dentry, name, nameSize);
#else
#ifdef DEBUG
  // This might work on earlier kernel versions but has not
  // been tested
  char *pathName;
  struct path *path = &f->f_path;
  pathName = d_path(path, name, nameSize);
  if (IS_ERR(pathName)) {
    return "N/A"; // d_path failed!
  }
  return pathName;
#else
  return "N/A"; // Return this until above code has been
                // tested!
#endif
#endif
}

/**
 *  Get device file minor device number from Linux struct
 * file.
 *
 *  @param  f       Pointer to Linux struct file which
 * represents an open file.
 *
 *  @return         Device file minor device number.
 */
static inline unsigned int
get_minor_from_file(struct file *f) {
#if LINUX_VERSION_CODE <                                   \
    KERNEL_VERSION(                                        \
        3, 9,                                              \
        0) // Checked at
           // http://elixir.free-electrons.com/linux/v3.9/ident/file_inode
  return iminor((f)->f_dentry->d_inode);
#else
  return iminor(file_inode(f));
#endif
}

/**
 *  Get virtual address of the PCIe BAR (Base Address
 * Register).
 *
 *  @param  pdev    Linux PCIe device context.
 *  @param  bar     PCIe BAR identifier.
 *
 *  @return         PCIe BAR virtual address.
 */
static inline void *get_pci_bar(device_context_t *pDevExt,
                                struct pci_dev *pdev,
                                int bar) {
  // Hmmm, pci_iomap seems to be present already in
  // kernel 2.6.11 but does it work correctly? Should
  // probably always use pci_iomap but ioremap_nocache seems
  // to work too.
  // http://elixir.free-electrons.com/linux/v2.6.11/source/arch/alpha/kernel/pci.c#L532

  // fixed by ekaline
  if (bar == BAR_0) {
    // void *bar0_start = pci_iomap(pdev, bar, 0);
    void *bar0_start =
        ioremap(pci_resource_start(pdev, bar),
                pci_resource_len(pdev, bar));

    // ioremap has provided non - cached semantics by
    // default since the
    //  Linux 2.6 days, so remove the additional
    //  ioremap_nocache interface.

    pDevExt->eka_bar0_nocache_a = (uint64_t)ioremap(
        pci_resource_start(pdev, bar), EKA_WC_REGION_OFFS);

    pDevExt->ekaline_wc_addr = (uint64_t)ioremap_wc(
        pci_resource_start(pdev, bar) + EKA_WC_REGION_OFFS,
        EKA_WC_REGION_SIZE);

    pDevExt->eka_bar0_nocache_b = (uint64_t)ioremap(
        pci_resource_start(pdev, bar) + EKA_WC_REGION_OFFS +
            EKA_WC_REGION_SIZE,
        pci_resource_len(pdev, bar) - EKA_WC_REGION_OFFS -
            EKA_WC_REGION_SIZE);

    PRINTK("EKALINE DEBUG: WRITE COMBINING: "
           "bar0_start = 0x%llx + 0x%llx, "
           "eka_bar0_nocache_a = 0x%llx + 0x%llx, "
           "ekaline_wc_addr = 0x%llx  + 0x%llx, "
           "eka_bar0_nocache_b = 0x%llx + 0x%llx",
           (uint64_t)bar0_start,
           pci_resource_len(pdev, bar),
           pDevExt->eka_bar0_nocache_a, EKA_WC_REGION_OFFS,
           pDevExt->ekaline_wc_addr, EKA_WC_REGION_SIZE,
           pDevExt->eka_bar0_nocache_b,
           pci_resource_len(pdev, bar) -
               EKA_WC_REGION_OFFS - EKA_WC_REGION_SIZE);
    return bar0_start;
  } else {
    return pci_iomap(pdev, bar, 0);
  }
}

static device_context_t *
_get_sc_device_from_file(struct file *f,
                         const char *caller) {
  unsigned int minor = get_minor_from_file(f);

  device_context_t *pDevExt = GetDevExt(minor, caller);

  PRINTK_C(LOG(LOG_FILE_OPS) && LOG(LOG_DETAIL),
           "_get_sc_device_from_file minor=%u; caller %s\n",
           minor, caller);

  return pDevExt;
}

#define get_sc_device_from_file(f)                         \
  _get_sc_device_from_file(f, __func__)

/**
 *  This code try and set the MaxPayloadSize of PCIe
 * transfers to host
 */
static u16
getPCIeMaxPayloadSize(const device_context_t *pDevExt,
                      struct pci_dev *pdev) {
  u16 ctl, mps;
  int cap = pdev->pcie_cap;
  pci_read_config_word(pdev, cap + PCI_EXP_DEVCTL, &ctl);
  mps = 128 << ((ctl & PCI_EXP_DEVCTL_PAYLOAD) >> 5);
  PRINTK_C(LOG(LOG_DEBUG), "PCIe MaxPayloadSize %d bytes\n",
           mps);
  mps = 0x2 << 5;
  if ((ctl & PCI_EXP_DEVCTL_PAYLOAD) != mps) {
    int err;
    ctl &= ~PCI_EXP_DEVCTL_PAYLOAD;
    ctl |= mps;
    err = pci_write_config_word(pdev, cap + PCI_EXP_DEVCTL,
                                ctl);
    PRINTK("PCIe setting MaxPayloadSize err=%d\n", err);
  }

  pci_read_config_word(pdev, cap + PCI_EXP_DEVCTL, &ctl);
  mps = 128 << ((ctl & PCI_EXP_DEVCTL_PAYLOAD) >> 5);
  return mps;
}

static void
enableExtendedTags(const device_context_t *pDevExt,
                   struct pci_dev *pdev) {
  // Code taken from capture driver:
  // Enable entended tags if supported.
  // Read DevCap to see if we support Extended Tags
  u16 cap = pci_find_capability(pdev, PCI_CAP_ID_EXP);
  u32 pcie_reg;

  if (!cap) {
    PRINTK_ERR("Unable to find pcie register\n");
  } else if (pci_read_config_dword(
                 pdev, cap + PCI_EXP_DEVCAP, &pcie_reg)) {
    PRINTK_ERR("Unable to read pcie DEVCAP register\n");
  } else {
    if (LOG(LOG_PCIe | LOG_DETAIL)) {
      PRINTK("DEVCAP:0x%08X\n", pcie_reg);
    }

    if (pcie_reg & PCI_EXP_DEVCAP_EXT_TAG) {
      PRINTK("   PCIe Extended Tag Capable Device\n");
      if (pci_read_config_dword(pdev, cap + PCI_EXP_DEVCTL,
                                &pcie_reg)) {
        PRINTK_ERR("Unable to read pcie DEVCTL register\n");
      } else {
        PRINTK_C(LOG(LOG_PCIe | LOG_DETAIL),
                 "DEVCTL before:0x%08X\n", pcie_reg);

        pcie_reg |= PCI_EXP_DEVCTL_EXT_TAG;
        if (pci_write_config_dword(
                pdev, cap + PCI_EXP_DEVCTL, pcie_reg)) {
          PRINTK_ERR(
              "Unable to write pcie DEVCTL register\n");
        } else {
          // Read DevCtl to verify that it worked
          if (pci_read_config_dword(
                  pdev, cap + PCI_EXP_DEVCTL, &pcie_reg)) {
            PRINTK_ERR(
                "Unable to read pcie DEVCTL register\n");
          }
          PRINTK_C(LOG(LOG_PCIe | LOG_DETAIL),
                   "DEVCTL after:0x%08X\n", pcie_reg);
        }
      }
    } else {
      PRINTK("Device cannot be configured to use Extended "
             "Tags\n");
    }
  }
}

/**
 *  Log the Device Serial Number if it is available.
 */
static void
logDeviceSerialNumber(const device_context_t *pDevExt,
                      struct pci_dev *pdev) {
  // Can we read Device Serial Number?
  int ret =
      pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_DSN);
  if (ret > 0) {
    u32 t1, t2;
    pci_read_config_dword(pdev, ret + 4, &t1);
    pci_read_config_dword(pdev, ret + 8, &t2);
    PRINTK("   PCIe Device Serial Number: "
           "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
           t2 >> 24, (t2 >> 16) & 0xff, (t2 >> 8) & 0xff,
           t2 & 0xff, t1 >> 24, (t1 >> 16) & 0xff,
           (t1 >> 8) & 0xff, t1 & 0xff);
  }
}

static inline uint64_t
get_link_status(const device_context_t *pDevExt) {
  uint64_t linkValue = *(uint32_t *)pDevExt->statusDmaVa;
  uint64_t linkValue2 =
      *((uint32_t *)pDevExt->statusDmaVa + 5);
  linkValue |= linkValue2 << 32UL;
  return linkValue;
}

void update_link_status(device_context_t *pDevExt) {
  int i;
  uint64_t linkStatus;

  // Process the OOB channels
  for (i = 0; i < SC_NO_NETWORK_INTERFACES; ++i) {
    if (pDevExt->nif[i].netdevice != NULL) {
      fb_channel_t *channel =
          pDevExt->channel[FIRST_OOB_CHANNEL + i];
      int mtu = pDevExt->nif[i].netdevice->mtu;
      if (netif_queue_stopped(pDevExt->nif[i].netdevice) &&
          channel_tx_possible(channel, mtu)) {
        netif_wake_queue(pDevExt->nif[i].netdevice);
      }
    }
  }

  linkStatus = get_link_status(pDevExt);
  fb_update_netdev_linkstatus(pDevExt, linkStatus);
}

/**
 *  This function is called at 100 ms (10 times a second)
 * intervals to poll any changes in the following areas:
 *
 *  - Poll TCP status and wake up Rx and Tx interrupts on
 * each TCP channel.
 *   -
 */
#if LINUX_VERSION_CODE <                                   \
    KERNEL_VERSION(                                        \
        4, 15, 0) /* Timer API changed in kernel 4.15 */
static void sc_timer_callback(unsigned long arg) {
  device_context_t *pDevExt = (device_context_t *)arg;
#else /* Linux kernel 4.15 or higher */
static void
sc_timer_callback(struct timer_list *pTimerList) {
  device_context_t *pDevExt =
      from_timer(pDevExt, pTimerList, timer);
#endif
  int dma_channel_no = 0;

#if 0
    if (LOGALL(LOG_PLDMA | LOG_DEBUG | LOG_DETAILS))
    {
        static int count = 0;
        int i;

        if ((count++ % 100) == 0)
        {
            for (i = 0; i < SC_MAX_CHANNELS; i++)
            {
                PRINTK("pldma %i %i : %llx\n", i, fb_phys_channel(i), pDevExt->plDmaVa->lastPacket[fb_phys_channel(i)]);
                hexdump(pDevExt, (const char *)pDevExt->plDmaVa, 8 * 64);
            }
        }
    }
#endif

  // Logging of interrupt counts:
  if (LOGANY(LOG_INTERRUPTS)) {
    static int s_InterruptTraceCount = 0;
    bool differentInterruptCounts = false;
    {
      static LogMask_t previousLogmask = LOG_NONE;
      if (logmask != previousLogmask) {
        if ((logmask & LOG_INTERRUPTS) != previousLogmask) {
          previousLogmask = logmask & LOG_INTERRUPTS;
          differentInterruptCounts =
              differentInterruptCounts; // Eliminate
                                        // compiler warning
          differentInterruptCounts = true;
        }
      }
    }

    if (s_InterruptTraceCount++ % 50 ==
        0) // Every 5 seconds
    {
      int i, length = 0;
      char interruptCountString[200];
      int64_t numberOfDifferences = 0;

      for (i = 0; i < MAX_INTR; ++i) {
        length +=
            snprintf(&interruptCountString[length],
                     sizeof(interruptCountString) - length,
                     "%8lld ", pDevExt->intrs[i].intr_cnt);
        differentInterruptCounts |=
            pDevExt->previous_intr_cnt[i] !=
            pDevExt->intrs[i].intr_cnt;
        numberOfDifferences +=
            pDevExt->intrs[i].intr_cnt -
            pDevExt->previous_intr_cnt[i];
        pDevExt->previous_intr_cnt[i] =
            pDevExt->intrs[i].intr_cnt;
      }
      if (differentInterruptCounts) {
        static int rowCount = 0;

        if (rowCount++ % 10 ==
            0) // Output header after every 10th row
        {
          // User logic interrupts 0-7, NIC interrupts 8-15
          PRINTK(
              "INTERRUPTS:      UL0      UL1      UL2      "
              "UL3      UL4      UL5      UL6      UL7"
              "     NIC0     NIC1     NIC2     NIC3     "
              "NIC4     NIC5     NIC6     NIC7   DELTA\n");
        }
        PRINTK("INTERRUPTS: %s  +%lld\n",
               &interruptCountString[0],
               numberOfDifferences);
        pDevExt->user_logic_interrupts.previous_mask =
            pDevExt->user_logic_interrupts.mask;
      }
    }
  }

  //{ static int kount = 0; if (++kount % 100 == 0)
  // PRINTK("timer count %d, pDevExt->reset_fpga = %d\n", 0,
  // kount, pDevExt->reset_fpga); }
  if (pDevExt->reset_fpga) {
    mod_timer(&pDevExt->timer,
              jiffies + msecs_to_jiffies(100));

    return; // Don't do anything while FPGA reset is in
            // progress.
  }

  // Process the tcp channels
  for (dma_channel_no = FIRST_TCP_CHANNEL;
       dma_channel_no <
       FIRST_TCP_CHANNEL + max_etlm_channels;
       dma_channel_no++) {
    fb_channel_t *channel =
        pDevExt->channel[dma_channel_no];
    fbProcessTCPChannel(pDevExt, dma_channel_no);
    if (channel->occupied &&
        (channel->owner_rx == OWNER_KERNEL) &&
        channel_rx_data_available(channel))
      wake_up_interruptible(&channel->inq);
    if (channel->occupied &&
        (channel->owner_tx == OWNER_KERNEL) &&
        channel_tx_possible(channel, 1))
      wake_up_interruptible(&channel->outq);
  }

  // Process the UL channels
  for (dma_channel_no = FIRST_UL_CHANNEL;
       dma_channel_no <
       FIRST_UL_CHANNEL + SC_MAX_ULOGIC_CHANNELS;
       dma_channel_no++) {
    fb_channel_t *channel =
        pDevExt->channel[dma_channel_no];
    if (channel->occupied &&
        (channel->owner_rx == OWNER_KERNEL) &&
        channel_rx_data_available(channel))
      wake_up_interruptible(&channel->inq);
    // FIXME missing kill_fasync(&pDevExt->fasync, SIGIO,
    // POLL_IN); for async notifications?
    if (channel->occupied &&
        (channel->owner_tx == OWNER_KERNEL) &&
        channel_tx_possible(channel, 1))
      wake_up_interruptible(&channel->outq);
  }

  // Check for incoming data on MON channels
  for (dma_channel_no = FIRST_MONITOR_CHANNEL;
       dma_channel_no <
       FIRST_MONITOR_CHANNEL + SC_MAX_MONITOR_CHANNELS;
       dma_channel_no++) {
    fb_channel_t *channel =
        pDevExt->channel[dma_channel_no];
    // PRINTK("mon DMA chan %d occup %d owner %d, data
    // %d\n", dma_channel_no,
    //         channel->occupied, channel->owner_rx,
    //         (channel->owner_rx == OWNER_KERNEL) ?
    //         channel_rx_data_available(channel) : -1);
    if (channel->occupied &&
        (channel->owner_rx == OWNER_KERNEL) &&
        channel_rx_data_available(channel))
      wake_up_interruptible(&channel->inq);
    // no tx on MON channels
  }

  // Check for incoming data on UDP channels
  for (dma_channel_no = FIRST_UDP_CHANNEL;
       dma_channel_no <
       FIRST_UDP_CHANNEL + SC_MAX_UDP_CHANNELS;
       dma_channel_no++) {
    fb_channel_t *channel =
        pDevExt->channel[dma_channel_no];
    if (channel->occupied &&
        (channel->owner_rx == OWNER_KERNEL) &&
        channel_rx_data_available(channel))
      wake_up_interruptible(&channel->inq);
    // no tx on UDP channels
  }

  update_link_status(pDevExt);

  // FIXME: this is a safe guard around a race condition
  // where an Oob Rx interrupt can be lost If a packet
  // arrives after napi_complete() before
  // enable_interrupts(), no interrupt will be send and
  // therefore napi will not be scheduled to process it. So
  // here we force a napi schedule in order to close that
  // possibility. The proper fix is to change the fpga to
  // check the read_ptr == write_ptr of the ring buffer at
  // the time the interrupts are reenabled and fire right
  // away if read_ptr != write_ptr.
  fb_schedule_napi_alldev(pDevExt);

  if (pDevExt->statistics_module_is_present) {
    getNextStatPacket(pDevExt);
  }

  panicParse(pDevExt, 0);

  if (!pDevExt->arp_context.arp_handled_by_user_space &&
      pDevExt->arp_context.timeout_in_jiffies > 0) {
    arp_timeout(
        &pDevExt->arp_context); // Process ARP timeouts
  }

  if (LOG(LOG_TX_ZERO_BYTES_ACK)) {
    for (dma_channel_no = 0; dma_channel_no < 8;
         ++dma_channel_no) {
      int bucket_no = 0;
      for (; bucket_no < MAX_BUCKETS; ++bucket_no) {
        fb_channel_t *pChannel =
            pDevExt->channel[dma_channel_no];
        if (pChannel->bucket_payload_length[bucket_no] !=
            0) {
          volatile uint64_t *pBucketStart =
              (uint64_t *)(pChannel->bucket[bucket_no]
                               ->data);
          uint64_t bucketStartBytes = *pBucketStart;
          if (bucketStartBytes == 0) {
            static volatile uint64_t
                s_BucketStartBytesZeroedCount = 0;

            ++s_BucketStartBytesZeroedCount;
            PRINTK_ERR("Timer check: channel %u bucket %d, "
                       "beginning bytes zeroed: count "
                       "%llu, bucket start *%p is 0x%llx\n",
                       pChannel->dma_channel_no, bucket_no,
                       s_BucketStartBytesZeroedCount,
                       pBucketStart, bucketStartBytes);
          }
        }
      }
    }
  }

  mod_timer(&pDevExt->timer,
            jiffies + msecs_to_jiffies(100));
}

/**
 *  Called from sc_probe.
 */
static int sc_init_device(device_context_t *pDevExt,
                          struct pci_dev *pdev) {
  int rc = 0;
  uint64_t prbSize = RECV_DMA_SIZE;

  mutex_init(&pDevExt->channelListLock);
  spin_lock_init(&pDevExt->tcpEngineLock);
  spin_lock_init(&pDevExt->statSpinLock);
  spin_lock_init(&pDevExt->registerLock);
  mutex_init(&pDevExt->pingLock);

  pDevExt->pdev = pdev;

  pDevExt->free_fifo_entries_normal = MAX_FIFO_ENTRIES;
  pDevExt->free_fifo_entries_priority = MAX_FIFO_ENTRIES;

  // start of ekaline patch
  // pDevExt->ekaline_wc_addr = 0;
  pDevExt->eka_debug = 0;
  pDevExt->eka_drop_all_rx_udp = 0;
  pDevExt->eka_drop_igmp = 0;
  pDevExt->eka_drop_arp = 0;
  // end of ekaline patch

  if (pDevExt->mmuSupport) {
    if (bufsize > 0) {
      pDevExt->pMMU = mmu_new(
          pDevExt, pDevExt->pdev, bufsize, SC_MAX_CHANNELS,
          &prbSize, "bufsize command line option");
    } else {
      PRINTK("MMU is present but not enabled because "
             "driver parameter 'bufsize' is zero.\n");
    }
  }

  if ((rc = init_channels(pDevExt, prbSize))) {
    return rc;
  }

  PRINTK_C(LOG(LOG_DEBUG), "PAGE_SIZE is %lu\n", PAGE_SIZE);

  pDevExt->plDmaVa = (sc_pldma_t *)pci_alloc_consistent(
      pdev, PL_DMA_SIZE, &pDevExt->plDmaPa);
  PRINTK_C(
      LOG(LOG_PCIE_MEMORY_ALLOCATIONS),
      "Allocated  %lu (0x%lx) bytes PL DMA PCIe memory at "
      "virtual address %p, physical address 0x%llx\n",
      PL_DMA_SIZE, PL_DMA_SIZE, pDevExt->plDmaVa,
      pDevExt->plDmaPa);
  if (pDevExt->plDmaVa == NULL)
    return -ENOMEM;
  memset(pDevExt->plDmaVa, 0, PL_DMA_SIZE);

  pDevExt->statusDmaVa =
      (sc_status_dma_t *)pci_alloc_consistent(
          pdev, STATUS_DMA_SIZE, &pDevExt->statusDmaPa);
  PRINTK_C(
      LOG(LOG_MEMORY_ALLOCATIONS),
      "Allocated  %lu (0x%lx) bytes Status DMA PCIe memory "
      "at virtual address %p, physical address 0x%llx\n",
      STATUS_DMA_SIZE, STATUS_DMA_SIZE,
      pDevExt->statusDmaVa, pDevExt->statusDmaPa);
  if (pDevExt->statusDmaVa == NULL)
    return -ENOMEM;
  memset(pDevExt->statusDmaVa, 0, STATUS_DMA_SIZE);

  pDevExt->statisticsDmaVa = (void *)pci_alloc_consistent(
      pdev, STATISTICS_DMA_SIZE, &pDevExt->statisticsDmaPa);
  PRINTK_C(LOG(LOG_MEMORY_ALLOCATIONS),
           "Allocated  %u (0x%x) bytes Statistics DMA PCIe "
           "memory at virtual address %p, physical address "
           "0x%llx\n",
           STATISTICS_DMA_SIZE, STATISTICS_DMA_SIZE,
           pDevExt->statisticsDmaVa,
           pDevExt->statisticsDmaPa);
  if (pDevExt->statisticsDmaVa == NULL)
    return -ENOMEM;
  memset(pDevExt->statisticsDmaVa, 0, STATISTICS_DMA_SIZE);

  // init_completion(&pDevExt->pingReply);

  pDevExt->iommu =
      virt_to_phys(pDevExt->plDmaVa) != pDevExt->plDmaPa;
  PRINTK("   IOMMU is %s\n", pDevExt->iommu ? "on" : "off");

  return rc;
}

/**
 *  Return true if exactly one bit is set in bits, false
 * otherwise.
 */
static inline bool onlyOneBitSet(uint64_t bits) {
  return (bits != 0) && !(bits & (bits - 1));
}

/**
 *  Format a human readable string of a bit mask.
 */
static int printMask(char *msg, int msgSize,
                     uint16_t mask) {
  int lane = 0, rangeStartLane = NO_NIF,
      rangeEndLane = NO_NIF;
  uint16_t oneBitMask = 1;
  const char *comma = " ";

  int length = 0;
  if (mask == 0) {
    return snprintf(&msg[length], msgSize - length,
                    " None");
  }

  while (mask != 0) {
    if ((mask & oneBitMask) != 0) {
      if (rangeStartLane == NO_NIF) {
        rangeStartLane = lane;
        length += snprintf(&msg[length], msgSize - length,
                           "%s%d", comma, rangeStartLane);
      }
      rangeEndLane = lane;
      comma = ",";
    } else if (rangeStartLane != NO_NIF) {
      if (rangeStartLane + 1 == rangeEndLane) {
        length += snprintf(&msg[length], msgSize - length,
                           ",%d", rangeEndLane);
      } else if (rangeStartLane < rangeEndLane) {
        length += snprintf(&msg[length], msgSize - length,
                           "-%d", rangeEndLane);
      }
      rangeStartLane = NO_NIF;
      rangeEndLane = NO_NIF;
    }

    mask &= ~oneBitMask;
    oneBitMask <<= 1;

    ++lane;
  }

  if (rangeStartLane != NO_NIF &&
      rangeStartLane < rangeEndLane) {
    length += snprintf(&msg[length], msgSize - length,
                       "-%d", rangeEndLane);
  }

  return length;
}

/**
 *  Format a human readable string of a lanes mask.
 */
static int printLanes(char *msg, int msgSize,
                      const char *what,
                      uint16_t lanesMask) {
  int length = snprintf(msg, msgSize, "%s lane", what);
  if (!onlyOneBitSet(lanesMask)) {
    length += snprintf(&msg[length], msgSize - length, "s");
  }
  length += snprintf(&msg[length], msgSize - length, ":");

  length +=
      printMask(&msg[length], msgSize - length, lanesMask);

  return length;
}

static bool
is_valid_oob_lane(const device_context_t *pDevExt,
                  int lane) {
  if (lane < 0 || lane >= SC_NO_NETWORK_INTERFACES) {
    return false;
  }
  return (pDevExt->oob_lane_mask & (1 << lane)) != 0;
}

static bool
is_valid_tcp_lane(const device_context_t *pDevExt,
                  int lane) {
  if (lane < 0 || lane >= SC_NO_NETWORK_INTERFACES) {
    return false;
  }
  return (pDevExt->tcp_lane_mask & (1 << lane)) != 0;
}

bool is_valid_udp_lane(const device_context_t *pDevExt,
                       int lane) {
  if (lane < 0 || lane >= SC_NO_NETWORK_INTERFACES) {
    return false;
  }
  return (pDevExt->udp_lane_mask & (1 << lane)) != 0;
}

/**
 *  Get the up/down interface status of all standard NIC
 * (OOB) interfaces.
 *
 *  @param  pDevExt     Pointer to device context.
 *  @param  upDownState Returns interfaces up/down state.
 * True means interface is up, false means interface is
 * down.
 */
static void get_oob_interfaces_state(
    const device_context_t *pDevExt,
    bool upDownState[SC_NO_NETWORK_INTERFACES]) {
  int lane;

  for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane) {
    upDownState[lane] = false;
    if (is_valid_oob_lane(pDevExt, lane)) {
      const sc_net_interface_t *nif = &pDevExt->nif[lane];
      const struct net_device *dev = nif->netdevice;
      upDownState[lane] =
          dev->flags & IFF_UP ? true : false;
    }
  }
}

/**
 *  Take all network interfaces down or up.
 *
 *  @param  pDevExt     Pointer to device context.
 *  @param  up          True if the interfaces should be
 * taken up, false if they should be taken down.
 *  @param  upDownState Interfaces up/down state before
 * reset. Only interfaces that were up before reset will be
 * brought up after reset, otherwise they remain down.
 *
 *  @return             Zero if everything is OK, nonzero on
 * error.
 */
static int enable_all_oob_interfaces(
    const device_context_t *pDevExt, bool up,
    bool upDownState[SC_NO_NETWORK_INTERFACES]) {
  int lane, errors = 0;

  for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane) {
    if (is_valid_oob_lane(pDevExt, lane)) {
      int rc;

      const sc_net_interface_t *nif = &pDevExt->nif[lane];
      struct net_device *dev = nif->netdevice;
      unsigned long upDownStateValue =
          up ? (upDownState[lane] ? NETDEV_UP : NETDEV_DOWN)
             : NETDEV_DOWN;

      rtnl_lock();
      rc = call_netdevice_notifiers(upDownStateValue, dev);
      rtnl_unlock();

      if (rc != NOTIFY_DONE && rc != NOTIFY_OK) {
        PRINTK_ERR("FPGA reset: failed to take lane %d up; "
                   "call_netdevice_notifiers error %d\n",
                   lane, rc);
        errors += -1;
      }
    }
  }

  return errors;
}

/**
 *  Do an FPGA reset without reloading the driver.
 *
 *  @param  pDevExt     Pointer to device context.
 *
 *  @return             Zero if everything is OK, nonzero on
 * error.
 */
static int sc_fpga_reset(device_context_t *pDevExt) {
  int rc = 0, lane;
  unsigned int dma_channel_no;
  bool upDownState[SC_NO_NETWORK_INTERFACES];

  // if (LOG(LOG_FPGA_RESET))
  {
    PRINTK("FPGA reset #%u started\n",
           ++pDevExt->fpga_reset_count);
  }

  get_oob_interfaces_state(
      pDevExt, upDownState); // Remember interfaces up/down
                             // state before reset
  // Take all network interfaces down:
  if (enable_all_oob_interfaces(pDevExt, false,
                                upDownState) != 0) {
    return -EFAULT;
  }

  // Stop and disable all FPGA related activity

  pDevExt->reset_fpga = true;
  // Above setting stops following functions to do any FPGA
  // related activity:
  // - reject other ioctl calls with EBUSY
  // - don't do anything in sc_timer_callback
  // - don't do anything in adjust_cardtime_thread
  // - stop fb_start_xmit from transmitting anything
  // (packets will be lost!)
  // - stop NAPI poll function fb_poll doing any work
  // - stop all other Linux callbacks to driver from doing
  // any FPGA access
  mdelay(200); // Wait for all above activity to finish what
               // they are currently doing

  stop_all_interrupts(pDevExt);

  if (pDevExt->udp_lane_mask != 0) {
    // Clean up UDP multicast:
    cleanup_igmp(&pDevExt->igmpContext);
  }
  mdelay(100); // Give time to send out pending IGMP leave
               // packets

  // DMAs have to be stopped before PL DMA is disabled
  for (dma_channel_no = 0; dma_channel_no < SC_MAX_CHANNELS;
       dma_channel_no++) {
    stop_dma(pDevExt, dma_channel_no);
  }

  enable_pldma(pDevExt, 0); // Disable PL DMA
  enable_status_dma(pDevExt, 0);
  if (pDevExt->statistics_module_is_present) {
    enable_statistic_dma(pDevExt, 0, pDevExt->filling_scale,
                         pDevExt->time_mode);
  }

  for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane) {
    setBypass(pDevExt, lane, 0);
    setPromisc(pDevExt, lane, 0);
  }

  for (dma_channel_no = 0; dma_channel_no < SC_MAX_CHANNELS;
       dma_channel_no++) {
    fb_channel_t *pChannel =
        pDevExt->channel[dma_channel_no];

    if (pChannel->type == SC_DMA_CHANNEL_TYPE_TCP) {
      // Abort connection and set state to closed.
      unsigned tcp_channel_number =
          pChannel->dma_channel_no - FIRST_TCP_CHANNEL;
      if (etlm_getTcpState(pDevExt, tcp_channel_number) !=
          ETLM_TCP_STATE_CLOSED) {
        etlm_abort_conn(pDevExt, tcp_channel_number);
      }
    }
  }

  // Give time for FPGA to complete already in-fly
  // operations
  mdelay(1);

  // Now do FPGA reset:
  if ((rc = fpga_reset(pDevExt)) != 0) {
    return -EBUSY; // Reset did not finish within a timeout.
  }

  // Bring up and initialize FPGA again

  for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane) {
    sc_net_interface_t *nif = &pDevExt->nif[lane];

    // Extract OOB (standard NIC) port numbers
    if (is_valid_oob_lane(pDevExt, lane)) {
      setSourceMac(pDevExt, lane, nif->mac_addr);
    }
    if (is_valid_tcp_lane(pDevExt, lane)) {
      // ETLM specific setup and initialization:
      etlm_setDelayedAck(
          pDevExt,
          false); // Disable ETLM delayed ACK functionality
      etlm_setSrcMac(pDevExt, nif->mac_addr);
    }

    setBypass(pDevExt, lane, 1);
    setPromisc(pDevExt, lane, 0);
  }

  setup_pl_dma(pDevExt);
  enable_pldma(pDevExt, 1);

  setup_status_dma(pDevExt);
  enable_status_dma(pDevExt, 1);

  pDevExt->curStatPacket =
      NULL; // Need to start statistics packets from the
            // beginning of the PRB!
  if (pDevExt->statistics_module_is_present) {
    setup_statistics_dma(pDevExt);
    enable_statistic_dma(pDevExt, 1, pDevExt->filling_scale,
                         pDevExt->time_mode);
  }

  for (dma_channel_no = 0; dma_channel_no < SC_MAX_CHANNELS;
       dma_channel_no++) {
    fb_channel_t *pChannel =
        pDevExt->channel[dma_channel_no];

    if (dma_channel_no >= FIRST_OOB_CHANNEL &&
        dma_channel_no <
            FIRST_OOB_CHANNEL +
                SC_MAX_OOB_CHANNELS) // OOB Channels
    {
      memset(pChannel->recv, 0,
             RECV_DMA_SIZE); // Clear receive PRB
      start_dma(pDevExt, dma_channel_no, pChannel->prbSize);

      // Initialize the buckets and flow control to zero
      channel_reset_buckets(pChannel);
    } else if (dma_channel_no >= FIRST_UL_CHANNEL &&
               dma_channel_no <
                   FIRST_UL_CHANNEL +
                       SC_MAX_ULOGIC_CHANNELS) // UL
                                               // channels
    {
      memset(pChannel->recv, 0,
             RECV_DMA_SIZE); // Clear receive PRB
      start_dma(pDevExt, dma_channel_no, pChannel->prbSize);

      // Initialize the buckets and flow control to zero
      channel_reset_buckets(pChannel);
    }
  }

  setTimerDelay(pDevExt, input_delay);
  setFpgaTime(pDevExt);
  setClockSyncMode(pDevExt,
                   TIMECONTROL_SOFTWARE_FPGA_RESET);

  // Reinitialize UDP multicast lanes:
  for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane) {
    if (is_valid_udp_lane(pDevExt, lane)) {
      init_igmp(pDevExt, &pDevExt->nif[lane]);
    }
  }

  start_all_oob_interrupts(pDevExt);

  // Release all functions stopped above
  pDevExt->reset_fpga = false;

  // Wait for everything start running normal again.
  // For all other tested systems 200 milliseconds in the
  // driver is enough but HP Proliant g9 servers DL360/DL380
  // running CentOS 7.4 seem to require at least 3 seconds.
  // Have not been able to determine why these HP servers
  // are different. This extra delay is implemented in the
  // user space API library after executing the driver
  // SC_IOCTL_FPGA_RESET function.
  mdelay(200);

  // Take all network interfaces up:
  if (enable_all_oob_interfaces(pDevExt, true,
                                upDownState) != 0) {
    return -EFAULT;
  }

  // if (LOG(LOG_FPGA_RESET))
  {
    PRINTK("FPGA reset #%u finished\n",
           pDevExt->fpga_reset_count);
  }

  return rc;
}

/**
 *  This function is called from sc_probe.
 */
static int sc_start_device(device_context_t *pDevExt) {
  int dma_channel_no = 0;
  int lane = 0; // Lane per card
  int i;

  // Static global lane numbering for all cards when
  // allocating network interfaces "feth0..fethN" First card
  // allocates interfaces feth0 to
  // feth$(NO_SC_NO_NETWORK_INTERFACES - 1) Second card
  // allocates interfaces feth$NO_SC_NO_NETWORK_INTERFACES
  // to feth$(2 * NO_SC_NO_NETWORK_INTERFACES - 1) etc...
  {
    uint64_t devConfig =
        getDeviceConfig((const device_context_t *)pDevExt);
    pDevExt->tcp_lane_mask = (devConfig >> 0) & 0xffff;
    pDevExt->udp_lane_mask = (devConfig >> 16) & 0xffff;
    pDevExt->oob_lane_mask = (devConfig >> 32) & 0xffff;
    pDevExt->mac_lane_mask = (devConfig >> 48) & 0xffff;
  }

  getFpgaAclQsfpMonitorConfiguration(
      pDevExt, &pDevExt->acl_context.acl_nif_mask,
      &pDevExt->qspf_lane_mask, &pDevExt->monitor_lane_mask,
      &pDevExt->statistics_module_is_present);

  {
    // Regardless of what oob_lane_mask says there is a
    // standard NIC on all lanes that have an TCP TOE or UDP
    // engine:
    uint16_t oob_lane_original_mask =
        pDevExt->oob_lane_mask;
    pDevExt->oob_lane_mask |=
        pDevExt->tcp_lane_mask | pDevExt->udp_lane_mask;

    PRINTK("   OOB lanes mask 0x%04x(0x%04x), TCP lanes "
           "mask 0x%04x, UDP lanes mask 0x%04x, MAC lanes "
           "mask 0x%04x\n",
           pDevExt->oob_lane_mask, oob_lane_original_mask,
           pDevExt->tcp_lane_mask, pDevExt->udp_lane_mask,
           pDevExt->mac_lane_mask);
    PRINTK("   QSFP port mask 0x%02x, monitor lanes mask "
           "0x%04x, ACL lanes mask 0x%02x, %s\n",
           pDevExt->qspf_lane_mask,
           pDevExt->monitor_lane_mask,
           pDevExt->acl_context.acl_nif_mask,
           pDevExt->statistics_module_is_present
               ? "statistics enabled"
               : "statistics not enabled");
  }

  // channel_reserve_tx_fifo_entries checks for the lock,
  // even if it is not relevant here
  mutex_lock(&pDevExt->channelListLock);

  // Channels 8-15 are use by OOB (standard NIC):
  for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane) {
    int dma_channel_no = FIRST_OOB_CHANNEL + lane;
    fb_channel_t *channel =
        pDevExt->channel[dma_channel_no];

    pDevExt->nif[lane].pDevExt = pDevExt;
    pDevExt->nif[lane].oob_channel_no = dma_channel_no;
    pDevExt->nif[lane].network_interface = lane;
    //---------------------------------------------------------------------------------------
    // start of ekaline patch
    pDevExt->nif[lane].eka_private_data =
        kmalloc(sizeof(eka_nif_state_t), GFP_KERNEL);
    if (pDevExt->nif[lane].eka_private_data == NULL) {
      PRINTK("EKALINE_DEBUG: %s: kmalloc failed...\n",
             __func__);
    } else {
      /* memset
       * (pDevExt->nif[lane].eka_private_data->eka_version,0,sizeof(eka_nif_state_t));
       */
      /* memset
       * (pDevExt->nif[lane].eka_private_data->eka_version,0,64);
       */
      /* strcpy(pDevExt->nif[lane].eka_private_data->eka_version,EKA_SN_DRIVER_BUILD_TIME);
       */
      /* memset
       * (pDevExt->nif[lane].eka_private_data->eka_release,0,256);
       */
      /* strcpy(pDevExt->nif[lane].eka_private_data->eka_release,EKA_SN_DRIVER_RELEASE_NOTE);
       */
      int eks;
      for (eks = 0; eks < EKA_SESSIONS_PER_NIF; eks++) {
        memset(&(pDevExt->nif[lane]
                     .eka_private_data->eka_session[eks]),
               0, sizeof(eka_session_t));
      }
    }
    // end of ekaline patch
    //---------------------------------------------------------------------------------------

    // Allocate fifo entries for the OOB channels
    channel_reserve_tx_fifo_entries(
        channel, FB_FIFO_ENTRIES_PER_CHANNEL, 0);

    if (is_valid_tcp_lane(pDevExt, lane)) {
      pDevExt->nif[lane].flags = FB_NIF_HANDLE_ARP;
    }

    if (is_valid_udp_lane(pDevExt, lane)) {
      pDevExt->nif[lane].flags =
          FB_NIF_HANDLE_ARP | FB_NIF_HANDLE_IGMP;
    }
  }

  // Cumulative number of lanes in cards before this one.
  // This ensures we name NIC interfaces with consistent
  // sequential numbering and we can also have a consistent
  // system wide lane numbering over multiple cards
  // regardless of in which order Linux calls sc_probe.
  // Lane numbering for each card starts from 0.
  pDevExt->lane_base = 0;
  for (i = 0; i < number_of_elements(dev_data); ++i) {
    device_context_t *pDevice = dev_data[i].pDevExt;
    if (pDevice != NULL) {
      if (i == pDevExt->minor) {
        break;
      }
      if (max_lanes_per_device < 0) {
        // Name NIC interfaces with a possible gap in naming
        // if dev_data[i].nb_netdev_used_for_lane_base is
        // less than abs(max_lanes_per_device)
        pDevExt->lane_base += abs(max_lanes_per_device);
      } else {
        // Name NIC interfaces in sequential order
        assert(dev_data[i].nb_netdev_used_for_lane_base >
               0);
        pDevExt->lane_base +=
            dev_data[i].nb_netdev_used_for_lane_base;
      }
    } else {
      // If we have holes in dev_data, i.e. devices that are
      // not yet allocated then we just use the maximum
      // number of 16 as number of lanes in this not yet
      // seen card as basis for lane_base computations. This
      // might result in discontinuities in NIC interface
      // numbering but the result is still consistent, i.e.
      // a specific NIC interface name always refers to the
      // same lane on the same card regardless of in which
      // order Linux calls sc_probe.
      assert(dev_data[i].nb_netdev_used_for_lane_base == 0);
      dev_data[i].nb_netdev_used_for_lane_base =
          abs(max_lanes_per_device);
      pDevExt->lane_base += abs(max_lanes_per_device);
    }
  }

  mutex_unlock(&pDevExt->channelListLock);

  PRINTK("   Lane base = %u\n", pDevExt->lane_base);

  // Currently the driver supports only 1 TCP (ETLM) and 1
  // UDP engine instance
  pDevExt->nb_netdev = 0;
  pDevExt->tcp_lane = NO_NIF;
  for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane) {
    // Extract OOB (standard NIC) port numbers
    if (is_valid_oob_lane(pDevExt, lane)) {
      int rc =
          fb_setup_netdev(pDevExt, &pDevExt->nif[lane]);
      if (rc != 0) {
        PRINTK_ERR("fb_setup_netdev failed on lane %d with "
                   "error code %d\n",
                   lane, rc);
      } else {
        pDevExt->nb_netdev++; // Count actual number of
                              // lanes found
      }
    }
    // Extract TCP port number
    if (is_valid_tcp_lane(pDevExt, lane)) {
      // WARN_ON(pDevExt->tcp_lane <
      // SC_NO_NETWORK_INTERFACES);    // Error if more than
      // one TCP port!
      if (pDevExt->tcp_lane < SC_NO_NETWORK_INTERFACES)
        PRINTK_ERR("more than one TOE detected when driver "
                   "supports only one\n");
      else
        pDevExt->tcp_lane = lane;
    }
  }
  if (dev_data[pDevExt->minor]
          .nb_netdev_used_for_lane_base == 0) {
    dev_data[pDevExt->minor].nb_netdev_used_for_lane_base =
        pDevExt->nb_netdev;
  }

  {
    char msg[150];
    int length =
        snprintf(msg, sizeof(msg),
                 "%d actual physical lane%s found: ",
                 pDevExt->nb_netdev,
                 pDevExt->nb_netdev != 1 ? "s" : "");
    length +=
        printLanes(&msg[length], sizeof(msg) - length,
                   "TCP (ETLM)", pDevExt->tcp_lane_mask);
    length += printLanes(&msg[length], sizeof(msg) - length,
                         "; UDP", pDevExt->udp_lane_mask);
    length += printLanes(&msg[length], sizeof(msg) - length,
                         "; NIC", pDevExt->oob_lane_mask);
    PRINTK("%s\n", msg);
  }

  setup_pl_dma(pDevExt);
  enable_pldma(pDevExt, 1);

  setup_status_dma(pDevExt);
  enable_status_dma(pDevExt, 1);

  if (pDevExt->statistics_module_is_present) {
    setup_statistics_dma(pDevExt);
    enable_statistic_dma(pDevExt, 1, pDevExt->filling_scale,
                         pDevExt->time_mode);
  }

  for (dma_channel_no = 0; dma_channel_no < SC_MAX_CHANNELS;
       dma_channel_no++) {
    fb_channel_t *pChannel =
        pDevExt->channel[dma_channel_no];

    init_waitqueue_head(&pChannel->inq);
    init_waitqueue_head(&pChannel->outq);
    if (dma_channel_no >= FIRST_OOB_CHANNEL &&
        dma_channel_no <
            FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS) {
      // OOB Channels
      start_dma(pDevExt, dma_channel_no, pChannel->prbSize);
    } else if (dma_channel_no >= FIRST_UL_CHANNEL &&
               dma_channel_no <
                   FIRST_UL_CHANNEL +
                       SC_MAX_ULOGIC_CHANNELS) {
      // User Logic channels
      start_dma(pDevExt, dma_channel_no, pChannel->prbSize);
    }
  }

  pDevExt->time_integral = 0;
  pDevExt->time_lastDiff = 0;
  pDevExt->time_deltaDiff = 0;

  setTimerDelay(pDevExt, input_delay);
  setFpgaTime(pDevExt);
  setClockSyncMode(pDevExt, TIMECONTROL_SOFTWARE);

  if (pDevExt->tcp_lane_mask != 0) {
    // ETLM specific setup and initialization:

    uint32_t num_conn, txPacketSz, rxPacketSz,
        txTotalMaxWinSz, rxTotalMaxWinSz;
    uint64_t txTotalWinSz, rxTotalWinSz;
    bool vlan_supported;
    uint64_t evaluationModes = 0;

    etlm_setDelayedAck(
        pDevExt,
        false); // Disable ETLM delayed ACK functionality

    etlm_getCapabilities(pDevExt, &num_conn, &txPacketSz,
                         &rxPacketSz, &txTotalMaxWinSz,
                         &rxTotalMaxWinSz, &vlan_supported);
    getFpgaToeWindowMemorySizes(
        pDevExt, &txTotalWinSz, &rxTotalWinSz,
        &pDevExt->toeIsConnectedToUserLogic,
        &evaluationModes);

    PRINTK("   TOE capabilities num_conn: %d, txPacketSz: "
           "%d, rxPacketSz: %d, "
           "txTotalWinSz: %llu (max %u), rxTotalWinSz: "
           "%llu (max %u)\n",
           num_conn, txPacketSz, rxPacketSz, txTotalWinSz,
           txTotalMaxWinSz, rxTotalWinSz, rxTotalMaxWinSz);
    PRINTK("                    vlan: %s, TOE is "
           "%sconnected to User Logic\n",
           vlan_supported ? "yes" : "no",
           pDevExt->toeIsConnectedToUserLogic ? ""
                                              : "not ");

    if (num_conn != SC_MAX_TCP_CHANNELS) {
      PRINTK_ERR("TOE engine supports only %u connections "
                 "(driver configured with %u)\n",
                 num_conn, SC_MAX_TCP_CHANNELS);
      return -EINVAL;
    }

    // Compute the usable TCP Rx and Tx window size
    pDevExt->toe_rx_win_size = round_lower_power_of_2(
        rxTotalWinSz / max_etlm_channels);
    pDevExt->toe_tx_win_size = round_lower_power_of_2(
        txTotalWinSz / max_etlm_channels);

    if (max_etlm_channels == SC_MAX_TCP_CHANNELS - 1) {
      PRINTK("   TOE window sizes: Rx %d, Tx %d on "
             "channels 0-%u, channel %u not available\n",
             pDevExt->toe_rx_win_size,
             pDevExt->toe_tx_win_size,
             max_etlm_channels - 1, max_etlm_channels);
    } else if (max_etlm_channels <
               SC_MAX_TCP_CHANNELS - 1) {
      PRINTK(
          "   TOE window sizes: Rx %d, Tx %d on channels "
          "0-%u, channels %u-%u not available\n",
          pDevExt->toe_rx_win_size,
          pDevExt->toe_tx_win_size, max_etlm_channels - 1,
          max_etlm_channels, SC_MAX_TCP_CHANNELS - 1);
    } else {
      PRINTK("   TOE window sizes: Rx %d, Tx %d on "
             "channels 0-%u\n",
             pDevExt->toe_rx_win_size,
             pDevExt->toe_tx_win_size,
             max_etlm_channels - 1);
    }

    {
      uint32_t slowTimerIntervalUs, fastTimerIntervalUs;
      etlm_getTimerIntervals(pDevExt, &slowTimerIntervalUs,
                             &fastTimerIntervalUs);
      PRINTK("   TOE timers: slow %d us, fast %d us\n",
             slowTimerIntervalUs, fastTimerIntervalUs);
    }
    {
      uint32_t retransMinUs, retransMaxUs, persistMinUs,
          persistMaxUs;
      etlm_getTCPTimers(pDevExt, &retransMinUs,
                        &retransMaxUs, &persistMinUs,
                        &persistMaxUs);
      PRINTK("   TOE retrans timer: min %d us, max %d us\n",
             retransMinUs, retransMaxUs);
      PRINTK("   TOE persist timer: min %d us, max %d us\n",
             persistMinUs, persistMaxUs);
    }

    if (evaluationModes > 0) {
      // Log FPGA evaluation modes:
      char message[100];

      message[0] = '\0';
      if (((evaluationModes >> 24) & 1) != 0) {
        strcat(message, " ComEval");
      }
      if (((evaluationModes >> 25) & 1) != 0) {
        strcat(message, " MacEval");
      }
      if (((evaluationModes >> 26) & 1) != 0) {
        strcat(message, " ToeEval");
      }
      if (((evaluationModes >> 27) & 1) != 0) {
        strcat(message, " NicEval");
      }
      if (((evaluationModes >> 28) & 1) != 0) {
        strcat(message, " StatsEval");
      }

      PRINTK("   Evaluation modes:%s\n", message);

      pDevExt->evaluation_mask =
          (uint16_t)((evaluationModes >> 24) & 0x1F);
    }
  }

  // Start timer which calls function sc_timer_callback
  // every 100ms
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
  setup_timer(&pDevExt->timer, sc_timer_callback,
              (unsigned long)pDevExt);
#else
  timer_setup(&pDevExt->timer, sc_timer_callback,
              /*flags*/ 0);
#endif
  mod_timer(&pDevExt->timer,
            jiffies + msecs_to_jiffies(100));

  // Initialize IGMP multicast for each UDP lane
  for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane) {
    if (is_valid_udp_lane(pDevExt, lane)) {
      init_igmp(pDevExt, &pDevExt->nif[lane]);
    }
  }

  return 0;
}

static void sc_free_device(device_context_t *pDevExt) {
  int dma_channel_no = 0;
  int i = 0;

  FUNCTION_ENTRY(LOG_INIT, " ");

  cleanup_igmp(&pDevExt->igmpContext);
  mdelay(100); // Give time to send out pending IGMP leave
               // packets

  // DMAs have to be stopped before PL DMA is disabled
  for (dma_channel_no = 0; dma_channel_no < SC_MAX_CHANNELS;
       dma_channel_no++) {
    stop_dma(pDevExt, dma_channel_no);
  }

  enable_pldma(pDevExt, 0); // Disable PL DMA
  enable_status_dma(pDevExt, 0);
  if (pDevExt->statistics_module_is_present) {
    enable_statistic_dma(pDevExt, 0, pDevExt->filling_scale,
                         pDevExt->time_mode);
  }

  mmu_enable(pDevExt->pMMU, false);

  del_timer_sync(&pDevExt->timer);

  // Give time for FPGA to complete already in-fly
  // operations
  mdelay(1);

  for (i = 0; i < SC_NO_NETWORK_INTERFACES; i++) {
    fb_cleanup_netdev(pDevExt, &pDevExt->nif[i]);
  }

  deinit_channels(pDevExt);

  if (pDevExt->pMMU != NULL) {
    mmu_delete(pDevExt->pMMU);
    pDevExt->pMMU = NULL;
  }

  mutex_destroy(&pDevExt->pingLock);
  mutex_destroy(&pDevExt->channelListLock);

  PRINTK_C(LOG(LOG_PCIE_MEMORY_ALLOCATIONS),
           "Deallocate %u (0x%x) bytes Statistics DMA PCIe "
           "memory at virtual address %p, physical address "
           "0x%llx\n",
           STATISTICS_DMA_SIZE, STATISTICS_DMA_SIZE,
           pDevExt->statisticsDmaVa,
           pDevExt->statisticsDmaPa);
  pci_free_consistent(pDevExt->pdev, STATISTICS_DMA_SIZE,
                      pDevExt->statisticsDmaVa,
                      pDevExt->statisticsDmaPa);

  PRINTK_C(
      LOG(LOG_PCIE_MEMORY_ALLOCATIONS),
      "Deallocate %lu (0x%lx) bytes Status DMA PCIe memory "
      "at virtual address %p, physical address 0x%llx\n",
      STATUS_DMA_SIZE, STATUS_DMA_SIZE,
      pDevExt->statusDmaVa, pDevExt->statusDmaPa);
  pci_free_consistent(pDevExt->pdev, STATUS_DMA_SIZE,
                      pDevExt->statusDmaVa,
                      pDevExt->statusDmaPa);

  PRINTK_C(
      LOG(LOG_PCIE_MEMORY_ALLOCATIONS),
      "Deallocate %lu (0x%lx) bytes PL DMA PCIe memory at "
      "virtual address %p, physical address 0x%llx\n",
      PL_DMA_SIZE, PL_DMA_SIZE, pDevExt->plDmaVa,
      pDevExt->plDmaPa);
  pci_free_consistent(pDevExt->pdev, PL_DMA_SIZE,
                      pDevExt->plDmaVa, pDevExt->plDmaPa);

  arp_context_destruct(&pDevExt->arp_context);

  PRINTK_C(LOG(LOG_MEMORY_ALLOCATIONS),
           "Deallocate device context %u at virtual "
           "address %p\n",
           pDevExt->minor, pDevExt);

  FUNCTION_EXIT(LOG_INIT, "");
  PRINTK("Unmapping: "
         "eka_bar0_nocache_a 0x%llx, "
         "ekaline_wc_addr 0x%llx, "
         "eka_bar0_nocache_b 0x%llx, "
         "bar0_va 0x%llx, "
         "bar2_va 0x%llx\n",
         pDevExt->eka_bar0_nocache_a,
         pDevExt->ekaline_wc_addr,
         pDevExt->eka_bar0_nocache_b,
         (uint64_t)pDevExt->bar0_va,
         (uint64_t)pDevExt->bar2_va);

  iounmap((void *)pDevExt->eka_bar0_nocache_a);
  iounmap((void *)pDevExt->ekaline_wc_addr);
  iounmap((void *)pDevExt->eka_bar0_nocache_b);
  iounmap(pDevExt->bar0_va);
  iounmap(pDevExt->bar2_va);
  kfree(pDevExt);
}

/**
 *  Allocate channel
 */
static int sc_allocate_channel(struct file *f,
                               device_context_t *pDevExt,
                               int type, uint32_t chan,
                               uint32_t lane,
                               uint8_t enableDma) {
  int rc = 0;
  int dma_channel_no = -1;
  fb_channel_t *channel = NULL;

  // A file is expected to enable safe releasing of
  // resources, the file must not have any channels
  // allocated already
  BUG_ON(!f);
  BUG_ON(f->private_data);

  if (lane >= SC_NO_NETWORK_INTERFACES) {
    PRINTK_ERR("invalid lane %d (max is %d)\n", lane,
               SC_NO_NETWORK_INTERFACES);
    return -EINVAL;
  }

  if (mutex_lock_interruptible(&pDevExt->channelListLock))
    return -ERESTARTSYS;

  if (type == SC_DMA_CHANNEL_TYPE_TCP) {
    if (chan >= FIRST_TCP_CHANNEL + SC_MAX_TCP_CHANNELS ||
        chan == -1) {
      dma_channel_no = find_vacant_tcp_channel(pDevExt);
    } else {
      dma_channel_no = chan;
    }
    if (!is_valid_tcp_lane(pDevExt, lane)) {
      PRINTK_ERR("invalid lane %u for TCP channel %d, "
                 "interface mask: 0x%04x\n",
                 lane, dma_channel_no,
                 pDevExt->tcp_lane_mask);
      mutex_unlock(&pDevExt->channelListLock);
      return -EINVAL;
    }
    // For TCP channels, the dma will be started when a
    // connection is established
    enableDma = 0;
  } else if (type == SC_DMA_CHANNEL_TYPE_UDP_MULTICAST) {
    dma_channel_no = find_vacant_udp_channel(pDevExt);
    if (!is_valid_udp_lane(pDevExt, lane)) {
      PRINTK_ERR("invalid lane %u for UDP channel %d, "
                 "interface mask: 0x%04x\n",
                 lane, dma_channel_no,
                 pDevExt->udp_lane_mask);
      mutex_unlock(&pDevExt->channelListLock);
      return -EINVAL;
    }
    // enableDma is specified by user for UDP channels
  } else if (type == SC_DMA_CHANNEL_TYPE_MONITOR) {
    dma_channel_no = FIRST_MONITOR_CHANNEL + lane;
    enableDma = 1;
  } else if (type == SC_DMA_CHANNEL_TYPE_USER_LOGIC) {
    if (chan >= SC_MAX_ULOGIC_CHANNELS) {
      PRINTK_ERR(
          "invalid ul channel number: %d (max is %d)\n",
          chan, SC_MAX_ULOGIC_CHANNELS);
      mutex_unlock(&pDevExt->channelListLock);
      return -EINVAL;
    }
    dma_channel_no = FIRST_UL_CHANNEL + chan;
    enableDma = 1;
  } else if (type == SC_DMA_CHANNEL_TYPE_STANDARD_NIC) {
    PRINTK_ERR("allocating OOB channel is forbidden\n");
  } else {
    PRINTK_ERR("unknown channel type %d\n", type);
  }

  if (dma_channel_no < 0) {
    mutex_unlock(&pDevExt->channelListLock);
    return dma_channel_no;
  }

  channel = pDevExt->channel[dma_channel_no];
  if (channel->occupied) {
    mutex_unlock(&pDevExt->channelListLock);
    PRINTK_ERR(
        "AllocChannel: Channel %d type %d is busy!\n",
        dma_channel_no, type);
    return -EBUSY;
  }

  if (type == SC_DMA_CHANNEL_TYPE_TCP) {
    unsigned tcp_conn =
        channel->dma_channel_no - FIRST_TCP_CHANNEL;
    int tcpState = etlm_getTcpState(pDevExt, tcp_conn);
    if (tcpState != ETLM_TCP_STATE_CLOSED) {
      PRINTK("AllocChannel: ETLM channel %u was not in "
             "ETLM_TCP_STATE_CLOSED state but in %s state; "
             "aborting connection\n",
             tcp_conn, EtlmState(tcpState));
      etlm_abort_conn(pDevExt, tcp_conn);
    }

    if (!etlm_waitForTcpState(
            pDevExt, tcp_conn, ETLM_TCP_STATE_CLOSED,
            2 * 1000 * 1000 /*timeout us*/)) {
      PRINTK_ERR("AllocChannel: Timeout reached why didn't "
                 "we reach closed state!\n");
      mutex_unlock(&pDevExt->channelListLock);
      return -EBUSY;
    }
  }

  // After this point the channel will be attached to the
  // process unconditionally, it is therefore ok to change
  // its state
  channel->owner_rx = OWNER_UNDECIDED;
  channel->owner_tx = OWNER_UNDECIDED;
  channel->occupied = 1;
  channel->reserved_fifo_entries_normal = 0;
  channel->reserved_fifo_entries_priority = 0;

  stop_dma(pDevExt, dma_channel_no);
  if (enableDma) {
    memset(channel->recv, 0,
           RECV_DMA_SIZE); // Clear receive PRB
    start_dma(pDevExt, dma_channel_no, channel->prbSize);
  }

  PRINTK_C(LOG(LOG_DEBUG), "allocating channel %i ",
           dma_channel_no);

  // Initialize the buckets to zero
  channel_reset_buckets(channel);

  f->private_data = channel;

  mutex_unlock(&pDevExt->channelListLock);

  return rc;
}

/** **************************************************************************
 * @defgroup FileOperationFunctions Device file related
 * operating system operations.
 * @{
 * @brief Device file related operating system operations
 * like mmap, open, close and ioctl.
 */

static long sc_ioctl(struct file *f, unsigned int op,
                     unsigned long data) {
  uint64_t arg64;
  int rc = 0;
  fb_channel_t *channel = NULL;
  device_context_t *pDevExt = get_sc_device_from_file(f);
  unsigned tcp_conn = ~0;
  unsigned int notCopiedBytes;

  if (pDevExt == NULL) {
    return -ENODEV;
  }

  PRINTK_C(
      LOG(LOG_IOCTL) || LOG(LOG_FILE_OPS),
      ">sc_ioctl: f=%p, f->private_data=%p , op=%s (%u)\n",
      f, f->private_data, IoCtlCmd(pDevExt, op), op);

  if (!pDevExt)
    return -EINVAL;

  if (pDevExt->reset_fpga) {
    PRINTK_C(LOG(LOG_FPGA_RESET),
             "FPGA RESET in progress in function %s\n",
             __func__);

    return -EBUSY; // Do not do anything else while FPGA
                   // reset is in progress.
  }

  switch (op) {
  case SC_IOCTL_GET_STAT: {
    fb_statistics_packet_t *statPacket;
    rc = 0;

    if (!pDevExt->statistics_module_is_present) {
      PRINTK_ERR("FPGA does not support statistics.\n");
      return -ENODEV;
    }

    spin_lock(&pDevExt->statSpinLock);
    statPacket = pDevExt->curStatPacket;

    PRINTK_C(LOG(LOG_STATISTICS), "Statistics packet %p\n",
             statPacket);

    if (statPacket == NULL) {
      rc = -EAGAIN;
    } else if ((rc = copy_to_user(
                    (void __user *)data,
                    &statPacket->payload,
                    sizeof(statPacket->payload)))) {
      PRINTK_C(LOG(LOG_STATISTICS),
               "Statistics failed %p %ld rc = %d\n",
               statPacket, sizeof(sc_stat_packet_payload_t),
               rc);
      rc = -EFAULT;
    }

    spin_unlock(&pDevExt->statSpinLock);

    return rc;
  }
    //--------------------------------------------------------------------------------
    // start of ekaline patch
  case SC_IOCTL_EKALINE_DATA: {
#include "ekaline_ioctl.c"
  }
    return 0;
    // end of ekaline patch
    //--------------------------------------------------------------------------------
  case SC_IOCTL_MISC: {
    sc_misc_t misc;

    if (copy_from_user(&misc, (void *)data, sizeof(misc))) {
      return -EFAULT;
    }

    switch (misc.command) {
    case SC_MISC_NONE:
      /*
      if
      (mutex_lock_interruptible(&pDevExt->igmp->groupsLock)
      != 0)
      {
          return -ERESTARTSYS;
      }
      if
      (mutex_lock_interruptible(&pDevExt->igmp->groupsLock)
      != 0)
      {
          return -ERESTARTSYS;
      }
      mutex_unlock(&pDevExt->igmp->groupsLock);
      mutex_unlock(&pDevExt->igmp->groupsLock);
      */
      break;

    case SC_MISC_GET_FPGA_VERSION: // fpga version
      misc.value1 = getFpgaVersion(pDevExt);
      misc.value2 = getFpgaExtendedBuildNumber(pDevExt);
      misc.mmu_is_enabled = pDevExt->pMMU != NULL;
      break;

    case SC_MISC_GET_BOARD_SERIAL: // board serial number
    {
      uint8_t mac_address[MAC_ADDR_LEN];
      fpga_readMac(pDevExt, mac_address);
      misc.value1 =
          ((unsigned int)(mac_address[4] & 0xff) << 4) +
          ((unsigned int)(mac_address[5] & 0xff) >> 4);
    } break;

    case SC_MISC_GET_DRIVER_LOG_MASK: // Get value of
                                      // logmask
      misc.value1 = logmask;
      break;

    case SC_MISC_SET_DRIVER_LOG_MASK: // Set value of
                                      // logmask
      logmask = misc.value1;
      break;

    case SC_MISC_GET_PORT_MASKS: // Get MAC, TCP, UDP and
                                 // NIC/OOB port masks
      misc.value1 =
          ((uint64_t)pDevExt->mac_lane_mask << 48) |
          ((uint64_t)pDevExt->oob_lane_mask << 32) |
          ((uint64_t)pDevExt->tcp_lane_mask << 16) |
          ((uint64_t)pDevExt->udp_lane_mask);
      misc.value2 =
          ((uint64_t)pDevExt->acl_context.acl_nif_mask
           << 48) |
          ((uint64_t)pDevExt->qspf_lane_mask << 32) |
          ((uint64_t)pDevExt->monitor_lane_mask << 16) |
          ((uint64_t)pDevExt->evaluation_mask);
      misc.number_of_dma_channels = SC_MAX_CHANNELS;
      misc.number_of_tcp_channels = max_etlm_channels;
      misc.number_of_nifs = pDevExt->nb_netdev;
      misc.is_toe_connected_to_user_logic =
          pDevExt->toeIsConnectedToUserLogic ? 1 : 0;
      misc.statistics_enabled =
          pDevExt->statistics_module_is_present ? 1 : 0;
      break;

    case SC_MISC_GET_PCI_DEVICE_NAME:
      strncpy(misc.name, pci_name(pDevExt->pdev),
              sizeof(misc.name));
      break;

    case SC_MISC_GET_INTERFACE_NAME: {
      sc_net_interface_t *nif;

      if (misc.value1 >=
          SC_MAX_NUMBER_OF_NETWORK_INTERFACES) {
        return -EINVAL;
      }
      nif = &pDevExt->nif[misc.value1];
      strncpy(misc.name, nif->netdevice->name,
              sizeof(misc.name));
    } break;

    case SC_MISC_SET_ARP_MODE:
      pDevExt->arp_context.arp_handled_by_user_space =
          misc.value1 != 0;
      break;

    case SC_MISC_SIGNAL_TEST: {
      int rc = 0;
      pid_t pid = misc.value1;
      int signalToSend = misc.value2;
      int16_t channel = 3;

      rc = send_signal_to_user_process(
          pDevExt, pid, signalToSend, channel,
          misc.pContext);
      return rc;
    }

    default:
      return -EINVAL;
    }

    if (copy_to_user((void __user *)data, &misc,
                     sizeof(misc))) {
      return -EFAULT;
    }
    return 0;
  }

  case SC_IOCTL_READ_TEMPERATURE: {
    sc_temperature_t temp;
    readFpgaTemperature(pDevExt, &temp.minTemp,
                        &temp.curTemp, &temp.maxTemp);
    if (copy_to_user((void __user *)data, &temp,
                     sizeof(temp))) {
      return -EFAULT;
    }
    return 0;
  }

  case SC_IOCTL_LINK_STATUS: {
    uint64_t linkValue = get_link_status(pDevExt);

    if (copy_to_user((void __user *)data, &linkValue,
                     sizeof(linkValue))) {
      return -EFAULT;
    }

    clearLinkStatus(pDevExt);

    return 0;
  }

  case SC_IOCTL_FPGA_RESET:

    return sc_fpga_reset(pDevExt);

  case SC_IOCTL_ENABLE_USER_LOGIC_INTERRUPTS: {
    uint8_t previousMask;

    sc_user_logic_interrupts user_logic_interrupts;
    if (copy_from_user(&user_logic_interrupts, (void *)data,
                       sizeof(user_logic_interrupts))) {
      return -EFAULT;
    }

    previousMask = pDevExt->user_logic_interrupts.mask;
    pDevExt->user_logic_interrupts = user_logic_interrupts;
    pDevExt->user_logic_interrupts.previous_mask =
        previousMask;

    enable_user_logic_interrupts(
        pDevExt, user_logic_interrupts.mask);

    return 0;
  }

  case SC_IOCTL_ALLOC_CHANNEL: {
    sc_alloc_info_t alloc_info;
    if (copy_from_user(&alloc_info, (void *)data,
                       sizeof(alloc_info))) {
      return -EFAULT;
    }

    // Fail if a channel is already allocated on this
    // context
    if (f->private_data) {
      return -EINVAL;
    }

    rc = sc_allocate_channel(f, pDevExt, alloc_info.type,
                             alloc_info.dma_channel_no,
                             alloc_info.network_interface,
                             alloc_info.enableDma);
    if (rc == 0) {
      channel = (fb_channel_t *)f->private_data;
      alloc_info.dma_channel_no =
          fb_phys_channel(channel->dma_channel_no);
      alloc_info.prb_size = channel->prbSize;
      if (copy_to_user((void __user *)data, &alloc_info,
                       sizeof(alloc_info))) {
        return -EFAULT;
      }
    }
    return rc;
  }

  case SC_IOCTL_DEALLOC_CHANNEL:
    // Explicit deallocation of channels not currently
    // supported. Channel is released/deallocated when the
    // corresponding file descriptor is closed.
    return -EFAULT;

  case SC_IOCTL_GET_LOCAL_ADDR: {
    sc_local_addr_t l;
    sc_net_interface_t *nif;
    if (copy_from_user(&l, (void *)data, sizeof(l))) {
      return -EFAULT;
    }
    if (l.network_interface >= SC_NO_NETWORK_INTERFACES) {
      return -EINVAL;
    }
    nif = &pDevExt->nif[l.network_interface];
    l.local_ip_addr = get_local_ip(nif);
    {
      IPBUF(0);
      PRINTK("nif on network interface %u local IP read as "
             "%s\n",
             nif->network_interface,
             LogIP(0, get_local_ip(nif)));
    }
    l.netmask = nif->netmask;
    l.gateway = nif->gateway;
    memcpy(l.mac_addr, nif->mac_addr, MAC_ADDR_LEN);
    if (copy_to_user((void __user *)data, &l, sizeof(l))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_SET_LOCAL_ADDR: {
    sc_local_addr_t l;
    sc_net_interface_t *nif;
    if (copy_from_user(&l, (void *)data, sizeof(l))) {
      return -EFAULT;
    }
    if (l.network_interface >= SC_NO_NETWORK_INTERFACES) {
      return -EINVAL;
    }
    nif = &pDevExt->nif[l.network_interface];
    // fixed by ekaline
    set_local_ip(nif, l.local_ip_addr);
    {
      IPBUF(0);
      PRINTK("nif local IP set to %s\n",
             LogIP(0, get_local_ip(nif)));
    }
#if 0
                if (nif->netdevice)
                {
                    struct sockaddr sa;
                    uint8_t *mac = nif->mac_addr;
                    if (memcmp(l.mac_addr, "\x00\x00\x00\x00\x00\x00", MAC_ADDR_LEN) != 0)
                    {
                        mac = l.mac_addr;
                    }

                    sa.sa_family = nif->netdevice->type;
                    memcpy(sa.sa_data, mac, MAC_ADDR_LEN);
                    rtnl_lock();
                    rc = dev_set_mac_address(nif->netdevice, &sa);
                    rtnl_unlock();
                    if (rc)
                    {
                        PRINTK_ERR("failed to set linux interface MAC address. Err %d\n", rc);
                        return rc;
                    }
                }

                if (memcmp(l.mac_addr, "\x00\x00\x00\x00\x00\x00", MAC_ADDR_LEN) != 0)
                {
                    memcpy(nif->mac_addr, l.mac_addr, MAC_ADDR_LEN);
                }
                set_local_ip(nif, l.local_ip_addr);
                { IPBUF(0); PRINTK("nif local IP set to %s\n", LogIP(0, get_local_ip(nif))); }
                nif->netmask = l.netmask;
                nif->gateway = l.gateway;
                setSourceMac(pDevExt, l.network_interface, nif->mac_addr);
                //setSrcIp(pDevExt, l.lane, get_local_ip(nif));
#endif
    if (is_valid_tcp_lane(pDevExt, l.network_interface)) {
      etlm_setSrcMac(pDevExt, nif->mac_addr);
      etlm_setSrcIp(pDevExt, get_local_ip(nif));
    }
  }
    return 0;

  case SC_IOCTL_TIMERMODE: {
    uint32_t mode;

    if (copy_from_user(&mode, (void *)data, sizeof(mode)))
      return -EINVAL;

    setClockSyncMode(pDevExt, mode);
    return 0;
  }

  case SC_IOCTL_ACL_CMD:
    if (pDevExt->acl_context.acl_nif_mask != 0) {
      return sc_acl_cmd(&pDevExt->acl_context, data);
    }
    return -ENODEV; // 19 = No such device

  case SC_IOCTL_GET_DRIVER_VERSION: {
    sc_sw_version_t driver_version;
    strncpy(driver_version.version, release_version,
            sizeof(driver_version.version) /
                    sizeof(driver_version.version[0]) -
                1);
    if (copy_to_user((void __user *)data, &driver_version,
                     sizeof(driver_version))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_GET_SYSTEM_INFO: {
    sc_system_info_t systemInfo;
    systemInfo.number_of_cards_found = s_NumberOfCardsFound;
    systemInfo.number_of_test_fpga_cards_found =
        s_NumberOfTestFpgaCardsFound;

    if (copy_to_user((void __user *)data, &systemInfo,
                     sizeof(systemInfo))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_DMA_INFO: {
    sc_dma_info_t dma_info;

    if (copy_from_user(&dma_info, (void *)data,
                       sizeof(dma_info))) {
      return -EFAULT;
    }

    if (dma_info.dma_channel >= SC_MAX_CHANNELS) {
      PRINTK_ERR("Invalid DMA channel number: %u\n",
                 dma_info.dma_channel);
      return -EINVAL;
    }

    if (dma_info.cmd == SC_DMA_INFO_GET) {
      dma_info.bar0_physical_address =
          pci_resource_start(pDevExt->pdev, BAR_0);
      dma_info.bar0_kernel_virtual_address =
          pDevExt->bar0_va;

      dma_info.user_logic_tx_send_register.pcie_bar = BAR_0;
      dma_info.user_logic_tx_send_register
          .offset_in_pcie_bar = 0;
      dma_info.user_logic_tx_send_register
          .physical_address = 0;
      dma_info.user_logic_tx_send_register
          .kernel_virtual_address = 0;
      dma_info.user_logic_tx_send_register.is_readable =
          false;
      dma_info.user_logic_tx_send_register.is_writable =
          true;

      dma_info.user_logic_tx_send_register.pcie_bar = BAR_0;
      dma_info.user_logic_tx_send_register
          .offset_in_pcie_bar = FB_RXDMA_REG(
          dma_info.dma_channel, PKG_RXDMA_RD_MARK);
      dma_info.user_logic_tx_send_register
          .physical_address =
          dma_info.bar0_physical_address +
          FB_RXDMA_REG(dma_info.dma_channel,
                       PKG_RXDMA_RD_MARK);
      dma_info.user_logic_tx_send_register
          .kernel_virtual_address =
          pDevExt->bar0_va +
          FB_RXDMA_REG(dma_info.dma_channel,
                       PKG_RXDMA_RD_MARK);
      dma_info.user_logic_tx_send_register.is_readable =
          false;
      dma_info.user_logic_tx_send_register.is_writable =
          true;

      PRINTK_C(LOGALL(LOG_GROUP_PLDMA),
               "Getting Tx DMA send register pa: 0x%llX\n",
               dma_info.user_logic_tx_send_register
                   .physical_address);
    } else {
      switch (dma_info.cmd) {
      case SC_RX_DMA_START:
      case SC_RX_DMA_STOP:
      case SC_DMA_INFO_START_PL_DMA:
      case SC_DMA_INFO_STOP_PL_DMA: {
        unsigned group = dma_info.dma_channel /
                         SC_NUMBER_OF_PLDMA_GROUPS;
        if (group == 0) {
          PRINTK_ERR("Rx PL DMA group 0 is reserved for "
                     "driver internal use!\n");
          return -EINVAL;
        }

        switch (dma_info.cmd) {
        case SC_RX_DMA_START:
          if (pDevExt->channel[dma_info.dma_channel] ==
              NULL) {
            PRINTK_ERR("Channel %u is not allocated, "
                       "cannot change settings.\n",
                       dma_info.dma_channel);
            return -EINVAL;
          }
          if (dma_info.dma_channel < FIRST_UL_CHANNEL ||
              dma_info.dma_channel >=
                  FIRST_UL_CHANNEL +
                      SC_MAX_ULOGIC_CHANNELS) {
            PRINTK_ERR("Channel %u is not a user logic "
                       "channel, cannot change settings.\n",
                       dma_info.dma_channel);
            return -EINVAL;
          }

          pDevExt->channel[dma_info.dma_channel]->dma_info =
              dma_info;

          // move to group function
          // setup_group_rx_pl_dma(pDevExt,
          // dma_info.dma_channel,
          // dma_info.pl_dma_group_physical_address, true);

          start_rx_dma_address(
              pDevExt, dma_info.dma_channel,
              dma_info.prb_physical_start_address,
              dma_info.prb_length, false);

          PRINTK_C(LOGALL(LOG_GROUP_PLDMA),
                   "Started PL DMA group %u channel %u\n",
                   group, dma_info.dma_channel);
          break;

        case SC_RX_DMA_STOP:
          stop_dma(pDevExt, dma_info.dma_channel);
          PRINTK_C(LOG(LOG_GROUP_PLDMA),
                   "Stopped PL DMA group %u channel %u\n",
                   group, dma_info.dma_channel);
          break;

        case SC_DMA_INFO_START_PL_DMA:
          pDevExt->plDmaPaGroups[group] =
              dma_info.pl_dma_group_physical_address;
          pDevExt->plDmaVaGroups[group] =
              NULL; /* We do not know the virtual address of
                       external PL DMA so we cannot read
                       this group in the driver! */
          setup_external_group_rx_pl_dma(
              pDevExt, dma_info.dma_channel,
              dma_info.pl_dma_group_physical_address, true);
          break;

        case SC_DMA_INFO_STOP_PL_DMA:
          setup_external_group_rx_pl_dma(
              pDevExt, dma_info.dma_channel,
              dma_info.pl_dma_group_physical_address,
              false);
          /* Restore internal PL DMA settings */
          pDevExt->plDmaVaGroups[group] =
              (sc_pldma_t *)&pDevExt->plDmaVa->lastPacket
                  [SC_NUMBER_OF_DMA_CHANNELS_PER_PLDMA_GROUP *
                   group];
          pDevExt->plDmaPaGroups[group] =
              pDevExt->plDmaPa +
              SC_NUMBER_OF_DMA_CHANNELS_PER_PLDMA_GROUP *
                  group * sizeof(void *);
          break;

        default:
          break;
        }
      } break;

      case SC_SET_PL_DMA_GROUP_OPTIONS:
        enable_group_rx_pl_dma(
            pDevExt, dma_info.group,
            dma_info.pl_dma_flush_packet_count,
            dma_info.pl_dma_flush_time_in_ns, true);
        PRINTK_C(
            LOGALL(LOG_GROUP_PLDMA),
            "Set PL DMA group %u channel %u options: flush "
            "packet count %u, flush time %u ns\n",
            dma_info.group, dma_info.dma_channel,
            dma_info.pl_dma_flush_packet_count,
            dma_info.pl_dma_flush_time_in_ns);
        break;

      case SC_GET_REGISTER_INFO:
        dma_info.register_info.pcie_bar = BAR_0;
        dma_info.register_info.is_readable = false;
        dma_info.register_info.is_writable = true;

        switch (dma_info.register_info.register_id) {
        case SC_FPGA_REGISTER_USER_LOGIC_TX_SEND:
          dma_info.register_info.offset_in_pcie_bar = 0;
          dma_info.register_info.physical_address = 0;
          dma_info.register_info.kernel_virtual_address = 0;
          break;
        case SC_FPGA_REGISTER_RX_READ_MARKER:
          dma_info.register_info.offset_in_pcie_bar =
              FB_RXDMA_REG(dma_info.dma_channel,
                           PKG_RXDMA_RD_MARK);
          dma_info.register_info.physical_address =
              pci_resource_start(pDevExt->pdev, BAR_0) +
              FB_RXDMA_REG(dma_info.dma_channel,
                           PKG_RXDMA_RD_MARK);
          dma_info.register_info.kernel_virtual_address =
              pDevExt->bar0_va +
              FB_RXDMA_REG(dma_info.dma_channel,
                           PKG_RXDMA_RD_MARK);
          break;
        case SC_FPGA_REGISTER_NONE:
        default:
          PRINTK_ERR("Invalid register id %u\n",
                     dma_info.register_info.register_id);
          return -EINVAL;
        }
        break;

      case SC_GET_CHANNEL_INFO: {
        const fb_channel_t *pChannel =
            pDevExt->channel[dma_info.dma_channel];

        memset(&dma_info.prb_memory_info, 0,
               sizeof(dma_info.prb_memory_info));
        if (pChannel != NULL) {
          dma_info.prb_memory_info.length =
              pChannel->prbSize;
          dma_info.prb_memory_info.kernel_virtual_address =
              (uint64_t)pChannel->recv;
          dma_info.prb_memory_info.physical_address =
              pChannel->recvDma;
          dma_info.prb_memory_info
              .user_space_virtual_address = 0;
          dma_info.prb_memory_info.fpga_start_address =
              pChannel->prbFpgaStartAddress;
        }
      } break;

      default:
        PRINTK_ERR("unknown external DMA info command: %u",
                   dma_info.cmd);
        return -EINVAL;
      }
    }

    if (copy_to_user((void __user *)data, &dma_info,
                     sizeof(dma_info))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_MEMORY_ALLOC_FREE:
    return -EINVAL;

  case SC_IOCTL_REFERENCE_COUNT: {
    /* Centralized reference counting service for user space
     * API library and applications */
    sc_reference_count_t referenceCount;
    static int64_t referenceCounts[SC_REFERENCE_COUNT_MAX] =
        {0};

    if (copy_from_user(&referenceCount, (void *)data,
                       sizeof(referenceCount))) {
      return -EFAULT;
    }

    switch (referenceCount.which) {
    case SC_REFERENCE_COUNT_USER_LOGIC_DEMO:
      switch (referenceCount.what) {
      case SC_REFERENCE_COUNT_INCREMENT:
        referenceCount.reference_count =
            __sync_add_and_fetch(
                &referenceCounts[referenceCount.which], 1);
        break;
      case SC_REFERENCE_COUNT_DECREMENT:
        referenceCount.reference_count =
            __sync_sub_and_fetch(
                &referenceCounts[referenceCount.which], 1);
        break;
      default:
        PRINTK_ERR(
            "Invalid referenceCount.which %d what %d\n",
            referenceCount.which, referenceCount.what);
        break;
      }
      break;

    default:
      PRINTK_ERR("Invalid referenceCount.which %d\n",
                 referenceCount.which);
      break;
    }

    if (copy_to_user((void __user *)data, &referenceCount,
                     sizeof(referenceCount))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_GET_TCP_STATUS: {
    uint32_t stat;

    if (copy_from_user(&stat, (void *)data, sizeof(stat))) {
      return -EFAULT;
    }

    if (pDevExt->tcp_lane_mask == 0) {
      PRINTK_ERR("SC_IOCTL_GET_TCP_STATUS: error, there is "
                 "no TCP engine in this setup\n");
      return -ENODEV;
    }

    if (stat >= SC_MAX_TCP_CHANNELS) {
      return -EINVAL;
    }

    stat = etlm_getTcpState(pDevExt, stat);

    if (copy_to_user((void __user *)data, &stat,
                     sizeof(stat))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_ARP: {
    int rc;
    sc_arp_t arp;
    if (copy_from_user(&arp, (void *)data, sizeof(arp))) {
      return -EFAULT;
    }

    pDevExt->arp_context.timeout_in_jiffies =
        msecs_to_jiffies(arp.arp_entry_timeout);

    switch (arp.command) {
    case ARP_COMMAND_SEND: {
      const sc_net_interface_t *nif;

      if (!is_valid_tcp_lane(pDevExt, arp.lane)) {
        PRINTK_ERR("Invalid ARP TOE lane %u\n", arp.lane);
        return -EINVAL;
      }
      nif = &pDevExt->nif[arp.lane];

      if ((rc = sendArpRequest(nif, arp.remote_ip,
                               arp.vlanTag)) !=
          NET_XMIT_SUCCESS) {
        IPBUF(1);
        PRINTK_ERR("sendArpRequest failed with rc = %d "
                   "(%s), remote IP %s, VLAN %d\n",
                   rc, send_skb_reason(rc),
                   LogIP(1, arp.remote_ip), arp.vlanTag);
        return -EBUSY;
      }

      // Register an IP address with zeroed MAC address;
      // this means we have sent an ARP request out.
      arp.arp_table_index = arp_register_ip_address(
          &pDevExt->arp_context, arp.remote_ip);

      if (LOG(LOG_ARP)) {
        IPBUF(1);
        PRINTK("ARP sent to remote host %s on lane %u, "
               "VLAN %d\n",
               LogIP(1, arp.remote_ip), arp.lane,
               arp.vlanTag);
      }
    } break;

    case ARP_COMMAND_POLL:
      if (arp_find_mac_address(
              &pDevExt->arp_context, arp.remote_ip,
              arp.remote_mac_address) == -1) {
        memset(arp.remote_mac_address, 0, MAC_ADDR_LEN);
        return -ENXIO;
      }
      break;

    case ARP_COMMAND_DUMP:
      arp_dump(&pDevExt->arp_context, false);
      break;

    case ARP_COMMAND_DUMP_ALL:
      arp_dump(&pDevExt->arp_context, true);
      break;

    default:
      PRINTK_ERR("Invalid ARP command %d\n", arp.command);
      return -EINVAL;
    }

    if (copy_to_user((void __user *)data, &arp,
                     sizeof(arp))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_EMPTY:
    return 0; // "Empty" ioctl call just for measuring the
              // execution time of an ioctl that does
              // nothing.

  default:
    break;
  }

  // Functionality that is specific to TCP, UDP and User
  // Logic channels:

  if (!f->private_data) {
    PRINTK_ERR("Private data expected on file desciptor "
               "related to a channel, ioctl operation %s\n",
               IoCtlCmd(pDevExt, op));
    LogAllIoCtlCmds(pDevExt);
    return -EINVAL;
  }

  channel = (fb_channel_t *)f->private_data;

  if (channel->type == SC_DMA_CHANNEL_TYPE_TCP) {
    tcp_conn = channel->dma_channel_no - FIRST_TCP_CHANNEL;

    if (tcp_conn >= max_etlm_channels) {
      PRINTK_ERR("TCP channel number %u is over maximum "
                 "allowed %u, ioctl operation %s\n",
                 tcp_conn, max_etlm_channels - 1,
                 IoCtlCmd(pDevExt, op));
      return -EINVAL;
    }
  } else if (channel->type ==
             SC_DMA_CHANNEL_TYPE_USER_LOGIC) {
    if (channel->dma_channel_no >=
        FIRST_UL_CHANNEL + SC_MAX_ULOGIC_CHANNELS) {
      PRINTK_ERR("User logic channel number %u is over "
                 "maximum allowed %u, ioctl operation %s\n",
                 channel->dma_channel_no,
                 FIRST_UL_CHANNEL + SC_MAX_ULOGIC_CHANNELS -
                     1,
                 IoCtlCmd(pDevExt, op));
      return -EINVAL;
    }

    PRINTK_C(LOG(LOG_USER_LOGIC | LOG_DETAIL),
             "User logic channel number %u, ioctl "
             "operation %ss\n",
             channel->dma_channel_no,
             IoCtlCmd(pDevExt, op));
  } else if (channel->type ==
             SC_DMA_CHANNEL_TYPE_UDP_MULTICAST) {
    if (channel->dma_channel_no < FIRST_UDP_CHANNEL ||
        channel->dma_channel_no >=
            FIRST_UDP_CHANNEL + SC_MAX_UDP_CHANNELS) {
      PRINTK_ERR("User logic channel number %u is out of "
                 "range [%u-%u], ioctl operation %s\n",
                 channel->dma_channel_no, FIRST_UDP_CHANNEL,
                 FIRST_UDP_CHANNEL + SC_MAX_UDP_CHANNELS -
                     1,
                 IoCtlCmd(pDevExt, op));
      return -EINVAL;
    }

    PRINTK_C(LOG(LOG_USER_LOGIC | LOG_DETAIL),
             "UDP channel number %u operation %s\n",
             channel->dma_channel_no,
             IoCtlCmd(pDevExt, op));
  } else {
    PRINTK_ERR("Channel type %d is not one of %d, %d or "
               "%d, ioctl operation %s\n",
               channel->type, SC_DMA_CHANNEL_TYPE_TCP,
               SC_DMA_CHANNEL_TYPE_UDP_MULTICAST,
               SC_DMA_CHANNEL_TYPE_USER_LOGIC,
               IoCtlCmd(pDevExt, op));
  }

  switch (op) {
  case SC_IOCTL_USERSPACE_RX:
    if (unlikely(channel->owner_rx == OWNER_KERNEL)) {
      PRINTK_ERR(
          "Rx on channel %i is reserved for kernel already",
          channel->dma_channel_no);
      return -EFAULT;
    }
    channel->owner_rx = OWNER_USERSPACE;
    return 0;

  case SC_IOCTL_USERSPACE_TX:
    if (unlikely(channel->owner_tx == OWNER_KERNEL)) {
      PRINTK_ERR(
          "Tx on channel %i is reserved for kernel already",
          channel->dma_channel_no);
      return -EFAULT;
    }
    channel->owner_tx = OWNER_USERSPACE;
    return 0;

  case SC_IOCTL_GET_BUCKET_HWADDR:
    if ((notCopiedBytes =
             copy_from_user(&arg64, (void *)data,
                            sizeof(uint64_t))) != 0) {
      PRINTK_ERR("SC_IOCTL_GET_BUCKET_HWADDR: %u bytes not "
                 "copied from user\n",
                 notCopiedBytes);
      return -EFAULT;
    }
    if (arg64 >= MAX_BUCKETS) {
      PRINTK_ERR("SC_IOCTL_GET_BUCKET_HWADDR: invalid "
                 "bucket index %llu\n",
                 arg64);
      return -EINVAL;
    }
    arg64 = channel->dmaBucket[arg64];
    if ((notCopiedBytes =
             copy_to_user((void __user *)data, &arg64,
                          sizeof(uint64_t))) != 0) {
      PRINTK_ERR("SC_IOCTL_GET_BUCKET_HWADDR: %u bytes not "
                 "copied to user\n",
                 notCopiedBytes);
      return -EFAULT;
    }
    return 0;

  case SC_IOCTL_GET_RECV_HWADDR:

    arg64 = channel->recvDma;

    if (copy_to_user((void __user *)data, &arg64,
                     sizeof(uint64_t))) {
      return -EFAULT;
    }
    return 0;

  case SC_IOCTL_CONNECT: {
    sc_connection_info_t conn_info;
    if (copy_from_user(&conn_info, (void *)data,
                       sizeof(conn_info))) {
      return -EFAULT;
    }

    if (pDevExt->tcp_lane_mask == 0) {
      return -ENODEV;
    }
    rc = fbConnect(channel, &conn_info);

    if (LOG(LOG_ETLM_TCP_CONNECTION)) {
      IPBUF(1);
      MACBUF(2);
      IPBUF(3);

      PRINTK("SC_IOCTL_CONNECT: channel=%d, remote_ip=%s, "
             "remote_port=%d, local_port=%d, "
             "remote_mac_address=%s"
             ", local IP=%s, VLAN tag=%d, rc=%d\n",
             channel->dma_channel_no,
             LogIP(1, conn_info.remote_ip),
             conn_info.remote_port, conn_info.local_port,
             LogMAC(2, conn_info.remote_mac_address),
             LogIP(3, conn_info.local_ip_address),
             conn_info.vlan_tag, rc);
    }

    if (rc) {
      PRINTK_ERR_C(LOG(LOG_ETLM_TCP_CONNECTION) ||
                       LOG(LOG_ERROR),
                   "fbConnect rc %d\n", rc);
      return -EBUSY;
    }

    if (conn_info.timeout > 0) {
      int channelConnectionState;

      if (wait_for_completion_interruptible_timeout(
              &channel->connectCompletion,
              msecs_to_jiffies(conn_info.timeout)) <= 0) {
        unsigned int retval =
            etlm_getTcpState(channel->pDevExt, tcp_conn);
        PRINTK("SC_IOCTL_CONNECT: Connection not "
               "established after %u milliseconds - "
               "current tcp status %s\n",
               conn_info.timeout, EtlmState(retval));
        return -ERESTARTSYS;
      }

      channelConnectionState = get_channel_connection_state(
          pDevExt, channel, nameof(SC_IOCTL_CONNECT), true);
      if (channelConnectionState != FB_CON_CONNECTED) {
        PRINTK(
            "ERROR: SC_IOCTL_CONNECT: ioctl completion "
            "failed ; ConnectionState=%s, ETLM State=%s\n",
            ConnState(channelConnectionState),
            EtlmState(etlm_getTcpState(channel->pDevExt,
                                       tcp_conn)));
        return -EBUSY;
      }
    }

    PRINTK_C(LOG(LOG_ETLM_TCP_CONNECTION),
             "SC_IOCTL_CONNECT: connected successfully\n");

    conn_info.local_port =
        channel->connectionInfo.local_port;

    if (copy_to_user((void __user *)data, &conn_info,
                     sizeof(conn_info))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_DISCONNECT: {
    int channelConnectionState;
    sc_disconnect_info_t disconnect_info;
    if (copy_from_user(&disconnect_info, (void *)data,
                       sizeof(disconnect_info))) {
      return -EFAULT;
    }

    if (pDevExt->tcp_lane_mask == 0) {
      return -ENODEV;
    }

    channelConnectionState = get_channel_connection_state(
        pDevExt, channel, nameof(SC_IOCTL_DISCONNECT) "#1",
        true);
    if (channelConnectionState != FB_CON_CLOSED) {
      fbDisconnect(channel);

      if (disconnect_info.timeout > 0) {
        if (wait_for_completion_interruptible_timeout(
                &channel->connectCompletion,
                msecs_to_jiffies(
                    disconnect_info.timeout)) <= 0) {
          unsigned int tcpState =
              etlm_getTcpState(channel->pDevExt, tcp_conn);
          PRINTK("SC_IOCTL_DISCONNECT: Could not "
                 "disconnect after %u milliseconds - "
                 "current tcp status %s\n",
                 disconnect_info.timeout,
                 EtlmState(tcpState));
          return -ERESTARTSYS;
        }

        channelConnectionState =
            get_channel_connection_state(
                pDevExt, channel,
                nameof(SC_IOCTL_DISCONNECT) "#2", true);
        if (channelConnectionState != FB_CON_CLOSED) {
          int tcpState =
              etlm_getTcpState(channel->pDevExt, tcp_conn);
          PRINTK("ERROR: SC_IOCTL_DISCONNECT: ioctl "
                 "completion failed; ConnectionState=%s, "
                 "ETLM State=%s\n",
                 ConnState(channelConnectionState),
                 EtlmState(tcpState));
          return -EBUSY;
        }
      }
    }
  }
    return 0;

  case SC_IOCTL_LISTEN: {
    int channelConnectionState;
    sc_listen_info_t listen_info;
    if (copy_from_user(&listen_info, (void *)data,
                       sizeof(listen_info))) {
      return -EFAULT;
    }

    listen_info.remote_ip_address = 0;
    listen_info.remote_ip_port = 0;
    memset(listen_info.remote_mac_address, 0,
           sizeof(listen_info.remote_mac_address));

    rc = fbListen(channel, &listen_info);
    if (rc)
      return -EBUSY;

    if (listen_info.cancel == 0) {
      // Listen

      channel->listen_pending = true;
      channel->listen_cancelled = false;
      if (listen_info.timeout <
          ~0U) // timeout -1 in API comes here as ~0U!
      {
        if (wait_for_completion_interruptible_timeout(
                &channel->connectCompletion,
                msecs_to_jiffies(listen_info.timeout)) <=
            0) {
          channel->listen_pending = false;
          channel->listen_cancelled = false;
          return -ERESTARTSYS;
        }
      } else if (wait_for_completion_interruptible(
                     &channel->connectCompletion) < 0) {
        channel->listen_pending = false;
        channel->listen_cancelled = false;
        return -ERESTARTSYS;
      }

      channelConnectionState = get_channel_connection_state(
          pDevExt, channel, nameof(SC_IOCTL_LISTEN), true);
      if (channelConnectionState == FB_CON_CONNECTED) {
        // Listen resulted in a connection
        etlm_getRemoteIpPortMac(
            pDevExt, tcp_conn,
            &listen_info.remote_ip_address,
            &listen_info.remote_ip_port,
            listen_info.remote_mac_address);
        if (copy_to_user((void __user *)data, &listen_info,
                         sizeof(listen_info))) {
          return -EFAULT;
        }
      } else {
        if (channel->listen_cancelled) {
          channel->listen_pending = false;
          channel->listen_cancelled = false;
          if (channelConnectionState != FB_CON_CLOSED) {
            unsigned tcp_conn =
                channel->dma_channel_no - FIRST_TCP_CHANNEL;
            PRINTK_ERR(
                "Listen cancel failed, TCP connection "
                "state is not closed on DMA channel %u, "
                "aborting the connection.\n",
                channel->dma_channel_no);
            etlm_abort_conn(pDevExt, tcp_conn);
          }

          PRINTK_C(LOG(LOG_CONNECTION),
                   "Listen successfully cancelled on DMA "
                   "channel %u\n",
                   channel->dma_channel_no);

          return 0;
        }

        PRINTK_C(LOG(LOG_CONNECTION),
                 "ERROR: SC_IOCTL_LISTEN ioctl completion "
                 "failed ;"
                 " ConnectionState=%d\n",
                 channelConnectionState);

        channel->listen_pending = false;
        channel->listen_cancelled = false;
        return -EBUSY;
      }
    } else {
      // Cancel listen

      channel->listen_cancelled = false;
      if (channel->listen_pending) {
        // Listen pending, cancel it
        unsigned tcp_conn =
            channel->dma_channel_no - FIRST_TCP_CHANNEL;
        channel->listen_pending = false;
        complete(&channel->connectCompletion);
        etlm_abort_conn(pDevExt, tcp_conn);
      } else {
        // No pending listen!
        channel->listen_pending = false;
        return -ENOENT;
      }
    }
  }

    channel->listen_pending = false;
    channel->listen_cancelled = false;

    // TODO: get remote IP address, port and MAC from TOE
    // TCB and return them to caller.

    return 0;

  case SC_IOCTL_SET_READMARK: {
    uint64_t value;
    sc_packet *releaseVa;

    if (copy_from_user(&value, (void *)data,
                       sizeof(value))) {
      return -EFAULT;
    }

    releaseVa =
        (sc_packet *)((uint8_t *)(channel->recv) + value);
    set_rx_dma_read_mark(pDevExt, channel->dma_channel_no,
                         (uint8_t *)releaseVa);
    channel->pCurrentPacket = releaseVa;
    channel->curLength = 0;
    channel->dataOffset = channel->curLength;
  }
    return 0;

  case SC_IOCTL_ABORT_ETLM_CHANNEL:
    if (pDevExt->tcp_lane_mask == 0) {
      return -ENODEV;
    }
    etlm_abort_conn(channel->pDevExt, tcp_conn);
    return 0;

  case SC_IOCTL_LAST_ACK_NO: {
    uint32_t lastAckNo;

    if (pDevExt->tcp_lane_mask == 0) {
      PRINTK_ERR("SC_IOCTL_GET_TCP_STATUS: error, there is "
                 "no TCP engine in this setup\n");
      return -ENODEV;
    }

    lastAckNo = etlm_getTcpConnSndUna(pDevExt, tcp_conn);
    lastAckNo -= channel->initialSeqNumber;
    // PRINTK("SC_IOCTL_LAST_ACK_NO: lastAckNo=0x%08x,
    // initialSeqNumber=0x%08x\n",
    //                           lastAckNo,
    //                           channel->initialSeqNumber);

    if (copy_to_user((void __user *)data, &lastAckNo,
                     sizeof(uint32_t))) {
      return -EFAULT;
    }
  }
    return 0;

  case SC_IOCTL_IGMPINFO: {
    sc_igmp_info_t value;
    if (copy_from_user(&value, (void *)data,
                       sizeof(value))) {
      return -EFAULT;
    }

    return handle_igmp_ioctl(&pDevExt->igmpContext, channel,
                             &value);
  }
    return 0;

  case SC_IOCTL_RESERVE_FIFO: {
    int rc;
    sc_fifo_t request;
    if (copy_from_user(&request, (void *)data,
                       sizeof(request))) {
      return -EFAULT;
    }
    PRINTK("SC_IOCTL_RESERVE_FIFO: %d %d\n", request.normal,
           request.priority);

    // We use the channelListLock to not create yet another
    // lock
    if (mutex_lock_interruptible(&pDevExt->channelListLock))
      return -ERESTARTSYS;

    rc = channel_reserve_tx_fifo_entries(
        channel, request.normal, request.priority);

    mutex_unlock(&pDevExt->channelListLock);

    if (rc)
      return -EINVAL;
  }
    return 0;

#if 0 // These are currently not used for anything
        case SC_IOCTL_ALLOC_DMABUF:
            {
                sc_dmabuf_t dmabuf;
                if (copy_from_user(&dmabuf, (void*)data, sizeof(dmabuf)))
                {
                    return -EFAULT;
                }

                rc = channel_allocate_dmabuf(channel, f, &dmabuf);
                if (rc)
                    return rc;

                if (copy_to_user((void __user *)data, &dmabuf, sizeof(dmabuf)))
                {
                    return -EFAULT;
                }
            }
            return 0;

        case SC_IOCTL_DEALLOC_DMABUF:
            {
                void * dmabuf;
                if (copy_from_user(&dmabuf, (void*)data, sizeof(dmabuf)))
                {
                    return -EFAULT;
                }

                rc = channel_free_dmabuf(channel, dmabuf);xxx
            }
            return rc;
#endif

  case SC_IOCTL_ETLM_DATA: {
    sc_etlm_data_t etlm_data;

    if (copy_from_user(&etlm_data, (void *)data,
                       sizeof(etlm_data)) != 0) {
      return -EINVAL;
    }

    if (pDevExt->tcp_lane_mask == 0) {
      return -ENODEV;
    }

    switch (etlm_data.operation) {
    case ETLM_GET_WRITE_BYTES_SEQ_OFFSET:
      etlm_data.u.write_bytes_sequence_offset =
          etlm_getInitialWriteSequenceOffset(
              pDevExt, etlm_data.channel_number);
      break;

    case ETLM_GET_TCB_SND_UNA:
      etlm_data.u.tcb_snd_una = etlm_getTcpConnSndUna(
          pDevExt, etlm_data.channel_number);
      break;

    case ETLM_GET_SIZES:
      etlm_getCapabilities(
          pDevExt,
          &etlm_data.u.capabilities
               .number_of_connections_supported,
          &etlm_data.u.capabilities.tx_packet_size,
          &etlm_data.u.capabilities.rx_packet_size,
          &etlm_data.u.capabilities.tx_total_window_size,
          &etlm_data.u.capabilities.rx_total_window_size,
          &etlm_data.u.capabilities.vlan_supported);
      etlm_data.u.capabilities.tx_window_size =
          pDevExt->toe_tx_win_size;
      etlm_data.u.capabilities.rx_window_size =
          pDevExt->toe_rx_win_size;
      break;

    default:
      PRINTK_ERR("Unknown ETLM data command %u\n",
                 etlm_data.operation);
      return -EINVAL;
    }

    if (copy_to_user((void *)data, &etlm_data,
                     sizeof(etlm_data)) != 0) {
      return -EFAULT;
    }
  }
    return 0;

  default:
    break;
  };
  return -ENOTTY;
}

/**
 *  Linux calls this function when user space programs make
 * system call open a related device file in the /dev/
 * directory.
 */
static int sc_open(struct inode *node, struct file *f) {
  unsigned int minor = get_minor_from_file(f);
  device_context_t *pDevExt = get_sc_device_from_file(f);
  int rc = 0;

  if (pDevExt == NULL) {
    return -ENODEV;
  }

  PRINTK_C(LOG(LOG_FILE_OPS),
           ">sc_open(f=%p, f->private_data=%p, minor=%u)\n",
           f, f->private_data, minor);

  if (pDevExt->reset_fpga) {
    PRINTK_C(LOG(LOG_FPGA_RESET),
             "FPGA RESET in progress in function %s\n",
             __func__);

    return -EBUSY; // Do not do anything else while FPGA
                   // reset is in progress.
  }

  if (support_ul_devices) {
    if (minor >= 16 && minor < 144) {
      // This is /dev/ul* device
      unsigned int ulChannelNumber =
          (minor - 16) %
          8; // ul channel number per card in range [0-7].
      rc = sc_allocate_channel(
          f, pDevExt, SC_DMA_CHANNEL_TYPE_USER_LOGIC,
          ulChannelNumber, 0, 1);
      if (rc < 0) {
        goto sc_open_exit;
      }
    } else {
      // This is not a user logic device, just continue
    }
  }

  rc = nonseekable_open(node, f);

sc_open_exit:

  PRINTK_C(LOG(LOG_FILE_OPS), "<sc_open(): %d\n", rc);

  return rc;
}

static unsigned int sc_ul_poll(struct file *f,
                               poll_table *wait) {
  if (!f->private_data) {
    return -EINVAL; // No channel related to this file
                    // descriptor
  }

  {
    unsigned int mask = 0;
    fb_channel_t *const channel =
        (fb_channel_t *)f->private_data;
    device_context_t *pDevExt = get_sc_device_from_file(f);

    if (pDevExt == NULL) {
      return -ENODEV;
    }

    if (pDevExt->reset_fpga) {
      PRINTK_C(LOG(LOG_FPGA_RESET),
               "FPGA RESET in progress in function %s\n",
               __func__);

      return -EBUSY; // Do not do anything else while FPGA
                     // reset is in progress.
    }

    if (channel->owner_rx == OWNER_UNDECIDED) {
      channel->owner_rx = OWNER_KERNEL;
    }
    if (channel->owner_rx == OWNER_KERNEL) {
      poll_wait(f, &channel->inq, wait);

      if (channel_rx_data_available(channel))
        mask |= POLLIN | POLLRDNORM;
    }

    if (channel->owner_tx == OWNER_UNDECIDED) {
      int rc;
      if (mutex_lock_interruptible(
              &pDevExt->channelListLock))
        return -ERESTARTSYS;
      rc = channel_reserve_tx_fifo_entries(
          channel, FB_FIFO_ENTRIES_PER_CHANNEL, 0);
      mutex_unlock(&pDevExt->channelListLock);

      if (rc)
        return -EINVAL;
      channel->owner_tx = OWNER_KERNEL;
    }
    if (channel->owner_tx == OWNER_KERNEL) {
      poll_wait(f, &channel->outq, wait);

      if (channel_tx_possible(channel, 1))
        mask |= POLLOUT | POLLWRNORM;
    }

    // PRINTK("poll returns 0x%x\n", mask);
    return mask;
  }
}

/**
 *  Called upon last close (pFile->f_count == 0).
 *
 *  @brief  Called when a user space device or channel
 * context file handle is closed either by an application or
 * by Linux if application fails to do it.
 *
 *  @param  pNode   Pointer to file system i-node.
 *  @param  pFile   Pointer to file structure.
 *
 *  @return     Return code which is ignored by VFS.
 */
static int sc_release(struct inode *pNode,
                      struct file *pFile) {
  device_context_t *pDevExt =
      get_sc_device_from_file(pFile);
  if (pDevExt == NULL) {
    return -EINVAL;
  }

  if (LOG(LOG_FILE_OPS)) {
    if (pFile->private_data != NULL) {
      fb_channel_t *pChannel =
          (fb_channel_t *)(pFile->private_data);
      PRINTK(">%s(f=%p, DMA channel=%u)\n", __func__, pFile,
             pChannel->dma_channel_no);
    } else {
      PRINTK(">%s(f=%p, f->private_data=NULL)\n", __func__,
             pFile);
    }
  }

  if (pDevExt->reset_fpga) {
    PRINTK_C(LOG(LOG_FPGA_RESET),
             "FPGA RESET in progress in function %s\n",
             __func__);

    return -EBUSY; // Do not do anything else while FPGA
                   // reset is in progress.
  }

  // Disable asynchronous notification for this file
  // descriptor
  fasync_helper(-1, pFile, 0, &pDevExt->fasync);

  if (pFile->private_data != NULL) {
    fb_channel_t *channel =
        (fb_channel_t *)(pFile->private_data);

    // Tcp channels
    if (channel->type == SC_DMA_CHANNEL_TYPE_TCP) {
      unsigned tcp_channel_number =
          channel->dma_channel_no - FIRST_TCP_CHANNEL;
      if (etlm_getTcpState(pDevExt, tcp_channel_number) !=
          ETLM_TCP_STATE_CLOSED) {
        etlm_abort_conn(pDevExt, tcp_channel_number);
      }
    } else if (channel->type ==
               SC_DMA_CHANNEL_TYPE_UDP_MULTICAST) {
      // Clean up any left subscriptions for the UDP channel
      igmp_cleanup_channel_subscriptions(
          &pDevExt->igmpContext, channel);
    }

    // Generic
    mutex_lock(&pDevExt->channelListLock);

    // Stop dma for all but ul channels
    stop_dma(pDevExt, channel->dma_channel_no);

    PRINTK_C(LOG(LOG_DEBUG), "releasing channel %i ",
             channel->dma_channel_no);

    channel->occupied = 0;

    channel_release_tx_fifo_entries(channel);

    // channel_free_all_dmabuf(channel);

    mutex_unlock(&pDevExt->channelListLock);
  }

  panicParse(pDevExt, 1);

  PRINTK_C(LOG(LOG_FILE_OPS), "<sc_release(): 0\n");

  return 0;
}

static ssize_t sc_ul_read(struct file *f, char __user *buf,
                          size_t size, loff_t *offset) {
  fb_channel_t *channel;
  const device_context_t *pDevExt;

  if (!f->private_data) {
    return -EINVAL;
  }

  channel = (fb_channel_t *)f->private_data;
  pDevExt = channel->pDevExt;

  PRINTK_C(LOG(LOG_USER_LOGIC_FILE_OPS),
           ">sc_ul_read(f=%p, f->private_data=%p, "
           "minor=%u, size=%lu, *offset=%llu)\n",
           f, f->private_data, get_minor_from_file(f), size,
           *offset);

  if (pDevExt->reset_fpga) {
    PRINTK_C(LOG(LOG_FPGA_RESET),
             "FPGA RESET in progress in function %s\n",
             __func__);

    return -EBUSY; // Do not do anything else while FPGA
                   // reset is in progress.
  }

  if (size == 0) {
    return 0; // No buffer space available!
  }

  {
    int len = 0;

    if (channel->owner_rx == OWNER_UNDECIDED) {
      channel->owner_rx = OWNER_KERNEL;
    } else if (channel->owner_rx == OWNER_USERSPACE) {
      PRINTK_ERR("rx on channel %i is handled in "
                 "userspace, cannot use read system call\n",
                 channel->dma_channel_no);
      return -EFAULT;
    }

    while (f->private_data != NULL) {
      /*
      int isSignalPending;
      int i;
      // Is there a signal pending?
      for (i = 0; i < _NSIG_WORDS && isSignalPending == 0;
      ++i)
      {
          isSignalPending = current->pending.signal.sig[i] &
      ~current->blocked.sig[i];
      }
      if (isSignalPending != 0)
      {
          // There is signal pending
          module_put(THIS_MODULE);
          return -EINTR;
      }*/

      len = channel_read(channel, buf, size);

      if (len > 0) {
        *offset += len;
        PRINTK_C(LOG(LOG_USER_LOGIC_FILE_OPS),
                 "<sc_ul_read(): %d\n", len);
        return len;
      } else {
        if (f->f_flags & O_NONBLOCK) {
          return -EAGAIN;
        }
      }

      if (wait_event_interruptible(
              channel->inq,
              channel_rx_data_available(channel))) {
        return -ERESTARTSYS;
      }
    }
  }

  return 0;
}

/*
call graph:

sc_ul_write
    get_sc_device_from_file
    mutex_lock_interruptible(&pDevExt->channelListLock)
    channel_reserve_tx_fifo_entries
    mutex_unlock(&pDevExt->channelListLock)
    channel_get_free_bucket
        channel_has_room
            fifo_has_room
                check_for_consumed_fifo_entries         //
Side effects: tx->fifo_marker_head = ...
                                                        //
tx->free_fifo_entries_normal++ get_dest_consumed_bytes_fast
// No side effects get_dest_consumed_bytes_precise     //
Side effects: tx->last_known_consumed_value = cb64
                        get_consumed_counter            //
No side effects dest_has_room // No side effects
                dest_free_space_fast                    //
No side effects get_dest_capacity                   // No
side effects get_dest_consumed_bytes_fast        // No side
effects dest_free_space_precise                 // No side
effects get_dest_capacity                   // No side
effects get_dest_consumed_bytes_precise     // Side effects:
tx->last_known_consumed_value = cb64
    wait_event_interruptible
    channel_tx_possible
    copy_from_user
    channel_send_bucket
*/

static ssize_t sc_ul_write(struct file *f,
                           const char __user *buf,
                           size_t size, loff_t *offset) {
  ssize_t len = size;
  int bucket;
  uint8_t *bucketPayload;

  fb_channel_t *channel = (fb_channel_t *)f->private_data;
  device_context_t *pDevExt = get_sc_device_from_file(f);

  PRINTK_C(LOG(LOG_USER_LOGIC_FILE_OPS),
           ">%s(f=%p, f->private_data=%p, minor=%u)\n",
           __func__, f, f->private_data,
           get_minor_from_file(f));

  if (pDevExt == NULL) {
    return -ENODEV;
  }

  if (f->private_data == NULL) {
    return -EINVAL;
  }

  if (!channel_can_do_tx(channel)) {
    return -EINVAL;
  }

  if (pDevExt == NULL) {
    return -EINVAL;
  }

  if (pDevExt->reset_fpga) {
    PRINTK_C(LOG(LOG_FPGA_RESET),
             "FPGA RESET in progress in function %s\n",
             __func__);

    return -EBUSY; // Do not do anything else while FPGA
                   // reset is in progress.
  }

  if (channel->owner_tx == OWNER_UNDECIDED) {
    int rc;
    if (mutex_lock_interruptible(&pDevExt->channelListLock))
      return -ERESTARTSYS;
    rc = channel_reserve_tx_fifo_entries(
        channel, FB_FIFO_ENTRIES_PER_CHANNEL, 0);
    mutex_unlock(&pDevExt->channelListLock);

    if (rc)
      return -EINVAL;
    channel->owner_tx = OWNER_KERNEL;
  } else if (channel->owner_tx == OWNER_USERSPACE) {
    PRINTK_ERR("tx on channel %i is handled in userspace, "
               "cannot use write system call\n",
               channel->dma_channel_no);
    return -EINVAL;
  }

  // Subtract 8 bytes to compensate for Tx DMA nullifying 8
  // bytes after payload after packet has left the FIFO,
  // i.e. need 8 bytes extra space after payload:
  if (size > channel->bucket_size - 8) {
    len = channel->bucket_size - 8;
  }

  // Grab a free memory bucket for sending data
  while ((bucket = channel_get_free_bucket(channel, len)) <
         0) {
    if (f->f_flags & O_NONBLOCK)
      return -EAGAIN;

    if (wait_event_interruptible(
            channel->outq,
            channel_tx_possible(channel, len)))
      return -ERESTARTSYS;

    PRINTK_WARN_C(LOG(LOG_NIC | LOG_TX | LOG_WARNING),
                  "No buckets available on channel %u, "
                  "size %lu, *offset %lld\n",
                  channel->dma_channel_no, size, *offset);
  }

  bucketPayload = channel->bucket[bucket]->data;

  if (copy_from_user(bucketPayload, buf, len))
    return -EFAULT;

  // PRINTK("writing packet, size %zu, on DMA channel %d,
  // using bucket %d\n", len, channel->dma_channel_no,
  // bucket);
  channel_send_bucket(channel, bucket, len);

  *offset += len;

  PRINTK_C(LOG(LOG_USER_LOGIC_FILE_OPS), "<%s(): %lu\n",
           __func__, len);

  return len;
}

static int sc_fasync(int fd, struct file *pFile, int on) {
  int rc = 0;
  device_context_t *pDevExt =
      get_sc_device_from_file(pFile);
  if (pDevExt == NULL) {
    return -EINVAL;
  }

  PRINTK_C(LOG(LOG_FILE_OPS),
           ">%s(fd=%d, pFile->private_data=%p, on=%d)\n",
           __func__, fd, pFile->private_data, on);

  if (pDevExt->reset_fpga) {
    PRINTK_C(LOG(LOG_FPGA_RESET),
             "FPGA RESET in progress in function %s\n",
             __func__);

    return -EBUSY; // Do not do anything else while FPGA
                   // reset is in progress.
  }

  rc = fasync_helper(fd, pFile, on, &pDevExt->fasync);

  PRINTK_C(LOG(LOG_FILE_OPS), "<%s(): %d\n", __func__, rc);

  return rc;
}

// FIXME calling mmap with a size that does not fit nicely
// with the different segments below (PL_DMA_SIZE,,
// STATUS_DMA_SIZE, ...) will probably crash the kernel.
// more sanity check is needed.
static int sc_mmap(struct file *filep,
                   struct vm_area_struct *vma) {
  // START ELE TODO
  //
  // Possible better mapping strategy would be to not map
  // FPGA registers, PL DMA and Status DMA when doing
  // channel specific Tx bucket DMA and Rx PRB DMA mappings.
  // In the library the first mappings should be available
  // in the device context for all channels with a single
  // mapping only done once when device context is created.
  // Channel specific mappings should only be available in
  // the channel context.
  //
  // Another idea would be instead of size
  //
  // END ELE TODO.

  int rc = 0;
  resource_size_t bar0_sz, bar2_sz;
  uint64_t requested_sz, map_size;
  const device_context_t *pDevExt =
      get_sc_device_from_file(filep);
  if (pDevExt == NULL) {
    return -ENODEV;
  }

  if (pDevExt->reset_fpga) {
    PRINTK_C(LOG(LOG_FPGA_RESET),
             "FPGA RESET in progress in function %s\n",
             __func__);

    return -EBUSY; // Do not do anything else while FPGA
                   // reset is in progress.
  }

  requested_sz = vma->vm_end - vma->vm_start;
  vma->vm_flags |= VM_LOCKED;

#if 1
  if (vma->vm_pgoff == SC_MMAP_ALLOCATE_PRB_OFFSET) {
    const fb_channel_t *pChannel;
    uint64_t prbOffset = 0;
    size_t numberOfPhysicalAddresses = 1;
    size_t offsetToNextPhysicalAddress = 0;
    size_t lengthOfSingleAllocation = RECV_DMA_SIZE;
    const dma_addr_t *pPhysicalAddresses;
    size_t memoryBlockCount = 0;

    if (filep->private_data == NULL) {
      PRINTK_ERR(
          "Allocating PRB memory but no channel found!\n");
      rc = -EINVAL;
    }
    pChannel = (const fb_channel_t *)filep->private_data;

    if (pDevExt->pMMU != NULL) {
      pPhysicalAddresses =
          mmu_get_dma_channel_physical_addresses(
              pDevExt->pMMU, pChannel->dma_channel_no,
              &numberOfPhysicalAddresses,
              &offsetToNextPhysicalAddress,
              &lengthOfSingleAllocation);
    } else {
      pPhysicalAddresses = &pChannel->recvDma;
    }

    map_size = lengthOfSingleAllocation;

    // while (prbOffset < pChannel->prbSize)
    while (prbOffset < requested_sz) {
      if (++memoryBlockCount > numberOfPhysicalAddresses) {
        PRINTK_ERR("Trying to map more than available %lu "
                   "MMU physical addresses\n",
                   numberOfPhysicalAddresses);
      }

      // Map a single PRB physical memory block
      if (remap_pfn_range(vma, vma->vm_start + prbOffset,
                          *pPhysicalAddresses >> PAGE_SHIFT,
                          map_size, vma->vm_page_prot)) {
        PRINTK_ERR("mmap: failed to map Rx MMU DMA memory, "
                   "PRB offset 0x%llX\n",
                   prbOffset);
        rc = -EAGAIN;
        goto sc_mmap_exit;
      }
      if (LOG(LOG_MMU | LOG_CHANNEL | LOG_MMAP)) {
        const uint8_t *pVirtStart =
            (const uint8_t *)(vma->vm_start + prbOffset);
        const uint8_t *pVirtEnd = pVirtStart + map_size;
        const uint8_t *pPhyStart =
            (const uint8_t *)(*pPhysicalAddresses);
        const uint8_t *pPhyEnd = pPhyStart + map_size;
        PRINTK("  mmap:   PRB MMU  DMA to:        "
               "va[0x%016llX-0x%016llX), "
               "pa[0x%016llX-0x%016llX), length 0x%llx, "
               "PRB offset 0x%010llX\n",
               (uint64_t)pVirtStart, (uint64_t)pVirtEnd,
               (uint64_t)pPhyStart, (uint64_t)pPhyEnd,
               map_size, prbOffset);
      }

      prbOffset += map_size;
      pPhysicalAddresses += offsetToNextPhysicalAddress;
    }

    goto sc_mmap_exit;
  }
#endif

  bar2_sz = pci_resource_len(pDevExt->pdev, BAR_2);
  if (requested_sz == 2 * BAR2_REGS_SIZE) {
    // Region 2 - User logic registers in BAR 2:
    map_size = BAR2_REGS_SIZE;
    PRINTK("EKALINE BAR2 mmap: vma->vm_start = 0x%lx",
           vma->vm_start);

    if (remap_pfn_range(
            vma, vma->vm_start + BAR2_REGS_OFFSET,
            pci_resource_start(pDevExt->pdev, BAR_2) >>
                PAGE_SHIFT,
            map_size, vma->vm_page_prot)) {
      PRINTK_ERR("mmap: failed to map PCI BAR 2 user logic "
                 "registers, requested size 0x%llX\n",
                 map_size);
      rc = -EAGAIN;
      goto sc_mmap_exit;
    }

    if (LOG(LOG_MMAP)) {
      const uint8_t *pVirtStart =
          (const uint8_t *)(vma->vm_start +
                            BAR2_REGS_OFFSET);
      const uint8_t *pVirtEnd = pVirtStart + map_size;
      const uint8_t *pPhyStart =
          (const uint8_t *)(pDevExt->bar2_va);
      const uint8_t *pPhyEnd = pPhyStart + map_size;

      PRINTK("BAR 2 mapping! requested_sz = %llu(0x%llx), "
             "bar2_sz=0x%llx\n",
             requested_sz, requested_sz, bar2_sz);

      PRINTK(
          "  mmap:   BAR 2 UL regs to:      va[0x%p-0x%p), "
          "pa[0x%p-0x%p), length 0x%llx\n",
          pVirtStart, pVirtEnd, pPhyStart, pPhyEnd,
          map_size);
    }

    goto sc_mmap_exit;
  }

//----------------------------------------------------
#if 1
  // fixed by ekaline
  if (requested_sz == EKA_WC_REGION_SIZE) {
    map_size = EKA_WC_REGION_SIZE;
    if (remap_pfn_range(
            vma, vma->vm_start,
            (pci_resource_start(pDevExt->pdev, BAR_0) +
             EKA_WC_REGION_OFFS) >>
                PAGE_SHIFT,
            map_size, vma->vm_page_prot)) {
      PRINTK_ERR("mmap: failed to map Ekaline WC Base, "
                 "requested size 0x%llX\n",
                 EKA_WC_REGION_SIZE);
      rc = -EAGAIN;
      goto sc_mmap_exit;
    }

    if (LOG(LOG_MMAP)) {
      PRINTK("   mmap:   EKALINE WC Region to:      "
             "va[0x%llx-0x%llx), pa[0x%llx-0x%llx)",
             (uint64_t)vma->vm_start,
             (uint64_t)vma->vm_start + EKA_WC_REGION_SIZE,
             (uint64_t)pci_resource_start(pDevExt->pdev,
                                          BAR_0),
             (uint64_t)pci_resource_start(pDevExt->pdev,
                                          BAR_0) +
                 EKA_WC_REGION_SIZE);
    }
    goto sc_mmap_exit;
  }
#endif
  //----------------------------------------------------

  bar0_sz = pci_resource_len(pDevExt->pdev, BAR_0);
  if (LOG(LOG_FILE_OPS) || LOG(LOG_MMAP)) {
    char name[50];
    PRINTK(
        ">%s(f=%p, f->private_data=%p), "
        "requested_sz=0x%llx, bar0_sz=0x%llx, "
        "bar2_sz=0x%llx, file name=%s\n",
        __func__, filep, filep->private_data, requested_sz,
        bar0_sz, bar2_sz,
        get_file_name_from_file(filep, name, sizeof(name)));
  }

  // PRINTK("mapping request start = 0x%lx, sz = %ld,
  // bar_0_sz = %d\n", vma->vm_start, requested_sz,
  // bar0_sz);

  // Print a warning if the mapping is missing some
  // registers. 0x10000 requested by fbupdate which only
  // maps basic FPGA registers, don't complain about that!
  if (bar0_sz > requested_sz && requested_sz != 0x10000) {
    PRINTK_ERR(
        "mmap: the PCI size for BAR0 (0x%llx) is bigger "
        "than the requested map size (0x%llx)\n",
        bar0_sz, requested_sz);
  }

  // remap_pfn_range - rempa kernel memory to userspace

  // Region 0 - pcie registers in BAR 0:
  map_size = requested_sz < BAR0_REGS_SIZE ? requested_sz
                                           : BAR0_REGS_SIZE;
  // map_size = EKA_WC_REGION_SIZE;
  PRINTK("EKALINE BAR0 mmap: vma->vm_start = 0x%llx, "
         "map_size = 0x%llx",
         (uint64_t)vma->vm_start, map_size);

  if (remap_pfn_range(
          vma, vma->vm_start + BAR0_REGS_OFFSET,
          pci_resource_start(pDevExt->pdev, BAR_0) >>
              PAGE_SHIFT,
          map_size, vma->vm_page_prot)) {
    PRINTK_ERR("mmap: failed to map PCI BAR 0 registers, "
               "requested size 0x%llX\n",
               requested_sz);
    rc = -EAGAIN;
    goto sc_mmap_exit;
  }

  if (LOG(LOG_MMAP)) {
    const uint8_t *pVirtStart =
        (const uint8_t *)(vma->vm_start + BAR0_REGS_OFFSET);
    const uint8_t *pVirtEnd = pVirtStart + map_size;
    const uint8_t *pPhyStart =
        (const uint8_t *)(pci_resource_start(pDevExt->pdev,
                                             BAR_0));
    const uint8_t *pPhyEnd = pPhyStart + map_size;
    PRINTK("%s: requested_sz = %llu(0x%llx)", __func__,
           requested_sz, requested_sz);
    PRINTK("  mmap: FPGA BAR 0 registers to: "
           "va[0x%p-0x%p), pa[0x%p-0x%p), length 0x%llx\n",
           pVirtStart, pVirtEnd, pPhyStart, pPhyEnd,
           map_size);
  }

  // fixed by ekaline
  //     pDevExt->eka_bar0_va    = (uint64_t)(vma->vm_start
  //     + BAR0_REGS_OFFSET);

  // User asked for the registers only
  if (requested_sz <= BAR0_REGS_SIZE) {
    if (LOG(LOG_MMAP)) {
      PRINTK("mmap: successfully mapped register memory, "
             "requested size 0x%llX\n",
             requested_sz);
    }
    goto sc_mmap_exit;
  }

  // Map pldma
  map_size = PL_DMA_SIZE;
  if (remap_pfn_range(vma, vma->vm_start + PL_DMA_OFFSET,
                      pDevExt->plDmaPa >> PAGE_SHIFT,
                      map_size, vma->vm_page_prot)) {
    PRINTK_ERR("mmap: failed to map PL DMA memory, "
               "requested size 0x%llX\n",
               requested_sz);
    rc = -EAGAIN;
    goto sc_mmap_exit;
  }
  if (LOG(LOG_MMAP)) {
    const uint8_t *pVirtStart =
        (const uint8_t *)(vma->vm_start + PL_DMA_OFFSET);
    const uint8_t *pVirtEnd = pVirtStart + map_size;
    const uint8_t *pPhyStart =
        (const uint8_t *)(pDevExt->plDmaPa);
    const uint8_t *pPhyEnd = pPhyStart + map_size;
    PRINTK("  mmap: PL DMA to:               "
           "va[0x%p-0x%p), pa[0x%p-0x%p), length 0x%llx\n",
           pVirtStart, pVirtEnd, pPhyStart, pPhyEnd,
           map_size);
  }

  // Map status DMA
  map_size = STATUS_DMA_SIZE;
  if (remap_pfn_range(vma,
                      vma->vm_start + STATUS_DMA_OFFSET,
                      pDevExt->statusDmaPa >> PAGE_SHIFT,
                      map_size, vma->vm_page_prot)) {
    PRINTK_ERR("mmap: failed to map Status DMA memory, "
               "requested size 0x%llX\n",
               requested_sz);
    rc = -EAGAIN;
    goto sc_mmap_exit;
  }
  if (LOG(LOG_MMAP)) {
    const uint8_t *pVirtStart =
        (const uint8_t *)(vma->vm_start +
                          STATUS_DMA_OFFSET);
    const uint8_t *pVirtEnd = pVirtStart + map_size;
    const uint8_t *pPhyStart =
        (const uint8_t *)(pDevExt->statusDmaPa);
    const uint8_t *pPhyEnd = pPhyStart + map_size;
    PRINTK("  mmap: status DMA to:           "
           "va[0x%p-0x%p), pa[0x%p-0x%p), length 0x%llx\n",
           pVirtStart, pVirtEnd, pPhyStart, pPhyEnd,
           map_size);
  }

  // If a channel is allocated, map the buckets followed by
  // the recv buffer
  if (filep->private_data) {
    fb_channel_t *channel =
        (fb_channel_t *)filep->private_data;

    PRINTK_C(LOG(LOG_CHANNEL | LOG_MMAP),
             "  mmap: channel %u:\n",
             channel->dma_channel_no);

    if (BUCKETS_OFFSET + BUCKETS_SIZE + RECV_DMA_SIZE >
        requested_sz) {
      PRINTK_ERR(
          "mmap: requested size 0x%llX is not enough to "
          "map the channel buckets and ring buffer. "
          "BUCKETS_OFFSET=0x%lx, BUCKETS_SIZE=0x%lx, "
          "RECV_DMA_SIZE=0x%x, BUCKETS_OFFSET + "
          "BUCKETS_SIZE + RECV_DMA_SIZE=0x%lx. Aborting.\n",
          requested_sz, BUCKETS_OFFSET, BUCKETS_SIZE,
          RECV_DMA_SIZE,
          BUCKETS_OFFSET + BUCKETS_SIZE + RECV_DMA_SIZE);
      rc = -EAGAIN;
      goto sc_mmap_exit;
    }

    // Map the send buckets
    WARN_ON(channel->bucket_size != SC_BUCKET_SIZE);
    map_size = MAX_BUCKETS * SC_BUCKET_SIZE;
    if (remap_pfn_range(vma,
                        vma->vm_start + BUCKET_OFFSET(0),
                        channel->dmaBucket[0] >> PAGE_SHIFT,
                        map_size, vma->vm_page_prot)) {
      PRINTK_ERR("mmap: failed to map Tx Bucket memory, "
                 "requested size 0x%llX\n",
                 requested_sz);
      rc = -EAGAIN;
      goto sc_mmap_exit;
    }
    if (LOG(LOG_CHANNEL | LOG_MMAP)) {
      const uint8_t *pVirtStart =
          (const uint8_t *)(vma->vm_start +
                            BUCKET_OFFSET(0));
      const uint8_t *pVirtEnd = pVirtStart + map_size;
      const uint8_t *pPhyStart =
          (const uint8_t *)(channel->dmaBucket[0]);
      const uint8_t *pPhyEnd = pPhyStart + map_size;
      PRINTK(
          "  mmap:   bucket DMA to:         va[0x%p-0x%p), "
          "pa[0x%p-0x%p), length 0x%llx\n",
          pVirtStart, pVirtEnd, pPhyStart, pPhyEnd,
          map_size);

      // PRINTK("mmap: DMA channel no=%d,
      // channel->dmaBucket[0]=%p, PAGE_SHIFT=%u,
      // channel->dmaBucket[0] >> PAGE_SHIFT=%p\n",
      //     channel->dma_channel_no, (void
      //     *)(channel->dmaBucket[0]), PAGE_SHIFT, (void
      //     *)(channel->dmaBucket[0] >> PAGE_SHIFT));
    }

    // Map the recv buffer
    map_size = RECV_DMA_SIZE;
    if (remap_pfn_range(vma,
                        vma->vm_start + RECV_DMA_OFFSET,
                        channel->recvDma >> PAGE_SHIFT,
                        RECV_DMA_SIZE, vma->vm_page_prot)) {
      PRINTK_ERR("mmap: failed to map Rx DMA memory, "
                 "requested size 0x%llX\n",
                 requested_sz);
      rc = -EAGAIN;
      goto sc_mmap_exit;
    }
    if (LOG(LOG_CHANNEL | LOG_MMAP)) {
      const uint8_t *pVirtStart =
          (const uint8_t *)(vma->vm_start +
                            RECV_DMA_OFFSET);
      const uint8_t *pVirtEnd = pVirtStart + map_size;
      const uint8_t *pPhyStart =
          (const uint8_t *)(channel->recvDma);
      const uint8_t *pPhyEnd = pPhyStart + map_size;
      PRINTK(
          "  mmap:   PRB DMA to:            va[0x%p-0x%p), "
          "pa[0x%p-0x%p), length 0x%llx\n",
          pVirtStart, pVirtEnd, pPhyStart, pPhyEnd,
          map_size);
    }
  }

sc_mmap_exit:

  if (rc != 0) {
    PRINTK_ERR("%s failed with error code %d, requested "
               "size 0x%llX\n",
               __func__, rc, requested_sz);
  }

  PRINTK_C(LOG(LOG_FILE_OPS) || LOG(LOG_MMAP),
           "<%s(): %d\n", __func__, rc);

  return rc;
}

static struct file_operations file_ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = sc_ioctl,
    .open = sc_open,
    .poll = sc_ul_poll,
    .release = sc_release, //<! Called upon last close
                           //(file->f_count == 0), int
                           // return value is ignored by VFS
    .read = sc_ul_read,
    .write = sc_ul_write,
    .fasync = sc_fasync,
    .mmap = sc_mmap,
};

/// @} FileOperationFunctions

/** **************************************************************************
 * @defgroup CardOperationFunctions Network card related
 * operating system operations.
 * @{
 * @brief Network card related operating system operations
 * like device id mapping, probe and remove.
 */

static void ReadFeatures(device_context_t *pDevExt,
                         char *featureLine,
                         size_t featureLineSize) {
  int totalNumberOfCharsUsed = 0;
  bool extendedRegisterMode, bulkSupported, txDmaSupport,
      warningSupport;
  const char *separator = "";

  getFeatures(pDevExt, &pDevExt->groupPlDma,
              &extendedRegisterMode, &bulkSupported,
              &txDmaSupport, &pDevExt->mmuSupport,
              &warningSupport);
  featureLine[0] = '\0';
  if (pDevExt->groupPlDma) {
    totalNumberOfCharsUsed +=
        snprintf(&featureLine[totalNumberOfCharsUsed],
                 featureLineSize - totalNumberOfCharsUsed,
                 "%sMultiple PL DMA groups", separator);
    separator = ", ";
  } else {
    totalNumberOfCharsUsed +=
        snprintf(&featureLine[totalNumberOfCharsUsed],
                 featureLineSize - totalNumberOfCharsUsed,
                 "%sSingle PL DMA", separator);
    separator = ", ";
  }
  if (extendedRegisterMode) {
    totalNumberOfCharsUsed +=
        snprintf(&featureLine[totalNumberOfCharsUsed],
                 featureLineSize - totalNumberOfCharsUsed,
                 "%sExtended Register Mode", separator);
    separator = ", ";
  }
  if (bulkSupported) {
    totalNumberOfCharsUsed +=
        snprintf(&featureLine[totalNumberOfCharsUsed],
                 featureLineSize - totalNumberOfCharsUsed,
                 "%sBulk Tx", separator);
    separator = ", ";
  }
  if (txDmaSupport) {
    totalNumberOfCharsUsed +=
        snprintf(&featureLine[totalNumberOfCharsUsed],
                 featureLineSize - totalNumberOfCharsUsed,
                 "%sTx DMA", separator);
    separator = ", ";
  }
  if (pDevExt->mmuSupport) {
    totalNumberOfCharsUsed +=
        snprintf(&featureLine[totalNumberOfCharsUsed],
                 featureLineSize - totalNumberOfCharsUsed,
                 "%sMMU", separator);
    separator = ", ";
  }
  if (warningSupport) {
    totalNumberOfCharsUsed +=
        snprintf(&featureLine[totalNumberOfCharsUsed],
                 featureLineSize - totalNumberOfCharsUsed,
                 "%sWarnings", separator);
    separator = ", ";
  }
}

/*
sc_probe
    pci_enable_device
    pci_request_regions
    pci_set_dma_mask
    pci_set_master
    pcie_set_readrq
    sc_alloc_device
    pci_set_drvdata
    getFpgaVersion
    getFpgaBuildTime
    getPCIeMaxPayloadSize
    enableExtendedTags
    reset
    readMac
    pci_find_ext_capability
    sc_start_device
    msix_setup
    start_all_oob_interrupts
    acl_reset_all
*/

static int
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
    __devinit
#endif
    sc_probe(struct pci_dev *pdev,
             const struct pci_device_id *ent) {
  int ret = 0;
  int i = 0;
  static atomic_t s_NumberOfCards = ATOMIC_INIT(0);
  unsigned int minor =
      atomic_inc_return(&s_NumberOfCards) - 1;
  const char *pcieName = pci_name(pdev);

  if (device_mapping[0] != '\0') {
    // Minor number is defined by module load parameter
    // 'device_mapping' which maps minor and a PCIe device
    // name together:
    bool deviceFound = false;
    minor = 0;
    while (minor < number_of_elements(dev_data) &&
           dev_data[minor].pcieDeviceName != NULL) {
      if (strcmp(pcieName,
                 dev_data[minor].pcieDeviceName) == 0) {
        printk(KERN_INFO MODULE_NAME
               ": Minor number %u explicitly mapped to "
               "PCIe device name %s\n",
               minor, pcieName);
        deviceFound = true;
        break;
      }
      ++minor;
    }
    if (!deviceFound) {
      if (minor < number_of_elements(dev_data)) {
        printk(KERN_INFO MODULE_NAME
               ": No matching PCIe device %s found for "
               "minor %u, adding as last device\n",
               pcieName, minor);
        dev_data[minor].pcieDeviceName = pcieName;
      } else {
        printk(KERN_ERR MODULE_NAME
               ": No matching PCIe device %s found and "
               "table is full. Initialization failed!\n",
               pcieName);
        return -ENODEV;
      }
    }
  } else {
    printk(KERN_INFO MODULE_NAME
           ": Minor number %u mapped to PCIe device name "
           "%s in probe order\n",
           minor, pcieName);
  }

  {
    uint8_t mac_address[MAC_ADDR_LEN];
    device_context_t *pDevExt =
        kmalloc(sizeof(device_context_t), GFP_KERNEL);
    bool pcieDmaMask32Bits = true;
    char featureLine[100];

    if (pDevExt == NULL) {
      return -ENOMEM;
    }
    memset(pDevExt, 0, sizeof(device_context_t));
    pDevExt->minor = minor;
    // pDevExt and pDevExt->minor are known after this point

    PRINTK_C(LOG(LOG_MEMORY_ALLOCATIONS),
             "Allocated  %lu (0x%lx) bytes device context "
             "at %p\n",
             sizeof(device_context_t),
             sizeof(device_context_t), pDevExt);

    PRINTK_C(LOG(LOG_DEBUG), "debug output enabled\n");

    for (i = 0;
         i < number_of_elements(
                 pDevExt->previousPlDmaVa.lastPacket);
         ++i) {
      pDevExt->previousPlDmaVa.lastPacket[i] =
          0xDEADBEEFDEADBEEF;
    }

    if (pad_frames) {
      PRINTK("padding frames to minimum of 60 bytes "
             "enabled\n");
    }
    if (max_etlm_channels < 1) {
      max_etlm_channels = 1;
    }
    if (max_etlm_channels > SC_MAX_TCP_CHANNELS) {
      max_etlm_channels = SC_MAX_TCP_CHANNELS;
    }
    if (max_etlm_channels != SC_MAX_TCP_CHANNELS) {
      PRINTK("Driver supports only %u TOE channels of "
             "maximum %u\n",
             max_etlm_channels, SC_MAX_TCP_CHANNELS);
    }

    ret = pci_enable_device(pdev);
    if (ret) {
      PRINTK_ERR("ERROR: Unabled to enable device (%s)",
                 pci_name(pdev));
      ret = -ENODEV;
      goto bail0;
    }

    ret = pci_request_regions(pdev, MODULE_NAME);
    if (ret) {
      PRINTK_ERR("ERROR: Can't request regions (%s)",
                 pci_name(pdev));
      ret = -ENODEV;
      goto bail1;
    }

    // FPGA DMA limit of 47 bits without MMU (2^47 = 131072
    // GB), 39 bits with MMU (2^39 = 512 GB)
    if (pci_set_dma_mask(pdev, DMA_BIT_MASK(39)) == 0) {
      ret = pci_set_consistent_dma_mask(pdev,
                                        DMA_BIT_MASK(39));
      pcieDmaMask32Bits = false;
    } else {
      ret = pci_set_dma_mask(
          pdev, DMA_BIT_MASK(32)); // 2^32 = 4 GB
      if (ret == 0) {
        ret = pci_set_consistent_dma_mask(pdev,
                                          DMA_BIT_MASK(32));
        pcieDmaMask32Bits = true;
      }
    }
    if (ret == 0) {
      PRINTK("   PCIe DMA mask is %u bits\n",
             pcieDmaMask32Bits ? 32 : 39);
    } else {
      PRINTK_ERR(
          "ERROR: pci_set_dma_mask failed, ret= %d\n", ret);
    }

    ret = pci_enable_pcie_error_reporting(pdev);
    if (ret) {
      PRINTK("Device support error reporting, ret = %d\n",
             ret);
    }

    pci_set_master(pdev);

    ret = pcie_set_readrq(pdev, 2048);
    if (ret) {
      PRINTK_WARN("warning: pcie_set_readrq() failed, "
                  "err=%d! Performance may suffer.\n",
                  ret);
    }

    pDevExt->bar0_va = get_pci_bar(pDevExt, pdev, BAR_0);
    if (LOG(LOG_PCIe)) {
      unsigned long bar0_start =
          pci_resource_start(pdev, BAR_0);
      unsigned long bar0_length =
          pci_resource_len(pdev, BAR_0);
      PRINTK(
          "Mapped BAR 0 memory or I/O address "
          "[0x%lx-0x%lx) to kernel virtual address 0x%p\n",
          bar0_start, bar0_start + bar0_length,
          pDevExt->bar0_va);
    }
    if (pDevExt->bar0_va == NULL) {
      ret = -ENODEV;
      PRINTK_ERR("Failed to map PICe register BAR0\n");
      goto bail2;
    }

    pDevExt->bar2_va = get_pci_bar(pDevExt, pdev, BAR_2);
    if (LOG(LOG_PCIe)) {
      unsigned long bar2_start =
          pci_resource_start(pdev, BAR_2);
      unsigned long bar2_length =
          pci_resource_len(pdev, BAR_2);
      PRINTK(
          "Mapped BAR 2 memory or I/O address "
          "[0x%lx-0x%lx) to kernel virtual address 0x%p\n",
          bar2_start, bar2_start + bar2_length,
          pDevExt->bar2_va);
    }
    if (pDevExt->bar2_va == NULL) {
      ret = -ENODEV;
      PRINTK_ERR("Failed to map PICe register BAR2\n");
      goto bail2;
    }

    ReadFeatures(pDevExt, featureLine, sizeof(featureLine));

    if (sc_init_device(pDevExt, pdev)) {
      ret = -ENOMEM;
      goto bail2;
    }
    dev_data[minor].pDevExt = pDevExt;

    assert("assert is working in a DEBUG VERSION! DO NOT "
           "RELEASE!" == NULL);

    pDevExt->acl_context.pDevExt = pDevExt;

    pci_set_drvdata(pdev, pDevExt);

    PRINTK("--- Silicom %s - PCIe %s, %s (0x%04x) ---\n",
           PRODUCT, pcieName, getCardName(ent->device),
           ent->device);
    pDevExt->rev.raw_rev = getFpgaVersion(pDevExt);
    LogRevision(pDevExt);

    if (featureLine[0] != '\0') {
      PRINTK("   FPGA features: %s\n", featureLine);
    }

    // Check the fpga is of a recognized type
    if (IsLeonbergFpga(pDevExt->rev.parsed.type) == NULL &&
        IsTestFpga(pDevExt->rev.parsed.type) == NULL) {
      PRINTK_ERR("unsupported fpga type 0x%x!\n",
                 pDevExt->rev.parsed.type);
      goto bail2;
    }

    {
      uint64_t buildtime = getFpgaBuildTime(pDevExt);
      struct tm buildtm;
      // time_to_tm(buildtime, 0, &buildtm);
      time64_to_tm(buildtime, 0, &buildtm);

      PRINTK("   FPGA build time: %llu (%02d-%02d-%04ld "
             "T%02d:%02d:%02d GMT)\n",
             buildtime, buildtm.tm_mday, buildtm.tm_mon + 1,
             1900 + buildtm.tm_year, buildtm.tm_hour,
             buildtm.tm_min, buildtm.tm_sec);
    }

    if (0) {
      u16 mps = getPCIeMaxPayloadSize(pDevExt, pdev);
      PRINTK_C(LOG(LOG_DEBUG),
               "PCIe MaxPayloadSize %d bytes\n", mps);
    }

    enableExtendedTags(pDevExt, pdev);

    ret = fpga_reset(pDevExt);
    if (ret) {
      PRINTK_ERR("ERROR: reset never reached ; timeout!\n");
      goto bail2;
    }

    {
      MACBUF(0);
      fpga_readMac(pDevExt, mac_address);
      PRINTK(
          "   Board serial number: %d (based on MAC %s)\n",
          ((unsigned int)(mac_address[4] & 0xff) << 4) +
              ((unsigned int)(mac_address[5] & 0xff) >> 4),
          LogMAC(0, mac_address));
    }

    for (i = 0; i < SC_NO_NETWORK_INTERFACES; i++) {
      memcpy(pDevExt->nif[i].mac_addr, mac_address,
             MAC_ADDR_LEN);
      pDevExt->nif[i].mac_addr[5] += i;
    }

    logDeviceSerialNumber(pDevExt, pdev);

    if (IsLeonbergFpga(pDevExt->rev.parsed.type) != NULL) {
      if (pDevExt->pMMU != NULL) {
        mmu_enable(pDevExt->pMMU, true);
      }

      if (pDevExt->acl_context.acl_nif_mask != 0) {
        // Make sure all ACL lists are in reset state after
        // new card is detected
        if ((ret = acl_reset_all(&pDevExt->acl_context)) !=
            0) {
          PRINTK_ERR("ERROR: reset of all ACL lists "
                     "failed, code %d\n",
                     ret);
        }
      }

      arp_context_construct(&pDevExt->arp_context, pDevExt);

      pDevExt->filling_scale = SC_FILLING_SCALE_1_KB;
      pDevExt->time_mode = SC_TIME_MODE_SECONDS_NANOSECONDS;

      if ((ret = interrupts_setup(pDevExt)) < 0)
        goto bail2;

      if ((ret = sc_start_device(pDevExt)) != 0)
        goto bail2;

      start_all_oob_interrupts(pDevExt);
    }

    //(*pDumpChannels)(pDevExt, DUMP_CHANNEL_USER_LOGIC_ALL,
    //-1);

    return 0;
  bail2:
    pci_release_regions(pdev);
  bail1:
    pci_disable_device(pdev);
  bail0:
    if (pDevExt != NULL) {
      kfree(pDevExt);
    }
    return ret;
  }
}

/**
 *  This function is called when the module is unloaded
 * (removed) from the Linux kernel.
 */
static void
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
    __devexit
#endif
    sc_remove(struct pci_dev *pdev) {
  int rc = 0;
  device_context_t *pDevExt =
      (device_context_t *)pci_get_drvdata(pdev);
  DummyDevice dummyDevice;
  dummyDevice.minor = pDevExt->minor;

  FUNCTION_ENTRY(LOG_INIT, " ");

  if (pDevExt->acl_context.acl_nif_mask != 0) {
    // Make sure that all ACL lists are reset when module is
    // removed or driver unloaded
    if ((rc = acl_reset_all(&pDevExt->acl_context)) != 0) {
      PRINTK_ERR(
          "ERROR: failed to reset ACL lists, code %d\n",
          rc);
    }
  }

  // Stop time-of-day adjust thread if it is running
  if (pDevExt->time_adjust_thread) {
    kthread_stop(pDevExt->time_adjust_thread);
    pDevExt->time_adjust_thread = NULL;
  }

  if (IsLeonbergFpga(pDevExt->rev.parsed.type) != NULL) {
    stop_all_interrupts(pDevExt);

    interrupts_teardown(pDevExt);

    sc_free_device(pDevExt);
    pDevExt = NULL;
  }

  {
    // Dummy device provides minor device number for
    // PRINTK's below, the real pDevExt has been delete
    // above!
    DummyDevice *pDevExt = &dummyDevice;

    unregister_chrdev(major, MODULE_NAME);
    PRINTK("device %s%u is removed\n", MODULE_NAME,
           pDevExt->minor);

    pci_get_drvdata(pdev);

    pci_release_regions(pdev);

    pci_disable_pcie_error_reporting(pdev);

    pci_disable_device(pdev);

    FUNCTION_EXIT(LOG_INIT, "");
  }
}

static struct pci_device_id sc_pci_tbl[] = {
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_LIVORNO_FULDA)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_LIVIGNO_FULDA_V7330)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE, DEVICE_ID_TEST_FPGA)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_LIVORNO_LEONBERG)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_LUCCA_LEONBERG_V7690)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_LIVIGNO_LEONBERG_V7330)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_LATINA_LEONBERG)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_MANGO_03_LEONBERG_VU9P_ES1)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_MANGO_LEONBERG_VU9P)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_PADUA_LEONBERG_VU125)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_MANGO_LEONBERG_VU125)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_LUCCA_LEONBERG_V7330)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_LIVIGNO_LEONBERG_V7690)},
    {PCI_DEVICE(VENDOR_ID_FIBERBLAZE,
                DEVICE_ID_MANGO_04_LEONBERG_VU9P)},
    {
        0,
    }};

static const char *getCardName(uint32_t deviceId) {
  switch (deviceId) {
  case DEVICE_ID_LIVORNO_FULDA:
    return "LIVORNO_FULDA";
  case DEVICE_ID_LIVIGNO_FULDA_V7330:
    return "LIVIGNO_FULDA_V7330";
  case DEVICE_ID_TEST_FPGA:
    return "TEST_FPGA";
  case DEVICE_ID_LIVORNO_LEONBERG:
    return "LIVORNO_LEONBERG";
  case DEVICE_ID_LUCCA_LEONBERG_V7690:
    return "LUCCA_LEONBERG_V7690";
  case DEVICE_ID_LIVIGNO_LEONBERG_V7330:
    return "LIVIGNO_LEONBERG_V7330";
  case DEVICE_ID_LATINA_LEONBERG:
    return "LATINA_LEONBERG";
  case DEVICE_ID_MANGO_03_LEONBERG_VU9P_ES1:
    return "MANGO_03_LEONBERG_VU9P_ES1";
  case DEVICE_ID_MANGO_LEONBERG_VU9P:
    return "MANGO_LEONBERG_VU9P";
  case DEVICE_ID_PADUA_LEONBERG_VU125:
    return "PADUA_LEONBERG_VU125";
  case DEVICE_ID_MANGO_LEONBERG_VU125:
    return "MANGO_LEONBERG_VU125";
  case DEVICE_ID_LUCCA_LEONBERG_V7330:
    return "LUCCA_LEONBERG_V7330";
  case DEVICE_ID_LIVIGNO_LEONBERG_V7690:
    return "LIVIGNO_LEONBERG_V7690";
  case DEVICE_ID_MANGO_04_LEONBERG_VU9P:
    return "MANGO_04_LEONBERG_VU9P";
  default:
    return "*** UNKNOWN CARD ***";
  }
}

static struct pci_driver sc_pci_driver = {
  .name = MODULE_NAME,
  .id_table = sc_pci_tbl,
  .probe = sc_probe,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
  .remove = __devexit_p(sc_remove),
#else
  .remove = sc_remove,
#endif
};

/// @} CardOperationFunctions

/** **************************************************************************
 * @defgroup ModuleLevelFunctions Driver module level
 * initialization and cleanup.
 * @{
 * @brief These functions are called when driver module is
 * loaded and unloaded by the operating system.
 */

/**
 *  This function is called when the module is loaded and
 * makes various once only initializations.
 */
static int __init init_sc_module(void) {
  int rc = 0;

  printk(KERN_INFO MODULE_NAME ": ====== %s ======\n",
         release_version);

  // Parse device mappings:
  if (device_mapping[0] != '\0') {
    static char device_mapping_copy[1000];
    char *next = &device_mapping_copy[0];
    unsigned int minor = 0;

    // strtok changes string contents so we make a copy that
    // is changed. Copy is a local static so that
    // dev_data[minor].pcieDeviceName always points to
    // something useful.
    strncpy(device_mapping_copy, device_mapping,
            sizeof(device_mapping_copy) - 1);

    while (true) {
      const char *pcieDeviceName = strtok(next, ',');
      if (pcieDeviceName == NULL) {
        break;
      }
      dev_data[minor].pcieDeviceName =
          (pcieDeviceName[0] == '\0' ? "" : pcieDeviceName);
      ++minor;
      next = NULL;
    }
  }

  if (logmask == LOG_DETECT_COMMAND_LINE_OVERRIDE) {
    /* Command line did not override logmask, set default
     * value. */

    logmask = LOG_NONE;

    // logmask |= LOG_MMAP | LOG_DEBUG | LOG_INIT |
    //           LOG_PCIE_MEMORY_ALLOCATIONS;

    // logmask = LOG_ALL;
    //  logmask |= LOG_NIC | LOG_TX;
    //  logmask |= LOG_TX_ZERO_BYTES_ACK;
    //  logmask |= LOG_INIT | LOG_MSI_MSIX;
    //  logmask |= LOG_USER_LOGIC_FILE_OPS | LOG_DEBUG;
    //  logmask |= LOG_IGMP;
    //  logmask |= LOG_ETLM_TCP_CONNECTION_REG_W;
    //  logmask |= LOG_ACL | LOG_DETAIL;
    //  logmask |= LOG_ENTRY | LOG_EXIT;
    //  logmask |= LOG_DMA | LOG_REG_W;
    //  logmask |= LOG_IGMP | LOG_UDP | LOG_DETAIL;
    //  logmask |= LOG_ALL;
    //  logmask &= ~(LOG_REG_W);
    //  logmask |= LOG_NIC_OPS;
    //  logmask |= LOG_ETLM_TCP_CONNECTION | LOG_ARP;
    //  logmask |= LOG_PCIE_MEMORY_ALLOCATIONS |
    //  LOG_IF_ALL_BITS_MATCH; logmask |= LOG_ARP; logmask
    //  |= LOG_MAC; logmask |= LOG_FPGA; logmask |=
    //  LOG_ETLM; logmask |= LOG_FPGA_RESET; logmask |=
    //  LOG_INTERRUPTS; logmask = 0;

    // if (bufsize > 0)
    //     logmask |= LOG_MMU;
  }

  // fixed by ekaline
  //     logmask |= LOG_IGMP | LOG_NIC_OPS;

  // Log module parameters:
  printk(KERN_INFO MODULE_NAME
         ": Driver load parameters:\n");
  printk(KERN_INFO MODULE_NAME
         ":           input_delay : %d quants of 6.4 ns\n",
         input_delay);
  printk(KERN_INFO MODULE_NAME
         ":            pad_frames : %s\n",
         pad_frames ? "yes" : "no");
#if defined(SUPPORT_LOGGING) && (SUPPORT_LOGGING == 1)
  printk(KERN_INFO MODULE_NAME
         ":               logmask : 0x%016lX\n",
         logmask);
#else
  printk(
      KERN_INFO MODULE_NAME
      ":               logmask : logging not supported\n");
#endif
  printk(KERN_INFO MODULE_NAME
         ":     max_etlm_channels : %u\n",
         max_etlm_channels);
  printk(KERN_INFO MODULE_NAME
         ":    support_ul_devices : %s\n",
         support_ul_devices ? "yes" : "no");
#if MAX_ARP_ENTRIES != SC_MAX_TCP_CHANNELS
  printk(KERN_INFO MODULE_NAME
         ":      # of ARP entries : %u\n",
         MAX_ARP_ENTRIES);
#endif
  if (device_mapping[0] != '\0') {
    const char *mapping = "device_mapping :";
    unsigned int minor = 0;
    while (dev_data[minor].pcieDeviceName != NULL) {
      if (dev_data[minor].pcieDeviceName[0] != '\0') {
        printk(
            KERN_INFO MODULE_NAME
            ":        %s minor %2u : PCIe device name %s\n",
            mapping, minor, dev_data[minor].pcieDeviceName);
        mapping = "                ";
      }
      ++minor;
    }
  } else {
    printk(KERN_INFO MODULE_NAME
           ":        device_mapping : in probe order\n");
  }
  printk(KERN_INFO MODULE_NAME
         ":  max_lanes_per_device : %d\n",
         max_lanes_per_device);
  printk(KERN_INFO MODULE_NAME
         ":        interface_name : %s\n",
         interface_name);
  printk(KERN_INFO MODULE_NAME
         ":               bufsize : %lu MB\n",
         bufsize);
  printk(KERN_INFO MODULE_NAME
         ":    LINUX_VERSION_CODE : 0x%x\n",
         LINUX_VERSION_CODE);

  rc = pci_register_driver(&sc_pci_driver);
  if (rc < 0) {
    goto err;
  }

  rc = register_chrdev(major, MODULE_NAME, &file_ops);
  if (rc < 0) {
    goto err;
  }

  major = rc;

  return 0;
err:
  return rc;
}

/**
 *  This function is called when the driver module is
 * unloaded and makes various clean-up.
 */
static void __exit cleanup_sc_module(void) {
  unregister_chrdev(major, MODULE_NAME);
  pci_unregister_driver(&sc_pci_driver);

  printk(KERN_INFO MODULE_NAME
         ": %s device driver unloaded\n",
         MODULE_NAME);

  return;
}

module_init(init_sc_module);
module_exit(cleanup_sc_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(
    EKA_SN_DRIVER_BUILD_TIME); // added by ekaline
MODULE_DESCRIPTION(
    EKA_SN_DRIVER_RELEASE_NOTE); // added by ekaline
MODULE_DESCRIPTION(DRIVER_DESC);

/// @} ModuleLevelFunctions
