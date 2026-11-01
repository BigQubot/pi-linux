#include "rwnx_main.h"
#include "rwnx_msg_tx.h"
#include "reg_access.h"
#include "aicwf_8800d80n.h"
#include "rwnx_platform.h"

#include <linux/firmware.h>

#ifdef AICWF_USB_SUPPORT

#define RAM_LMAC_FW_ADDR                   0x00150000
#define RAM_FMAC_FW_ADDR                   0x00120000

#define USER_PWROFST_COVER_CALIB_FLAG   (0x01U << 0)
#define USER_CHAN_MAX_TXPWR_EN_FLAG     (0x01U << 1)
#define USER_APM_PRBRSP_OFFLOAD_DISABLE_FLAG    (0x01U << 3)
#define USER_HE_MU_EDCA_UPDATE_DISABLE_FLAG     (0x01U << 4)
#define USER_RX_CSI_SUPPORT_FLAG        (0x01U << 16)

#define USER_EXT_FLAGS_DEFAULT_D80N  \
	(USER_PWROFST_COVER_CALIB_FLAG)

#define CHIP_INFO_FLAG_CINIT_BEGIN          (0x01U << 12)
#define CHIP_INFO_FLAG_CINIT_DONE           (0x01U << 13)

#define PATCH_VAR_FLAG_CALIB_BEGIN          (0x01U << 0)
#define PATCH_VAR_FLAG_CALIB_DONE           (0x01U << 1)

#if defined(CONFIG_POWER_LIMIT)
#define CFG_USER_CHAN_MAX_TXPWR_EN          (1)
#else
#define CFG_USER_CHAN_MAX_TXPWR_EN          (0)
#endif
#if defined(CONFIG_PRBREQ_REPORT)
#define CFG_USER_APM_PRBRSP_OFFLOAD_DISABLE (1)
#else
#define CFG_USER_APM_PRBRSP_OFFLOAD_DISABLE (0)
#endif

#define CFG_USER_HE_MU_EDCA_UPDATE_DISABLE  (0)
#define CFG_USER_PWROFST_COVER_CALIB_EN     (1)

#if defined(CONFIG_SUPPORT_CSI)
#define CFG_USER_RX_CSI_SUPPORT_EN          (1)
#else
#define CFG_USER_RX_CSI_SUPPORT_EN          (0)
#endif

u32 syscfg_tbl_masked_8800d80n[][3] = {
	// {Address, mask, value}
	{0x00000000, 0x00000000, 0x00000000}, // last one
};

u32 patch_tbl_wifisetting_8800d80n[][2] =
{
	//{0x00b8, 0x00009d08 | (0x01U << 13)}, // debug_mask, bit13: CALIB_BIT
	{0x017c,
		(USER_EXT_FLAGS_DEFAULT_D80N |
			#if CFG_USER_CHAN_MAX_TXPWR_EN
			USER_CHAN_MAX_TXPWR_EN_FLAG |
			#endif
			#if CFG_USER_APM_PRBRSP_OFFLOAD_DISABLE
			USER_APM_PRBRSP_OFFLOAD_DISABLE_FLAG |
			#endif
			#if CFG_USER_HE_MU_EDCA_UPDATE_DISABLE
			USER_HE_MU_EDCA_UPDATE_DISABLE_FLAG |
			#endif
			#if CFG_USER_RX_CSI_SUPPORT_EN
			USER_RX_CSI_SUPPORT_FLAG |
			#endif
		0) & ~(
			#if !CFG_USER_PWROFST_COVER_CALIB_EN
			USER_PWROFST_COVER_CALIB_FLAG |
			#endif
		0)
	}, // user_ext_flags
#ifdef CONFIG_PLATFORM_HI
	{0x0150, 0x01140A01}, //rx aggr counter
#endif

};

//adap test
u32 adaptivity_patch_tbl_8800d80n[][2] = {
	{0x0160, 0x00010000},
};

u32 patch_tbl_rf_func_8800d80n[][2] =
{
};

#define ROM_FMAC_PATCH_ADDR_8800D80N_U02    0x00188000
#define RWNX_MAC_PATCH_NAME_8800D80N_U02    "aic8800d80n/usb/fmacfw_patch_8800d80n_u02.bin"
#define RWNX_MAC_PATCH_NAME_8800D80N_U02C    "aic8800d80n/usb/fmacfw_patch_8800d80n_u02c.bin"
#define ROM_FMAC_REKEY_ADDR_8800D80N_U02    0x00160000
#define RWNX_MAC_FW_REKEY_NAME_8800D80N_U02 "aic8800d80n/usb/fmacfw_rekey_8800d80n_u02.bin"
#define RWNX_MAC_FW_REKEY_NAME_8800D80N_U02C "aic8800d80n/usb/fmacfw_rekey_8800d80n_u02c.bin"
#define RAM_LMAC_FW_RF_ADDR_8800D80N        0x00132C00
#define RWNX_MAC_FW_RF_BASE_NAME_8800D80N   "aic8800d80n/usb/lmacfw_rf_8800d80n.bin"
#define RWNX_MAC_PATCHTBL_NAME_8800D80N_U02 "aic8800d80n/usb/fmacfw_patch_tbl_8800d80n_u02.bin"
#define RWNX_MAC_PATCHTBL_NAME_8800D80N_U02C "aic8800d80n/usb/fmacfw_patch_tbl_8800d80n_u02c.bin"
#define ROM_FMAC_CINIT_ADDR_8800D80N_U02    0x00133000
#define RWNX_MAC_FW_CINIT_NAME_8800D80N_U02 "aic8800d80n/usb/fmacfw_cinit_8800d80n_u02.bin"
#define RWNX_MAC_FW_CINIT_NAME_8800D80N_U02C "aic8800d80n/usb/fmacfw_cinit_8800d80n_u02c.bin"
#define ROM_FMAC_CALIB_ADDR_8800D80N_U02    0x00138000
#define RWNX_MAC_FW_CALIB_NAME_8800D80N_U02 "aic8800d80n/usb/fmacfw_calib_8800d80n_u02.bin"
#define RWNX_MAC_FW_CALIB_NAME_8800D80N_U02C "aic8800d80n/usb/fmacfw_calib_8800d80n_u02c.bin"

typedef u32 (*array3_tbl_t)[3];

#endif

int aicwf_plat_secure_calib_exec_8800d80n(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;
	struct mm_set_rf_calib_cfm cfm;
	printk("exec secure calib\n");
	ret = rwnx_send_rf_secure_calib_req(rwnx_hw, &cfm, 0);
	if (ret) {
		printk("secure calib fail, ret=%d\n", ret);
		return ret;
	}
	return ret;
}

int aicwf_set_rf_config_8800d80n(struct rwnx_hw *rwnx_hw, struct mm_set_rf_calib_cfm *cfm)
{
	int ret = 0;

	ret = rwnx_send_txpwr_lvl_v3_req(rwnx_hw);
	if (ret)
		return ret;

	ret = rwnx_send_txpwr_ofst2x_req(rwnx_hw);
	if (ret)
		return ret;

	ret = rwnx_send_txpwr_lvl_adj_req(rwnx_hw);
	if (ret)
		return ret;

#if 0
	ret = rwnx_send_rf_calib_req(rwnx_hw, cfm);
	if (ret)
		return ret;
#endif
	return ret;
}

#ifdef AICWF_USB_SUPPORT
static int aicwf_plat_patch_load_8800d80n(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;

	if (aicwf_feature.chipinfo->subrev == 1) {
		ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_PATCH_ADDR_8800D80N_U02, RWNX_MAC_PATCH_NAME_8800D80N_U02);
	} else {
		ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_PATCH_ADDR_8800D80N_U02, RWNX_MAC_PATCH_NAME_8800D80N_U02C);
	}
	if (ret) {
		printk("patch upload fail: %d\n", ret);
		return ret;
	}
	return ret;
}

#ifdef CONFIG_REKEY_OFFLOAD
int aicwf_plat_rekey_exec_8800d80n(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;

	if (aicwf_feature.chipinfo->subrev == 1) {
		ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_REKEY_ADDR_8800D80N_U02, RWNX_MAC_FW_REKEY_NAME_8800D80N_U02);
	} else {
		ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_REKEY_ADDR_8800D80N_U02, RWNX_MAC_FW_REKEY_NAME_8800D80N_U02C);
	}
	if (ret) {
		printk("load rekey bin fail: %d\n", ret);
		return ret;
	}
	return ret;
}
#endif

int aicwf_plat_patch_table_load_8800d80n(struct rwnx_hw *rwnx_hw)
{
	int err = 0;
	int i, size;
	u32 *dst = NULL;
	char *filename;

	if (aicwf_feature.chipinfo->subrev == 1) {
		filename = RWNX_MAC_PATCHTBL_NAME_8800D80N_U02;
	} else {
		filename = RWNX_MAC_PATCHTBL_NAME_8800D80N_U02C;
	}
	/* Copy the file on the Embedded side */
	printk("### Upload %s \n", filename);

	const struct firmware *fw = NULL;
	int ret = request_firmware(&fw, filename, NULL);

	printk("rwnx_request_firmware, name: %s\n", filename);
	if (ret < 0) {
		printk("Load %s fail\n", filename);
		return ret;
	}

	size = fw->size;
	dst = (u32 *)fw->data;

	if (size <= 0) {
		printk("wrong size of firmware file\n");
		release_firmware(fw);
		return -1;
	}

	printk("tbl size = %d \n", size);

	if (!err) {
		for (i = 0; i < (size / 4); i += 2) {
			if ((dst[i] == 0x0) || (dst[i] == 0xFFFFFFFF)) {
				break; // end of tbl
			}
			printk("patch_tbl:  %x  %x\n", dst[i], dst[i+1]);
			err = rwnx_send_dbg_mem_write_req(rwnx_hw, dst[i], dst[i+1]);
		}
		if (err) {
			printk("tbl bin upload fail: %x, err:%d\r\n", dst[i], err);
		}
	}

	release_firmware(fw);

	return err;
}

static int aic8800d80n_wifi_patch_config(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;
	int cnt = 0;

	if (aicwf_feature.cpmode == AICBSP_CPMODE_WORK) {
		const u32 cfg_base = 0x10170;
		u32 wifisetting_cfg_addr;
		u32 agc_cfg_addr;
		u32 txgain_cfg_24g_addr;
		u32 txgain_cfg_5g_addr;
		struct dbg_mem_read_cfm cfm;
		int adap_patch_num;

		u32 jump_tbl_addr = 0;
		u32 patch_tbl_wifisetting_num = sizeof(patch_tbl_wifisetting_8800d80n)/sizeof(u32)/2;;

		ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base, &cfm);
		if (ret) {
			pr_err("setting base[0x%x] rd fail: %d\n", cfg_base, ret);
			goto out;
		}
		wifisetting_cfg_addr = cfm.memdata;

		ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 4, &cfm);
		if (ret) {
			pr_err("setting base[0x%x] rd fail: %d\n", cfg_base + 4, ret);
			goto out;
		}
		jump_tbl_addr = cfm.memdata;

		ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x10, &cfm);
		if (ret) {
			pr_err("setting base[0x%x] rd fail: %d\n", cfg_base + 0x10, ret);
			goto out;
		}
		agc_cfg_addr = cfm.memdata;

		if (rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x14, &cfm)) {
			pr_err("setting base[0x%x] rd fail: %d\n", cfg_base + 0x14, ret);
			goto out;
		}
		txgain_cfg_24g_addr = cfm.memdata;

		if (rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x18, &cfm)) {
			pr_err("setting base[0x%x] rd fail: %d\n", cfg_base + 0x18, ret);
			goto out;
		}
		txgain_cfg_5g_addr = cfm.memdata;

		printk("wifisetting_cfg_addr=%x, agc_cfg_addr=%x, txgain_cfg_24g_addr=%x, txgain_cfg_5g_addr=%x\n", wifisetting_cfg_addr, agc_cfg_addr, txgain_cfg_24g_addr, txgain_cfg_5g_addr);

		for (cnt = 0; cnt < patch_tbl_wifisetting_num; cnt++) {
			ret = rwnx_send_dbg_mem_write_req(rwnx_hw, wifisetting_cfg_addr + patch_tbl_wifisetting_8800d80n[cnt][0], patch_tbl_wifisetting_8800d80n[cnt][1]);
			if (ret) {
				pr_err("wifisetting %x write fail\n", patch_tbl_wifisetting_8800d80n[cnt][0]);
				goto out;
			}
		}

		// adap test
		if (aicwf_feature.adap_test == 1) {
			printk("%s for adaptivity test \r\n", __func__);
			adap_patch_num = sizeof(adaptivity_patch_tbl_8800d80n)/sizeof(u32)/2;
			for (cnt = 0; cnt < adap_patch_num; cnt++) {
				ret = rwnx_send_dbg_mem_write_req(rwnx_hw, wifisetting_cfg_addr + adaptivity_patch_tbl_8800d80n[cnt][0], adaptivity_patch_tbl_8800d80n[cnt][1]);
				if (ret) {
					printk("%x write fail\n", wifisetting_cfg_addr + adaptivity_patch_tbl_8800d80n[cnt][0]);
				}
			}
		}
		//adap test
	} else {
		u32 patch_tbl_rf_func_num = sizeof(patch_tbl_rf_func_8800d80n)/sizeof(u32)/2;
		for (cnt = 0; cnt < patch_tbl_rf_func_num; cnt++) {
			ret = rwnx_send_dbg_mem_write_req(rwnx_hw, patch_tbl_rf_func_8800d80n[cnt][0], patch_tbl_rf_func_8800d80n[cnt][1]);
			if (ret) {
				pr_err("patch_tbl_rf_func %x write fail\n", patch_tbl_rf_func_8800d80n[cnt][0]);
				goto out;
			}
		}
	}

out:
	return ret;
}

int aicwf_plat_cinit_exec_8800d80n(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;
	uint32_t fw_addr, boot_type;
	uint32_t mem_addr = 0x40500184;
	uint32_t mem_val;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;
	ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
	if (ret) {
		printk("%x rd fail: %d\n", mem_addr, ret);
		return ret;
	}
	mem_val = rd_mem_addr_cfm.memdata;
	if (mem_val & CHIP_INFO_FLAG_CINIT_BEGIN) {
		// wait done
		while (!(mem_val & CHIP_INFO_FLAG_CINIT_DONE)) {
			printk("cinit rd chipinfo=%x\n", mem_val);
			ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
			if (ret) {
				printk("%x rd fail: %d\n", mem_addr, ret);
				return ret;
			}
			mem_val = rd_mem_addr_cfm.memdata;
		}
		printk("cinit executed, chipinfo=%x\n", mem_val);
		return ret;
	}
	if (aicwf_feature.chipinfo->subrev == 1) {
		ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_CINIT_ADDR_8800D80N_U02, RWNX_MAC_FW_CINIT_NAME_8800D80N_U02);
	} else {
		ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_CINIT_ADDR_8800D80N_U02, RWNX_MAC_FW_CINIT_NAME_8800D80N_U02C);
	}
	if (ret) {
		printk("load cinit bin fail: %d\n", ret);
		return ret;
	}
	/* fw start */
	fw_addr = ROM_FMAC_CINIT_ADDR_8800D80N_U02 + 0x0009;
	boot_type = HOST_START_APP_FNCALL;
	printk("Start app: %08x, %d\n", fw_addr, boot_type);
	ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type);
	if (ret) {
		printk("start app fail: %d\n", ret);
		return ret;
	}
	return ret;
}

int aicwf_plat_calib_exec_8800d80n(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;
	uint32_t fw_addr, boot_type;
	uint32_t patch_var_flags_addr = ROM_FMAC_PATCH_ADDR_8800D80N_U02 + 0x08;
	uint32_t patch_var_flags_val;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;
	ret = rwnx_send_dbg_mem_read_req(rwnx_hw, patch_var_flags_addr, &rd_mem_addr_cfm);
	if (ret) {
		printk("%x rd fail: %d\n", patch_var_flags_addr, ret);
		return ret;
	}
	patch_var_flags_val = rd_mem_addr_cfm.memdata;
	if (patch_var_flags_val & (PATCH_VAR_FLAG_CALIB_BEGIN | PATCH_VAR_FLAG_CALIB_DONE)) {
		printk("calib executed, var_flags=%x\n", patch_var_flags_val);
		return ret;
	}
	#if (CONFIG_SECURE_CALIB_EXEC_ENABLED)
	patch_var_flags_val |= PATCH_VAR_FLAG_SECURE_CALIB_EN;
	ret = rwnx_send_dbg_mem_write_req(rwnx_hw, patch_var_flags_addr, patch_var_flags_val);
	if (ret) {
		printk("%x wr fail: %d\n", patch_var_flags_addr, ret);
		return ret;
	}
	#endif
	if (aicwf_feature.chipinfo->subrev == 1) {
		ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_CALIB_ADDR_8800D80N_U02, RWNX_MAC_FW_CALIB_NAME_8800D80N_U02);
	} else {
		ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_CALIB_ADDR_8800D80N_U02, RWNX_MAC_FW_CALIB_NAME_8800D80N_U02C);
	}
	if (ret) {
		printk("load calib bin fail: %d\n", ret);
		return ret;
	}
	/* fw start */
	fw_addr = ROM_FMAC_CALIB_ADDR_8800D80N_U02 + 0x0009;
	boot_type = HOST_START_APP_FNCALL;
	printk("Start app: %08x, %d\n", fw_addr, boot_type);
	ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type);
	if (ret) {
		printk("start app fail: %d\n", ret);
		return ret;
	}
	return ret;
}

int aicwf_plat_rftest_load_8800d80n(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;
	ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, RAM_LMAC_FW_RF_ADDR_8800D80N, RWNX_MAC_FW_RF_BASE_NAME_8800D80N);
	if (ret) {
		printk("load rftest bin fail: %d\n", ret);
		return ret;
	}
	return ret;
}

void system_config_8800d80n(struct rwnx_hw *rwnx_hw)
{
	u32 mem_addr = 0x40500000;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;

	int syscfg_num, cnt, ret;
	array3_tbl_t p_syscfg_msk_tbl;

	if (rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm))
	if (ret) {
		printk("%x rd fail: %d\n", mem_addr, ret);
		return;
	}
	aicwf_feature.chipinfo->rev = (u8)((rd_mem_addr_cfm.memdata >> 16) & 0x3F);
	aicwf_feature.chipinfo->is_chip_id_h = (u8)IS_CHIP_ID_H(rd_mem_addr_cfm.memdata >> 16);

	aicwf_feature.chipinfo->mcuid = 0;
	if (((rd_mem_addr_cfm.memdata >> 25) & 0x01UL) == 0x00UL)
		aicwf_feature.chipinfo->mcuid = 1;

	mem_addr = 0x00000020;
	ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
	if (ret) {
		printk("[0x00000020] rd fail: %d\n", ret);
		return;
	}
	aicwf_feature.chipinfo->subrev = (u8)rd_mem_addr_cfm.memdata;

	printk("%s(%d), rev id: 0x%x, subrev id: 0x%x, mcu id: 0x%x, is_chip_id_h: %d\n", __func__, __LINE__,
			aicwf_feature.chipinfo->rev, aicwf_feature.chipinfo->subrev, aicwf_feature.chipinfo->mcuid, aicwf_feature.chipinfo->is_chip_id_h);

	rwnx_hw->rev = aicwf_feature.chipinfo->rev;
	rwnx_hw->subrev = aicwf_feature.chipinfo->subrev;

	syscfg_num = sizeof(syscfg_tbl_masked_8800d80n) / sizeof(u32) / 3;
	p_syscfg_msk_tbl = syscfg_tbl_masked_8800d80n;

	for (cnt = 0; cnt < syscfg_num; cnt++) {
		if (p_syscfg_msk_tbl[cnt][0] == 0x00000000)
			break;

		ret = rwnx_send_dbg_mem_mask_write_req(rwnx_hw,
			p_syscfg_msk_tbl[cnt][0], p_syscfg_msk_tbl[cnt][1], p_syscfg_msk_tbl[cnt][2]);
		if (ret) {
			printk("%x mask write fail: %d\n", p_syscfg_msk_tbl[cnt][0], ret);
			return;
		}
	}
}

static int start_from_bootrom(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;

	/* memory access */
#ifdef CONFIG_ROM_PATCH_EN
	const u32 rd_addr = RAM_LMAC_FW_ADDR;
	const u32 fw_addr = RAM_LMAC_FW_ADDR;
#else
	const u32 rd_addr = RAM_FMAC_FW_ADDR;
	const u32 fw_addr = RAM_FMAC_FW_ADDR;
#endif
	u32 boot_type;
	struct dbg_mem_read_cfm rd_cfm;
	printk("Read FW mem: %08x\n", rd_addr);
	if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, rd_addr, &rd_cfm))) {
		return -1;
	}
	printk("cfm: [%08x] = %08x\n", rd_cfm.memaddr, rd_cfm.memdata);

	if (aicwf_feature.cpmode == AICBSP_CPMODE_WORK) {
		boot_type = HOST_START_APP_DUMMY;
	} else {
		boot_type = HOST_START_APP_AUTO;
	}
	/* fw start */
	printk("Start app: %08x, %d\n", fw_addr, boot_type);
	if ((ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type))) {
		return -1;
	}
	return 0;
}

int aicwf_plat_rftest_exec_8800d80n(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;
	uint32_t fw_addr, boot_type;
	uint32_t rst_hdlr_addr = RAM_LMAC_FW_RF_ADDR_8800D80N + 0x04;
	uint32_t rst_hdlr_val;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;
	ret = rwnx_send_dbg_mem_read_req(rwnx_hw, rst_hdlr_addr, &rd_mem_addr_cfm);
	if (ret) {
		printk("%x rd fail: %d\n", rst_hdlr_addr, ret);
		return ret;
	}
	rst_hdlr_val = rd_mem_addr_cfm.memdata;
	if ((rst_hdlr_val & ~0x03FF) == RAM_LMAC_FW_RF_ADDR_8800D80N) {
		printk("rftest loaded, hdlr=%x\n", rst_hdlr_val);
	}
	/* fw start */
	fw_addr = RAM_LMAC_FW_RF_ADDR_8800D80N;
	boot_type = HOST_START_APP_AUTO;
	printk("Start app: %08x, %d\n", fw_addr, boot_type);
	ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type);
	if (ret) {
		printk("start app fail: %d\n", ret);
		return ret;
	}
	return ret;
}

int aicfw_8800d80n_fw_init(struct rwnx_hw *rwnx_hw)
{
	int ret;
	system_config_8800d80n(rwnx_hw);

	if (aicwf_feature.cpmode == AICBSP_CPMODE_WORK){
		ret = aicwf_plat_patch_load_8800d80n(rwnx_hw);
		if (ret)
			return ret;
#ifdef CONFIG_REKEY_OFFLOAD
		ret = aicwf_plat_rekey_exec_8800d80n(rwnx_hw);
		if (ret) {
			printk("rekey upload fail: %d\n", ret);
			return ret;
		}
#endif
		ret = aicwf_plat_patch_table_load_8800d80n(rwnx_hw);
		if (ret) {
			printk("patch_tbl upload fail: %d\r\n", ret);
			return ret;
		}

		ret = aic8800d80n_wifi_patch_config(rwnx_hw);
		if (ret)
			return ret;

		aicwf_plat_cinit_exec_8800d80n(rwnx_hw);
		aicwf_plat_calib_exec_8800d80n(rwnx_hw);
	}
	else if (aicwf_feature.cpmode == AICBSP_CPMODE_TEST) {
		aicwf_plat_cinit_exec_8800d80n(rwnx_hw);
		printk("%s load rftest bin\n", __func__);
		ret = aicwf_plat_rftest_load_8800d80n(rwnx_hw);
		if (ret) {
			printk("load rftest bin fail: %d\n", ret);
			return ret;
		}
	}

	if (aicwf_feature.cpmode == AICBSP_CPMODE_WORK) {
		ret = start_from_bootrom(rwnx_hw);
		if (ret) {
			printk("start from bootrom fail: %d\n", ret);
			return ret;
		}
	}
	else {
		printk("%s exec rftest bin\n", __func__);
		ret = aicwf_plat_rftest_exec_8800d80n(rwnx_hw);
		if (ret) {
			printk("exec rftest bin fail: %d\n", ret);
			return ret;
		}
	}

	return 0;
}
#endif
