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

#include <linux/kernel.h>

#include "main.h"
#include "acl.h"
#include "fpga.h"

#define ACL_ENGINE              0x8     // Offset of ACL engine control register in BAR 2 (Software Interfaces: 13 ACL module)

#define ACL_ENABLE              0x02
#define ACL_SET_IP_RX           0x21
#define ACL_SET_IP_MASK_RX      0x22
#define ACL_SET_PORT_RANGE_RX   0x23
#define ACL_SET_IP_TX           0x24
#define ACL_SET_IP_MASK_TX      0x25
#define ACL_SET_PORT_RANGE_TX   0x26

#define FPGA_ACL_ID_RX          0   // User interface value is ACL_ID_RX
#define FPGA_ACL_ID_TX          1   // User interface value is ACL_ID_TX

static void acl_write_ip_addr(const device_context_t * pDevExt, uint8_t network_interface, int acl_id_index, uint8_t index, uint32_t ip)
{
    uint64_t value;
    value = (uint64_t)network_interface << 56;
    value |= (uint64_t)(acl_id_index == FPGA_ACL_ID_TX ? ACL_SET_IP_TX : ACL_SET_IP_RX) << 40;
    value |= (uint64_t)index << 32;
    value |= ip;

    PRINTK_C(LOG(LOG_ACL | LOG_REG_W), "acl_write_ip_addr writes 0x%016llx to reg 0x%x at addr 0x%p\n",
        value, ACL_ENGINE, pDevExt->bar2_va + ACL_ENGINE);

    writeq(value, pDevExt->bar2_va + ACL_ENGINE);
}

static void acl_write_ip_mask(const device_context_t * pDevExt, uint8_t network_interface, int acl_id_index, uint8_t index, uint32_t ip_mask)
{
    uint64_t value;
    value = (uint64_t)network_interface << 56;
    value |= (uint64_t)(acl_id_index == FPGA_ACL_ID_TX ? ACL_SET_IP_MASK_TX : ACL_SET_IP_MASK_RX) << 40;
    value |= (uint64_t)index << 32;
    value |= ip_mask;

    PRINTK_C(LOG(LOG_ACL | LOG_REG_W), "acl_write_ip_mask writes 0x%016llx to reg 0x%x at addr 0x%p\n",
        value, ACL_ENGINE, pDevExt->bar2_va + ACL_ENGINE);

    writeq(value, pDevExt->bar2_va + ACL_ENGINE);
}

static void acl_write_ports_minmax(const device_context_t * pDevExt, uint8_t network_interface, int acl_id, uint8_t index, uint16_t port_min, uint16_t port_max)
{
    uint64_t value;
    value = (uint64_t)network_interface << 56;
    value |= (uint64_t)(acl_id == ACL_ID_TX ? ACL_SET_PORT_RANGE_TX : ACL_SET_PORT_RANGE_RX) << 40;
    value |= (uint64_t)index << 32;
    value |= (uint64_t)port_min << 16;
    value |= port_max;

    PRINTK_C(LOG(LOG_ACL | LOG_REG_W), "acl_write_ports_minmax writes 0x%016llx to reg 0x%x at addr 0x%p\n",
        value, ACL_ENGINE, pDevExt->bar2_va + ACL_ENGINE);

    writeq(value, pDevExt->bar2_va + ACL_ENGINE);
}

static void acl_enable(const device_context_t * pDevExt, uint8_t network_interface, uint8_t enable)
{
    uint64_t value;
    value = (uint64_t)network_interface << 56;
    value |= (uint64_t)ACL_ENABLE << 40;
    value |= enable ? 4 : 8;

    PRINTK_C(LOG(LOG_ACL | LOG_REG_W), "acl_enable writes 0x%016llx to reg 0x%x at addr 0x%p\n",
        value, ACL_ENGINE, pDevExt->bar2_va + ACL_ENGINE);

    writeq(value, pDevExt->bar2_va + ACL_ENGINE);
}

static int acl_edit_entry(acl_context_t * this, const sc_acl_config_t *acl)
{
    if (acl->acl_id != ACL_ID_TX && acl->acl_id != ACL_ID_RX)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_edit_entry: invalid acl_id value %d\n", acl->acl_id);
        }
        return -EINVAL;
    }

    if (acl->acl_id_index < 0 || acl->acl_id_index >= ACL_ID_COUNT)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_edit_entry: invalid acl_id_index value %d\n", acl->acl_id_index);
        }
        return -EINVAL;
    }

    if (acl->index < 0 || acl->index >= ACL_TABLE_SZ)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_edit_entry: invalid index value %d\n", acl->index);
        }
        return -EINVAL;
    }

    if (acl->network_interface < 0 || acl->network_interface >= this->pDevExt->nb_netdev)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_edit_entry: invalid network interface value %d\n", acl->network_interface);
        }
        return -EINVAL;
    }

    // Change values in shadow ACL table
    this->table[acl->acl_id_index][acl->network_interface][acl->index].ip = acl->ip;
    this->table[acl->acl_id_index][acl->network_interface][acl->index].ip_mask = acl->ip_mask;
    this->table[acl->acl_id_index][acl->network_interface][acl->index].port_min = acl->port_min;
    this->table[acl->acl_id_index][acl->network_interface][acl->index].port_max = acl->port_max;

    acl_write_ip_addr(this->pDevExt, acl->network_interface, acl->acl_id_index, acl->index, acl->ip);
    acl_write_ip_mask(this->pDevExt, acl->network_interface, acl->acl_id_index, acl->index, acl->ip_mask);
    acl_write_ports_minmax(this->pDevExt, acl->network_interface, acl->acl_id_index, acl->index, acl->port_min, acl->port_max);

    return 0;
}

static int acl_reset_entry(acl_context_t * this, uint8_t network_interface, int acl_id_index, uint16_t index)
{
    const uint32_t RESET_MASK = 0xFFFFFFFF;

    if (acl_id_index < 0 || acl_id_index >= ACL_ID_COUNT)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_reset_entry: invalid acl_id_index value %d\n", acl_id_index);
        }
        return -EINVAL;
    }

    if (index < 0 || index >= ACL_TABLE_SZ)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_reset_entry: invalid index value %d\n", index);
        }
        return -EINVAL;
    }

    if (network_interface < 0 || network_interface >= this->pDevExt->nb_netdev)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_reset_entry: invalid network interface value %d\n", network_interface);
        }
        return -EINVAL;
    }

    // Change values in shadow ACL table
    this->table[acl_id_index][network_interface][index].ip = 0;
    this->table[acl_id_index][network_interface][index].ip_mask = RESET_MASK;
    this->table[acl_id_index][network_interface][index].port_min = 0;
    this->table[acl_id_index][network_interface][index].port_max = 0;

    acl_write_ip_addr(this->pDevExt, network_interface, acl_id_index, index, 0);
    acl_write_ip_mask(this->pDevExt, network_interface, acl_id_index, index, RESET_MASK);
    acl_write_ports_minmax(this->pDevExt, network_interface, acl_id_index, index, 0, 0);

    return 0;
}

int acl_reset_all(acl_context_t * this)
{
    uint8_t phys_port;
    uint16_t index;

    int rc = 0;

    for (phys_port = 0; phys_port < this->pDevExt->nb_netdev; ++phys_port)
    {
        for (index = 0; index < ACL_TABLE_SZ; ++index)
        {
            rc |= acl_reset_entry(this, phys_port, FPGA_ACL_ID_TX, index);
            rc |= acl_reset_entry(this, phys_port, FPGA_ACL_ID_RX, index);
        }
    }

    if (LOG(LOG_ACL))
    {
        const device_context_t * pDevExt = this->pDevExt;
        PRINTK("All ACL lists have been reset.\n");
    }

    return rc;
}

static int acl_get_entry(acl_context_t * this, sc_acl_config_t *acl)
{
    if (acl->acl_id != ACL_ID_TX && acl->acl_id != ACL_ID_RX)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_get_entry: invalid acl_id value %d\n", acl->acl_id);
        }
        return -EINVAL;
    }

    if (acl->acl_id_index < 0 || acl->acl_id_index >= ACL_ID_COUNT)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_get_entry: invalid acl_id_index value %d\n", acl->acl_id_index);
        }
        return -EINVAL;
    }

    if (acl->index < 0 || acl->index >= ACL_TABLE_SZ)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_get_entry: invalid index value %d\n", acl->index);
        }
        return -EINVAL;
    }

    if (acl->network_interface < 0 || acl->network_interface >= this->pDevExt->nb_netdev)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("acl_get_entry: invalid network interface value %d\n", acl->network_interface);
        }
        return -EINVAL;
    }

    // Change values in shadow ACL table
    acl->ip = this->table[acl->acl_id_index][acl->network_interface][acl->index].ip;
    acl->ip_mask = this->table[acl->acl_id_index][acl->network_interface][acl->index].ip_mask;
    acl->port_min = this->table[acl->acl_id_index][acl->network_interface][acl->index].port_min;
    acl->port_max = this->table[acl->acl_id_index][acl->network_interface][acl->index].port_max;

    return 0;
}

int sc_acl_cmd(acl_context_t * this, unsigned long data)
{
    int rc = 0;
    const char * acl_command = " *** UNKNOWN COMMAND *** ";

    sc_acl_config_t acl;
    if (copy_from_user(&acl, (void*)data, sizeof(acl)))
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("%s: copy_from_user failed\n", __func__);
        }
        return -EFAULT;
    }

    switch (acl.cmd)
    {
        case ACL_CMD_EDIT:          acl_command = "ACL_CMD_EDIT"; break;
        case ACL_CMD_RESET:         acl_command = "ACL_CMD_RESET"; break;
        case ACL_CMD_GET:           acl_command = "ACL_CMD_GET"; break;
        case ACL_CMD_ENABLE:        acl_command = "ACL_CMD_ENABLE"; break;
        case ACL_CMD_DISABLE:       acl_command = "ACL_CMD_DISABLE"; break;
        case ACL_CMD_IS_SUPPORTED:  acl_command = "ACL_CMD_IS_SUPPORTED"; break;
        default: break;
    }

    if (LOG(LOG_ACL | LOG_DETAIL))
    {
        const device_context_t * pDevExt = this->pDevExt;
        PRINTK(">%s: cmd %s network interface %d id %d index %d ip 0x%08x mask 0x%08x port min %u port max %u\n", __func__,
            acl_command, acl.network_interface, acl.acl_id, acl.index, acl.ip, acl.ip_mask, acl.port_min, acl.port_max);
    }

    if (acl.network_interface < -1 || acl.network_interface >= SC_NO_NETWORK_INTERFACES)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("%s: invalid network interface %d\n", __func__, acl.network_interface);
        }
        return -ENODEV; // Invalid network interface
    }

    if (acl.network_interface != -1 && (this->acl_nif_mask & (1 << acl.network_interface)) == 0)
    {
        if (LOG(LOG_ACL | LOG_ERROR))
        {
            const device_context_t * pDevExt = this->pDevExt;
            PRINTK_ERR("%s: ACL network interface %d not present in FPGA configuration\n", __func__, acl.network_interface);
        }
        return -ENODEV; // ACL not present in FPGA configuration on this network interface
    }

    acl.acl_id_index = acl.acl_id == ACL_ID_TX ? FPGA_ACL_ID_TX : (acl.acl_id == ACL_ID_RX ? FPGA_ACL_ID_RX : -1);

    switch (acl.cmd)
    {
        case ACL_CMD_EDIT:
            if (LOG(LOG_ACL))
            {
                const device_context_t * pDevExt = this->pDevExt;
                PRINTK("ACL: cmd %d, nif %d, ACL id %d, index %d\n",
                    acl.cmd, acl.network_interface, acl.acl_id, acl.index);
            }
            rc = acl_edit_entry(this, &acl);
            break;

        case ACL_CMD_RESET:
            {
                int port, pmin, pmax;
                int idx, idxmin, idxmax;

                if (LOG(LOG_ACL))
                {
                    const device_context_t * pDevExt = this->pDevExt;
                    PRINTK("ACL reset: cmd %d, nif %d, ACL id %d, index %d\n",
                        acl.cmd, acl.network_interface, acl.acl_id_index, acl.index);
                }
                pmin = acl.network_interface;
                pmax = pmin + 1;
                if (acl.network_interface == NO_NIF)
                {
                    pmin = 0;
                    pmax = this->pDevExt->nb_netdev;
                }
                idxmin = acl.index;
                idxmax = idxmin + 1;
                if (acl.index == -1)
                {
                    idxmin = 0;
                    idxmax = ACL_TABLE_SZ;
                }

                rc = 0;
                for (port = pmin; port < pmax; port++)
                {
                    for (idx = idxmin; idx < idxmax; idx++)
                    {
                        if (acl.acl_id == ACL_ID_BOTH) { // ACL_ID_BOTH clears both Tx and Rx tables
                            if (LOG(LOG_ACL))
                            {
                                const device_context_t * pDevExt = this->pDevExt;
                                PRINTK("ACL reset: %d %d %d\n", port, FPGA_ACL_ID_TX, idx);
                            }
                            rc |= acl_reset_entry(this, port, FPGA_ACL_ID_TX, idx);
                            if (LOG(LOG_ACL))
                            {
                                const device_context_t * pDevExt = this->pDevExt;
                                PRINTK("ACL reset: %d %d %d\n", port, FPGA_ACL_ID_RX, idx);
                            }
                            rc |= acl_reset_entry(this, port, FPGA_ACL_ID_RX, idx);
                        }
                        else
                        {
                            if (LOG(LOG_ACL))
                            {
                                const device_context_t * pDevExt = this->pDevExt;
                                PRINTK("ACL reset: %d %d %d\n", port, acl.acl_id_index, idx);
                            }
                            rc |= acl_reset_entry(this, port, acl.acl_id_index, idx);
                        }
                    }
                }
            }
            break;

        case ACL_CMD_GET:
            rc = acl_get_entry(this, &acl);
            if (LOG(LOG_ACL | LOG_DETAIL))
            {
                const device_context_t * pDevExt = this->pDevExt;
                PRINTK("<%s: cmd %s network interface %d id %d index %d ip 0x%08x mask 0x%08x port min %u port max %u\n", __func__,
                    acl_command, acl.network_interface, acl.acl_id, acl.index, acl.ip, acl.ip_mask, acl.port_min, acl.port_max);
            }
            break;

        case ACL_CMD_ENABLE:
            acl_enable(this->pDevExt, acl.network_interface, 1);
            break;

        case ACL_CMD_DISABLE:
            acl_enable(this->pDevExt, acl.network_interface, 0);
            break;

        case ACL_CMD_IS_SUPPORTED:
            acl.acl_nif_mask = this->acl_nif_mask;
            break;

        default:
            rc = -EINVAL;
            break;
    }

    if (copy_to_user((void __user *)data, &acl, sizeof(acl)))
    {
        return -EFAULT;
    }

    return rc;
}
