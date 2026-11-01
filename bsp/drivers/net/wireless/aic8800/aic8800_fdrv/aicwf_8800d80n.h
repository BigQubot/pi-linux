#ifndef _AICWF_8800D80N_H_
#define _AICWF_8800D80N_H_
#include <linux/types.h>
#include "aic_bsp_export.h"
#include "rwnx_defs.h"

#if defined(CONFIG_RF_CALIB_LATER)
#define CONFIG_SECURE_CALIB_EXEC_ENABLED    1
#else
#define CONFIG_SECURE_CALIB_EXEC_ENABLED    0
#endif

int aicwf_plat_secure_calib_exec_8800d80n(struct rwnx_hw *rwnx_hw);

int aicwf_set_rf_config_8800d80n(struct rwnx_hw *rwnx_hw, struct mm_set_rf_calib_cfm *cfm);

#ifdef AICWF_USB_SUPPORT
int aicfw_8800d80n_fw_init(struct rwnx_hw *rwnx_hw);
#endif
#endif
