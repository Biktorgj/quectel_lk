/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <reg.h>
#include <err.h>
#include <clock.h>
#include <clock_pll.h>
#include <clock_lib2.h>
#include <platform/clock.h>
#include <platform/iomap.h>
#include <platform.h>

/* Mux source select values */
#define cxo_source_val    0
#define gpll0_source_val  1
#define gpll4_source_val  2
#define cxo_mm_source_val 0
#define gpll0_mm_source_val 1

struct clk_freq_tbl rcg_dummy_freq = F_END;


/* Clock Operations */
static struct clk_ops clk_ops_branch =
{
	.enable     = clock_lib2_branch_clk_enable,
	.disable    = clock_lib2_branch_clk_disable,
	.set_rate   = clock_lib2_branch_set_rate,
};

static struct clk_ops clk_ops_rcg_mnd =
{
	.enable     = clock_lib2_rcg_enable,
	.set_rate   = clock_lib2_rcg_set_rate,
};

static struct clk_ops clk_ops_rcg =
{
	.enable     = clock_lib2_rcg_enable,
	.set_rate   = clock_lib2_rcg_set_rate,
};

static struct clk_ops clk_ops_cxo =
{
	.enable     = cxo_clk_enable,
	.disable    = cxo_clk_disable,
};

static struct clk_ops clk_ops_pll_vote =
{
	.enable     = pll_vote_clk_enable,
	.disable    = pll_vote_clk_disable,
	.auto_off   = pll_vote_clk_disable,
	.is_enabled = pll_vote_clk_is_enabled,
};

static struct clk_ops clk_ops_vote =
{
	.enable     = clock_lib2_vote_clk_enable,
	.disable    = clock_lib2_vote_clk_disable,
};

/* Clock Sources */
static struct fixed_clk cxo_clk_src =
{
	.c = {
		.rate     = 19200000,
		.dbg_name = "cxo_clk_src",
		.ops      = &clk_ops_cxo,
	},
};

static struct pll_vote_clk gpll0_clk_src =
{
	.en_reg       = (void *) APCS_GPLL_ENA_VOTE,
	.en_mask      = BIT(0),
	.status_reg   = (void *) GPLL0_MODE,
	.status_mask  = BIT(30),
	.parent       = &cxo_clk_src.c,

	.c = {
		.rate     = 800000000,
		.dbg_name = "gpll0_clk_src",
		.ops      = &clk_ops_pll_vote,
	},
};

/* UART Clocks */
static struct clk_freq_tbl ftbl_gcc_blsp1_2_uart5_apps_clk[] =
{
	F( 3686400,  gpll0,    1,  72,  15625),
	F( 7372800,  gpll0,    1, 144,  15625),
	F(14745600,  gpll0,    1, 288,  15625),
	F(16000000,  gpll0,   10,   1,      5),
	F(19200000,    cxo,    1,   0,      0),
	F(24000000,  gpll0,    1,   3,    100),
	F(25000000,  gpll0,   16,   1,      2),
	F(32000000,  gpll0,    1,   1,     25),
	F(40000000,  gpll0,    1,   1,     20),
	F(46400000,  gpll0,    1,  29,    500),
	F(48000000,  gpll0,    1,   3,     50),
	F(51200000,  gpll0,    1,   8,    125),
	F(56000000,  gpll0,    1,   7,    100),
	F(58982400,  gpll0,    1,1152,  15625),
	F(60000000,  gpll0,    1,   3,     40),
	F_END
};

static struct rcg_clk blsp1_uart5_apps_clk_src =
{
	.cmd_reg      = (uint32_t *) BLSP1_UART5_APPS_CMD_RCGR,
	.cfg_reg      = (uint32_t *) BLSP1_UART5_APPS_CFG_RCGR,
	.m_reg        = (uint32_t *) BLSP1_UART5_APPS_M,
	.n_reg        = (uint32_t *) BLSP1_UART5_APPS_N,
	.d_reg        = (uint32_t *) BLSP1_UART5_APPS_D,

	.set_rate     = clock_lib2_rcg_set_rate_mnd,
	.freq_tbl     = ftbl_gcc_blsp1_2_uart5_apps_clk,
	.current_freq = &rcg_dummy_freq,

	.c = {
		.dbg_name = "blsp1_uart2_apps_clk",
		.ops      = &clk_ops_rcg_mnd,
	},
};

static struct branch_clk gcc_blsp1_uart5_apps_clk =
{
	.cbcr_reg     = (uint32_t *) BLSP1_UART5_APPS_CBCR,
	.parent       = &blsp1_uart5_apps_clk_src.c,

	.c = {
		.dbg_name = "gcc_blsp1_uart5_apps_clk",
		.ops      = &clk_ops_branch,
	},
};

static struct vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg     = (uint32_t *) BLSP1_AHB_CBCR,
	.vote_reg     = (uint32_t *) APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask      = BIT(10),

	.c = {
		.dbg_name = "gcc_blsp1_ahb_clk",
		.ops      = &clk_ops_vote,
	},
};

/* USB Clocks */
static struct clk_freq_tbl ftbl_gcc_usb_hs_system_clk[] =
{
	F(133330000,  gpll0,    6,   0,   0),
	F_END
};

static struct rcg_clk usb_hs_system_clk_src =
{
	.cmd_reg      = (uint32_t *) USB_HS_SYSTEM_CMD_RCGR,
	.cfg_reg      = (uint32_t *) USB_HS_SYSTEM_CFG_RCGR,

	.set_rate     = clock_lib2_rcg_set_rate_hid,
	.freq_tbl     = ftbl_gcc_usb_hs_system_clk,
	.current_freq = &rcg_dummy_freq,

	.c = {
		.dbg_name = "usb_hs_system_clk",
		.ops      = &clk_ops_rcg,
	},
};

static struct branch_clk gcc_usb_hs_system_clk =
{
	.cbcr_reg     = (uint32_t *) USB_HS_SYSTEM_CBCR,
	.parent       = &usb_hs_system_clk_src.c,

	.c = {
		.dbg_name = "gcc_usb_hs_system_clk",
		.ops      = &clk_ops_branch,
	},
};

static struct branch_clk gcc_usb_hs_ahb_clk =
{
	.cbcr_reg     = (uint32_t *) USB_HS_AHB_CBCR,
	.has_sibling  = 1,

	.c = {
		.dbg_name = "gcc_usb_hs_ahb_clk",
		.ops      = &clk_ops_branch,
	},
};
static struct clk_freq_tbl ftbl_gcc_ce1_clk[] = {
	F(160000000,  gpll0,   5,   0,   0),
	F_END
};

static struct rcg_clk ce1_clk_src = {
	.cmd_reg      = (uint32_t *) GCC_CRYPTO_CMD_RCGR,
	.cfg_reg      = (uint32_t *) GCC_CRYPTO_CFG_RCGR,
	.set_rate     = clock_lib2_rcg_set_rate_hid,
	.freq_tbl     = ftbl_gcc_ce1_clk,
	.current_freq = &rcg_dummy_freq,

	.c = {
		.dbg_name = "ce1_clk_src",
		.ops      = &clk_ops_rcg,
	},
};

static struct vote_clk gcc_ce1_clk = {
	.cbcr_reg      = (uint32_t *) GCC_CRYPTO_CBCR,
	.vote_reg      = (uint32_t *) APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask       = BIT(2),

	.c = {
		.dbg_name  = "gcc_ce1_clk",
		.ops       = &clk_ops_vote,
	},
};

static struct vote_clk gcc_ce1_ahb_clk = {
	.cbcr_reg     = (uint32_t *) GCC_CRYPTO_AHB_CBCR,
	.vote_reg     = (uint32_t *) APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask      = BIT(0),

	.c = {
		.dbg_name = "gcc_ce1_ahb_clk",
		.ops      = &clk_ops_vote,
	},
};

static struct vote_clk gcc_ce1_axi_clk = {
	.cbcr_reg     = (uint32_t *) GCC_CRYPTO_AXI_CBCR,
	.vote_reg     = (uint32_t *) APCS_CLOCK_BRANCH_ENA_VOTE,
	.en_mask      = BIT(1),

	.c = {
		.dbg_name = "gcc_ce1_axi_clk",
		.ops      = &clk_ops_vote,
	},
};
#if MMC_SDHCI_SUPPORT
/* SDCC Clocks */
static struct clk_freq_tbl ftbl_gcc_sdcc1_2_apps_clk[] =
{
	F(    144000,            cxo,   16,    3,    25),
	F(    400000,            cxo,   12,    1,     4),
	F(  20000000,          gpll0,   10,    1,     4),
	F(  25000000,          gpll0,   16,    1,     2),
	F(  50000000,          gpll0,   16,    0,     0),
	F( 100000000,          gpll0,    8,    0,     0),
	F( 177777778,          gpll0,  4.5,    0,     0),
	F( 200000000,          gpll0,    4,    0,     0),
	F_END
};

static struct rcg_clk sdcc1_apps_clk_src =
{
	.cmd_reg      = (uint32_t *) SDCC1_CMD_RCGR,
	.cfg_reg      = (uint32_t *) SDCC1_CFG_RCGR,
	.m_reg        = (uint32_t *) SDCC1_M,
	.n_reg        = (uint32_t *) SDCC1_N,
	.d_reg        = (uint32_t *) SDCC1_D,

	.set_rate     = clock_lib2_rcg_set_rate_mnd,
	.freq_tbl     = ftbl_gcc_sdcc1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,

	.c = {
		.dbg_name = "sdc1_clk",
		.ops      = &clk_ops_rcg_mnd,
	},
};

static struct branch_clk gcc_sdcc1_apps_clk =
{
	.cbcr_reg     = (uint32_t *) SDCC1_APPS_CBCR,
	.parent       = &sdcc1_apps_clk_src.c,

	.c = {
		.dbg_name = "gcc_sdcc1_apps_clk",
		.ops      = &clk_ops_branch,
	},
};

static struct branch_clk gcc_sdcc1_ahb_clk =
{
	.cbcr_reg     = (uint32_t *) SDCC1_AHB_CBCR,
	.has_sibling  = 1,

	.c = {
		.dbg_name = "gcc_sdcc1_ahb_clk",
		.ops      = &clk_ops_branch,
	},
};

static struct rcg_clk sdcc2_apps_clk_src =
{
	.cmd_reg      = (uint32_t *) SDCC2_CMD_RCGR,
	.cfg_reg      = (uint32_t *) SDCC2_CFG_RCGR,
	.m_reg        = (uint32_t *) SDCC2_M,
	.n_reg        = (uint32_t *) SDCC2_N,
	.d_reg        = (uint32_t *) SDCC2_D,

	.set_rate     = clock_lib2_rcg_set_rate_mnd,
	.freq_tbl     = ftbl_gcc_sdcc1_2_apps_clk,
	.current_freq = &rcg_dummy_freq,

	.c = {
		.dbg_name = "sdc2_clk",
		.ops      = &clk_ops_rcg_mnd,
	},
};

static struct branch_clk gcc_sdcc2_apps_clk =
{
	.cbcr_reg     = (uint32_t *) SDCC2_APPS_CBCR,
	.parent       = &sdcc2_apps_clk_src.c,

	.c = {
		.dbg_name = "gcc_sdcc2_apps_clk",
		.ops      = &clk_ops_branch,
	},
};

static struct branch_clk gcc_sdcc2_ahb_clk =
{
	.cbcr_reg     = (uint32_t *) SDCC2_AHB_CBCR,
	.has_sibling  = 1,

	.c = {
		.dbg_name = "gcc_sdcc2_ahb_clk",
		.ops      = &clk_ops_branch,
	},
};
#endif

/* Clock lookup table */
static struct clk_lookup mdm_clocks_9607[] =
{
#if MMC_SDHCI_SUPPORT	//add by carl, for sdio clk
	CLK_LOOKUP("sdc1_iface_clk", gcc_sdcc1_ahb_clk.c),
	CLK_LOOKUP("sdc1_core_clk",  gcc_sdcc1_apps_clk.c),
	CLK_LOOKUP("sdc2_iface_clk", gcc_sdcc2_ahb_clk.c),
	CLK_LOOKUP("sdc2_core_clk",  gcc_sdcc2_apps_clk.c),
#endif
	CLK_LOOKUP("uart5_iface_clk", gcc_blsp1_ahb_clk.c),
	CLK_LOOKUP("uart5_core_clk",  gcc_blsp1_uart5_apps_clk.c),

	CLK_LOOKUP("usb_iface_clk",  gcc_usb_hs_ahb_clk.c),
	CLK_LOOKUP("usb_core_clk",   gcc_usb_hs_system_clk.c),

	CLK_LOOKUP("ce1_ahb_clk",  gcc_ce1_ahb_clk.c),
	CLK_LOOKUP("ce1_axi_clk",  gcc_ce1_axi_clk.c),
	CLK_LOOKUP("ce1_core_clk", gcc_ce1_clk.c),
	CLK_LOOKUP("ce1_src_clk",  ce1_clk_src.c),
};

void platform_clock_init(void)
{
	clk_init(mdm_clocks_9607, ARRAY_SIZE(mdm_clocks_9607));
}
