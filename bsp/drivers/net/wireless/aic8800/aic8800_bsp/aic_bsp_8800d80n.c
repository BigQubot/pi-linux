/**
 ******************************************************************************
 *
 * aic_bsp_8800d80n.c
 *
 * Copyright (C) RivieraWaves 2014-2019
 *
 ******************************************************************************
 */

#include <linux/list.h>
#include <linux/version.h>
#include <linux/firmware.h>
#include "aic_bsp_txrxif.h"
#include "aicsdio.h"
#include "aic_bsp_driver.h"
#include <linux/mmc/sdio_func.h>

#define RAM_LMAC_FW_RF_ADDR_8800D80N        0x00132C00
#define ROM_FMAC_CINIT_ADDR_8800D80N_U02    0x00133000
#define ROM_FMAC_CALIB_ADDR_8800D80N_U02    0x00138000
#define ROM_FMAC_PATCH_ADDR_8800D80N_U02    0x00188000

#define CHIP_INFO_FLAG_CINIT_BEGIN          (0x01U << 12)
#define CHIP_INFO_FLAG_CINIT_DONE           (0x01U << 13)

#define PATCH_VAR_FLAG_CALIB_BEGIN          (0x01U << 0)
#define PATCH_VAR_FLAG_CALIB_DONE           (0x01U << 1)

#if 0
#define AIC_PATCH_MAGIG_NUM                 0x48435450 // "PTCH"
#define AIC_PATCH_MAGIG_NUM_2               0x50544348 // "HCTP"
#define AIC_PATCH_BLOCK_MAX                 4
#endif

#ifdef CONFIG_SDIO_F1_FLAG
extern bool sdio_f1_flag;
#endif

#if 0
typedef struct {
	uint32_t magic_num;
	uint32_t pair_start;
	uint32_t magic_num_2;
	uint32_t pair_count;
	uint32_t block_dst[AIC_PATCH_BLOCK_MAX];
	uint32_t block_src[AIC_PATCH_BLOCK_MAX];
	uint32_t block_size[AIC_PATCH_BLOCK_MAX]; // word count
} aic_patch_t;

#define AIC_PATCH_OFST(mem) ((size_t) &((aic_patch_t *)0)->mem)
#define AIC_PATCH_ADDR(mem) ((u32)(aic_patch_str_base + AIC_PATCH_OFST(mem)))
#endif

#if defined(AICWF_SDIO_SUPPORT)
#define RAM_LMAC_FW_ADDR               0x00150000
#define RAM_FMAC_FW_ADDR               0x00120000

u32 syscfg_tbl_masked_8800d80n[][3] = {
    // {Address, mask, value}
    {0x00000000, 0x00000000, 0x00000000}, // last one
};

u32 patch_tbl_wifisetting_8800d80n[][2] =
{
//	{0x00b8, 0x00009d08 | (0x01U << 13)}, // debug_mask, bit13: CALIB_BIT
    {0x170, 0x50000001}, //gpio_wakeup_en
};

//adap test
u32 adaptivity_patch_tbl_8800d80n[][2] = {
};

u32 patch_tbl_rf_func_8800d80n[][2] =
{
};

#ifdef CONFIG_OOB
// for 8800d40/d80     map data1 isr to gpiob1
u32 gpio_cfg_tbl_8800d40d80[][2] = {
	{0x40504084, 0x00000006},
	{0x40500040, 0x00000000},
	{0x40100030, 0x00000001},
	{0x40241020, 0x00000001},
	{0x40240030, 0x00000004},
	{0x40240020, 0x03020700},
};
#endif

static const struct aicbsp_firmware fw_u02[] = {
	[AICBSP_CPMODE_WORK] = {
		.desc          = "normal work mode(sdio u02)",
		.bt_adid       = "aic8800d80n/fw_adid_8800d80n_u02.bin",
		.bt_patch      = "aic8800d80n/fw_patch_8800d80n_u02.bin",
		.bt_table      = "aic8800d80n/fw_patch_table_8800d80n_u02.bin",
		.bt_ext_patch  = "aic8800d80n/fw_patch_8800d80n_u02_ext",
		.wl_fw         = "aic8800d80n/fmacfw_patch_8800d80n_u02.bin",
		.wl_table      = "aic8800d80n/fmacfw_patch_tbl_8800d80n_u02.bin",
		.wl_calib      = "aic8800d80n/fmacfw_calib_8800d80n_u02.bin",
		.wl_cinit      = "aic8800d80n/fmacfw_cinit_8800d80n_u02.bin"
	},

	[AICBSP_CPMODE_TEST] = {
		.desc          = "rf test mode(sdio u02)",
		.bt_adid       = "aic8800d80n/fw_adid_8800d80n_u02.bin",
		.bt_patch      = "aic8800d80n/fw_patch_8800d80n_rf_u02.bin",
		.bt_table      = "aic8800d80n/fw_patch_table_8800d80n_rf_u02.bin",
		.bt_ext_patch  = "aic8800d80n/fw_patch_8800d80n_rf_u02_ext",
		.wl_fw         = "aic8800d80n/lmacfw_rf_8800d80n.bin",
		.wl_cinit      = "aic8800d80n/fmacfw_cinit_8800d80n_u02.bin"
	},
};

static int aicbt_init(struct priv_dev *aicdev)
{
	int ret = 0;
	struct aicbt_patch_info_t patch_info = {
		.info_len          = 0,
		.adid_addrinf      = 0,
		.addr_adid         = 0,
		.patch_addrinf     = 0,
		.addr_patch        = 0,
		.adid_flag_addr    = 0,
		.adid_flag         = 0,
		.ext_patch_nb_addr = 0,
		.ext_patch_nb      = 0,
	};

	struct aicbt_info_t aicbt_info = {
		.btmode        = AICBT_BTMODE_BT_WIFI_COANT,
		.btport        = AICBT_BTPORT_DEFAULT,
		.uart_baud     = AICBT_UART_BAUD_DEFAULT,
		.uart_flowctrl = AICBT_UART_FC_DEFAULT,
		.lpm_enable    = AICBT_LPM_ENABLE_DEFAULT,
		.txpwr_lvl     = AICBT_TXPWR_LVL_8800D80N,
	};

	struct aicbt_patch_table *head = aicbt_patch_table_alloc(aicbsp_firmware_list[aicbsp_info.cpmode].bt_table);
	if (head == NULL) {
		printk("aicbt_patch_table_alloc fail\n");
		return -1;
	}

	aicbt_reload_config(aicbsp_firmware_list[aicbsp_info.cpmode].hw_config, &aicbt_info);

	ret = aicbt_patch_info_unpack(head, &patch_info);
	if (ret) {
		pr_warn("%s no patch info found in bt fw\n", __func__);
	}

	if (rwnx_plat_bin_fw_upload_android(aicdev, patch_info.addr_adid, aicbsp_firmware_list[aicbsp_info.cpmode].bt_adid)) {
		return -1;
		goto err;
	}
	if (rwnx_plat_bin_fw_upload_android(aicdev, patch_info.addr_patch, aicbsp_firmware_list[aicbsp_info.cpmode].bt_patch)) {
		return -1;
		goto err;
	}

	if (patch_info.ext_patch_nb) {
		printk("[0x40506004]: 0x04338000\n");
		ret = rwnx_send_dbg_mem_write_req(aicdev, 0x40506004, 0x04338000);
		printk("[0x40509000]: 0x00040000\n");
		ret = rwnx_send_dbg_mem_write_req(aicdev, 0x40509000, 0x00040000);
	}

	if (aicbt_ext_patch_data_load(aicdev, &patch_info)) {
		printk("aicbt_ext_patch_data_load fail\n");
		ret = -1;
		goto err;
	}

	if (aicbt_patch_table_load(aicdev, &aicbt_info, head)) {
		printk("aicbt_patch_table_load fail\n");
		ret = -1;
		goto err;
	}

err:
	aicbt_patch_table_free(&head);
	return ret;
}

static int aicwf_plat_patch_load_8800d80n(struct priv_dev *aicdev)
{
	int ret = 0;
	char patch_ver_str[128];
	//wifi patch
	ret = rwnx_plat_bin_fw_upload_2_with_version(aicdev,
		ROM_FMAC_PATCH_ADDR_8800D80N_U02, aicbsp_firmware_list[aicbsp_info.cpmode].wl_fw, patch_ver_str, sizeof(patch_ver_str));
	if (ret) {
		printk("load patch bin fail: %d\n", ret);
		return ret;
	}
	printk("PatchVer: %s", patch_ver_str);
	return ret;
}

int aicwf_plat_patch_table_load_8800d80n(struct priv_dev *aicdev)
{
	int err = 0;
	unsigned int i, size;
	u32 *dst = NULL;
	char *filename = (char *)aicbsp_firmware_list[aicbsp_info.cpmode].wl_table;
	//struct device *dev = aicdev->dev;

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

	if (!err) {
		for (i = 0; i < (size / 4); i += 2) {
			if ((dst[i] == 0x0) || (dst[i] == 0xFFFFFFFF)) {
				break; // end of tbl
			}
			printk("patch_tbl:  %x  %x\n", dst[i], dst[i+1]);
			err = rwnx_send_dbg_mem_write_req(aicdev, dst[i], dst[i+1]);
		}
		if (err) {
			printk("tbl bin upload fail: %x, err:%d\r\n", dst[i], err);
		}
	}

	release_firmware(fw);
	return err;
}

int rwnx_plat_bin_fw_upload_2_with_version(struct priv_dev *aicdev, u32 fw_addr,
							const char *filename, char *version_str, int version_size)
{
	int err = 0;
	unsigned int i = 0;
	int size;
	u32 *dst = NULL;

	char *bin_str = NULL;
	int char_idx = 0;
	int hdr_size = 0;
	int max_cnt = 0;
	char *tmp = NULL;

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

	printk("size=%d, dst[0]=%x\n", size, dst[0]);

	// get version if exist
	if (version_str) {
		bin_str = (char *)&dst[4];
		hdr_size = (int)(dst[3] & 0xFFF);
		if (hdr_size) {
			bin_str = (char *)&dst[0] + hdr_size;
		} else {
			max_cnt = 512;
			tmp = bin_str;
			while (max_cnt--) {
				if (((tmp[0] >= 'a') && (tmp[0] <= 'z')) &&
				((tmp[1] >= 'a') && (tmp[1] <= 'z')) &&
				(tmp[2] == ' ') ){
					bin_str = tmp;
					break;
				}
				tmp++;
			}
		}
		for (char_idx = 0; char_idx < version_size; char_idx++) {
			version_str[char_idx] = bin_str[char_idx];
			if (bin_str[char_idx] == '\0') {
				break;
			}
		}
		if (char_idx == version_size) {
			version_str[version_size - 1] = '\0';
		}
		printk("version_str=%s\n", version_str);
	}

	// upload
	if (size > 512) {
		for (i = 0; i < (size - 512); i += 512) {
			//printk("wr blk 0: %p -> %x\r\n", dst + i / 4, fw_addr + i);
			err = rwnx_send_dbg_mem_block_write_req(aicdev, fw_addr + i, 512, dst + i / 4);
			if (err) {
				printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
				break;
			}
		}
	}
	if (!err && (i < size)) {
		//printk("wr blk 1: %p -> %x\r\n", dst + i / 4, fw_addr + i);
		err = rwnx_send_dbg_mem_block_write_req(aicdev, fw_addr + i, size - i, dst + i / 4);
		if (err) {
			printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
		}
	}

	release_firmware(fw);
	return err;
}


void aicwf_patch_config_8800d80n(struct priv_dev *aicdev)
{
#if 1 //def CONFIG_ROM_PATCH_EN
	int ret = 0;
	int cnt = 0;
	const u32 cfg_base = 0x10170;
	struct dbg_mem_read_cfm cfm;
	//int i;
	u32 wifisetting_cfg_addr;
	u32 agc_cfg_addr;
	u32 txgain_cfg_24g_addr, txgain_cfg_5g_addr;
	u32 jump_tbl_addr = 0;
	u32 patch_tbl_wifisetting_num = sizeof(patch_tbl_wifisetting_8800d80n)/sizeof(u32)/2;
	u32 patch_tbl_rf_func_num = sizeof(patch_tbl_rf_func_8800d80n)/sizeof(u32)/2;

	//adap test
	int adap_patch_num = 0;
	//adap test

	if (aicbsp_info.cpmode == AICBSP_CPMODE_WORK) {
		if ((ret = rwnx_send_dbg_mem_read_req(aicdev, cfg_base, &cfm))) {
			printk("setting base[0x%x] rd fail: %d\n", cfg_base, ret);
		}
		wifisetting_cfg_addr = cfm.memdata;

		if ((ret = rwnx_send_dbg_mem_read_req(aicdev, cfg_base + 4, &cfm))) {
			printk("jump_tbl base[0x%x] rd fail: %d\n", cfg_base + 4, ret);
		}
		jump_tbl_addr = cfm.memdata;

		if ((ret = rwnx_send_dbg_mem_read_req(aicdev, cfg_base + 0x10, &cfm))) {
			printk("agc_cfg base[0x%x] rd fail: %d\n", cfg_base + 0xc, ret);
		}
		agc_cfg_addr = cfm.memdata;

		if ((ret = rwnx_send_dbg_mem_read_req(aicdev, cfg_base + 0x14, &cfm))) {
			printk("txgain_cfg_24g base[0x%x] rd fail: %d\n", cfg_base + 0x10, ret);
		}
		txgain_cfg_24g_addr = cfm.memdata;

		if ((ret = rwnx_send_dbg_mem_read_req(aicdev, cfg_base + 0x18, &cfm))) {
			printk("txgain_cfg_5g base[0x%x] rd fail: %d\n", cfg_base + 0x10, ret);
		}
		txgain_cfg_5g_addr = cfm.memdata;

		printk("wifisetting_cfg_addr=%x, jump_tbl_addr=%x, agc_cfg_addr=%x, txgain_cfg_24g_addr=%x, txgain_cfg_5g_addr=%x\n",
			wifisetting_cfg_addr, jump_tbl_addr, agc_cfg_addr, txgain_cfg_24g_addr, txgain_cfg_5g_addr);

		for (cnt = 0; cnt < patch_tbl_wifisetting_num; cnt++) {
			if ((ret = rwnx_send_dbg_mem_write_req(aicdev, wifisetting_cfg_addr + patch_tbl_wifisetting_8800d80n[cnt][0], patch_tbl_wifisetting_8800d80n[cnt][1]))) {
				printk("wifisetting %x write fail\n", patch_tbl_wifisetting_8800d80n[cnt][0]);
			}
		}

		//adap test
		if (aicbsp_info.adap_test) {
			adap_patch_num = sizeof(adaptivity_patch_tbl_8800d80n)/sizeof(u32)/2;
			for(cnt = 0; cnt < adap_patch_num; cnt++)
			{
				if((ret = rwnx_send_dbg_mem_write_req(aicdev, wifisetting_cfg_addr + adaptivity_patch_tbl_8800d80n[cnt][0], adaptivity_patch_tbl_8800d80n[cnt][1]))) {
					printk("%x write fail\n", wifisetting_cfg_addr + adaptivity_patch_tbl_8800d80n[cnt][0]);
				}
			}
		}
		//adap test
	}
	else {
		for (cnt = 0; cnt < patch_tbl_rf_func_num; cnt++) {
			if ((ret = rwnx_send_dbg_mem_write_req(aicdev, patch_tbl_rf_func_8800d80n[cnt][0], patch_tbl_rf_func_8800d80n[cnt][1]))) {
				printk("patch_tbl_rf_func %x write fail\n", patch_tbl_rf_func_8800d80n[cnt][0]);
			}
		}
	}
#endif
}

static int aicwf_plat_calib_exec_8800d80n(struct priv_dev *aicdev)
{
	int ret = 0;
	uint32_t fw_addr, boot_type;
	uint32_t patch_var_flags_addr = ROM_FMAC_PATCH_ADDR_8800D80N_U02 + 0x08;
	uint32_t patch_var_flags_val;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;

	ret = rwnx_send_dbg_mem_read_req(aicdev, patch_var_flags_addr, &rd_mem_addr_cfm);
	if (ret) {
		printk("%x rd fail: %d\n", patch_var_flags_addr, ret);
		return ret;
	}
	patch_var_flags_val = rd_mem_addr_cfm.memdata;
	if (patch_var_flags_val & (PATCH_VAR_FLAG_CALIB_BEGIN | PATCH_VAR_FLAG_CALIB_DONE)) {
		printk("calib executed, var_flags=%x\n", patch_var_flags_val);
		return ret;
	}
	ret = rwnx_plat_bin_fw_upload_android(aicdev, ROM_FMAC_CALIB_ADDR_8800D80N_U02, aicbsp_firmware_list[aicbsp_info.cpmode].wl_calib);
	if (ret) {
		printk("load calib bin fail: %d\n", ret);
		return ret;
	}
	/* fw start */
	fw_addr = ROM_FMAC_CALIB_ADDR_8800D80N_U02 + 0x0009;
	boot_type = HOST_START_APP_FNCALL;
	printk("Start app: %08x, %d\n", fw_addr, boot_type);
	ret = rwnx_send_dbg_start_app_req(aicdev, fw_addr, boot_type, NULL);
	if (ret) {
		printk("start app fail: %d\n", ret);
		return ret;
	}
	return ret;
}

static int aicwf_plat_rftest_load_8800d80n(struct priv_dev *aicdev)
{
	int ret = 0;
	ret = rwnx_plat_bin_fw_upload_android(aicdev, RAM_LMAC_FW_RF_ADDR_8800D80N, aicbsp_firmware_list[aicbsp_info.cpmode].wl_fw);
	if (ret) {
		printk("load rftest bin fail: %d\n", ret);
		return ret;
	}
	return ret;
}

static int aicwf_plat_rftest_exec_8800d80n(struct priv_dev *aicdev)
{
	int ret = 0;
	uint32_t fw_addr, boot_type;
	uint32_t rst_hdlr_addr = RAM_LMAC_FW_RF_ADDR_8800D80N + 0x04;
	uint32_t rst_hdlr_val;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;

	ret = rwnx_send_dbg_mem_read_req(aicdev, rst_hdlr_addr, &rd_mem_addr_cfm);
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
	ret = rwnx_send_dbg_start_app_req(aicdev, fw_addr, boot_type, NULL);
	if (ret) {
		printk("start app fail: %d\n", ret);
		return ret;
	}
	return ret;
}

static int aicwifi_init(struct priv_dev *aicdev)
{
	int ret = 0;
	u32 rd_addr, fw_addr;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;
	u32 boot_type;
//goto skip_wifi;
	if (aicbsp_info.cpmode == AICBSP_CPMODE_WORK) {
		ret = aicwf_plat_patch_load_8800d80n(aicdev);
		if (ret) {
			printk("patch upload fail: %d\n", ret);
			return ret;
		}
		ret = aicwf_plat_patch_table_load_8800d80n(aicdev);
		if (ret) {
			printk("patch_tbl upload fail: %d\r\n", ret);
			return ret;
		}
		aicwf_patch_config_8800d80n(aicdev);
		ret = aicwf_plat_calib_exec_8800d80n(aicdev);
		if (ret) {
			printk("plat_calib_exec fail: %d\n", ret);
			return ret;
		}
	} else if (aicbsp_info.cpmode == AICBSP_CPMODE_TEST) {
		printk("%s load rftest bin\n", __func__);
		ret = aicwf_plat_rftest_load_8800d80n(aicdev);
		if (ret) {
			printk("load rftest bin fail: %d\n", ret);
			return ret;
		}
	}
//skip_wifi:
	if (aicbsp_info.cpmode == AICBSP_CPMODE_WORK) {
		rd_addr = RAM_FMAC_FW_ADDR;
		fw_addr = RAM_FMAC_FW_ADDR;

		printk("Read FW mem: %08x\n", rd_addr);
		if (rwnx_send_dbg_mem_read_req(aicdev, rd_addr, &rd_mem_addr_cfm))
			return -1;

		printk("cfm: [%08x] = %08x\n", rd_mem_addr_cfm.memaddr, rd_mem_addr_cfm.memdata);

		boot_type = HOST_START_APP_DUMMY;

		/* fw start */
		printk("Start app: %08x, %d\n", fw_addr, boot_type);
		if (rwnx_send_dbg_start_app_req(aicdev, fw_addr, boot_type, NULL))
			return -1;
	} else {
		printk("%s exec rftest bin\n", __func__);
		ret = aicwf_plat_rftest_exec_8800d80n(aicdev);
		if (ret) {
			printk("exec rftest bin fail: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int aicbsp_system_config(struct priv_dev *aicdev)
{
	int syscfg_num = 0;
	int ret, cnt;
	array3_tbl_t p_syscfg_msk_tbl;

	syscfg_num = sizeof(syscfg_tbl_masked_8800d80n) / sizeof(u32) / 3;
	p_syscfg_msk_tbl = syscfg_tbl_masked_8800d80n;

	for (cnt = 0; cnt < syscfg_num; cnt++) {
		if (p_syscfg_msk_tbl[cnt][0] == 0x00000000) {
			break;
		}

		ret = rwnx_send_dbg_mem_mask_write_req(aicdev,
			p_syscfg_msk_tbl[cnt][0], p_syscfg_msk_tbl[cnt][1], p_syscfg_msk_tbl[cnt][2]);
		if (ret) {
			printk("%x mask write fail: %d\n", p_syscfg_msk_tbl[cnt][0], ret);
			return -1;
		}
	}
	return 0;
}

static int aicwf_plat_cinit_exec_8800d80n(struct priv_dev *aicdev)
{
	int ret = 0;
	u32 fw_addr, boot_type;
	u32 mem_addr = 0x40500184;
	u32 mem_val;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;

	ret = rwnx_send_dbg_mem_read_req(aicdev, mem_addr, &rd_mem_addr_cfm);
	if (ret) {
		printk("%x rd fail: %d\n", mem_addr, ret);
		return ret;
	}
	mem_val = rd_mem_addr_cfm.memdata;
	if (mem_val & CHIP_INFO_FLAG_CINIT_BEGIN) {
		// wait done
		while (!(mem_val & CHIP_INFO_FLAG_CINIT_DONE)) {
			printk("cinit rd chipinfo=%x\n", mem_val);
			ret = rwnx_send_dbg_mem_read_req(aicdev, mem_addr, &rd_mem_addr_cfm);
			if (ret) {
				printk("%x rd fail: %d\n", mem_addr, ret);
				return ret;
			}
			mem_val = rd_mem_addr_cfm.memdata;
		}
		printk("cinit executed, chipinfo=%x\n", mem_val);
		return ret;
	}
	ret = rwnx_plat_bin_fw_upload_android(aicdev, ROM_FMAC_CINIT_ADDR_8800D80N_U02, aicbsp_firmware_list[aicbsp_info.cpmode].wl_cinit);
	if (ret) {
		printk("load cinit bin fail: %d\n", ret);
		return ret;
	}
	/* fw start */
	fw_addr = ROM_FMAC_CINIT_ADDR_8800D80N_U02 + 0x0009;
	boot_type = HOST_START_APP_FNCALL;
	printk("Start app: %08x, %d\n", fw_addr, boot_type);
	ret = rwnx_send_dbg_start_app_req(aicdev, fw_addr, boot_type, NULL);
	if (ret) {
		printk("start app fail: %d\n", ret);
		return ret;
	}
	return ret;
}

int aicbsp_8800d80n_fw_init(struct priv_dev *aicdev)
{
	const u32 mem_addr = 0x40500000;
	struct dbg_mem_read_cfm rd_mem_addr_cfm;

	uint8_t binding_status;
	uint8_t dout[16];
	int ret = 0;
	need_binding_verify = false;

	//if (rwnx_send_dbg_mem_write_req(aicdev, 0x40500058, 0x40))
	//	return -1;
#ifdef CONFIG_SDIO_F1_FLAG
	printk("aicbsp:sdio_f1_flag %d\n",sdio_f1_flag);
	if (sdio_f1_flag) {
#endif
		sdio_claim_host(aicdev->func[0]);
		sdio_f0_writeb(aicdev->func[0], 0x0, 0xF1, &ret);
		if (ret) {
			bsp_err("set iopad delay1 fail %d\n", ret);
			sdio_release_host(aicdev->func[0]);
			return ret;
		}
		msleep(1);
		sdio_release_host(aicdev->func[0]);
#ifdef CONFIG_SDIO_F1_FLAG
	}
#endif

	if (rwnx_send_dbg_mem_read_req(aicdev, mem_addr, &rd_mem_addr_cfm))
		return -1;
	aicbsp_info.chipinfo->rev = (u8)((rd_mem_addr_cfm.memdata >> 16) & 0x3F);
	printk("aicbsp: %s, rev id: 0x%x\n", __func__, aicbsp_info.chipinfo->rev);

	if (aicbsp_info.chipinfo->rev >= CHIP_REV_ID_U02) {
		aicbsp_firmware_list = fw_u02;
	} else {
		pr_err("aicbsp: %s, unsupport chip rev id: 0x%x\n", __func__, aicbsp_info.chipinfo->rev);
		return -1;
	}

	if (aicwf_plat_cinit_exec_8800d80n(aicdev)) {
		printk("plat_cinit_exec fail: %d\n", ret);
		return ret;
	}

	if (aicbt_init(aicdev))
		return -1;

	if (aicbsp_system_config(aicdev))
		return -1;

	if (aicwifi_init(aicdev))
		return -1;

	if (need_binding_verify) {
		printk("aicbsp: crypto data %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
						binding_enc_data[0],  binding_enc_data[1],  binding_enc_data[2],  binding_enc_data[3],
						binding_enc_data[4],  binding_enc_data[5],  binding_enc_data[6],  binding_enc_data[7],
						binding_enc_data[8],  binding_enc_data[9],  binding_enc_data[10], binding_enc_data[11],
						binding_enc_data[12], binding_enc_data[13], binding_enc_data[14], binding_enc_data[15]);

		/* calculate verify data from crypto data */
		if (wcn_bind_verify_calculate_verify_data(binding_enc_data, dout)) {
			pr_err("aicbsp: %s, binding encrypt data incorrect\n", __func__);
			return -1;
		}

		printk("aicbsp: verify data %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
						dout[0],  dout[1],  dout[2],  dout[3],
						dout[4],  dout[5],  dout[6],  dout[7],
						dout[8],  dout[9],  dout[10], dout[11],
						dout[12], dout[13], dout[14], dout[15]);

		if (rwnx_send_dbg_binding_req(aicdev, dout, &binding_status)) {
			pr_err("aicbsp: %s, send binding request failn", __func__);
			return -1;
		}

		if (binding_status) {
			pr_err("aicbsp: %s, binding verify fail\n", __func__);
			return -1;
		}
	}

#if defined(CONFIG_SDIO_PWRCTRL)
	if (aicwf_sdio_writeb(aicdev->func[0], aicdev->sdio_reg.wakeup_reg, 4)) {
		bsp_err("reg:%d write failed!\n", aicdev->sdio_reg.wakeup_reg);
		return -1;
	}
#endif

	return 0;
}

#else
int aicbsp_8800d80n_fw_init(struct priv_dev *aicdev)
{
	(void)aicdev;
	return 0;
}
#endif