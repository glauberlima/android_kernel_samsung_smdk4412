/* linux/drivers/video/samsung/boot_progressbar.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung Display Controller (FIMD) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <plat/clock.h>
#include <plat/media.h>
#include <mach/media.h>
#include <mach/map.h>
#include "s3cfb.h"

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

#define TRUE    1
#define FALSE   0
#define DISPLAY_BOOT_PROGRESS /* progress bar while kernel-loading */
#define LOGO_MEM_SIZE		        (800*480*4)
#define LPDDR1_BASE_ADDR	      0x50000000
/* LPDDR1_BASE_ADDR = Bootloader */
#define LOGO_MEM_BASE	(LPDDR1_BASE_ADDR + 0x0EC00000)


static int progress_flag = FALSE;
static int progress_pos;
static struct timer_list progress_timer;

#define PROGRESS_BAR_WIDTH	    4
#define PROGRESS_BAR_HEIGHT	    8
#define PROGRESS_BAR_LEFT_POS	    54
#define PROGRESS_BAR_RIGHT_POS	   (425-PROGRESS_BAR_WIDTH)
#define PROGRESS_BAR_START_Y	    576

static unsigned char anycall_progress_bar_left[] = {
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00
};

static unsigned char anycall_progress_bar_right[] = {
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00
};

static unsigned char anycall_progress_bar_center[] = {
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0xf3, 0xc5, 0x00, 0x00, 0xf3, 0xc5, 0x00, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00
};

static unsigned char anycall_progress_bar[] = {
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00
};

static void progress_timer_handler(unsigned long data);
static int show_progress = 1;
module_param_named(progress, show_progress, bool, 0);

static void s3cfb_update_framebuffer( \
	struct fb_info *fb, int x, int y, void *buffer, \
	int src_width, int src_height)
{
	struct s3cfb_global *fbdev =
			platform_get_drvdata(to_platform_device(fb->device));
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct fb_var_screeninfo *var = &fb->var;
	int row;
	int bytes_per_pixel = (var->bits_per_pixel / 8);

	unsigned char *pSrc = buffer;
	unsigned char *pDst = fb->screen_base;

	if (x+src_width > var->xres || y+src_height > var->yres) {
		dev_err(fbdev->dev, "invalid destination coordinate or" \
			" source size (%d, %d) (%d %d)\n", \
			x, y, src_width, src_height);
		return;
	}

	pDst += y * fix->line_length + x * bytes_per_pixel;

	for (row = 0; row < src_height ; row++) {
		memcpy(pDst, pSrc, src_width * bytes_per_pixel);
		pSrc += src_width * bytes_per_pixel;
		pDst += fix->line_length;
	}
}


/*
if Updated-pixel is overwrited by other color, progressbar-Update Stop.
return value : TRUE(update), FALSE(STOP)
*/
static int s3cfb_check_progress(struct fb_info *fb, const int progress_pos)
{
	unsigned char *pDst = fb->screen_base;
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct fb_var_screeninfo *var = &fb->var;
	int bytes_per_pixel = (var->bits_per_pixel / 8);
	int x = PROGRESS_BAR_LEFT_POS;
	int y = PROGRESS_BAR_START_Y;

	if (progress_pos + PROGRESS_BAR_WIDTH >= PROGRESS_BAR_RIGHT_POS)
		return FALSE;

	pDst += y * fix->line_length + x * bytes_per_pixel;
	if (*pDst == anycall_progress_bar_left[0])
		return TRUE;
	else
		return FALSE;
}


void s3cfb_start_progress(struct fb_info *fb)
{
	int x_pos;

	if (!show_progress)
		return;

	init_timer(&progress_timer);

	progress_timer.expires  = (get_jiffies_64() + (HZ/20));
	progress_timer.data     = (long)fb;
	progress_timer.function = progress_timer_handler;
	progress_pos = PROGRESS_BAR_LEFT_POS;

	/* draw progress background. */
	for (x_pos = PROGRESS_BAR_LEFT_POS; x_pos <= PROGRESS_BAR_RIGHT_POS; \
		x_pos += PROGRESS_BAR_WIDTH){
		s3cfb_update_framebuffer(fb,
			x_pos,
			PROGRESS_BAR_START_Y,
			(void *)anycall_progress_bar,
			PROGRESS_BAR_WIDTH,
			PROGRESS_BAR_HEIGHT);
	}
	s3cfb_update_framebuffer(fb,
		PROGRESS_BAR_LEFT_POS,
		PROGRESS_BAR_START_Y,
		(void *)anycall_progress_bar_left,
		PROGRESS_BAR_WIDTH,
		PROGRESS_BAR_HEIGHT);

	progress_pos += PROGRESS_BAR_WIDTH;

	s3cfb_update_framebuffer(fb,
		progress_pos,
		PROGRESS_BAR_START_Y,
		(void *)anycall_progress_bar_right,
		PROGRESS_BAR_WIDTH,
		PROGRESS_BAR_HEIGHT);

	add_timer(&progress_timer);
	progress_flag = TRUE;

}

static void s3cfb_stop_progress(void)
{
	if (progress_flag == FALSE)
		return;
	del_timer(&progress_timer);
	progress_flag = 0;
}

static void progress_timer_handler(unsigned long data)
{
	int i;

	if (s3cfb_check_progress((struct fb_info *)data, progress_pos)) {
		for (i = 0; i < PROGRESS_BAR_WIDTH; i++) {
			s3cfb_update_framebuffer((struct fb_info *)data,
				progress_pos++,
				PROGRESS_BAR_START_Y,
				(void *)anycall_progress_bar_center,
				1,
				PROGRESS_BAR_HEIGHT);
	}

	s3cfb_update_framebuffer((struct fb_info *)data,
		progress_pos,
		PROGRESS_BAR_START_Y,
		(void *)anycall_progress_bar_right,
		PROGRESS_BAR_WIDTH,
		PROGRESS_BAR_HEIGHT);

		progress_timer.expires = (get_jiffies_64() + (HZ/20));
		progress_timer.function = progress_timer_handler;
		add_timer(&progress_timer);
	} else
		s3cfb_stop_progress();

}
