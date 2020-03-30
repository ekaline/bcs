#ifndef ADMIN_COMMANDS_H
#define ADMIN_COMMANDS_H

/**
  * List of command code and subcommand codes of the admin protocol
  * between testfgpa command line tool and SAVONA
  * (i.e. testfpga <-> PCIe <-> FPGA <-> SAVONA_CPU )
  */

//Command codes
#define GECKO_CMD_NOP                                   0x0
#define GECKO_CMD_WRITE_FLASH                           0x1
#define GECKO_CMD_READ_FLASH                            0x2
#define GECKO_CMD_LOG_AREA_READ                         0x3
#define GECKO_CMD_READ_TELEMETRY                        0x4
// Commands 0x5, 0x6 are unused
#define GECKO_CMD_CONTROL                               0x7



// Gecko Control sub commands
#define GECKO_SUBCMD_CTRL_GECKO_STATUS                  0x01
#define GECKO_SUBCMD_CTRL_FLASH_STATUS                  0x02
#define GECKO_SUBCMD_CTRL_SELECT_PRIMARY_FPGA_FLASH     0x03
#define GECKO_SUBCMD_CTRL_SELECT_FAILSAFE_FPGA_FLASH    0x04
#define GECKO_SUBCMD_CTRL_GET_REVISION_MSH              0x05
#define GECKO_SUBCMD_CTRL_GET_REVISION_LSH              0x06
#define GECKO_SUBCMD_CTRL_REBOOT_FPGA                   0x07
#define GECKO_SUBCMD_CTRL_GET_PLL_VERSION               0x08
#define GECKO_SUBCMD_CTRL_CHECK_FAILSAFE_BUTTON         0x09  // 1 Button is released, 2 button is pressed

#define GECKO_SUBCMD_CTRL_LED_TEST_START                0x10
#define GECKO_SUBCMD_CTRL_LED_TEST_STOP                 0x11

#define GECKO_SUBCMD_FLASH_BOOT_IMAGE_1                 0x22
#define GECKO_SUBCMD_FLASH_BOOT_IMAGE_2                 0x23

// Read of LSB stores the MSB for later read
#define GECKO_SUBCMD_CTRL_TIMER_GET_LSB                 0x30
#define GECKO_SUBCMD_CTRL_TIMER_GET_MSB                 0x31

#define GECKO_SUBCMD_CTRL_SET_TIMER_VALUE               0x32

// Write flash sub commands
#define GECKO_SUBCMD_FLASH_WRITE_START_IMAGE_1          0x01
#define GECKO_SUBCMD_FLASH_WRITE_START_PLL              0x02
#define GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_1	0x03
#define GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_2	0x04
#define GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_3	0x05
#define GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_4	0x06
#define GECKO_SUBCMD_FLASH_WRITE_START_IMAGE_2          0x07
#define GECKO_SUBCMD_FLASH_WRITE_ERASE_PAGES            0x08
#define GECKO_SUBCMD_FLASH_WRITE_DATA                   0x09
#define GECKO_SUBCMD_FLASH_WRITE_END                    0x0A
#define GECKO_SUBCMD_FLASH_WRITE_START_LOG_AREA         0x0B

// Read flash sub commands
#define GECKO_SUBCMD_FLASH_READ_IMAGE_1                 0x01
#define GECKO_SUBCMD_FLASH_READ_PLL                     0x02
#define GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_1          0x03
#define GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_2          0x04
#define GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_3          0x05
#define GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_4          0x06
#define GECKO_SUBCMD_FLASH_READ_IMAGE_2                 0x07

#define GECKO_SUBCMD_FLASH_READ_VERIFIED_IMAGE_1        0x0A
#define GECKO_VERIFY_SUCCEDED                           0x01
#define GECKO_VERIFY_FAILED                             0x02

#define GECKO_SUBCMD_FLASH_READ_GET_FIRST_LOG           0x20
#define GECKO_SUBCMD_FLASH_READ_GET_NEXT_LOG            0x21
#define GECKO_SUBCMD_FLASH_READ_LOG                     0x22

// Telemetry read sub commands
#define GECKO_SUBCMD_TELEMETRY_17V                                0x10
#define GECKO_SUBCMD_TELEMETRY_FPGA_V_3V3PCI                      0x11
#define GECKO_SUBCMD_TELEMETRY_FPGA_V_3V3                         0x12
#define GECKO_SUBCMD_TELEMETRY_DDR_VTT_AB_0V6                     0x13
#define GECKO_SUBCMD_TELEMETRY_DDR_VTT_CD_0V6                     0x14
#define GECKO_SUBCMD_TELEMETRY_DDR_VPP_2V5                        0x15
#define GECKO_SUBCMD_TELEMETRY_AUX_12V_SOURCE                     0x16
#define GECKO_SUBCMD_TELEMETRY_PCIE_12V_SOURCE                    0x17

#define GECKO_SUBCMD_TELEMETRY_AUX_12V_OUTPUT_CURRENT             0x18
#define GECKO_SUBCMD_TELEMETRY_PCIE_12V_OUTPUT_CURRENT            0x19
#define GECKO_SUBCMD_TELEMETRY_FPGA_I_3V3                         0x1A
#define GECKO_SUBCMD_TELEMETRY_DDR_VTT_AB_0V6_CURRENT             0x1B
#define GECKO_SUBCMD_TELEMETRY_DDR_VTT_CD_0V6_CURRENT             0x1C


#define GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_VOUT               0x20
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_IOUT               0x21
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE2_TEMP               0x22

#define GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_VOUT                  0x23
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_IOUT                  0x24
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_1V2_TEMP                  0x25

#define GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_VOUT                  0x26
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_IOUT                  0x27
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_0V9_TEMP                  0x28

#define GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_VOUT                  0x29
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_IOUT                  0x2A
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_1V8_TEMP                  0x2B

#define GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_VOUT               0x2C
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_IOUT               0x2D
#define GECKO_SUBCMD_TELEMETRY_INTERSIL_VCORE1_TEMP               0x2E

#define GECKO_SUBCMD_TELEMETRY_CPU_TEMP                           0x30
#define GECKO_SUBCMD_TELEMETRY_FPGA_CORE_TEMP                     0x31
#define GECKO_SUBCMD_TELEMETRY_FAN_INLET_TEMP                     0x32
#define GECKO_SUBCMD_TELEMETRY_DDR4_TEMP                          0x33



#endif
