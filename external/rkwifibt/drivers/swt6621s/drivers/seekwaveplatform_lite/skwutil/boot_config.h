/******************************************************************************
 *
 * Copyright(c) 2020-2030  Seekwave Corporation.
 *
 *****************************************************************************/
#ifndef __BOOT_CONFIG_H__
#define __BOOT_CONFIG_H__
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#ifdef CONFIG_SKW_NO_CONFIG
#define  MODEM_ENABLE_GPIO   	9
#define  HOST_WAKEUP_GPIO_IN 	10
#define  MODEM_WAKEUP_GPIO_OUT  3
#else
#define  MODEM_ENABLE_GPIO   	106
//#error "WIFI CHIP_EN Is Not Configured Pls Check The Config!!!"
#define  HOST_WAKEUP_GPIO_IN 	-1
#define  MODEM_WAKEUP_GPIO_OUT  -1
#endif
#define  SEEKWAVE_NV_NAME  "SEEKWAVE_NV_SWT6621S.bin"
//#define CONFIG_SEEKWAVE_FIRMWARE_LOAD
//#define  SKW_IRAM_FILE_PATH  "/vendor/firmware/SWT6621S_IRAM_USB.bin"
//#define  SKW_DRAM_FILE_PATH  "/vendor/firmware/SWT6621S_DRAM_USB.bin"
#define  SKW_POWER_OFF_VALUE   0

//#define SKW_SUPPORT_MMC_NONREMOVABLE //support the host mmc card is not removable but sdcard is removable default
#define CONFIG_SEEKWAVE_PLD_RELEASE 1
#define SKW_DUMP_BUFFER_SIZE     (1200*1024)
#define POWERON_DELAY_TIME  200
#define  SKW_DMA_TYPE_CFG   ADMA
#define  SKW_BT_ANTENNA_CFG   0

#define CONFIG_NO_SERVICE_PD 0 // default ps mode cp not poweroff


/***********************************************************
**CONFIG_SKW_HOST_SUPPORT_ADMA 1 : use ADMA	0 : use SDMA
**
***********************************************************/
//#define CONFIG_SKW_HOST_SUPPORT_ADMA

#if defined(CONFIG_SKW_HOST_SUPPORT_ADMA)
#define TX_DMA_TYPE		TX_ADMA
#else
#define TX_DMA_TYPE		TX_SDMA
#endif
//#define CONFIG_SKW_HOST_PLATFORM_AMLOGIC 1
//#define CONFIG_SKW_HOST_PLATFORM_FULLHAN

//#define  USB_POWEROFF_IN_LOWPOWER  1
#define SKW_CHIP_POWEROFF(gpiono) \
{ \
	if(gpiono >= 0) { \
		gpio_set_value(gpiono>>1, (gpiono&0x01)); \
	} \
}

#define SKW_CHIP_POWERON(gpiono) \
{ \
	if(gpiono >= 0) { \
	gpio_set_value(gpiono>>1, 1-(gpiono&0x01)); \
	} \
}
#define SKW_MMC_HOST_SD_INDEX  1 //default sd index is 1 if not 1 pls set to 0 or 2

#if defined(CONFIG_SKW_HOST_PLATFORM_FULLHAN)
#define SKW_MMC_HOST_SD_INDEX  1 //default sd index is 1 if not 1 pls set to 0 or 2
extern void fh_sdio_card_scan(int sd_id); //fullhan sdio card scan
#endif
#if defined(CONFIG_SKW_HOST_PLATFORM_AMLOGIC)
extern void extern_wifi_set_enable(int is_on);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ALLWINER)
extern void sunxi_wlan_set_power(int on);
extern void sunxi_mmc_rescan_card(unsigned ids);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ROCKCHIP)
extern int rockchip_wifi_power(int on);
extern int rockchip_wifi_set_carddetect(int val);
#else
extern int skw_chipen_gpio_reset(int on);
static inline int skw_chip_power_ops(int on)
{
	return skw_chipen_gpio_reset(on);
}
#endif

static inline void skw_chip_set_power(int on)
{
#if defined(CONFIG_SKW_HOST_PLATFORM_AMLOGIC)
	extern_wifi_set_enable(on);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ALLWINER)
	sunxi_wlan_set_power(on);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ROCKCHIP)
	rockchip_wifi_power(on);
#elif defined(CONFIG_SKW_HOST_PLATFORM_HISI_BIGFISH)
	hi_drv_gpio_set_dir_bit(MODEM_ENABLE_GPIO, 0);
	hi_drv_gpio_write_bit(MODEM_ENABLE_GPIO, on);
#else
	skw_chip_power_ops(on);
#endif

}
static inline void skw_chip_power_reset(void)
{
#if defined(CONFIG_SKW_HOST_PLATFORM_AMLOGIC)
	printk("amlogic skw chip power reset !!\n");
	extern_wifi_set_enable(0);
	msleep(POWERON_DELAY_TIME);
	extern_wifi_set_enable(1);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ALLWINER)
	printk("allwinner skw chip power reset !!\n");
	sunxi_wlan_set_power(0);
	msleep(POWERON_DELAY_TIME);
	sunxi_wlan_set_power(1);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ROCKCHIP)
	printk("rockchip skw chip power reset !!\n");
	rockchip_wifi_power(0);
	msleep(POWERON_DELAY_TIME);
	rockchip_wifi_power(1);
#elif defined(CONFIG_SKW_HOST_PLATFORM_HISI_BIGFISH)
	printk("hisi skw chip power reset !!\n");
	hi_drv_gpio_set_dir_bit(MODEM_ENABLE_GPIO,0);
	hi_drv_gpio_write_bit(MODEM_ENABLE_GPIO,0);
	msleep(POWERON_DELAY_TIME);
	hi_drv_gpio_write_bit(MODEM_ENABLE_GPIO,1);
#else
	printk("self skw chip power reset !!\n");
	skw_chip_power_ops(0);
	msleep(POWERON_DELAY_TIME);
	skw_chip_power_ops(1);
#endif
}
#endif /* __BOOT_CONFIG_H__ */
