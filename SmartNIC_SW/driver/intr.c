/*
 ***************************************************************************
 *
 * Copyright (c) 2008-2019, Silicom Denmark A/S
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Silicom nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with a
 *  Silicom network adapter product.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
#include <linux/sched.h>
#else
#include <linux/sched/signal.h> // Kernel 4.11 or newer has moved the declaration of send_sig_info here!
#endif

#include "main.h"
#include "intr.h"
#include "fpga.h"
#include "ndev.h"

#ifdef SC_SMARTNIC_API
int send_signal_to_user_process(const device_context_t * pDevExt, pid_t pid, int signalToSend, unsigned int channelNumber, void * pContext)
{
    int rc = 0;
    /* start of Ekaline fix*/
#if defined(RHEL_RELEASE_CODE) && defined(RHEL_RELEASE_VERSION)
    #if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 0)
    struct kernel_siginfo info;
    #else
    siginfo_t info;
    #endif
#else
    //    siginfo_t info;
    struct kernel_siginfo info;
#endif

    /* siginfo_t info; */
    /* end of Ekaline fix*/

    struct task_struct * pTask;
    sc_user_logic_interrupt_results * pInterruptResults = (sc_user_logic_interrupt_results *)((uint8_t *)(&info) + sizeof(info) - sizeof(sc_user_logic_interrupt_results));

    memset(&info, 0, sizeof(info));
    info.si_signo = signalToSend;
    info.si_code = SI_QUEUE;
    info.si_int = SC_MAGIC_SIGNAL_INT;

    // Interrupt results are located at the end of the siginfo_t structure.
    pInterruptResults->minor = pDevExt->minor;
    pInterruptResults->channel = channelNumber;
    pInterruptResults->pContext = pContext;
    //    getnstimeofday(&pInterruptResults->timestamp);
    ktime_get_real_ts64(&pInterruptResults->timestamp);

    // Locate user space task from the process identifier pid:
    pTask = get_pid_task(find_get_pid(pid), PIDTYPE_PID);
    if (pTask == NULL)
    {
        PRINTK_ERR("Failed to find user space process PID %d\n", pid);
        return -ENODEV;
    }
    rc = send_sig_info(signalToSend, &info, pTask); // Send signal to user space
    if (rc < 0)
    {
        PRINTK_ERR("Failed to send signal %u to user process PID %u on channel %u pContext %p, rc = %d\n", signalToSend, pid, channelNumber, pContext, rc);
        return -EFAULT;
    }

    PRINTK_C(LOGALL(LOG_INTERRUPTS | LOG_DETAIL), "successfully sent signal %d to user space process PID %d, channel %d, pContext %p\n", signalToSend, pid, channelNumber, pContext);

    return rc;
}
#endif /* SC_SMARTNIC_API */

static inline int oob_channel_to_physical_port(const device_context_t * pDevExt, int dma_channel_no)
{
    BUG_ON(dma_channel_no < FIRST_OOB_CHANNEL || dma_channel_no >= FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS);
    return dma_channel_no - FIRST_OOB_CHANNEL;
}

static irqreturn_t sc_msix_oob_isr(int irq, void * data)
{
    struct intr_setup_t * intr_setup = (struct intr_setup_t *)data;
    const device_context_t * pDevExt = (device_context_t *)intr_setup->pDevExt;
    unsigned int dma_channel_no = intr_setup->dma_channel_no;

    __sync_fetch_and_add(&intr_setup->intr_cnt, 1);

    // No need to disable interrupts since they are one-shot

    if (dma_channel_no >= FIRST_OOB_CHANNEL && dma_channel_no < FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS)
    {
        // OOB (standard NIC) interrupt

        int dev_no = oob_channel_to_physical_port(pDevExt, dma_channel_no);
        fb_schedule_napi_dev(pDevExt, dev_no);
    }
    else if (dma_channel_no >= FIRST_UL_CHANNEL && dma_channel_no < FIRST_UL_CHANNEL + SC_MAX_ULOGIC_CHANNELS)
    {
        // User logic interrupt

#ifdef SC_SMARTNIC_API
        const sc_user_logic_interrupts * p = &pDevExt->user_logic_interrupts;
        if (p->mask != 0)
        {
            send_signal_to_user_process(pDevExt, p->pid, p->signal, dma_channel_no, p->pContext);
        }
#endif /* SC_SMARTNIC_API */
    }

    return IRQ_HANDLED;
}

static int msix_setup(device_context_t * pDevExt)
{
    int ret = 0;
    int i = 0;
    struct pci_dev * pdev = pDevExt->pdev;
    struct msix_entry *msix_entries = NULL;

    // Channels use the interrupt corresponding to their channel numbers (implicit one-to-one mapping)
    // This means that feth7 (channel 15) will use interrupt 15, hence this check
#ifdef SC_SMARTNIC_API
    #if FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS != MAX_INTR
        #error "Number of interrupts required is different from hardcoded MAX_INTR, code must be changed"
    #endif
    #ifdef SC_PACKETMOVER_API
        #error "what???"
    #endif /* SC_PACKETMOVER_API */
#endif /* SC_SMARTNIC_API */
    pDevExt->num_irq = MAX_INTR;

    msix_entries = kcalloc(pDevExt->num_irq, sizeof(struct msix_entry), GFP_KERNEL);
    if (!msix_entries)
    {
        PRINTK_ERR("ERROR: failed to allocate msix entry array\n");
        return -ENOMEM;
    }

    for (i = 0; i < pDevExt->num_irq; i++)
    {
        msix_entries[i].vector = 0; /* kernel uses to write alloc vector */
        msix_entries[i].entry = i; /* driver uses to specify entry */
    };

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 13, 11) // Below Ubuntu 12.05.5 and 14.04; CentOS 6.9 and 7.4
    // dmesg/syslog entries like " 0000:01:00.0: irq 59 for MSI/MSI-X" are output by this function
    ret = pci_enable_msix(pdev, msix_entries, pDevExt->num_irq);
    if (ret)
    {
        PRINTK_ERR("ERROR: pci_enable_msix failed! rc: %i\n", ret);
#else
    ret = pci_enable_msix_range(pdev, msix_entries, pDevExt->num_irq, pDevExt->num_irq);

    PRINTK_C(LOG(LOG_MSI_MSIX), "Called pci_enable_msix_range with result %d, pDevExt->num_irq %d 0x%x 0x%x\n",
        ret, pDevExt->num_irq, LINUX_VERSION_CODE, KERNEL_VERSION(3, 13, 11));

    if (ret < 0 || ret != pDevExt->num_irq)
    {
        PRINTK_ERR("ERROR: pci_enable_msix_range failed! rc: %i, pDevExt->num_irq %d\n", ret, pDevExt->num_irq);
#endif
        kfree(msix_entries);
        if (ret < 0)
        {
            return ret;
        }
        return -EINVAL;
    }

    PRINTK_C(LOG(LOG_INIT | LOG_MSI_MSIX), "pci_enable_msix allocated %i entries successfully\n", pDevExt->num_irq);

    for (i = 0; i < pDevExt->num_irq; i++)
    {
        struct intr_setup_t * intr = &(pDevExt->intrs[i]);
        int irq = msix_entries[i].vector;

        intr->pDevExt = pDevExt;
        intr->irq = irq;
        intr->dma_channel_no = i;
        intr->intr_cnt = 0;

        intr->name = kmalloc(32, GFP_KERNEL);
        snprintf(intr->name, 32, MODULE_NAME "-%d", i);

        ret = request_irq(irq, sc_msix_oob_isr, 0, pDevExt->intrs[i].name, intr);
        if (ret)
        {
            PRINTK_ERR("ERROR: Can't assign interrupt request_irq returned %i, when requesting IRQ %i\n", ret, irq);

            while (--i >= 0)
            {
                struct intr_setup_t * intr = &(pDevExt->intrs[i]);
                free_irq(intr->irq, intr);
                free_cpumask_var(intr->affinity_mask);
                kfree(intr->name);
                intr->name = NULL;
            }

            pDevExt->num_irq = 0;
            kfree(msix_entries);
            return ret;
        }


        if (!alloc_cpumask_var(&intr->affinity_mask, GFP_KERNEL))
        {
            PRINTK_WARN("could not allocate cpu affinity hint for interrupt %d\n", irq);
        }
        else
        {
            const int num_cpus = num_online_cpus();
            /* Interrupt affinity:
                - start with CPU 2 to avoid CPU 0/1,
                - use even cores (2,4,6,..) at first iteration to avoid hyperthreaded cores
            */
            int j = i + 2;
            int cpu = (2 * j) % num_cpus;
            cpu += (2 * j / num_cpus) % 2;
            cpumask_set_cpu(cpu, intr->affinity_mask);
            if (irq_set_affinity_hint(irq, intr->affinity_mask))
            {
                PRINTK_WARN("setting cpu affinity hint for interrupt %d failed!\n", irq);
            }
            else
            {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 13, 11) // Above Ubuntu 12.05.5 and 14.04
                if (LOG(LOG_INIT | LOG_MSI_MSIX)) // Log always on newer kernel versions
#endif
                {
                    PRINTK("intr %d affinity to cpu %d of %d\n", i, cpu, num_cpus);
                }
            }
        }
    }

    PRINTK_C(LOG(LOG_INIT | LOG_MSI_MSIX), "msiz_setup finished successfully\n");

    kfree(msix_entries);
    return 0;
    }

static void msix_teardown(device_context_t * pDevExt)
{
    struct pci_dev * pdev = pDevExt->pdev;
    int i;

    for (i = 0; i < pDevExt->num_irq; i++)
    {
        struct intr_setup_t * intr = &(pDevExt->intrs[i]);
        irq_set_affinity_hint(intr->irq, NULL);
        free_irq(intr->irq, intr);

        free_cpumask_var(intr->affinity_mask);

        kfree(intr->name);
        pDevExt->intrs[i].name = NULL;
    }

    pci_disable_msix(pdev);

    pDevExt->num_irq = 0;
}

int interrupts_setup(device_context_t * pDevExt)
{
    return msix_setup(pDevExt);
}

void interrupts_teardown(device_context_t * pDevExt)
{
    msix_teardown(pDevExt);
}
