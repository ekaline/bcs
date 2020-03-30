#include <cassert>
#include <cstring>
#include <fstream>
#include <string>

#include "smartnic.h"
#include "smartnic_internal.h"

//  Useful ARP commands:
//      List entries:
//          arp
//          arp -a -n
//          cat /proc/net/arp
//      Delete ARP entry from local ARP table: sudo arp -d 10.0.0.2
//


ARP::ARP(SC_DeviceId deviceId, const std::string & linuxArpTableName)
    : _deviceId(deviceId), _arpTable(), _linuxArpTableName(linuxArpTableName)
    , _retryLimit(3), _retryCount(_retryLimit)
    , _quiet(" -q")
{
}

/**
 *  Parse output from /proc/net/arp.
 */
SC_Error ARP::UpdateARP()
{
    std::ifstream arpFile(_linuxArpTableName);
    if (arpFile.fail())
    {
        return CallErrorHandler(SC_ERR_INVALID_ARGUMENT, "Cannot open ARP table at '%s'", _linuxArpTableName.c_str());
    }

    std::vector<ARP_Info> arpTable;

    if (LOG(_deviceId, LOG_ARP))
    {
        LogToFileNoTime(stdout, _deviceId, "UpdateARP: IP address       HW type     Flags       HW address            Mask     Interface\n");
    }

    bool firstLine = true;
    while (arpFile.good())
    {
        std::string arpLine;
        std::getline(arpFile, arpLine);
        if (!firstLine && arpLine.length() > 0)
        {
            ARP_Info arpInfo;
            char host[20], mac[30], mask[10], interface[100];

            // Parse one line from /proc/net/arp output like this:
            // 10.100.0.20      0x1         0x2         00:27:0e:07:d1:ef     *        eno1
            int numberOfItemsFound = std::sscanf(arpLine.c_str(), "%s %x %x %s %s %s", host, &arpInfo.HWType, &arpInfo.Flags, mac, mask, interface);
            if (numberOfItemsFound != 6)
            {
                return CallErrorHandler(SC_ERR_ARP_FAILED, "Failed to parse 6 values from ARP table line '%s'", arpLine.c_str());
            }

            in_addr address;
            if (inet_aton(host, &address) == 0)
            {
                return CallErrorHandler(SC_ERR_ARP_FAILED, "Failed to parse rmeote host IP address from ARP table line '%s'", arpLine.c_str());
            }
            arpInfo.IpAddress = address.s_addr;
            arpInfo.Host = host;
            if (!ParseMacAddress(mac, arpInfo.MacAddress))
            {
                return CallErrorHandler(SC_ERR_ARP_FAILED, "Failed to parse MAC address from ARP table line '%s'", arpLine.c_str());
            }
            arpInfo.Mask = mask;
            arpInfo.Interface = interface;

            arpTable.push_back(arpInfo);

            if (LOG(_deviceId, LOG_ARP))
            {
                std::vector<char> constructedLine;
                constructedLine.reserve(1000);
                snprintf(&constructedLine[0], constructedLine.capacity(), "%-17s0x%-10x0x%-10x%-22s%-9s%s",
                    arpInfo.Host.c_str(), arpInfo.HWType, arpInfo.Flags, mac, arpInfo.Mask.c_str(), arpInfo.Interface.c_str());
                std::string s = &constructedLine[0];
                //SCI_Assert(arpLine == s.erase(s.find_last_not_of(" ") + 1));
                SCI_Assert(arpLine == s);

                LogToFileNoTime(stdout, _deviceId, "           %s%s\n           %s%s\n",
                    arpLine.append(85 - arpLine.length(), ' ').c_str(), "# original",
                    s.append(85 - s.length(), ' ').c_str(), ("# constructed from " + _linuxArpTableName).c_str());
            }
        }
        firstLine = false;
    }

    _arpTable = arpTable;

    return SC_ERR_SUCCESS;
}

void ARP::DumpARP(FILE * pFile)
{
    LogToFileNoTime(pFile, _deviceId, "               IP address       HW type     Flags       HW address            Mask     Interface\n");

    for (std::size_t i = 0; i < _arpTable.size(); ++i)
    {
        std::string macString;

        const ARP_Info & arpInfo = _arpTable[i];

        std::vector<char> constructedLine;
        constructedLine.reserve(1000);
        snprintf(&constructedLine[0], constructedLine.capacity(), "%-17s0x%-10x0x%-10x%-22s%-9s%s",
            arpInfo.Host.c_str(), arpInfo.HWType, arpInfo.Flags, SCI_LogMAC(macString, arpInfo.MacAddress), arpInfo.Mask.c_str(), arpInfo.Interface.c_str());

        LogToFileNoTime(pFile, _deviceId, "%s\n", &constructedLine[0]);
    }
}

SC_Error ARP::MakeArpRequest(ArpMethod arpMethod, uint8_t networkInterface, const std::string & interface, const std::string & remoteHost, int16_t vlanTag)
{
    switch (arpMethod)
    {
        case ARP_METHOD_ARPING_UTILITY:
            {
                std::string arpingCommand = "arping" + _quiet + " -c 1 -I " + interface + " " + remoteHost;
                int status = system(arpingCommand.c_str());
                if (status != 0)
                {
                    return CallErrorHandler(SC_ERR_ARP_FAILED, "arping command failed with status code %d. No MAC address found for host %s", status, remoteHost.c_str());
                }
            }
            break;

        case ARP_METHOD_PING_UTILITY:
            {
                std::string pingCommand = "ping" + _quiet + " -c 1 " + remoteHost + ">/dev/null 2>/dev/null";
                //printf("running '%s'\n", pingCommand.c_str());
                int status = system(pingCommand.c_str());
                //printf("'%s' returned %d\n", pingCommand.c_str(), status);
                if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGINT || WTERMSIG(status) == SIGQUIT))
                {
                    return CallErrorHandler(SC_ERR_ARP_FAILED, "ping command '%s' stopped by signal (status %d). No MAC address found for host %s",
                        pingCommand.c_str(), status, remoteHost.c_str());
                }
                else if (WIFEXITED(status) != 0 && WEXITSTATUS(status) != 0)
                {
                    return CallErrorHandler(SC_ERR_ARP_FAILED,
                        "ping command '%s' failed with status code %d (WIFEXITED(status) = %d, WEXITSTATUS(status) = %d). No MAC address found for host %s",
                        pingCommand.c_str(), status, WIFEXITED(status), WEXITSTATUS(status), remoteHost.c_str());
                }
            }
            break;

        case ARP_METHOD_ARP_REQUEST_VIA_DRIVER:
            {
                struct in_addr address;
                if (inet_aton(remoteHost.c_str(), &address) == 0)
                {
                    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT, "Invalid remote IP address %s. errno %d", remoteHost.c_str(), errno);
                }

                // Make a few retries to send ARP request because there might be temporary congestion in Linux send queue (send_skb in driver).
                int retries = 5;
                while (true)
                {
                    int rc;
                    int fd = SC_GetFileDescriptor(_deviceId);

                    sc_arp_t arp;
                    memset(&arp, 0, sizeof(arp));

                    arp.command = sc_arp_t::ARP_COMMAND_SEND;
                    arp.remote_ip = ntohl(address.s_addr);
                    arp.vlanTag = vlanTag;
                    arp.lane = networkInterface;
                    arp.arp_entry_timeout = _deviceId->InternalDeviceOptions.DeviceOptions.ArpEntryTimeout;
                    if ((rc = ioctl(fd, SC_IOCTL_ARP, &arp)) == 0)
                    {
                        break; // Succeeded, get out
                    }
                    if (--retries < 0)
                    {
                        return CallErrorHandler(SC_ERR_ARP_FAILED, "Failed to send ARP request to remote host %s, errno %d", remoteHost.c_str(), errno);
                    }
                }
            }
            break;

        case ARP_METHOD_NONE:
        case ARP_METHOD_ARP_TABLE_LOOKUP_ONLY:
            return CallErrorHandler(SC_ERR_ARP_FAILED, "ARP method %d not implemented! No MAC address found for host %s", arpMethod, remoteHost.c_str());
    }

    return UpdateARP(); // Read Linux ARP table to see if the MAC address is now resolved
}

SC_Error ARP::GetMacAddress(ArpMethod arpMethod, uint8_t networkInterface, const std::string & interface, const std::string & remoteHost,
                            int16_t vlanTag, MacAddress_t macAddress, uint64_t acquireMacAddressViaArpTimeout)
{
    const int PauseTimeInMilliseconds = 1;
    int64_t timeout = int64_t(acquireMacAddressViaArpTimeout);
    bool firstTry = true;

    do
    {
        memset(macAddress, 0, MAC_ADDR_LEN);

        for (std::size_t i = 0; i < _arpTable.size(); ++i)
        {
            const ARP_Info & arpInfo = _arpTable[i];
            if (arpInfo.Host == remoteHost && arpInfo.Flags == 2)
            {
                // Found the host, is MAC address valid (different from zero)?
                memcpy(macAddress, arpInfo.MacAddress, MAC_ADDR_LEN);
                if (MacAddressIsNotZero(arpInfo.MacAddress))
                {
                    if (LOG(_deviceId, LOG_ARP))
                    {
                        Log(_deviceId, "Found MAC address for remote host %s on interface %s\n", remoteHost.c_str(), interface.c_str());
                    }
                    return SC_ERR_SUCCESS; // MAC address found in table
                }

                // Host found in table but MAC address is missing (incomplete), send ARP request
                break;
            }
        }

        switch (arpMethod)
        {
            case ARP_METHOD_ARP_TABLE_LOOKUP_ONLY:
                return SetLastErrorCode(SC_ERR_ARP_FAILED);
            case ARP_METHOD_NONE:
                return CallErrorHandler(SC_ERR_ARP_FAILED, "MAC address for remote host %s on network interface %u not found in ARP table after %lu milliseconds!",
                    remoteHost.c_str(), networkInterface, acquireMacAddressViaArpTimeout);
            case ARP_METHOD_ARPING_UTILITY:
            case ARP_METHOD_PING_UTILITY:
            case ARP_METHOD_ARP_REQUEST_VIA_DRIVER:
                break;
        }

        if (firstTry)
        {
            // MAC address not found in internal table, read Linux ARP table and retry.
            UpdateARP();
            firstTry = false;
        }
        else
        {
            // Not found in interbal table or Linux ARP table, send ARP request
            SC_Error errorCode = MakeArpRequest(arpMethod, networkInterface, interface, remoteHost, vlanTag);
            if (errorCode != SC_ERR_SUCCESS)
            {
                return errorCode;
            }

            usleep(PauseTimeInMilliseconds * 1000); // Wait a millisecond and then retry
            timeout -= PauseTimeInMilliseconds;
        }

    } while (timeout > 0);

    return CallErrorHandler(SC_ERR_ARP_FAILED, "MAC address via ARP for host %s on network interface %u not found after %lu milliseconds!",
        remoteHost.c_str(), networkInterface, acquireMacAddressViaArpTimeout);
}

void SCI_DumpARP(SC_DeviceId deviceId, FILE * pFile)
{
    deviceId->Arp.DumpARP(pFile);
}

SC_Error SCI_ARP_Test(uint8_t networkInterface, const std::string & interface, const std::string & remoteHost, uint8_t macAddress[MAC_ADDR_LEN], uint64_t timeout)
{
    ARP arp(nullptr);
    //arp.UpdateArpInfo();
    //uint8_t macAddress[MAC_ADDR_LEN];
    //return arp.FindMacAddress("eno1", "172.16.10.1", true, macAddress);
    return arp.GetMacAddress(ARP::ARP_METHOD_PING_UTILITY, networkInterface, interface, remoteHost, NO_VLAN_TAG, macAddress, timeout);
}

