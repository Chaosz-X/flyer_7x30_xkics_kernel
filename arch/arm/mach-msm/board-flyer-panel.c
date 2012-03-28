/* linux/arch/arm/mach-msm/board-flyer-panel.c
 *
 * Copyright (c) 2010 HTC.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/io.h>
#include <asm/mach-types.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <mach/vreg.h>
#include <mach/msm_fb.h>
#include <mach/msm_iomap.h>
#include <mach/atmega_microp.h>
#include <mach/panel_id.h>
#include <mach/debug_display.h>

#include "board-flyer.h"
#include "devices.h"
#include "proc_comm.h"
#include "../../../drivers/video/msm/mdp_hw.h"

#define DEBUG_LCM

#ifdef DEBUG_LCM
#define LCMDBG(fmt, arg...)     printk("[DISP][lcm]%s"fmt, __func__, ##arg)
#else
#define LCMDBG(fmt, arg...)     {}
#endif

#define BRIGHTNESS_DEFAULT_LEVEL        102

enum {
	PANEL_ID_FLR_SMD_XB,
};

int qspi_send_16bit(unsigned char id, unsigned data);
int qspi_send_9bit(struct spi_msg *msg);

#define LCM_GPIO_CFG(gpio, func) \
PCOM_GPIO_CFG(gpio, func, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA)
static DEFINE_MUTEX(panel_lock);
static struct vreg *vreg_lcm_1v8;

static atomic_t lcm_init_done = ATOMIC_INIT(1);
static uint8_t last_val = BRIGHTNESS_DEFAULT_LEVEL;
static int color_enhancement = 0;

static void flyer_panel_power(bool on_off)
{
	int gpio_lcm_en, gpio_lvds_on;

	if (panel_type != PANEL_ID_FLR_SMD_XB) {
		gpio_lcm_en = FLYER_LCM_3V3_EN_XC;
		gpio_lvds_on = FLYER_LVDS_ON_XC;
	} else {
		gpio_lcm_en = FLYER_LCM_3V3_EN;
		gpio_lvds_on = FLYER_LVDS_ON;
	}

	if (!!on_off) {
		LCMDBG("%s(%d):\n", __func__, on_off);
		vreg_enable(vreg_lcm_1v8);
		gpio_set_value(gpio_lcm_en, 1);
		if(panel_type == PANEL_ID_FLR_LG_XC || panel_type == PANEL_ID_FLR_LG_WS2)
			hr_msleep(50);
		else
			hr_msleep(90);
		gpio_set_value(gpio_lvds_on, 1);
	} else {
		gpio_set_value(gpio_lcm_en, 0);
		if(panel_type == PANEL_ID_FLR_LG_XC || panel_type == PANEL_ID_FLR_LG_WS2)
			hr_msleep(50);
		else
			hr_msleep(60);
		gpio_set_value(gpio_lvds_on, 0);
		vreg_disable(vreg_lcm_1v8);
	}

}

struct mdp_reg flyer_lg_mdp_init_lut[] = {
	{0x94800, 0x020004, 0x0},
	{0x94804, 0x030104, 0x0},
	{0x94808, 0x040205, 0x0},
	{0x9480C, 0x050306, 0x0},
	{0x94810, 0x060407, 0x0},
	{0x94814, 0x070508, 0x0},
	{0x94818, 0x080609, 0x0},
	{0x9481C, 0x08070A, 0x0},
	{0x94820, 0x09080A, 0x0},
	{0x94824, 0x0A090B, 0x0},
	{0x94828, 0x0B0A0C, 0x0},
	{0x9482C, 0x0C0B0D, 0x0},
	{0x94830, 0x0D0C0E, 0x0},
	{0x94834, 0x0E0D0F, 0x0},
	{0x94838, 0x0F0E0F, 0x0},
	{0x9483C, 0x100F10, 0x0},
	{0x94840, 0x111011, 0x0},
	{0x94844, 0x111112, 0x0},
	{0x94848, 0x121213, 0x0},
	{0x9484C, 0x131314, 0x0},
	{0x94850, 0x141415, 0x0},
	{0x94854, 0x151515, 0x0},
	{0x94858, 0x161616, 0x0},
	{0x9485C, 0x171717, 0x0},
	{0x94860, 0x181818, 0x0},
	{0x94864, 0x191919, 0x0},
	{0x94868, 0x1A1A1A, 0x0},
	{0x9486C, 0x1B1B1A, 0x0},
	{0x94870, 0x1B1C1B, 0x0},
	{0x94874, 0x1C1D1C, 0x0},
	{0x94878, 0x1D1E1D, 0x0},
	{0x9487C, 0x1E1F1E, 0x0},
	{0x94880, 0x1F201F, 0x0},
	{0x94884, 0x20211F, 0x0},
	{0x94888, 0x212220, 0x0},
	{0x9488C, 0x222321, 0x0},
	{0x94890, 0x232422, 0x0},
	{0x94894, 0x242523, 0x0},
	{0x94898, 0x242624, 0x0},
	{0x9489C, 0x252725, 0x0},
	{0x948A0, 0x262825, 0x0},
	{0x948A4, 0x272926, 0x0},
	{0x948A8, 0x282A27, 0x0},
	{0x948AC, 0x292B28, 0x0},
	{0x948B0, 0x2A2C29, 0x0},
	{0x948B4, 0x2B2D2A, 0x0},
	{0x948B8, 0x2C2E2A, 0x0},
	{0x948BC, 0x2D2F2B, 0x0},
	{0x948C0, 0x2E302C, 0x0},
	{0x948C4, 0x2E312D, 0x0},
	{0x948C8, 0x2F322E, 0x0},
	{0x948CC, 0x30332F, 0x0},
	{0x948D0, 0x313430, 0x0},
	{0x948D4, 0x323530, 0x0},
	{0x948D8, 0x333631, 0x0},
	{0x948DC, 0x343732, 0x0},
	{0x948E0, 0x353833, 0x0},
	{0x948E4, 0x363934, 0x0},
	{0x948E8, 0x373A35, 0x0},
	{0x948EC, 0x373B35, 0x0},
	{0x948F0, 0x383C36, 0x0},
	{0x948F4, 0x393D37, 0x0},
	{0x948F8, 0x3A3E38, 0x0},
	{0x948FC, 0x3B3F39, 0x0},
	{0x94900, 0x3C403A, 0x0},
	{0x94904, 0x3D413B, 0x0},
	{0x94908, 0x3E423B, 0x0},
	{0x9490C, 0x3F433C, 0x0},
	{0x94910, 0x40443D, 0x0},
	{0x94914, 0x41453E, 0x0},
	{0x94918, 0x41463F, 0x0},
	{0x9491C, 0x424740, 0x0},
	{0x94920, 0x434840, 0x0},
	{0x94924, 0x444941, 0x0},
	{0x94928, 0x454A42, 0x0},
	{0x9492C, 0x464B43, 0x0},
	{0x94930, 0x474C44, 0x0},
	{0x94934, 0x484D45, 0x0},
	{0x94938, 0x494E45, 0x0},
	{0x9493C, 0x4A4F46, 0x0},
	{0x94940, 0x4A5047, 0x0},
	{0x94944, 0x4B5148, 0x0},
	{0x94948, 0x4C5249, 0x0},
	{0x9494C, 0x4D534A, 0x0},
	{0x94950, 0x4E544B, 0x0},
	{0x94954, 0x4F554B, 0x0},
	{0x94958, 0x50564C, 0x0},
	{0x9495C, 0x51574D, 0x0},
	{0x94960, 0x52584E, 0x0},
	{0x94964, 0x53594F, 0x0},
	{0x94968, 0x545A50, 0x0},
	{0x9496C, 0x545B50, 0x0},
	{0x94970, 0x555C51, 0x0},
	{0x94974, 0x565D52, 0x0},
	{0x94978, 0x575E53, 0x0},
	{0x9497C, 0x585F54, 0x0},
	{0x94980, 0x596055, 0x0},
	{0x94984, 0x5A6156, 0x0},
	{0x94988, 0x5B6256, 0x0},
	{0x9498C, 0x5C6357, 0x0},
	{0x94990, 0x5D6458, 0x0},
	{0x94994, 0x5D6559, 0x0},
	{0x94998, 0x5E665A, 0x0},
	{0x9499C, 0x5F675B, 0x0},
	{0x949A0, 0x60685B, 0x0},
	{0x949A4, 0x61695C, 0x0},
	{0x949A8, 0x626A5D, 0x0},
	{0x949AC, 0x636B5E, 0x0},
	{0x949B0, 0x646C5F, 0x0},
	{0x949B4, 0x656D60, 0x0},
	{0x949B8, 0x666E60, 0x0},
	{0x949BC, 0x676F61, 0x0},
	{0x949C0, 0x677062, 0x0},
	{0x949C4, 0x687163, 0x0},
	{0x949C8, 0x697264, 0x0},
	{0x949CC, 0x6A7365, 0x0},
	{0x949D0, 0x6B7466, 0x0},
	{0x949D4, 0x6C7566, 0x0},
	{0x949D8, 0x6D7667, 0x0},
	{0x949DC, 0x6E7768, 0x0},
	{0x949E0, 0x6F7869, 0x0},
	{0x949E4, 0x70796A, 0x0},
	{0x949E8, 0x707A6B, 0x0},
	{0x949EC, 0x717B6B, 0x0},
	{0x949F0, 0x727C6C, 0x0},
	{0x949F4, 0x737D6D, 0x0},
	{0x949F8, 0x747E6E, 0x0},
	{0x949FC, 0x757F6F, 0x0},
	{0x94A00, 0x768070, 0x0},
	{0x94A04, 0x778171, 0x0},
	{0x94A08, 0x788271, 0x0},
	{0x94A0C, 0x798372, 0x0},
	{0x94A10, 0x7A8473, 0x0},
	{0x94A14, 0x7A8574, 0x0},
	{0x94A18, 0x7B8675, 0x0},
	{0x94A1C, 0x7C8776, 0x0},
	{0x94A20, 0x7D8876, 0x0},
	{0x94A24, 0x7E8977, 0x0},
	{0x94A28, 0x7F8A78, 0x0},
	{0x94A2C, 0x808B79, 0x0},
	{0x94A30, 0x818C7A, 0x0},
	{0x94A34, 0x828D7B, 0x0},
	{0x94A38, 0x838E7C, 0x0},
	{0x94A3C, 0x838F7C, 0x0},
	{0x94A40, 0x84907D, 0x0},
	{0x94A44, 0x85917E, 0x0},
	{0x94A48, 0x86927F, 0x0},
	{0x94A4C, 0x879380, 0x0},
	{0x94A50, 0x889481, 0x0},
	{0x94A54, 0x899581, 0x0},
	{0x94A58, 0x8A9682, 0x0},
	{0x94A5C, 0x8B9783, 0x0},
	{0x94A60, 0x8C9884, 0x0},
	{0x94A64, 0x8D9985, 0x0},
	{0x94A68, 0x8D9A86, 0x0},
	{0x94A6C, 0x8E9B86, 0x0},
	{0x94A70, 0x8F9C87, 0x0},
	{0x94A74, 0x909D88, 0x0},
	{0x94A78, 0x919E89, 0x0},
	{0x94A7C, 0x929F8A, 0x0},
	{0x94A80, 0x93A08B, 0x0},
	{0x94A84, 0x94A18C, 0x0},
	{0x94A88, 0x95A28C, 0x0},
	{0x94A8C, 0x96A38D, 0x0},
	{0x94A90, 0x96A48E, 0x0},
	{0x94A94, 0x97A58F, 0x0},
	{0x94A98, 0x98A690, 0x0},
	{0x94A9C, 0x99A791, 0x0},
	{0x94AA0, 0x9AA891, 0x0},
	{0x94AA4, 0x9BA992, 0x0},
	{0x94AA8, 0x9CAA93, 0x0},
	{0x94AAC, 0x9DAB94, 0x0},
	{0x94AB0, 0x9EAC95, 0x0},
	{0x94AB4, 0x9FAD96, 0x0},
	{0x94AB8, 0xA0AE97, 0x0},
	{0x94ABC, 0xA0AF97, 0x0},
	{0x94AC0, 0xA1B098, 0x0},
	{0x94AC4, 0xA2B199, 0x0},
	{0x94AC8, 0xA3B29A, 0x0},
	{0x94ACC, 0xA4B39B, 0x0},
	{0x94AD0, 0xA5B49C, 0x0},
	{0x94AD4, 0xA6B59C, 0x0},
	{0x94AD8, 0xA7B69D, 0x0},
	{0x94ADC, 0xA8B79E, 0x0},
	{0x94AE0, 0xA9B89F, 0x0},
	{0x94AE4, 0xA9B9A0, 0x0},
	{0x94AE8, 0xAABAA1, 0x0},
	{0x94AEC, 0xABBBA2, 0x0},
	{0x94AF0, 0xACBCA2, 0x0},
	{0x94AF4, 0xADBDA3, 0x0},
	{0x94AF8, 0xAEBEA4, 0x0},
	{0x94AFC, 0xAFBFA5, 0x0},
	{0x94B00, 0xB0C0A6, 0x0},
	{0x94B04, 0xB1C1A7, 0x0},
	{0x94B08, 0xB2C2A7, 0x0},
	{0x94B0C, 0xB3C3A8, 0x0},
	{0x94B10, 0xB3C4A9, 0x0},
	{0x94B14, 0xB4C5AA, 0x0},
	{0x94B18, 0xB5C6AB, 0x0},
	{0x94B1C, 0xB6C7AC, 0x0},
	{0x94B20, 0xB7C8AC, 0x0},
	{0x94B24, 0xB8C9AD, 0x0},
	{0x94B28, 0xB9CAAE, 0x0},
	{0x94B2C, 0xBACBAF, 0x0},
	{0x94B30, 0xBBCCB0, 0x0},
	{0x94B34, 0xBCCDB1, 0x0},
	{0x94B38, 0xBCCEB2, 0x0},
	{0x94B3C, 0xBDCFB2, 0x0},
	{0x94B40, 0xBED0B3, 0x0},
	{0x94B44, 0xBFD1B4, 0x0},
	{0x94B48, 0xC0D2B5, 0x0},
	{0x94B4C, 0xC1D3B6, 0x0},
	{0x94B50, 0xC2D4B7, 0x0},
	{0x94B54, 0xC3D5B7, 0x0},
	{0x94B58, 0xC4D6B8, 0x0},
	{0x94B5C, 0xC5D7B9, 0x0},
	{0x94B60, 0xC6D8BA, 0x0},
	{0x94B64, 0xC6D9BB, 0x0},
	{0x94B68, 0xC7DABC, 0x0},
	{0x94B6C, 0xC8DBBD, 0x0},
	{0x94B70, 0xC9DCBD, 0x0},
	{0x94B74, 0xCADDBE, 0x0},
	{0x94B78, 0xCBDEBF, 0x0},
	{0x94B7C, 0xCCDFC0, 0x0},
	{0x94B80, 0xCDE0C1, 0x0},
	{0x94B84, 0xCEE1C2, 0x0},
	{0x94B88, 0xCFE2C2, 0x0},
	{0x94B8C, 0xCFE3C3, 0x0},
	{0x94B90, 0xD0E4C4, 0x0},
	{0x94B94, 0xD1E5C5, 0x0},
	{0x94B98, 0xD2E6C6, 0x0},
	{0x94B9C, 0xD3E7C7, 0x0},
	{0x94BA0, 0xD4E8C8, 0x0},
	{0x94BA4, 0xD5E9C8, 0x0},
	{0x94BA8, 0xD6EAC9, 0x0},
	{0x94BAC, 0xD7EBCA, 0x0},
	{0x94BB0, 0xD8ECCB, 0x0},
	{0x94BB4, 0xD9EDCC, 0x0},
	{0x94BB8, 0xD9EECD, 0x0},
	{0x94BBC, 0xDAEFCD, 0x0},
	{0x94BC0, 0xDBF0CE, 0x0},
	{0x94BC4, 0xDCF1CF, 0x0},
	{0x94BC8, 0xDDF2D0, 0x0},
	{0x94BCC, 0xDEF3D1, 0x0},
	{0x94BD0, 0xDFF4D2, 0x0},
	{0x94BD4, 0xE0F5D2, 0x0},
	{0x94BD8, 0xE1F6D3, 0x0},
	{0x94BDC, 0xE2F7D4, 0x0},
	{0x94BE0, 0xE2F8D5, 0x0},
	{0x94BE4, 0xE3F9D6, 0x0},
	{0x94BE8, 0xE4FAD7, 0x0},
	{0x94BEC, 0xE5FBD8, 0x0},
	{0x94BF0, 0xE6FCD8, 0x0},
	{0x94BF4, 0xE7FDD9, 0x0},
	{0x94BF8, 0xE8FEDA, 0x0},
	{0x94BFC, 0xE9FFDB, 0x0},
	{0x90070, 0x000007, 0x7},
};

struct mdp_reg flyer_lg_ws2_mdp_init_lut[] = {
	{0x94800, 0x000000, 0x0},
	{0x94804, 0x000101, 0x0},
	{0x94808, 0x010202, 0x0},
	{0x9480C, 0x020303, 0x0},
	{0x94810, 0x030404, 0x0},
	{0x94814, 0x040505, 0x0},
	{0x94818, 0x050606, 0x0},
	{0x9481C, 0x060707, 0x0},
	{0x94820, 0x070808, 0x0},
	{0x94824, 0x080909, 0x0},
	{0x94828, 0x090A0A, 0x0},
	{0x9482C, 0x0A0B0B, 0x0},
	{0x94830, 0x0B0C0C, 0x0},
	{0x94834, 0x0C0D0D, 0x0},
	{0x94838, 0x0D0E0E, 0x0},
	{0x9483C, 0x0E0F0F, 0x0},
	{0x94840, 0x0F1010, 0x0},
	{0x94844, 0x101111, 0x0},
	{0x94848, 0x101212, 0x0},
	{0x9484C, 0x111313, 0x0},
	{0x94850, 0x121414, 0x0},
	{0x94854, 0x131515, 0x0},
	{0x94858, 0x141616, 0x0},
	{0x9485C, 0x151717, 0x0},
	{0x94860, 0x161818, 0x0},
	{0x94864, 0x171919, 0x0},
	{0x94868, 0x181A1A, 0x0},
	{0x9486C, 0x191B1B, 0x0},
	{0x94870, 0x1A1C1C, 0x0},
	{0x94874, 0x1B1D1D, 0x0},
	{0x94878, 0x1C1E1E, 0x0},
	{0x9487C, 0x1D1F1F, 0x0},
	{0x94880, 0x1E2020, 0x0},
	{0x94884, 0x1F2121, 0x0},
	{0x94888, 0x202222, 0x0},
	{0x9488C, 0x212323, 0x0},
	{0x94890, 0x212424, 0x0},
	{0x94894, 0x222525, 0x0},
	{0x94898, 0x232626, 0x0},
	{0x9489C, 0x242727, 0x0},
	{0x948A0, 0x252828, 0x0},
	{0x948A4, 0x262929, 0x0},
	{0x948A8, 0x272A2A, 0x0},
	{0x948AC, 0x282B2B, 0x0},
	{0x948B0, 0x292C2C, 0x0},
	{0x948B4, 0x2A2D2D, 0x0},
	{0x948B8, 0x2B2E2E, 0x0},
	{0x948BC, 0x2C2F2F, 0x0},
	{0x948C0, 0x2D3030, 0x0},
	{0x948C4, 0x2E3131, 0x0},
	{0x948C8, 0x2F3232, 0x0},
	{0x948CC, 0x303333, 0x0},
	{0x948D0, 0x313434, 0x0},
	{0x948D4, 0x323535, 0x0},
	{0x948D8, 0x323636, 0x0},
	{0x948DC, 0x333737, 0x0},
	{0x948E0, 0x343838, 0x0},
	{0x948E4, 0x353939, 0x0},
	{0x948E8, 0x363A3A, 0x0},
	{0x948EC, 0x373B3B, 0x0},
	{0x948F0, 0x383C3C, 0x0},
	{0x948F4, 0x393D3D, 0x0},
	{0x948F8, 0x3A3E3E, 0x0},
	{0x948FC, 0x3B3F3F, 0x0},
	{0x94900, 0x3C4040, 0x0},
	{0x94904, 0x3D4141, 0x0},
	{0x94908, 0x3E4242, 0x0},
	{0x9490C, 0x3F4343, 0x0},
	{0x94910, 0x404444, 0x0},
	{0x94914, 0x414545, 0x0},
	{0x94918, 0x424646, 0x0},
	{0x9491C, 0x434747, 0x0},
	{0x94920, 0x434848, 0x0},
	{0x94924, 0x444949, 0x0},
	{0x94928, 0x454A4A, 0x0},
	{0x9492C, 0x464B4B, 0x0},
	{0x94930, 0x474C4C, 0x0},
	{0x94934, 0x484D4D, 0x0},
	{0x94938, 0x494E4E, 0x0},
	{0x9493C, 0x4A4F4F, 0x0},
	{0x94940, 0x4B5050, 0x0},
	{0x94944, 0x4C5151, 0x0},
	{0x94948, 0x4D5252, 0x0},
	{0x9494C, 0x4E5353, 0x0},
	{0x94950, 0x4F5454, 0x0},
	{0x94954, 0x505555, 0x0},
	{0x94958, 0x515656, 0x0},
	{0x9495C, 0x525757, 0x0},
	{0x94960, 0x535858, 0x0},
	{0x94964, 0x545959, 0x0},
	{0x94968, 0x555A5A, 0x0},
	{0x9496C, 0x555B5B, 0x0},
	{0x94970, 0x565C5C, 0x0},
	{0x94974, 0x575D5D, 0x0},
	{0x94978, 0x585E5E, 0x0},
	{0x9497C, 0x595F5F, 0x0},
	{0x94980, 0x5A6060, 0x0},
	{0x94984, 0x5B6161, 0x0},
	{0x94988, 0x5C6262, 0x0},
	{0x9498C, 0x5D6363, 0x0},
	{0x94990, 0x5E6464, 0x0},
	{0x94994, 0x5F6565, 0x0},
	{0x94998, 0x606666, 0x0},
	{0x9499C, 0x616767, 0x0},
	{0x949A0, 0x626868, 0x0},
	{0x949A4, 0x636969, 0x0},
	{0x949A8, 0x646A6A, 0x0},
	{0x949AC, 0x656B6B, 0x0},
	{0x949B0, 0x666C6C, 0x0},
	{0x949B4, 0x666D6D, 0x0},
	{0x949B8, 0x676E6E, 0x0},
	{0x949BC, 0x686F6F, 0x0},
	{0x949C0, 0x697070, 0x0},
	{0x949C4, 0x6A7171, 0x0},
	{0x949C8, 0x6B7272, 0x0},
	{0x949CC, 0x6C7373, 0x0},
	{0x949D0, 0x6D7474, 0x0},
	{0x949D4, 0x6E7575, 0x0},
	{0x949D8, 0x6F7676, 0x0},
	{0x949DC, 0x707777, 0x0},
	{0x949E0, 0x717878, 0x0},
	{0x949E4, 0x727979, 0x0},
	{0x949E8, 0x737A7A, 0x0},
	{0x949EC, 0x747B7B, 0x0},
	{0x949F0, 0x757C7C, 0x0},
	{0x949F4, 0x767D7D, 0x0},
	{0x949F8, 0x777E7E, 0x0},
	{0x949FC, 0x777F7F, 0x0},
	{0x94A00, 0x788080, 0x0},
	{0x94A04, 0x798181, 0x0},
	{0x94A08, 0x7A8282, 0x0},
	{0x94A0C, 0x7B8383, 0x0},
	{0x94A10, 0x7C8484, 0x0},
	{0x94A14, 0x7D8585, 0x0},
	{0x94A18, 0x7E8686, 0x0},
	{0x94A1C, 0x7F8787, 0x0},
	{0x94A20, 0x808888, 0x0},
	{0x94A24, 0x818989, 0x0},
	{0x94A28, 0x828A8A, 0x0},
	{0x94A2C, 0x838B8B, 0x0},
	{0x94A30, 0x848C8C, 0x0},
	{0x94A34, 0x858D8D, 0x0},
	{0x94A38, 0x868E8E, 0x0},
	{0x94A3C, 0x878F8F, 0x0},
	{0x94A40, 0x889090, 0x0},
	{0x94A44, 0x889191, 0x0},
	{0x94A48, 0x899292, 0x0},
	{0x94A4C, 0x8A9393, 0x0},
	{0x94A50, 0x8B9494, 0x0},
	{0x94A54, 0x8C9595, 0x0},
	{0x94A58, 0x8D9696, 0x0},
	{0x94A5C, 0x8E9797, 0x0},
	{0x94A60, 0x8F9898, 0x0},
	{0x94A64, 0x909999, 0x0},
	{0x94A68, 0x919A9A, 0x0},
	{0x94A6C, 0x929B9B, 0x0},
	{0x94A70, 0x939C9C, 0x0},
	{0x94A74, 0x949D9D, 0x0},
	{0x94A78, 0x959E9E, 0x0},
	{0x94A7C, 0x969F9F, 0x0},
	{0x94A80, 0x97A0A0, 0x0},
	{0x94A84, 0x98A1A1, 0x0},
	{0x94A88, 0x99A2A2, 0x0},
	{0x94A8C, 0x99A3A3, 0x0},
	{0x94A90, 0x9AA4A4, 0x0},
	{0x94A94, 0x9BA5A5, 0x0},
	{0x94A98, 0x9CA6A6, 0x0},
	{0x94A9C, 0x9DA7A7, 0x0},
	{0x94AA0, 0x9EA8A8, 0x0},
	{0x94AA4, 0x9FA9A9, 0x0},
	{0x94AA8, 0xA0AAAA, 0x0},
	{0x94AAC, 0xA1ABAB, 0x0},
	{0x94AB0, 0xA2ACAC, 0x0},
	{0x94AB4, 0xA3ADAD, 0x0},
	{0x94AB8, 0xA4AEAE, 0x0},
	{0x94ABC, 0xA5AFAF, 0x0},
	{0x94AC0, 0xA6B0B0, 0x0},
	{0x94AC4, 0xA7B1B1, 0x0},
	{0x94AC8, 0xA8B2B2, 0x0},
	{0x94ACC, 0xA9B3B3, 0x0},
	{0x94AD0, 0xAAB4B4, 0x0},
	{0x94AD4, 0xAAB5B5, 0x0},
	{0x94AD8, 0xABB6B6, 0x0},
	{0x94ADC, 0xACB7B7, 0x0},
	{0x94AE0, 0xADB8B8, 0x0},
	{0x94AE4, 0xAEB9B9, 0x0},
	{0x94AE8, 0xAFBABA, 0x0},
	{0x94AEC, 0xB0BBBB, 0x0},
	{0x94AF0, 0xB1BCBC, 0x0},
	{0x94AF4, 0xB2BDBD, 0x0},
	{0x94AF8, 0xB3BEBE, 0x0},
	{0x94AFC, 0xB4BFBF, 0x0},
	{0x94B00, 0xB5C0C0, 0x0},
	{0x94B04, 0xB6C1C1, 0x0},
	{0x94B08, 0xB7C2C2, 0x0},
	{0x94B0C, 0xB8C3C3, 0x0},
	{0x94B10, 0xB9C4C4, 0x0},
	{0x94B14, 0xBAC5C5, 0x0},
	{0x94B18, 0xBBC6C6, 0x0},
	{0x94B1C, 0xBBC7C7, 0x0},
	{0x94B20, 0xBCC8C8, 0x0},
	{0x94B24, 0xBDC9C9, 0x0},
	{0x94B28, 0xBECACA, 0x0},
	{0x94B2C, 0xBFCBCB, 0x0},
	{0x94B30, 0xC0CCCC, 0x0},
	{0x94B34, 0xC1CDCD, 0x0},
	{0x94B38, 0xC2CECE, 0x0},
	{0x94B3C, 0xC3CFCF, 0x0},
	{0x94B40, 0xC4D0D0, 0x0},
	{0x94B44, 0xC5D1D1, 0x0},
	{0x94B48, 0xC6D2D2, 0x0},
	{0x94B4C, 0xC7D3D3, 0x0},
	{0x94B50, 0xC8D4D4, 0x0},
	{0x94B54, 0xC9D5D5, 0x0},
	{0x94B58, 0xCAD6D6, 0x0},
	{0x94B5C, 0xCBD7D7, 0x0},
	{0x94B60, 0xCCD8D8, 0x0},
	{0x94B64, 0xCCD9D9, 0x0},
	{0x94B68, 0xCDDADA, 0x0},
	{0x94B6C, 0xCEDBDB, 0x0},
	{0x94B70, 0xCFDCDC, 0x0},
	{0x94B74, 0xD0DDDD, 0x0},
	{0x94B78, 0xD1DEDE, 0x0},
	{0x94B7C, 0xD2DFDF, 0x0},
	{0x94B80, 0xD3E0E0, 0x0},
	{0x94B84, 0xD4E1E1, 0x0},
	{0x94B88, 0xD5E2E2, 0x0},
	{0x94B8C, 0xD6E3E3, 0x0},
	{0x94B90, 0xD7E4E4, 0x0},
	{0x94B94, 0xD8E5E5, 0x0},
	{0x94B98, 0xD9E6E6, 0x0},
	{0x94B9C, 0xDAE7E7, 0x0},
	{0x94BA0, 0xDBE8E8, 0x0},
	{0x94BA4, 0xDCE9E9, 0x0},
	{0x94BA8, 0xDDEAEA, 0x0},
	{0x94BAC, 0xDDEBEB, 0x0},
	{0x94BB0, 0xDEECEC, 0x0},
	{0x94BB4, 0xDFEDED, 0x0},
	{0x94BB8, 0xE0EEEE, 0x0},
	{0x94BBC, 0xE1EFEF, 0x0},
	{0x94BC0, 0xE2F0F0, 0x0},
	{0x94BC4, 0xE3F1F1, 0x0},
	{0x94BC8, 0xE4F2F2, 0x0},
	{0x94BCC, 0xE5F3F3, 0x0},
	{0x94BD0, 0xE6F4F4, 0x0},
	{0x94BD4, 0xE7F5F5, 0x0},
	{0x94BD8, 0xE8F6F6, 0x0},
	{0x94BDC, 0xE9F7F7, 0x0},
	{0x94BE0, 0xEAF8F8, 0x0},
	{0x94BE4, 0xEBF9F9, 0x0},
	{0x94BE8, 0xECFAFA, 0x0},
	{0x94BEC, 0xEDFBFB, 0x0},
	{0x94BF0, 0xEEFCFC, 0x0},
	{0x94BF4, 0xEFFDFD, 0x0},
	{0x94BF8, 0xEFFEFE, 0x0},
	{0x94BFC, 0xF1FFFF, 0x0},

	{0x90070, 0x000007, 0x7},
};

struct mdp_reg flyer_mdp_init_color[] = {
	{0x90070, 0x0008, 0x8},
	{0x93400, 0x024A, 0x0},
	{0x93404, 0xFFC3, 0x0},
	{0x93408, 0xFFF6, 0x0},
	{0x9340C, 0xFFE0, 0x0},
	{0x93410, 0x0229, 0x0},
	{0x93414, 0xFFF2, 0x0},
	{0x93418, 0xFFDF, 0x0},
	{0x9341C, 0xFFC5, 0x0},
	{0x93420, 0x025F, 0x0},

	{0x93600, 0x0000, 0x0},
	{0x93604, 0x00FF, 0x0},
	{0x93608, 0x0000, 0x0},
	{0x9360C, 0x00FF, 0x0},
	{0x93610, 0x0000, 0x0},
	{0x93614, 0x00FF, 0x0},
	{0x93680, 0x0000, 0x0},
	{0x93684, 0x00FF, 0x0},
	{0x93688, 0x0000, 0x0},
	{0x9368C, 0x00FF, 0x0},
	{0x93690, 0x0000, 0x0},
	{0x93694, 0x00FF, 0x0},
};

void flyer_mdp_color_enhancement(struct mdp_device *mdp_dev)
{
	struct mdp_info *mdp = container_of(mdp_dev, struct mdp_info, mdp_dev);
	if (panel_type == PANEL_ID_FLR_LG_XC) {
		mdp->write_regs(mdp, flyer_lg_mdp_init_lut, ARRAY_SIZE(flyer_lg_mdp_init_lut));
	} else if (panel_type == PANEL_ID_FLR_LG_WS2) {
		mdp->write_regs(mdp, flyer_lg_ws2_mdp_init_lut, ARRAY_SIZE(flyer_lg_ws2_mdp_init_lut));
		mdp->write_regs(mdp, flyer_mdp_init_color, ARRAY_SIZE(flyer_mdp_init_color));
	}
}

static struct msm_mdp_platform_data mdp_pdata = {
	.color_format	= MSM_MDP_OUT_IF_FMT_RGB888,
};

struct lcm_cmd {
        uint8_t		cmd;
        uint8_t		data;
};

static struct gamma_curvy lg_ws2_gamma_tbl = {
	.gamma_len = 33,
	.bl_len = 8,
	.ref_y_gamma = {0, 1, 2, 4, 8, 14, 22, 32, 45, 61, 77, 96,
			119, 144, 170, 196, 227, 262, 300, 340, 381,
			429, 475, 523, 572, 628, 682, 738, 797, 858,
			920, 976, 1024},
	.ref_y_shade = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352,
			384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704,
			736, 768, 800, 832, 864, 896, 928, 960, 992, 1024},
	.ref_bl_lvl = {0, 128, 256, 384, 576, 704, 864, 1024},
	.ref_y_lvl = {0, 138, 218, 298, 424, 601, 818, 1024},
};

static struct gamma_curvy lg_gamma_tbl = {
	.gamma_len = 33,
	.bl_len = 8,
	.ref_y_gamma = {0, 1, 2, 5, 9, 16, 24, 35, 49, 66, 83, 104,
			127, 153, 181, 208, 238, 271, 308, 341, 377,
			416, 456, 485, 539, 592, 644, 677, 756, 826,
			868, 958, 1024},
	.ref_y_shade = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352,
			384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704,
			736, 768, 800, 832, 864, 896, 928, 960, 992, 1024},
	.ref_bl_lvl = {0, 128, 256, 384, 576, 704, 864, 1024},
	.ref_y_lvl = {0, 146, 228, 306, 431, 605, 817, 1024},
};

static struct gamma_curvy gamma_tbl = {
	.gamma_len = 33,
	.bl_len = 8,
	.ref_y_gamma = {0, 1, 2, 5, 9, 16, 24, 35, 49, 66, 83, 104,
			127, 153, 181, 208, 238, 271, 308, 341, 377,
			416, 456, 485, 539, 592, 644, 677, 756, 826,
			868, 958, 1024},
	.ref_y_shade = {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352,
			384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704,
			736, 768, 800, 832, 864, 896, 928, 960, 992, 1024},
	.ref_bl_lvl = {0, 128, 256, 384, 576, 704, 864, 1024},
	.ref_y_lvl = {0, 128, 256, 384, 576, 704, 864, 1024},
};

#define LCM_MDELAY	0x03

#define PWM_USER_DEF			143
#define PWM_USER_MIN			30
#define PWM_USER_DIM			20
#define PWM_USER_MAX			255

#define PWM_SAM_DEF			82
#define PWM_SAM_MIN			25
#define PWM_SAM_MAX			255

#define PWM_LG_DEF			80
#define PWM_LG_MIN			27
#define PWM_LG_MAX			200

#define PWM_LGWS2_DEF			80
#define PWM_LGWS2_MIN			27
#define PWM_LGWS2_MAX			200


static uint32_t display_on_gpio_table[] = {
	LCM_GPIO_CFG(FLYER_LCD_PCLK, 1),
	LCM_GPIO_CFG(FLYER_LCD_DE, 1),
	LCM_GPIO_CFG(FLYER_LCD_VSYNC, 1),
	LCM_GPIO_CFG(FLYER_LCD_HSYNC, 1),
	LCM_GPIO_CFG(FLYER_LCD_G0, 1),
	LCM_GPIO_CFG(FLYER_LCD_G1, 1),
	LCM_GPIO_CFG(FLYER_LCD_G2, 1),
	LCM_GPIO_CFG(FLYER_LCD_G3, 1),
	LCM_GPIO_CFG(FLYER_LCD_G4, 1),
	LCM_GPIO_CFG(FLYER_LCD_G5, 1),
	LCM_GPIO_CFG(FLYER_LCD_G6, 1),
	LCM_GPIO_CFG(FLYER_LCD_G7, 1),
	LCM_GPIO_CFG(FLYER_LCD_B0, 1),
	LCM_GPIO_CFG(FLYER_LCD_B1, 1),
	LCM_GPIO_CFG(FLYER_LCD_B2, 1),
	LCM_GPIO_CFG(FLYER_LCD_B3, 1),
	LCM_GPIO_CFG(FLYER_LCD_B4, 1),
	LCM_GPIO_CFG(FLYER_LCD_B5, 1),
	LCM_GPIO_CFG(FLYER_LCD_B6, 1),
	LCM_GPIO_CFG(FLYER_LCD_B7, 1),
	LCM_GPIO_CFG(FLYER_LCD_R0, 1),
	LCM_GPIO_CFG(FLYER_LCD_R1, 1),
	LCM_GPIO_CFG(FLYER_LCD_R2, 1),
	LCM_GPIO_CFG(FLYER_LCD_R3, 1),
	LCM_GPIO_CFG(FLYER_LCD_R4, 1),
	LCM_GPIO_CFG(FLYER_LCD_R5, 1),
	LCM_GPIO_CFG(FLYER_LCD_R6, 1),
	LCM_GPIO_CFG(FLYER_LCD_R7, 1),
};

static uint32_t display_off_gpio_table[] = {
	LCM_GPIO_CFG(FLYER_LCD_PCLK, 0),
	LCM_GPIO_CFG(FLYER_LCD_DE, 0),
	LCM_GPIO_CFG(FLYER_LCD_VSYNC, 0),
	LCM_GPIO_CFG(FLYER_LCD_HSYNC, 0),
	LCM_GPIO_CFG(FLYER_LCD_G0, 0),
	LCM_GPIO_CFG(FLYER_LCD_G1, 0),
	LCM_GPIO_CFG(FLYER_LCD_G2, 0),
	LCM_GPIO_CFG(FLYER_LCD_G3, 0),
	LCM_GPIO_CFG(FLYER_LCD_G4, 0),
	LCM_GPIO_CFG(FLYER_LCD_G5, 0),
	LCM_GPIO_CFG(FLYER_LCD_G6, 0),
	LCM_GPIO_CFG(FLYER_LCD_G7, 0),
	LCM_GPIO_CFG(FLYER_LCD_B0, 0),
	LCM_GPIO_CFG(FLYER_LCD_B1, 0),
	LCM_GPIO_CFG(FLYER_LCD_B2, 0),
	LCM_GPIO_CFG(FLYER_LCD_B3, 0),
	LCM_GPIO_CFG(FLYER_LCD_B4, 0),
	LCM_GPIO_CFG(FLYER_LCD_B5, 0),
	LCM_GPIO_CFG(FLYER_LCD_B6, 0),
	LCM_GPIO_CFG(FLYER_LCD_B7, 0),
	LCM_GPIO_CFG(FLYER_LCD_R0, 0),
	LCM_GPIO_CFG(FLYER_LCD_R1, 0),
	LCM_GPIO_CFG(FLYER_LCD_R2, 0),
	LCM_GPIO_CFG(FLYER_LCD_R3, 0),
	LCM_GPIO_CFG(FLYER_LCD_R4, 0),
	LCM_GPIO_CFG(FLYER_LCD_R5, 0),
	LCM_GPIO_CFG(FLYER_LCD_R6, 0),
	LCM_GPIO_CFG(FLYER_LCD_R7, 0),
};

static int panel_gpio_switch(int on)
{
	config_gpio_table(
		!!on ? display_on_gpio_table : display_off_gpio_table,
		!!on ? ARRAY_SIZE(display_on_gpio_table) : ARRAY_SIZE(display_off_gpio_table));

	return 0;
}

static int flyer_shrink_pwm(int brightness, int user_def,
                int user_min, int user_max, int panel_def,
                int panel_min, int panel_max)
{
        if (brightness < PWM_USER_DIM) {
                return 0;
        }

        if (brightness < user_min) {
                return panel_min;
        }

        if (brightness > user_def) {
                brightness = (panel_max - panel_def) *
                        (brightness - user_def) /
                        (user_max - user_def) +
                        panel_def;
        } else {
                        brightness = (panel_def - panel_min) *
                        (brightness - user_min) /
                        (user_def - user_min) +
                        panel_min;
        }

        return brightness;
}

/*----------------------------------------------------------------------------*/
static int flyer_adjust_backlight(enum led_brightness val)
{
	uint8_t shrink_br = 0;
        uint8_t data[4] = {     /* PWM setting of microp, see p.8 */
                0x05,           /* Fading time; suggested: 5/10/15/20/25 */
                val,            /* Duty Cycle */
                0x00,           /* Channel H byte */
                0x20,           /* Channel L byte */
                };

	if(val == 0)
		data[0] = 0;

        mutex_lock(&panel_lock);

	if(panel_type == PANEL_ID_FLR_LG_XC)
		shrink_br = flyer_shrink_pwm(val, PWM_USER_DEF,
                                PWM_USER_MIN, PWM_USER_MAX, PWM_LG_DEF,
                                PWM_LG_MIN, PWM_LG_MAX);
	else if(panel_type == PANEL_ID_FLR_LG_WS2)
		shrink_br = flyer_shrink_pwm(val, PWM_USER_DEF,
                                PWM_USER_MIN, PWM_USER_MAX, PWM_LGWS2_DEF,
                                PWM_LGWS2_MIN, PWM_LGWS2_MAX);
	else
		shrink_br = flyer_shrink_pwm(val, PWM_USER_DEF,
                                PWM_USER_MIN, PWM_USER_MAX, PWM_SAM_DEF,
                                PWM_SAM_MIN, PWM_SAM_MAX);

        data[1] = shrink_br;

		PR_DISP_DEBUG("[lcm](%d), shrink_br=%d\n", val, shrink_br);
        microp_i2c_write(0x25, data, sizeof(data));
        last_val = shrink_br ? shrink_br: last_val;
        mutex_unlock(&panel_lock);

#if 0
        return shrink_br;
#else
	return val;
#endif
}

static int flyer_sharp_panel_init(struct msm_lcdc_panel_ops *ops)
{
        LCMDBG("\n");

        mutex_lock(&panel_lock);
        flyer_panel_power(1);
		panel_gpio_switch(1);
        mutex_unlock(&panel_lock);

        return 0;
}

static int flyer_sharp_panel_uninit(struct msm_lcdc_panel_ops *ops)
{
        LCMDBG("\n");

        mutex_lock(&panel_lock);
        flyer_panel_power(0);
		panel_gpio_switch(0);
        mutex_unlock(&panel_lock);

        return 0;
}

static int flyer_sharp_panel_unblank(struct msm_lcdc_panel_ops *ops)
{
	if (color_enhancement == 0) {
		flyer_mdp_color_enhancement(mdp_pdata.mdp_dev);
		color_enhancement = 1;
	}
	hr_msleep(250);
	atomic_set(&lcm_init_done, 1);
	flyer_adjust_backlight(last_val);
        return 0;
}

static int flyer_sharp_panel_blank(struct msm_lcdc_panel_ops *ops)
{
	flyer_adjust_backlight(0);
	atomic_set(&lcm_init_done, 0);
	hr_msleep(250);
        return 0;
}

static int flyer_sharp_panel_shutdown(struct msm_lcdc_panel_ops *ops)
{
	flyer_adjust_backlight(0);
	hr_msleep(250);
	mutex_lock(&panel_lock);
	flyer_panel_power(0);
	atomic_set(&lcm_init_done, 0);
	mutex_unlock(&panel_lock);
        return 0;
}

static struct msm_lcdc_panel_ops flyer_sharp_panel_ops = {
	.init		= flyer_sharp_panel_init,
	.uninit		= flyer_sharp_panel_uninit,
	.blank		= flyer_sharp_panel_blank,
	.unblank	= flyer_sharp_panel_unblank,
	.shutdown	= flyer_sharp_panel_shutdown,
};

static struct msm_lcdc_timing flyer_smd_timing = {
		.clk_rate				= 40960000,
        .hsync_pulse_width      = 30,
        .hsync_back_porch       = 60,
        .hsync_front_porch      = 36,
        .hsync_skew             = 0,
        .vsync_pulse_width      = 10,
        .vsync_back_porch       = 11,
        .vsync_front_porch      = 10,
        .vsync_act_low          = 0,
        .hsync_act_low          = 0,
        .den_act_low            = 0,
};


static struct msm_lcdc_timing flyer_lg_timing = {
#ifdef CONFIG_TIMING_PCLK_53M
		.clk_rate				= 53512000,
#else
		.clk_rate				= 48083000,
#endif
        .hsync_pulse_width      = 30,
        .hsync_back_porch       = 160,
        .hsync_front_porch      = 160,
        .hsync_skew             = 0,
        .vsync_pulse_width      = 10,
        .vsync_back_porch       = 23,
        .vsync_front_porch      = 12,
        .vsync_act_low          = 0,
        .hsync_act_low          = 0,
        .den_act_low            = 0,
};

/*----------------------------------------------------------------------------*/

static struct resource resources_msm_fb[] = {
	{
		.start = MSM_FB_BASE,
		.end = MSM_FB_BASE + MSM_FB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct msm_fb_data flyer_lcdc_fb_data = {
	.xres		= 1024,
	.yres		= 600,
	.width		= 153,
	.height		= 90,
	.output_format	= 0,
};

static struct msm_lcdc_platform_data flyer_lcdc_platform_data = {
	.fb_id		= 0,
	.fb_data	= &flyer_lcdc_fb_data,
	.fb_resource	= &resources_msm_fb[0],
};

static struct platform_device flyer_lcdc_device = {
	.name	= "msm_mdp_lcdc",
	.id	= -1,
	.dev	= {
		.platform_data = &flyer_lcdc_platform_data,
	},
};
/*----------------------------------------------------------------------------*/

static void flyer_brightness_set(struct led_classdev *led_cdev,
		enum led_brightness val)
{
#if 0
	uint8_t data[4] = {	/* PWM setting of microp, see p.8 */
		0x05,		/* Fading time; suggested: 5/10/15/20/25 */
		128,		/* Duty Cycle */
		0x00,		/* Channel H byte */
		0x20,		/* Channel L byte */
		};

	if (atomic_read(&lcm_init_done) == 0) {
		last_val = val ? val : last_val;
		LCMDBG(":lcm not ready, val=%d\n", val);
		return;
	}
	mutex_lock(&panel_lock);
	LCMDBG("brightness=%d\n", val);
	microp_i2c_write(0x25, data, sizeof(data));
	last_val = val ? val : last_val;
	mutex_unlock(&panel_lock);
#else
	if (atomic_read(&lcm_init_done) == 0) {
		last_val = val ? val : last_val;
		LCMDBG(":lcm not ready, val=%d\n", val);
		return;
	}
	led_cdev->brightness = flyer_adjust_backlight(val);
#endif
}

static struct led_classdev flyer_backlight_led = {
	.name = "lcd-backlight",
	.brightness = LED_FULL,
	.brightness_set = flyer_brightness_set,
};

static int flyer_backlight_probe(struct platform_device *pdev)
{
	int rc;

	rc = led_classdev_register(&pdev->dev, &flyer_backlight_led);
	if (rc)
		LCMDBG("backlight: failure on register led_classdev\n");
	return 0;
}

static struct platform_device flyer_backlight_pdev = {
	.name = "flyer-backlight",
};

static struct platform_driver flyer_backlight_pdrv = {
	.probe          = flyer_backlight_probe,
	.driver         = {
		.name   = "flyer-backlight",
		.owner  = THIS_MODULE,
	},
};

static int __init flyer_backlight_init(void)
{
	return platform_driver_register(&flyer_backlight_pdrv);
}

/*----------------------------------------------------------------------------*/

int __init flyer_init_panel(void)
{
	int ret;
//	unsigned id;

        vreg_lcm_1v8 = vreg_get(0, "wlan2");
        if (IS_ERR(vreg_lcm_1v8))
                return PTR_ERR(vreg_lcm_1v8);
#if 0
	if (panel_type != PANEL_ID_FLR_SMD_XB) {
	        id = PCOM_GPIO_CFG(FLYER_LCM_3V3_EN_XC, 1, GPIO_OUTPUT,
		 GPIO_NO_PULL, GPIO_4MA);
	        msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = PCOM_GPIO_CFG(FLYER_LVDS_ON_XC, 1, GPIO_OUTPUT,
			 GPIO_NO_PULL, GPIO_4MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	} else {
	        id = PCOM_GPIO_CFG(FLYER_LCM_3V3_EN, 1, GPIO_OUTPUT,
			 GPIO_NO_PULL, GPIO_4MA);
	        msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = PCOM_GPIO_CFG(FLYER_LVDS_ON, 1, GPIO_OUTPUT,
			 GPIO_NO_PULL, GPIO_4MA);
	        msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	}
#endif
	mdp_pdata.overrides = MSM_MDP_ABL_ENABLE;

	if (panel_type == PANEL_ID_FLR_LG_WS2)
		mdp_pdata.abl_gamma_tbl = &lg_ws2_gamma_tbl;
	else if (panel_type == PANEL_ID_FLR_LG_XC)
		mdp_pdata.abl_gamma_tbl = &lg_gamma_tbl;
	else
		mdp_pdata.abl_gamma_tbl = &gamma_tbl;

	msm_device_mdp.dev.platform_data = &mdp_pdata;
	ret = platform_device_register(&msm_device_mdp);
	if (ret != 0)
		return ret;

	if(panel_type == PANEL_ID_FLR_LG_XC || PANEL_ID_FLR_LG_WS2)
		flyer_lcdc_platform_data.timing = &flyer_lg_timing;
	else
		flyer_lcdc_platform_data.timing = &flyer_smd_timing;
	flyer_lcdc_platform_data.panel_ops = &flyer_sharp_panel_ops;

	ret = platform_device_register(&flyer_lcdc_device);
	if (ret != 0)
		return ret;

	ret = platform_device_register(&flyer_backlight_pdev);
        if (ret)
                return ret;

	return 0;
}

module_init(flyer_backlight_init);
