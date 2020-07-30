/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of The Linux Foundation nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <app.h>
#include <debug.h>
#include <arch/arm.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <kernel/thread.h>
#include <arch/ops.h>

#include <dev/flash.h>
#include <dev/flash-ubi.h>
#include <lib/ptable.h>
#include <dev/keys.h>
#include <dev/fbcon.h>
#include <baseband.h>
#include <target.h>
#include <mmc.h>
#include <partition_parser.h>
#include <platform.h>
#include <crypto_hash.h>
#include <malloc.h>
#include <boot_stats.h>
#include <sha.h>
#include <platform/iomap.h>
#include <boot_device.h>
#include <boot_verifier.h>
#include <image_verify.h>
#include <decompress.h>
#include <platform/timer.h>
#if USE_RPMB_FOR_DEVINFO
#include <rpmb.h>
#endif

#if ENABLE_WBC
#include <pm_app_smbchg.h>
#endif

#if DEVICE_TREE
#include <libfdt.h>
#include <dev_tree.h>
#endif

#if WDOG_SUPPORT
#include <wdog.h>
#endif

#include <reboot.h>
#include "image_verify.h"
#include "recovery.h"
#include "bootimg.h"
#include "fastboot.h"
#include "sparse_format.h"
#include "meta_format.h"
#include "mmc.h"
#include "devinfo.h"
#include "board.h"
#include "scm.h"
#include "mdtp.h"
#include "fastboot_test.h"
#include "qpic_nand.h"
#include "fotainfo.h"


#define QUECTEL_ALL_RESTORE // quectel add


extern  bool target_use_signed_kernel(void);
extern void platform_uninit(void);
extern void target_uninit(void);
extern int get_target_boot_params(const char *cmdline, const char *part,
				  char **buf);

void *info_buf;
void write_device_info_mmc(device_info *dev);
void write_device_info_flash(device_info *dev);
static int aboot_save_boot_hash_mmc(uint32_t image_addr, uint32_t image_size);
static int aboot_frp_unlock(char *pname, void *data, unsigned sz);

/* fastboot command function pointer */
typedef void (*fastboot_cmd_fn) (const char *, void *, unsigned);

struct fastboot_cmd_desc {
	char * name;
	fastboot_cmd_fn cb;
};

#define EXPAND(NAME) #NAME
#define TARGET(NAME) EXPAND(NAME)

#ifdef MEMBASE
#define EMMC_BOOT_IMG_HEADER_ADDR (0xFF000+(MEMBASE))
#else
#define EMMC_BOOT_IMG_HEADER_ADDR 0xFF000
#endif

#ifndef MEMSIZE
#define MEMSIZE 1024*1024
#endif

#define MAX_TAGS_SIZE   1024

/* make 4096 as default size to ensure EFS,EXT4's erasing */
#define DEFAULT_ERASE_SIZE  4096
#define MAX_PANEL_BUF_SIZE 128

#define DISPLAY_DEFAULT_PREFIX "mdss_mdp"
#define BOOT_DEV_MAX_LEN  64

#define IS_ARM64(ptr) (ptr->magic_64 == KERNEL64_HDR_MAGIC) ? true : false

#define ADD_OF(a, b) (UINT_MAX - b > a) ? (a + b) : UINT_MAX

#if USE_BOOTDEV_CMDLINE
static const char *emmc_cmdline = " androidboot.bootdevice=";
#else
static const char *emmc_cmdline = " androidboot.emmc=true";
#endif
static const char *usb_sn_cmdline = " androidboot.serialno=";
static const char *androidboot_mode = " androidboot.mode=";
static const char *alarmboot_cmdline = " androidboot.alarmboot=true";
static const char *loglevel         = " quiet";
static const char *battchg_pause = " androidboot.mode=charger";
static const char *auth_kernel = " androidboot.authorized_kernel=true";
static const char *secondary_gpt_enable = " gpt";
static const char *mdtp_activated_flag = " mdtp";

static const char *baseband_apq     = " androidboot.baseband=apq";
static const char *baseband_msm     = " androidboot.baseband=msm";
static const char *baseband_csfb    = " androidboot.baseband=csfb";
static const char *baseband_svlte2a = " androidboot.baseband=svlte2a";
static const char *baseband_mdm     = " androidboot.baseband=mdm";
static const char *baseband_mdm2    = " androidboot.baseband=mdm2";
static const char *baseband_sglte   = " androidboot.baseband=sglte";
static const char *baseband_dsda    = " androidboot.baseband=dsda";
static const char *baseband_dsda2   = " androidboot.baseband=dsda2";
static const char *baseband_sglte2  = " androidboot.baseband=sglte2";
static const char *warmboot_cmdline = " qpnp-power-on.warm_boot=1";

//add by len[for quectel cmdline] 2018-1-18
#ifndef QUECTEL_FLASH_SUPPORT_1G
static char *quectel_cmdline = " recovery=0";
#endif
//add end

static unsigned page_size = 0;
static unsigned page_mask = 0;
static char ffbm_mode_string[FFBM_MODE_BUF_SIZE];
static bool boot_into_ffbm;
static char *target_boot_params = NULL;
static bool boot_reason_alarm;
static bool devinfo_present = true;
bool boot_into_fastboot = false;

/* Assuming unauthorized kernel image by default */
static int auth_kernel_img = 0;

static device_info device = {DEVICE_MAGIC, 0, 0, 0, 0, {0}, {0},{0}};
static bool is_allow_unlock = 0;

static char frp_ptns[2][8] = {"config","frp"};

struct atag_ptbl_entry
{
	char name[16];
	unsigned offset;
	unsigned size;
	unsigned flags;
};

/*
 * Partition info, required to be published
 * for fastboot
 */
struct getvar_partition_info {
	const char part_name[MAX_GPT_NAME_SIZE]; /* Partition name */
	char getvar_size[MAX_GET_VAR_NAME_SIZE]; /* fastboot get var name for size */
	char getvar_type[MAX_GET_VAR_NAME_SIZE]; /* fastboot get var name for type */
	char size_response[MAX_RSP_SIZE];        /* fastboot response for size */
	char type_response[MAX_RSP_SIZE];        /* fastboot response for type */
};

/*
 * Right now, we are publishing the info for only
 * three partitions
 */
struct getvar_partition_info part_info[] =
{
	{ "system"  , "partition-size:", "partition-type:", "", "ext4" },
	{ "userdata", "partition-size:", "partition-type:", "", "ext4" },
	{ "cache"   , "partition-size:", "partition-type:", "", "ext4" },
};

char max_download_size[MAX_RSP_SIZE];
char charger_screen_enabled[MAX_RSP_SIZE];
char sn_buf[13];
char display_panel_buf[MAX_PANEL_BUF_SIZE];
char panel_display_mode[MAX_RSP_SIZE];

extern int emmc_recovery_init(void);

#if NO_KEYPAD_DRIVER
extern int fastboot_trigger(void);
#endif

#define	QUECTEL_FASTBOOT	1  // quectel deine
static char temp_buf[4096];  // quectel add

#if QUECTEL_FASTBOOT
/* author:	ramos
 * date:	2017/02/28
 * purpose:	fastboot enter flag set , clean and get
 */

static const int MISC_FASTBOOT_COMMAND_BLOCK=1; //1block

struct fastboot_message {
	char command[32];
	char status[32];
};

int set_fastboot_message(const struct fastboot_message *in)
{
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned offset = 0;
	unsigned pagesize = flash_page_size();
	uint32_t blocksize = flash_block_size();
//	char buffer[4096];

	ptable = flash_get_ptable();

	if (ptable == NULL) {
		dprintf(CRITICAL, "ERROR: Partition table not found\n");
		return -1;
	}
	ptn = ptable_find(ptable, "misc");

	if (ptn == NULL) {
		dprintf(CRITICAL, "ERROR: No misc partition found\n");
		return -1;
	}

	offset += MISC_FASTBOOT_COMMAND_BLOCK*blocksize; // Ϣڵһblockĵһpage
	memset((void *)temp_buf,0,pagesize);
	memcpy((void *) temp_buf, in, sizeof(*in));
	dprintf(CRITICAL, "[Ramos] set fastboot message start !!!\n");
	if (Quectel_flash_write(ptn,offset, 0, temp_buf, sizeof(temp_buf))) 
	{
		dprintf(CRITICAL, "ERROR: FASTBOOT flash write fail!\n");
		return -1;
	}
	dprintf(CRITICAL, "[Ramos] set fastboot message end !!!\n");
	return 0;
}

int get_fastboot_message(const struct fastboot_message *out)
{
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned offset = 0;
	unsigned pagesize = flash_page_size();
	uint32_t blocksize = flash_block_size();
//	char buffer[4096];

	ptable = flash_get_ptable();

	if (ptable == NULL) {
		dprintf(CRITICAL, "ERROR: Partition table not found\n");
		return -1;
	}
	ptn = ptable_find(ptable, "misc");

	if (ptn == NULL) {
		dprintf(CRITICAL, "ERROR: No misc partition found\n");
		return -1;
	}

	offset += MISC_FASTBOOT_COMMAND_BLOCK*blocksize; // Ϣڵһblockĵһpage
	memset((void *)temp_buf,0,pagesize);
	dprintf(CRITICAL, "[Ramos] get fastboot message start !!!\n");
	if (flash_read(ptn,offset,temp_buf, sizeof(temp_buf))) 
	{
		dprintf(CRITICAL, "ERROR: FASTBOOT flash write fail!\n");
		return -1;
	}
	dprintf(CRITICAL, "[Ramos] get fastboot message end !!!\n");
	memcpy(out, temp_buf, sizeof(*out));
	return 0;
}

int quectel_fastboot_force_entry_flag_set(void)
{
	struct fastboot_message msg;

	strlcpy(msg.command, "boot_fastboot", sizeof(msg.command));	
	strlcpy(msg.status, "force", sizeof(msg.status));
	if(0 != set_fastboot_message(&msg))
	{
		return -1;
	}	
	return 0 ;
}
int quectel_fastboot_force_entry_flag_clean(void)
{
	struct fastboot_message msg;

    memset(&msg, 0, sizeof(msg));
	if(0 != set_fastboot_message(&msg))
	{
		return -1;
	}
	return 0 ;
}

int quectel_is_fastboot_entry_force(void)
{
	struct fastboot_message msg;

	if(0 == get_fastboot_message(&msg))
	{
		if((!strcmp(msg.command, "boot_fastboot")) && (!strcmp(msg.status, "force")))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
	return 0;
}
#endif


#if 1 //  QUECTEL_SYSTEM_BACKUP modem backup  use a individual partition for modem restore.

#define DATA_CACHE_LEN          (4200) // page 4096
#define CEFS_FILE_MAGIC1        (0x51D24368)
#define CEFS_FILE_MAGIC2        (0x4378AC6E)

#define QUEC_BACKUP_MAGIC1        (0x78E5D4C2)
#define QUEC_BACKUP_MAGIC2        (0x54F7D60E)
#define QUEC_BACKUP_INFO_BLOCK_NUMS (3)  // the 3 block used for save restore flag 
//the "sys_rev" partition last CEFS_BACKUP_INFO_BLOCK_NUMS blocks for save restroe flag
#ifdef QUECTEL_ALL_RESTORE
#define QUEC_ALL_RESTORE_FLAG_BLOCK_INDEX (6) // the 3 block reserved for All parition restoring flag
//the "sys_rev" partition last QUEC_ALL_RESTORE_FLAG_BLOCK_INDEX to CEFS_BACKUP_INFO_BLOCK_NUMS reserved for this flag store.
#endif

#define BACKUP_SYSTEM_SIZE  (50*1024*1024)
#define BACKUP_EFS2_SIZE (3*1024*1024) // 3M for cefs backup is enough
#define BACKUP_MODEM_SIZE (50*1024*1024)//  modem ubi backup 50M, 
#define BACKUP_RECOVERYFS_SIZE (13*1024*1024)
#define BACKUP_IMAGE_SIZE (8*1024*1024) // boot img backup size
static char temp_buf[4096];
#define BACKUP_INFO_BLOCK_NUMS (3)

typedef enum
{
	QUECTEL_RESTOREFLAG_NONE = 0,
	QUECTEL_RESTOREFLAG_LINUXFS,
	QUECTEL_RESTOREFLAG_CEFS,
	QUECTEL_RESTOREFLAG_MODEM,
	QUECTEL_RESTOREFLAG_IMAGE,
	QUECTEL_RESTOREFLAG_RECOVERYFS,
#ifdef QUECTEL_ALL_RESTORE
	QUECTEL_ALL_RESTORE_BEGIN,
#endif

} quectel_RestoreFlg_type;

typedef struct 
{
  uint32 magic1;  
  uint32 magic2;
  uint32 page_count;
  uint32 data_crc;
  
  uint32 reserve1;  
  uint32 reserve2;
  uint32 reserve3;
  uint32 reserve4;
} quec_cefs_file_header_type;

typedef struct
{
  uint32_t magic1;  
  uint32_t magic2;

  uint32_t cefs_restore_flag;
  uint32_t cefs_restore_times;
  uint32_t cefs_backup_times;
  uint32_t cefs_crash[10];  // cefs crash where
   
  // Ramos add for linux fs backup restore times
    uint32_t linuxfs_restore_flag;
    int32_t linuxfs_restore_times;
    uint32_t linuxfs_backup_times;
    uint32_t linuxfs_crash[10];  //  linux fs crash where
    
    // modem backup restore flag
    uint32_t modem_restore_flag;
    uint32_t modem_restore_times;
    uint32_t modem_backup_times;
    uint32_t modem_crash[10];  // modem crash where

    //  recovery restore flag
    uint32_t recovery_restore_flag;
    uint32_t recovery_restore_times;
    uint32_t recovery_backup_times;
    uint32_t recovery_crash[10]; 

    // other image restore flag
    uint32_t image_restoring_flag;
    uint32_t reserved1;
    uint32_t reserved2;
    
} quec_backup_info_type;
extern uint32 Q_crc_32_calc(  uint8  *buf_ptr,  uint16  len,  uint32  seed);
quec_backup_info_type g_QuecBackupInfo = {0};

#ifdef QUECTEL_ALL_RESTORE
uint32_t g_AllRestoring_flag =0 ;
quectel_RestoreFlg_type g_Restore_stage = QUECTEL_RESTOREFLAG_NONE;

typedef struct
{
	uint32_t All_Restoring_flag;
	uint32_t fota_upgradedFlag;  // module have upgraded fota flag 1: haved done fota , 2: done fota and need to update recovery img partition.
	uint32_t fota_updateRecoveryImgFlag;// after fota, we need update recovery img.
} quec_All_RestoringInfo;

//this function set flag, which record the module is in All(system,modem,boot) restore stage 
int Ql_Set_AllRestoring_Flag(int flag)
{
	struct ptentry *ptn;
	struct ptable *ptable;
	uint32_t offset = 0;
	uint32_t pagesize = flash_page_size();
	uint32_t blocksize = flash_block_size();
	quec_All_RestoringInfo AllRestoringflag;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		dprintf(CRITICAL, "@Ramos ERROR: Ql_Set_AllRestoring_Flag Partition table not found\n");
		return -1;
	}
	ptn = ptable_find(ptable, "sys_rev");
	if (ptn == NULL) {
		dprintf(CRITICAL, "@Ramos ERROR: Ql_Set_AllRestoring_Flag No misc partition found\n");
		return -1;
	}

	memset((void *)temp_buf , 0x00, pagesize);
	memset((void *)&AllRestoringflag,0x00, sizeof(AllRestoringflag));
	AllRestoringflag.All_Restoring_flag =flag ; //set all partition restoring flag.
	memcpy(temp_buf,&AllRestoringflag,sizeof(AllRestoringflag));
	
	// the offset if the "sys_rev" partition last QUEC_ALL_RESTORE_FLAG_BLOCK_INDEX
	offset = (ptn->length - QUEC_ALL_RESTORE_FLAG_BLOCK_INDEX)*blocksize;
	if(Quectel_flash_write(ptn,offset, 0, temp_buf, pagesize))
	{
		dprintf(CRITICAL, "@Ramos ERROR: set all partition restoring flag fail!\n");
		return -1;
	}
	g_AllRestoring_flag =flag;
	dprintf(CRITICAL, "@Ramos set AllRestoringFlag=%d \n",flag);
	if( 0 == flag)
	{
		/*0 == flag, clear the AllRestoring flag, 
AllRestore success, we reboot the system */
		mdelay(1000);
		reboot_device(0);
	}

	return 0;
}


//this function, check if the module is in all restore stage, even if the power is shutdown, and power on.
/* error return -1 */
/* clean return 0  */
/* set   return 1  */
int Ql_check_AllRestoring_Flag()
{
	struct ptentry *ptn;
	struct ptable *ptable;
	uint32_t offset = 0;
	uint32_t pagesize = flash_page_size();
	uint32_t blocksize = flash_block_size();
	quec_All_RestoringInfo AllRestoringflag;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		dprintf(CRITICAL, "@Ramos ERROR: Ql_Set_AllRestoring_Flag Partition table not found\n");
		return -1;
	}
	ptn = ptable_find(ptable, "sys_rev");
	if (ptn == NULL) {
		dprintf(CRITICAL, "@Ramos ERROR: Ql_Set_AllRestoring_Flag No misc partition found\n");
		return -1;
	}

	offset = (ptn->length - QUEC_ALL_RESTORE_FLAG_BLOCK_INDEX)*blocksize;  // offset, read restore information flag 
	memset((void *)temp_buf , 0x00, pagesize);
	if (Quectel_flash_nand_read(ptn, 0,offset, (void *) temp_buf, pagesize)) 
	{
		dprintf(CRITICAL, "@Ramos ERROR: Cannot read AllRestoring Flag!! read failed\n");
		return -1;
	}

	memset((void *)&AllRestoringflag,0x00,sizeof(AllRestoringflag));
	memcpy(&AllRestoringflag,temp_buf,sizeof(AllRestoringflag));
	
	dprintf(CRITICAL, "@Ramos check AllRestoring flag =%d,fota_updateRecoveryImgFlag=%d\n",\
		AllRestoringflag.All_Restoring_flag,AllRestoringflag.fota_updateRecoveryImgFlag);
	if( 1 == AllRestoringflag.fota_updateRecoveryImgFlag )
	{
	
		dprintf(CRITICAL,"@Ramos update recovery img\n");
		Ql_Restore_partition("boot", "recovery");
		AllRestoringflag.fota_updateRecoveryImgFlag = 0;
		// clear the recovery img update flag
		memcpy(temp_buf,&AllRestoringflag,sizeof(AllRestoringflag));
		offset = (ptn->length - QUEC_ALL_RESTORE_FLAG_BLOCK_INDEX)*blocksize; // offset ,write restore information 
		if (Quectel_flash_write(ptn,offset, 0, temp_buf, pagesize))
		{
			dprintf(CRITICAL, "@Ramos clear update recovery img flag flash write fail!\n");
			return -1;
		}
	}
	g_AllRestoring_flag = AllRestoringflag.All_Restoring_flag;
	if( 1 == AllRestoringflag.All_Restoring_flag)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


// this functino, set the system ,modem ,boot,recoveryfs restore flag in bootloader
int Ql_Set_Restore_Flags(uint32_t systemFlag,uint32_t modemFlag,uint32_t bootFlag,uint32_t recoveryfsFlag)
{
	struct ptentry *ptn;
	struct ptable *ptable;
	uint32_t offset = 0;
	uint32_t pagesize = flash_page_size();
	uint32_t blocksize = flash_block_size();
	quec_backup_info_type QuecBackupInfo;
	  //  dprintf(INFO,"@Ramos Ql_Set_Restore_Flags restore flag message\r\n");
	ptable = flash_get_ptable();
	if (ptable == NULL) {
		dprintf(CRITICAL, "@Ramos ERROR:Ql_Set_Restore_Flags Partition table not found\n");
		return -1;
	}
	ptn = ptable_find(ptable, "sys_rev");

	if (ptn == NULL) {
		dprintf(CRITICAL, "@Ramos ERROR:Ql_Set_Restore_Flags No misc partition found\n");
		return -1;
	}

	offset = (ptn->length - QUEC_BACKUP_INFO_BLOCK_NUMS)*blocksize; // offset, read restore information flag 
	memset((void *)temp_buf , 0x00, pagesize);
	if (Quectel_flash_nand_read(ptn, 0,offset, (void *) temp_buf, pagesize)) 
	{
		dprintf(CRITICAL, "@Ramos ERROR:Ql_Set_Restore_Flags Cannot read Restore Flag header\n");
		return -1;
	}
	memcpy(&QuecBackupInfo,temp_buf,sizeof(QuecBackupInfo));
	if((QUEC_BACKUP_MAGIC1 != QuecBackupInfo.magic1) ||(QUEC_BACKUP_MAGIC2 != QuecBackupInfo.magic2))
	{
		QuecBackupInfo.magic1 = QUEC_BACKUP_MAGIC1;
		QuecBackupInfo.magic2 = QUEC_BACKUP_MAGIC2;
	}

	QuecBackupInfo.linuxfs_restore_flag=systemFlag;
	QuecBackupInfo.modem_restore_flag=modemFlag;
	QuecBackupInfo.image_restoring_flag=bootFlag;
	QuecBackupInfo.recovery_restore_flag=recoveryfsFlag;
	
	memcpy(temp_buf,&QuecBackupInfo,sizeof(QuecBackupInfo));
	offset = (ptn->length - QUEC_BACKUP_INFO_BLOCK_NUMS)*blocksize; // offset ,write restore information 
	if (Quectel_flash_write(ptn,offset, 0, temp_buf, pagesize))
	{
		dprintf(CRITICAL, "@Ramos ERROR: Ql_Set_Restore_Flags flash write fail!\n");
		return -1;
	}
	dprintf(INFO,"Ql_Set_Restore_Flags system=%d,modem=%d images=%d, recoveryfs=%d restore flag\n",systemFlag,modemFlag,bootFlag,recoveryfsFlag);
	mdelay(200);
	reboot_device(0);
	return 0;
}
#endif

int Ql_SetRestorecountClearFlag( quectel_RestoreFlg_type flag )
{
	struct ptentry *ptn;
	struct ptable *ptable;
	uint32_t offset = 0;
	uint32_t pagesize = flash_page_size();
	uint32_t blocksize = flash_block_size();
		quec_backup_info_type QuecBackupInfo;
	  //  dprintf(INFO,"@Ramos clean restore flag message\r\n");
	ptable = flash_get_ptable();
	if (ptable == NULL) {
		dprintf(CRITICAL, "@Ramos ERROR: Partition table not found\n");
		return -1;
	}
	ptn = ptable_find(ptable, "sys_rev");

	if (ptn == NULL) {
		dprintf(CRITICAL, "@Ramos ERROR: No misc partition found\n");
		return -1;
	}

	offset = (ptn->length - QUEC_BACKUP_INFO_BLOCK_NUMS)*blocksize; // offset, read restore information flag 
	memset((void *)temp_buf , 0x00, pagesize);
	if (Quectel_flash_nand_read(ptn, 0,offset, (void *) temp_buf, pagesize)) 
	{
		dprintf(CRITICAL, "@Ramos ERROR: Cannot read Restore Flag header\n");
		return -1;
	}
	memcpy(&QuecBackupInfo,temp_buf,sizeof(QuecBackupInfo));

	if(QUECTEL_RESTOREFLAG_LINUXFS == flag)
	{
		QuecBackupInfo.linuxfs_restore_times++;
		dprintf(INFO,"\r\n\r\n\r\nlinux system restore times=%d!!!\r\n\r\n\r\n",QuecBackupInfo.linuxfs_restore_times);
		QuecBackupInfo.linuxfs_restore_flag=0;//erase the restore flag
	}
	else if(QUECTEL_RESTOREFLAG_CEFS == flag)
	{
		QuecBackupInfo.cefs_restore_times++;
		dprintf(INFO,"\r\n\r\n\r\n CEFS restore times=%d!!!\r\n\r\n\r\n",QuecBackupInfo.cefs_restore_times);
		QuecBackupInfo.cefs_restore_flag=0;//erase the restore flag
	}
	else if(QUECTEL_RESTOREFLAG_MODEM == flag)
	{
		QuecBackupInfo.modem_restore_times++;
		dprintf(INFO,"\r\n\r\n\r\n modem restore times=%d!!!\r\n\r\n\r\n",QuecBackupInfo.modem_restore_times);
		QuecBackupInfo.modem_restore_flag=0;//erase the restore flag
	}
	else if(QUECTEL_RESTOREFLAG_RECOVERYFS == flag)
	{
		QuecBackupInfo.recovery_restore_times++;
		dprintf(INFO,"\r\n\r\n\r\n recovery restore times=%d!!!\r\n\r\n\r\n",QuecBackupInfo.recovery_restore_times);
		QuecBackupInfo.recovery_restore_flag=0;//erase the restore flag
	}
	else if(QUECTEL_RESTOREFLAG_IMAGE == flag)
	{
		dprintf(INFO,"\r\n\r\n\r\n boot image restore\r\n\r\n\r\n");
		QuecBackupInfo.image_restoring_flag=0;//erase the restore flag
	}
	memcpy(temp_buf,&QuecBackupInfo,sizeof(QuecBackupInfo));
	offset = (ptn->length - QUEC_BACKUP_INFO_BLOCK_NUMS)*blocksize; // offset ,write restore information 
	if (Quectel_flash_write(ptn,offset, 0, temp_buf, pagesize))
	{
		dprintf(CRITICAL, "@Ramos ERROR: flash write fail!\n");
		return -1;
	}
/*
    memset((void *)temp_buf , 0x00, pagesize);
    if (flash_read(ptn, offset, (void *) temp_buf, pagesize)) 
    {
        dprintf(CRITICAL, "@Ramos ERROR: Cannot read Restore Flag header\n");
        return -1;
    }
*/
    return 0;
}


quectel_RestoreFlg_type Ql_check_RestoreFlag(void )
{
    quec_backup_info_type QuecBackupInfo;
    struct ptentry *ptn;
    struct ptable *ptable;
    uint32_t offset = 0;
    uint32_t pagesize = flash_page_size();
    uint32_t blocksize = flash_block_size();
        int result = 0;
    
    ptable = flash_get_ptable();
    if (ptable == NULL) {
        dprintf(CRITICAL, "@Ramos ERROR: Partition table not found\n");
        return QUECTEL_RESTOREFLAG_NONE;
    }
    ptn = ptable_find(ptable, "sys_rev");
    if (ptn == NULL) {
        dprintf(CRITICAL, "@Ramos  ERROR: No misc partition found\n");
        return QUECTEL_RESTOREFLAG_NONE;
    }

    offset = (ptn->length - QUEC_BACKUP_INFO_BLOCK_NUMS)*blocksize;  // offset, read restore information
    memset((void *)temp_buf , 00, pagesize);
    result =  Quectel_flash_nand_read(ptn, 0,offset, (void *) temp_buf, pagesize);
    if (NANDC_RESULT_BAD_PAGE == result) 
        {
			/* NANDC_RESULT_BAD_PAGE == result	that means the block unsteadiness, In the Quectel_flash_nand_read function we have erased this block
			but FAE of naya think maby the block have bitfilp also, now we write it to aviod this issue
			*/
			QuecBackupInfo.magic1 = CEFS_FILE_MAGIC1;
			QuecBackupInfo.magic2 = CEFS_FILE_MAGIC2;
			QuecBackupInfo.linuxfs_restore_flag = 0;
			QuecBackupInfo.cefs_restore_flag = 0;
			QuecBackupInfo.modem_restore_flag = 0;
			QuecBackupInfo.recovery_restore_flag = 0;
			memset((void *)temp_buf , 0xFF, pagesize);
			memcpy(temp_buf,&QuecBackupInfo,sizeof(QuecBackupInfo));
			Quectel_flash_write(ptn,offset, 0, temp_buf, pagesize);
			dprintf(INFO, "@Ramos the system restore flag block error!!!!!\n");
			// reboot system 
			mdelay(200);
			reboot_device(0);
	}
		else if(NANDC_RESULT_SUCCESS !=result)
		{
			dprintf(CRITICAL, "@Ramos ERROR: Cannot read Restore Flag header\n");
			g_Restore_stage = QUECTEL_RESTOREFLAG_NONE;
			return QUECTEL_RESTOREFLAG_NONE;
		}
	memcpy((void *)&QuecBackupInfo, temp_buf, sizeof(quec_backup_info_type));
	memcpy((void *)&g_QuecBackupInfo, temp_buf, sizeof(quec_backup_info_type));

	dprintf(CRITICAL, "@Ramos Ql_check_RestoreFlag:offset=%x, magic1=%x,magic2=%x,linuxfs_restoreFlag=%d, cefs_restoreFlag=%d, modem_restoreFlag=%d,recoveryfs_restoreFlag=%d image_restoring_flag=%d \n", \
		offset,QuecBackupInfo.magic1,QuecBackupInfo.magic2,QuecBackupInfo.linuxfs_restore_flag,QuecBackupInfo.cefs_restore_flag,QuecBackupInfo.modem_restore_flag, QuecBackupInfo.recovery_restore_flag,QuecBackupInfo.image_restoring_flag );
	dprintf(CRITICAL, "@Ramos linuxfs_restore_times=%d, cefs_restore_times=%d, modem_restore_times=%d\n", \
		QuecBackupInfo.linuxfs_restore_times,QuecBackupInfo.cefs_restore_times,QuecBackupInfo.modem_restore_times);

	if((QUEC_BACKUP_MAGIC1 != QuecBackupInfo.magic1) ||(QUEC_BACKUP_MAGIC2 != QuecBackupInfo.magic2))
	{
		g_Restore_stage = QUECTEL_RESTOREFLAG_NONE;
		return QUECTEL_RESTOREFLAG_NONE;
	}

#ifdef QUECTEL_ALL_RESTORE
	if( (1 == QuecBackupInfo.linuxfs_restore_flag) && (1 == QuecBackupInfo.image_restoring_flag) && (1 == QuecBackupInfo.modem_restore_flag))
	{
		g_Restore_stage = QUECTEL_ALL_RESTORE_BEGIN;
		return QUECTEL_ALL_RESTORE_BEGIN;
	}
#endif

	if(1 == QuecBackupInfo.linuxfs_restore_flag)
	{
		g_Restore_stage = QUECTEL_RESTOREFLAG_LINUXFS;
		return QUECTEL_RESTOREFLAG_LINUXFS;
	}
	if(1 == QuecBackupInfo.cefs_restore_flag)
	{
		g_Restore_stage = QUECTEL_RESTOREFLAG_CEFS;
		return QUECTEL_RESTOREFLAG_CEFS;
	}
	if(1 == QuecBackupInfo.modem_restore_flag)
	{
		g_Restore_stage = QUECTEL_RESTOREFLAG_MODEM;
		return QUECTEL_RESTOREFLAG_MODEM;
	}
	if(1 == QuecBackupInfo.recovery_restore_flag)
	{
		g_Restore_stage = QUECTEL_RESTOREFLAG_RECOVERYFS;
		return QUECTEL_RESTOREFLAG_RECOVERYFS;
	}
	if(1 == QuecBackupInfo.image_restoring_flag)
	{
		g_Restore_stage = QUECTEL_RESTOREFLAG_IMAGE;
		return QUECTEL_RESTOREFLAG_IMAGE;
	}
	g_Restore_stage = QUECTEL_RESTOREFLAG_NONE;
	return QUECTEL_RESTOREFLAG_NONE;
}

/**
 * Author : Darren
 * Date : 2017/8/1
 * get_ubiimg_size -- the function can accurately get ubi 
 * image.the flash bad block unaffect this fuction.
 * 0 -- get ubi image size failed
 * positive -- get ubi image size success
 */
static int32_t get_ubiimg_size(struct ptentry *ptn)
{
    uint32_t offset = 0;
    uint32_t image_size_in_byte = 0;
    void *pagebuf = NULL;
    uint32_t pagesize  = flash_page_size();
    uint32_t blocksize = flash_block_size();
    if (ptn == NULL)
	{
        dprintf(INFO,"%s:invalid parameter ptn\n", __func__);
		return 0;
	}
    pagebuf = malloc(pagesize);
    ASSERT(pagebuf);
	memset(pagebuf, 0, pagesize);
    dprintf(INFO,"get \"%s\" partition ubi image size!\n",ptn->name);
    do
    {
        flash_read(ptn, offset, pagebuf, pagesize);
        if (!memcmp((void *)pagebuf, UBI_MAGIC, UBI_MAGIC_SIZE))
        {
            offset += blocksize;
	        memset(pagebuf, 0, pagesize);
            continue;
        }
        image_size_in_byte = offset;
        break;
    }while(1);
    dprintf(INFO,"%s,pagesize = %d KiB blocksize = %d KiB, img_size = %d KiB\n", __func__, \
            pagesize/1024, blocksize/1024, image_size_in_byte/1024);
    free(pagebuf);
    return image_size_in_byte;
}

bool  Ql_Restore_partition(const char *source_ptn, const char *dest_ptn)
{
    struct ptentry *ptn;
    struct ptable *ptable;
    unsigned extra = 0;
    char *data = (char *)0x89000000;
    uint32_t sz=0;
    uint32_t  datalen=0;
    quectel_RestoreFlg_type Restore_flag = QUECTEL_RESTOREFLAG_NONE;
    uint32_t pagesize = flash_page_size();
    
    ptable = flash_get_ptable();
    if (ptable == NULL) 
    {
        fastboot_fail("partition table doesn't exist");
        return FALSE;
    }
   //read restroe data form the  backup  partition 
    ptn = ptable_find(ptable, source_ptn);
    if (ptn == NULL) 
    {
        dprintf(INFO, "unknown partition name (%s). Trying updatevol\n", source_ptn);    
        return FALSE;
    }

    dprintf(INFO,"@Ramos Get backup partition:%s, size=%d\r\n", ptn->name, ptn->length);

	if(!strcmp(dest_ptn, "system"))
	{
		//datalen = BACKUP_SYSTEM_SIZE;   // read enough vaild data form the backup partition,
		Restore_flag = QUECTEL_RESTOREFLAG_LINUXFS;
		datalen = get_ubiimg_size(ptn);
		if (datalen == 0)
			datalen = BACKUP_SYSTEM_SIZE;
	}
	else if(!strcmp(dest_ptn, "efs2")) // read enough vaild data form the backup partition, 
	{
		datalen = BACKUP_EFS2_SIZE;  
		Restore_flag = QUECTEL_RESTOREFLAG_CEFS;
	}
	else if(!strcmp(dest_ptn, "modem"))// read enough vaild data form the backup partition, 
	{
		//datalen = BACKUP_MODEM_SIZE; 
		Restore_flag = QUECTEL_RESTOREFLAG_MODEM;
		datalen = get_ubiimg_size(ptn);
		if (datalen == 0)
			datalen = BACKUP_MODEM_SIZE;
	}
	else if(!strcmp(dest_ptn, "recoveryfs"))	
	{
		//datalen = BACKUP_RECOVERYFS_SIZE;
		Restore_flag = QUECTEL_RESTOREFLAG_RECOVERYFS;
		datalen = get_ubiimg_size(ptn);
		if (datalen == 0)
		datalen = BACKUP_RECOVERYFS_SIZE;
		
	}
	else if (!strcmp(dest_ptn, "boot")) 
	{
		datalen = BACKUP_IMAGE_SIZE;
		Restore_flag = QUECTEL_RESTOREFLAG_IMAGE;
	}
	else if (!strcmp(dest_ptn, "recovery")) // recovery update
	{
		datalen = BACKUP_IMAGE_SIZE;
		Restore_flag = QUECTEL_RESTOREFLAG_NONE;
		// fota upgrade then when system booton we update recovery img here. the flag no in quec_backup_info_type struct
	}
	
	sz = datalen;
		data = (char*)VA((addr_t)data);
		flash_read(ptn, 0,	data, datalen );

   	// find the restroe partition
    ptn = ptable_find(ptable, dest_ptn);
    if (ptn == NULL) 
    {
        dprintf(INFO, "@Ramos unknown partition name (%s).\n", dest_ptn);    
        return FALSE;
    }
    dprintf(INFO,"@Ramos Restroe  partition:%s, size=%d\r\n", ptn->name, ptn->length);    

    if (!strcmp(ptn->name, "boot") || !strcmp(ptn->name, "recovery")) 
    {
        if (memcmp((void *)data, BOOT_MAGIC, BOOT_MAGIC_SIZE)) 
        {
            dprintf(INFO,"@Ramos image is not a boot image");
            return FALSE;
        }
    }
            
    if(!strcmp(dest_ptn, "efs2"))
    {
        // sys_rev first page is use save the cefs backup information ,header and crc value
        quec_cefs_file_header_type BackupCefs_Info;
        uint32_t crc = 0;
        uint32_t i =0;
        static char s_buff_temp[DATA_CACHE_LEN];
        memset((void *)&BackupCefs_Info, 0x00, sizeof(quec_cefs_file_header_type));
        memcpy((void *)&BackupCefs_Info, data, sizeof(quec_cefs_file_header_type));

        if((CEFS_FILE_MAGIC1 != BackupCefs_Info.magic1) || (CEFS_FILE_MAGIC2 != BackupCefs_Info.magic2))
        {
            dprintf(INFO, "@Ramos efs2 restore file magic1 error !!!\r\n\r\n");
            return FALSE;
        }        
        sz = (BackupCefs_Info.page_count)*pagesize;
        data = data + pagesize;

        for (i=0; i<BackupCefs_Info.page_count; i++)
        {       
              memset(s_buff_temp, 0xFF, sizeof(s_buff_temp));// set buff to 0xff, because modem backup cefs set it is 0xff,  or  crc check error
              memcpy(s_buff_temp, (data+i*pagesize),pagesize);
              crc = Q_crc_32_calc((void *)s_buff_temp, 2048*8,crc); // 2048*8 keep with modem side
        }
        dprintf(INFO, "@Ramos efs2 restore sz=%d, page_count=%d,file magic1=%x,crc=%x,data_crc=%x\n", sz,BackupCefs_Info.page_count,BackupCefs_Info.magic1,crc,BackupCefs_Info.data_crc);
        if(crc != BackupCefs_Info.data_crc)
        {
            dprintf(INFO, "@Ramos efs2 restore file CRC check  error !!!\r\n\r\n");
            return FALSE;
        }
        flash_erase(ptn);// efs partition erase all
    }

    sz = ROUND_TO_PAGE(sz, page_mask);
    dprintf(INFO, "@Ramos writing 0x%x bytes to '%s'\n", sz, ptn->name);
    Quectel_flash_erase(ptn,sz); // erase the partiton first
    if (flash_write(ptn, 0, data, sz)) //this fuction not use the write restore flag
    {
        dprintf(INFO,"@Ramos flash write failure !!!!!\r\n\r\n\r\n");
        return FALSE;
    }

	if(QUECTEL_RESTOREFLAG_NONE != Restore_flag)
	{
		//clear the restroe flag , record the restroe tiems
		Ql_SetRestorecountClearFlag(Restore_flag);
#ifndef QUECTEL_ALL_RESTORE// if defined QUECTEL_ALL_RESTORE, not reboot system when one parition restore success.
			dprintf(INFO, "partition '%s' Restroe  succeed, reboot Now !!!!!\n", ptn->name);
			   //  success ,  reboot
			mdelay(1000);
			reboot_device(0);
#endif
	}

return TRUE;

}
#endif

static void update_ker_tags_rdisk_addr(struct boot_img_hdr *hdr, bool is_arm64)
{
	/* overwrite the destination of specified for the project */
#ifdef ABOOT_IGNORE_BOOT_HEADER_ADDRS
	if (is_arm64)
		hdr->kernel_addr = ABOOT_FORCE_KERNEL64_ADDR;
	else
		hdr->kernel_addr = ABOOT_FORCE_KERNEL_ADDR;
	hdr->ramdisk_addr = ABOOT_FORCE_RAMDISK_ADDR;
	hdr->tags_addr = ABOOT_FORCE_TAGS_ADDR;

#endif
}

static void ptentry_to_tag(unsigned **ptr, struct ptentry *ptn)
{
	struct atag_ptbl_entry atag_ptn;

	memcpy(atag_ptn.name, ptn->name, 16);
	atag_ptn.name[15] = '\0';
	atag_ptn.offset = ptn->start;
	atag_ptn.size = ptn->length;
	atag_ptn.flags = ptn->flags;
	memcpy(*ptr, &atag_ptn, sizeof(struct atag_ptbl_entry));
	*ptr += sizeof(struct atag_ptbl_entry) / sizeof(unsigned);
}

unsigned char *update_cmdline(const char * cmdline)
{
	int cmdline_len = 0;
	int have_cmdline = 0;
	unsigned char *cmdline_final = NULL;
	int pause_at_bootup = 0;
	bool warm_boot = false;
	bool gpt_exists = partition_gpt_exists();
	int have_target_boot_params = 0;
	char *boot_dev_buf = NULL;
    bool is_mdtp_activated = 0;
#ifdef MDTP_SUPPORT
    mdtp_activated(&is_mdtp_activated);
#endif /* MDTP_SUPPORT */

	if (cmdline && cmdline[0]) {
		cmdline_len = strlen(cmdline);
		have_cmdline = 1;
	}
	if (target_is_emmc_boot()) {
		cmdline_len += strlen(emmc_cmdline);
#if USE_BOOTDEV_CMDLINE
		boot_dev_buf = (char *) malloc(sizeof(char) * BOOT_DEV_MAX_LEN);
		ASSERT(boot_dev_buf);
		platform_boot_dev_cmdline(boot_dev_buf);
		cmdline_len += strlen(boot_dev_buf);
#endif
	}

	cmdline_len += strlen(usb_sn_cmdline);
	cmdline_len += strlen(sn_buf);

	if (boot_into_recovery && gpt_exists)
		cmdline_len += strlen(secondary_gpt_enable);

	if(is_mdtp_activated)
		cmdline_len += strlen(mdtp_activated_flag);

	if (boot_into_ffbm) {
		cmdline_len += strlen(androidboot_mode);
		cmdline_len += strlen(ffbm_mode_string);
		/* reduce kernel console messages to speed-up boot */
		cmdline_len += strlen(loglevel);
	} else if (boot_reason_alarm) {
		cmdline_len += strlen(alarmboot_cmdline);
	} else if ((target_build_variant_user() || device.charger_screen_enabled)
			&& target_pause_for_battery_charge()) {
		pause_at_bootup = 1;
		cmdline_len += strlen(battchg_pause);
	}

	if(target_use_signed_kernel() && auth_kernel_img) {
		cmdline_len += strlen(auth_kernel);
	}

	if (get_target_boot_params(cmdline, boot_into_recovery ? "recoveryfs" :
								 "system",
						&target_boot_params) == 0) {
		have_target_boot_params = 1;
		cmdline_len += strlen(target_boot_params);
	}

	/* Determine correct androidboot.baseband to use */
	switch(target_baseband())
	{
		case BASEBAND_APQ:
			cmdline_len += strlen(baseband_apq);
			break;

		case BASEBAND_MSM:
			cmdline_len += strlen(baseband_msm);
			break;

		case BASEBAND_CSFB:
			cmdline_len += strlen(baseband_csfb);
			break;

		case BASEBAND_SVLTE2A:
			cmdline_len += strlen(baseband_svlte2a);
			break;

		case BASEBAND_MDM:
			cmdline_len += strlen(baseband_mdm);
			break;

		case BASEBAND_MDM2:
			cmdline_len += strlen(baseband_mdm2);
			break;

		case BASEBAND_SGLTE:
			cmdline_len += strlen(baseband_sglte);
			break;

		case BASEBAND_SGLTE2:
			cmdline_len += strlen(baseband_sglte2);
			break;

		case BASEBAND_DSDA:
			cmdline_len += strlen(baseband_dsda);
			break;

		case BASEBAND_DSDA2:
			cmdline_len += strlen(baseband_dsda2);
			break;
	}

	if (cmdline) {
		if ((strstr(cmdline, DISPLAY_DEFAULT_PREFIX) == NULL) &&
			target_display_panel_node(display_panel_buf,
			MAX_PANEL_BUF_SIZE) &&
			strlen(display_panel_buf)) {
			cmdline_len += strlen(display_panel_buf);
		}
	}

	if (target_warm_boot()) {
		warm_boot = true;
		cmdline_len += strlen(warmboot_cmdline);
	}

    // add by len for quectel_cmdline, 2018-1-18
#ifndef QUECTEL_FLASH_SUPPORT_1G
	if (strlen(quectel_cmdline) && boot_into_recovery ){
        cmdline_len += strlen(quectel_cmdline);
    }
#endif
	//add end

	if (cmdline_len > 0) {
		const char *src;
		unsigned char *dst;

		cmdline_final = (unsigned char*) malloc((cmdline_len + 4) & (~3));
		ASSERT(cmdline_final != NULL);
		memset((void *)cmdline_final, 0, sizeof(*cmdline_final));
		dst = cmdline_final;

		/* Save start ptr for debug print */
		if (have_cmdline) {
			src = cmdline;
			while ((*dst++ = *src++));
		}
		if (target_is_emmc_boot()) {
			src = emmc_cmdline;
			if (have_cmdline) --dst;
			have_cmdline = 1;
			while ((*dst++ = *src++));
#if USE_BOOTDEV_CMDLINE
			src = boot_dev_buf;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
#endif
		}

		src = usb_sn_cmdline;
		if (have_cmdline) --dst;
		have_cmdline = 1;
		while ((*dst++ = *src++));
		src = sn_buf;
		if (have_cmdline) --dst;
		have_cmdline = 1;
		while ((*dst++ = *src++));
		if (warm_boot) {
			if (have_cmdline) --dst;
			src = warmboot_cmdline;
			while ((*dst++ = *src++));
		}

		if (boot_into_recovery && gpt_exists) {
			src = secondary_gpt_enable;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		}

		if (is_mdtp_activated) {
			src = mdtp_activated_flag;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		}

		if (boot_into_ffbm) {
			src = androidboot_mode;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
			src = ffbm_mode_string;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
			src = loglevel;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		} else if (boot_reason_alarm) {
			src = alarmboot_cmdline;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		} else if (pause_at_bootup) {
			src = battchg_pause;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		}

		if(target_use_signed_kernel() && auth_kernel_img) {
			src = auth_kernel;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		}

		switch(target_baseband())
		{
			case BASEBAND_APQ:
				src = baseband_apq;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_MSM:
				src = baseband_msm;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_CSFB:
				src = baseband_csfb;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_SVLTE2A:
				src = baseband_svlte2a;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_MDM:
				src = baseband_mdm;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_MDM2:
				src = baseband_mdm2;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_SGLTE:
				src = baseband_sglte;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_SGLTE2:
				src = baseband_sglte2;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_DSDA:
				src = baseband_dsda;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;

			case BASEBAND_DSDA2:
				src = baseband_dsda2;
				if (have_cmdline) --dst;
				while ((*dst++ = *src++));
				break;
		}

		if (strlen(display_panel_buf)) {
			src = display_panel_buf;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		}

		if (have_target_boot_params) {
			if (have_cmdline) --dst;
			src = target_boot_params;
			while ((*dst++ = *src++));
			free(target_boot_params);
		}

        // add by len for quectel_cmdline, 2018-1-18
#ifndef QUECTEL_FLASH_SUPPORT_1G
		if (strlen(quectel_cmdline) && boot_into_recovery) {
            src = quectel_cmdline;
            if (have_cmdline) --dst;
            while ((*dst++ = *src++));
        }
#endif
		// add end
	}


	if (boot_dev_buf)
		free(boot_dev_buf);

	if (cmdline_final)
		dprintf(INFO, "cmdline: %s\n", cmdline_final);
	else
		dprintf(INFO, "cmdline is NULL\n");
	return cmdline_final;
}

unsigned *atag_core(unsigned *ptr)
{
	/* CORE */
	*ptr++ = 2;
	*ptr++ = 0x54410001;

	return ptr;

}

unsigned *atag_ramdisk(unsigned *ptr, void *ramdisk,
							   unsigned ramdisk_size)
{
	if (ramdisk_size) {
		*ptr++ = 4;
		*ptr++ = 0x54420005;
		*ptr++ = (unsigned)ramdisk;
		*ptr++ = ramdisk_size;
	}

	return ptr;
}

unsigned *atag_ptable(unsigned **ptr_addr)
{
	int i;
	struct ptable *ptable;

	if ((ptable = flash_get_ptable()) && (ptable->count != 0)) {
		*(*ptr_addr)++ = 2 + (ptable->count * (sizeof(struct atag_ptbl_entry) /
							sizeof(unsigned)));
		*(*ptr_addr)++ = 0x4d534d70;
		for (i = 0; i < ptable->count; ++i)
			ptentry_to_tag(ptr_addr, ptable_get(ptable, i));
	}

	return (*ptr_addr);
}

unsigned *atag_cmdline(unsigned *ptr, const char *cmdline)
{
	int cmdline_length = 0;
	int n;
	char *dest;

	cmdline_length = strlen((const char*)cmdline);
	n = (cmdline_length + 4) & (~3);

	*ptr++ = (n / 4) + 2;
	*ptr++ = 0x54410009;
	dest = (char *) ptr;
	while ((*dest++ = *cmdline++));
	ptr += (n / 4);

	return ptr;
}

unsigned *atag_end(unsigned *ptr)
{
	/* END */
	*ptr++ = 0;
	*ptr++ = 0;

	return ptr;
}

void generate_atags(unsigned *ptr, const char *cmdline,
                    void *ramdisk, unsigned ramdisk_size)
{

	ptr = atag_core(ptr);
	ptr = atag_ramdisk(ptr, ramdisk, ramdisk_size);
	ptr = target_atag_mem(ptr);

	/* Skip NAND partition ATAGS for eMMC boot */
	if (!target_is_emmc_boot()){
		ptr = atag_ptable(&ptr);
	}

	ptr = atag_cmdline(ptr, cmdline);
	ptr = atag_end(ptr);
}

typedef void entry_func_ptr(unsigned, unsigned, unsigned*);
void boot_linux(void *kernel, unsigned *tags,
		const char *cmdline, unsigned machtype,
		void *ramdisk, unsigned ramdisk_size)
{
	unsigned char *final_cmdline;
#if DEVICE_TREE
	int ret = 0;
#endif

	void (*entry)(unsigned, unsigned, unsigned*) = (entry_func_ptr*)(PA((addr_t)kernel));
	uint32_t tags_phys = PA((addr_t)tags);
	struct kernel64_hdr *kptr = ((struct kernel64_hdr*)(PA((addr_t)kernel)));

	ramdisk = (void *)PA((addr_t)ramdisk);

	final_cmdline = update_cmdline((const char*)cmdline);

#if DEVICE_TREE
	dprintf(INFO, "Updating device tree: start\n");

	/* Update the Device Tree */
	ret = update_device_tree((void *)tags,(const char *)final_cmdline, ramdisk, ramdisk_size);
	if(ret)
	{
		dprintf(CRITICAL, "ERROR: Updating Device Tree Failed \n");
		ASSERT(0);
	}
	dprintf(INFO, "Updating device tree: done\n");
#else
	/* Generating the Atags */
	generate_atags(tags, final_cmdline, ramdisk, ramdisk_size);
#endif

	free(final_cmdline);

#if VERIFIED_BOOT
	/* Write protect the device info */
	if (!boot_into_recovery && target_build_variant_user() && devinfo_present && mmc_write_protect("devinfo", 1))
	{
		dprintf(INFO, "Failed to write protect dev info\n");
		ASSERT(0);
	}
#endif

	/* Turn off splash screen if enabled */
#if DISPLAY_SPLASH_SCREEN
	target_display_shutdown();
#endif

	/* Perform target specific cleanup */
	target_uninit();

	dprintf(INFO, "booting linux @ %p, ramdisk @ %p (%d), tags/device tree @ %p\n",
		entry, ramdisk, ramdisk_size, (void *)tags_phys);

	enter_critical_section();

	/* Initialise wdog to catch early kernel crashes */
#if WDOG_SUPPORT
	msm_wdog_init();
#endif
	/* do any platform specific cleanup before kernel entry */
	platform_uninit();

	arch_disable_cache(UCACHE);

#if ARM_WITH_MMU
	arch_disable_mmu();
#endif
	bs_set_timestamp(BS_KERNEL_ENTRY);

	if (IS_ARM64(kptr))
		/* Jump to a 64bit kernel */
		scm_elexec_call((paddr_t)kernel, tags_phys);
	else
		/* Jump to a 32bit kernel */
		entry(0, machtype, (unsigned*)tags_phys);
}

/* Function to check if the memory address range falls within the aboot
 * boundaries.
 * start: Start of the memory region
 * size: Size of the memory region
 */
int check_aboot_addr_range_overlap(uint32_t start, uint32_t size)
{
	/* Check for boundary conditions. */
	if ((UINT_MAX - start) < size)
		return -1;

	/* Check for memory overlap. */
	if ((start < MEMBASE) && ((start + size) <= MEMBASE))
		return 0;
	else if (start >= (MEMBASE + MEMSIZE))
		return 0;
	else
		return -1;
}

#define ROUND_TO_PAGE(x,y) (((x) + (y)) & (~(y)))

BUF_DMA_ALIGN(buf, BOOT_IMG_MAX_PAGE_SIZE); //Equal to max-supported pagesize
#if DEVICE_TREE
BUF_DMA_ALIGN(dt_buf, BOOT_IMG_MAX_PAGE_SIZE);
#endif

static void verify_signed_bootimg(uint32_t bootimg_addr, uint32_t bootimg_size)
{
	int ret;

#if !VERIFIED_BOOT
#if IMAGE_VERIF_ALGO_SHA1
	uint32_t auth_algo = CRYPTO_AUTH_ALG_SHA1;
#else
	uint32_t auth_algo = CRYPTO_AUTH_ALG_SHA256;
#endif
#endif

	/* Assume device is rooted at this time. */
	device.is_tampered = 1;

	dprintf(INFO, "Authenticating boot image (%d): start\n", bootimg_size);

#if VERIFIED_BOOT
	if(boot_into_recovery)
	{
		ret = boot_verify_image((unsigned char *)bootimg_addr,
				bootimg_size, "/recovery");
	}
	else
	{
		ret = boot_verify_image((unsigned char *)bootimg_addr,
				bootimg_size, "/boot");
	}
	boot_verify_print_state();
#else
	ret = image_verify((unsigned char *)bootimg_addr,
					   (unsigned char *)(bootimg_addr + bootimg_size),
					   bootimg_size,
					   auth_algo);
#endif
	dprintf(INFO, "Authenticating boot image: done return value = %d\n", ret);

	if (ret)
	{
		/* Authorized kernel */
		device.is_tampered = 0;
		auth_kernel_img = 1;
	}

#ifdef MDTP_SUPPORT
	{
		/* Verify MDTP lock.
		 * For boot & recovery partitions, use aboot's verification result.
		 */
		mdtp_ext_partition_verification_t ext_partition;
		ext_partition.partition = boot_into_recovery ? MDTP_PARTITION_RECOVERY : MDTP_PARTITION_BOOT;
		ext_partition.integrity_state = device.is_tampered ? MDTP_PARTITION_STATE_INVALID : MDTP_PARTITION_STATE_VALID;
		ext_partition.page_size = 0; /* Not needed since already validated */
		ext_partition.image_addr = 0; /* Not needed since already validated */
		ext_partition.image_size = 0; /* Not needed since already validated */
		ext_partition.sig_avail = FALSE; /* Not needed since already validated */
		mdtp_fwlock_verify_lock(&ext_partition);
	}
#endif /* MDTP_SUPPORT */

#if USE_PCOM_SECBOOT
	set_tamper_flag(device.is_tampered);
#endif

#if VERIFIED_BOOT
	if(boot_verify_get_state() == RED)
	{
		if(!boot_into_recovery)
		{
			dprintf(CRITICAL,
					"Device verification failed. Rebooting into recovery.\n");
			mdelay(1000);
			reboot_device(RECOVERY_MODE);
		}
		else
		{
			dprintf(CRITICAL,
					"Recovery image verification failed. Asserting..\n");
			ASSERT(0);
		}
	}
#endif

	if(device.is_tampered)
	{
		write_device_info_mmc(&device);
	#ifdef TZ_TAMPER_FUSE
		set_tamper_fuse_cmd();
	#endif
	#ifdef ASSERT_ON_TAMPER
		dprintf(CRITICAL, "Device is tampered. Asserting..\n");
		ASSERT(0);
	#endif
	}

}

static bool check_format_bit()
{
	bool ret = false;
	int index;
	uint64_t offset;
	struct boot_selection_info *in = NULL;
	char *buf = NULL;

	index = partition_get_index("bootselect");
	if (index == INVALID_PTN)
	{
		dprintf(INFO, "Unable to locate /bootselect partition\n");
		return ret;
	}
	offset = partition_get_offset(index);
	if(!offset)
	{
		dprintf(INFO, "partition /bootselect doesn't exist\n");
		return ret;
	}
	buf = (char *) memalign(CACHE_LINE, ROUNDUP(page_size, CACHE_LINE));
	ASSERT(buf);
	if (mmc_read(offset, (uint32_t *)buf, page_size))
	{
		dprintf(INFO, "mmc read failure /bootselect %d\n", page_size);
		free(buf);
		return ret;
	}
	in = (struct boot_selection_info *) buf;
	if ((in->signature == BOOTSELECT_SIGNATURE) &&
			(in->version == BOOTSELECT_VERSION)) {
		if ((in->state_info & BOOTSELECT_FORMAT) &&
				!(in->state_info & BOOTSELECT_FACTORY))
			ret = true;
	} else {
		dprintf(CRITICAL, "Signature: 0x%08x or version: 0x%08x mismatched of /bootselect\n",
				in->signature, in->version);
		ASSERT(0);
	}
	free(buf);
	return ret;
}

void boot_verifier_init()
{
	uint32_t boot_state;
	/* Check if device unlock */
	if(device.is_unlocked)
	{
		boot_verify_send_event(DEV_UNLOCK);
		boot_verify_print_state();
		dprintf(CRITICAL, "Device is unlocked! Skipping verification...\n");
		return;
	}
	else
	{
		boot_verify_send_event(BOOT_INIT);
	}

	/* Initialize keystore */
	boot_state = boot_verify_keystore_init();
	if(boot_state == YELLOW)
	{
		boot_verify_print_state();
		dprintf(CRITICAL, "Keystore verification failed! Continuing anyways...\n");
	}
}

int boot_linux_from_mmc(void)
{
	struct boot_img_hdr *hdr = (void*) buf;
	struct boot_img_hdr *uhdr;
	unsigned offset = 0;
	int rcode;
	unsigned long long ptn = 0;
	int index = INVALID_PTN;

	unsigned char *image_addr = 0;
	unsigned kernel_actual;
	unsigned ramdisk_actual;
	unsigned imagesize_actual;
	unsigned second_actual = 0;

	unsigned int dtb_size = 0;
	unsigned int out_len = 0;
	unsigned int out_avai_len = 0;
	unsigned char *out_addr = NULL;
	uint32_t dtb_offset = 0;
	unsigned char *kernel_start_addr = NULL;
	unsigned int kernel_size = 0;
	int rc;

#if DEVICE_TREE
	struct dt_table *table;
	struct dt_entry dt_entry;
	unsigned dt_table_offset;
	uint32_t dt_actual;
	uint32_t dt_hdr_size;
	unsigned char *best_match_dt_addr = NULL;
#endif
	struct kernel64_hdr *kptr = NULL;

	if (check_format_bit())
		boot_into_recovery = 1;

	if (!boot_into_recovery) {
		memset(ffbm_mode_string, '\0', sizeof(ffbm_mode_string));
		rcode = get_ffbm(ffbm_mode_string, sizeof(ffbm_mode_string));
		if (rcode <= 0) {
			boot_into_ffbm = false;
			if (rcode < 0)
				dprintf(CRITICAL,"failed to get ffbm cookie");
		} else
			boot_into_ffbm = true;
	} else
		boot_into_ffbm = false;
	uhdr = (struct boot_img_hdr *)EMMC_BOOT_IMG_HEADER_ADDR;
	if (!memcmp(uhdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		dprintf(INFO, "Unified boot method!\n");
		hdr = uhdr;
		goto unified_boot;
	}
	if (!boot_into_recovery) {
		index = partition_get_index("boot");
		ptn = partition_get_offset(index);
		if(ptn == 0) {
			dprintf(CRITICAL, "ERROR: No boot partition found\n");
                    return -1;
		}
	}
	else {
		index = partition_get_index("recovery");
		ptn = partition_get_offset(index);
		if(ptn == 0) {
			dprintf(CRITICAL, "ERROR: No recovery partition found\n");
                    return -1;
		}
	}
	/* Set Lun for boot & recovery partitions */
	mmc_set_lun(partition_get_lun(index));

	if (mmc_read(ptn + offset, (uint32_t *) buf, page_size)) {
		dprintf(CRITICAL, "ERROR: Cannot read boot image header\n");
                return -1;
	}

	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		dprintf(CRITICAL, "ERROR: Invalid boot image header\n");
                return -1;
	}

	if (hdr->page_size && (hdr->page_size != page_size)) {

		if (hdr->page_size > BOOT_IMG_MAX_PAGE_SIZE) {
			dprintf(CRITICAL, "ERROR: Invalid page size\n");
			return -1;
		}
		page_size = hdr->page_size;
		page_mask = page_size - 1;
	}

	/* ensure commandline is terminated */
	hdr->cmdline[BOOT_ARGS_SIZE-1] = 0;

	kernel_actual  = ROUND_TO_PAGE(hdr->kernel_size,  page_mask);
	ramdisk_actual = ROUND_TO_PAGE(hdr->ramdisk_size, page_mask);

	image_addr = (unsigned char *)target_get_scratch_address();

#if DEVICE_TREE
	dt_actual = ROUND_TO_PAGE(hdr->dt_size, page_mask);
	if (UINT_MAX < ((uint64_t)kernel_actual + (uint64_t)ramdisk_actual+ (uint64_t)dt_actual + page_size)) {
		dprintf(CRITICAL, "Integer overflow detected in bootimage header fields at %u in %s\n",__LINE__,__FILE__);
		return -1;
	}
	imagesize_actual = (page_size + kernel_actual + ramdisk_actual + dt_actual);
#else
	if (UINT_MAX < ((uint64_t)kernel_actual + (uint64_t)ramdisk_actual + page_size)) {
		dprintf(CRITICAL, "Integer overflow detected in bootimage header fields at %u in %s\n",__LINE__,__FILE__);
		return -1;
	}
	imagesize_actual = (page_size + kernel_actual + ramdisk_actual);
#endif

#if VERIFIED_BOOT
	boot_verifier_init();
#endif

	if (check_aboot_addr_range_overlap((uint32_t) image_addr, imagesize_actual))
	{
		dprintf(CRITICAL, "Boot image buffer address overlaps with aboot addresses.\n");
		return -1;
	}

	/*
	 * Update loading flow of bootimage to support compressed/uncompressed
	 * bootimage on both 64bit and 32bit platform.
	 * 1. Load bootimage from emmc partition onto DDR.
	 * 2. Check if bootimage is gzip format. If yes, decompress compressed kernel
	 * 3. Check kernel header and update kernel load addr for 64bit and 32bit
	 *    platform accordingly.
	 * 4. Sanity Check on kernel_addr and ramdisk_addr and copy data.
	 */

	dprintf(INFO, "Loading (%s) image (%d): start\n",
			(!boot_into_recovery ? "boot" : "recovery"),imagesize_actual);
	bs_set_timestamp(BS_KERNEL_LOAD_START);

	/* Read image without signature */
	if (mmc_read(ptn + offset, (void *)image_addr, imagesize_actual))
	{
		dprintf(CRITICAL, "ERROR: Cannot read boot image\n");
		return -1;
	}

	dprintf(INFO, "Loading (%s) image (%d): done\n",
			(!boot_into_recovery ? "boot" : "recovery"),imagesize_actual);

	bs_set_timestamp(BS_KERNEL_LOAD_DONE);

	/* Authenticate Kernel */
	dprintf(INFO, "use_signed_kernel=%d, is_unlocked=%d, is_tampered=%d.\n",
		(int) target_use_signed_kernel(),
		device.is_unlocked,
		device.is_tampered);

	/* Change the condition a little bit to include the test framework support.
	 * We would never reach this point if device is in fastboot mode, even if we did
	 * that means we are in test mode, so execute kernel authentication part for the
	 * tests */
	if((target_use_signed_kernel() && (!device.is_unlocked)) || boot_into_fastboot)
	{
		offset = imagesize_actual;
		if (check_aboot_addr_range_overlap((uint32_t)image_addr + offset, page_size))
		{
			dprintf(CRITICAL, "Signature read buffer address overlaps with aboot addresses.\n");
			return -1;
		}

		/* Read signature */
		if(mmc_read(ptn + offset, (void *)(image_addr + offset), page_size))
		{
			dprintf(CRITICAL, "ERROR: Cannot read boot image signature\n");
			return -1;
		}

		verify_signed_bootimg((uint32_t)image_addr, imagesize_actual);
		/* The purpose of our test is done here */
		if (boot_into_fastboot && auth_kernel_img)
			return 0;
	} else {
		second_actual  = ROUND_TO_PAGE(hdr->second_size,  page_mask);
		#ifdef TZ_SAVE_KERNEL_HASH
		aboot_save_boot_hash_mmc((uint32_t) image_addr, imagesize_actual);
		#endif /* TZ_SAVE_KERNEL_HASH */

#ifdef MDTP_SUPPORT
		{
			/* Verify MDTP lock.
			 * For boot & recovery partitions, MDTP will use boot_verifier APIs,
			 * since verification was skipped in aboot. The signature is not part of the loaded image.
			 */
			mdtp_ext_partition_verification_t ext_partition;
			ext_partition.partition = boot_into_recovery ? MDTP_PARTITION_RECOVERY : MDTP_PARTITION_BOOT;
			ext_partition.integrity_state = MDTP_PARTITION_STATE_UNSET;
			ext_partition.page_size = page_size;
			ext_partition.image_addr = (uint32)image_addr;
			ext_partition.image_size = imagesize_actual;
			ext_partition.sig_avail = FALSE;
			mdtp_fwlock_verify_lock(&ext_partition);
		}
#endif /* MDTP_SUPPORT */
	}

	/*
	 * Check if the kernel image is a gzip package. If yes, need to decompress it.
	 * If not, continue booting.
	 */
	if (is_gzip_package((unsigned char *)(image_addr + page_size), hdr->kernel_size))
	{
		out_addr = (unsigned char *)(image_addr + imagesize_actual + page_size);
		out_avai_len = target_get_max_flash_size() - imagesize_actual - page_size;
		dprintf(INFO, "decompressing kernel image: start\n");
		rc = decompress((unsigned char *)(image_addr + page_size),
				hdr->kernel_size, out_addr, out_avai_len,
				&dtb_offset, &out_len);
		if (rc)
		{
			dprintf(CRITICAL, "decompressing kernel image failed!!!\n");
			ASSERT(0);
		}

		dprintf(INFO, "decompressing kernel image: done\n");
		kptr = (struct kernel64_hdr *)out_addr;
		kernel_start_addr = out_addr;
		kernel_size = out_len;
	} else {
		kptr = (struct kernel64_hdr *)(image_addr + page_size);
		kernel_start_addr = (unsigned char *)(image_addr + page_size);
		kernel_size = hdr->kernel_size;
	}

	/*
	 * Update the kernel/ramdisk/tags address if the boot image header
	 * has default values, these default values come from mkbootimg when
	 * the boot image is flashed using fastboot flash:raw
	 */
	update_ker_tags_rdisk_addr(hdr, IS_ARM64(kptr));

	/* Get virtual addresses since the hdr saves physical addresses. */
	hdr->kernel_addr = VA((addr_t)(hdr->kernel_addr));
	hdr->ramdisk_addr = VA((addr_t)(hdr->ramdisk_addr));
	hdr->tags_addr = VA((addr_t)(hdr->tags_addr));

	
    kernel_size = ROUND_TO_PAGE(kernel_size,  page_mask);
	/* Check if the addresses in the header are valid. */
	if (check_aboot_addr_range_overlap(hdr->kernel_addr, kernel_size) ||
		check_aboot_addr_range_overlap(hdr->ramdisk_addr, ramdisk_actual))
	{
		dprintf(CRITICAL, "kernel/ramdisk addresses overlap with aboot addresses.\n");
		return -1;
	}

#ifndef DEVICE_TREE
	if (check_aboot_addr_range_overlap(hdr->tags_addr, MAX_TAGS_SIZE))
	{
		dprintf(CRITICAL, "Tags addresses overlap with aboot addresses.\n");
		return -1;
	}
#endif

	/* Move kernel, ramdisk and device tree to correct address */
	memmove((void*) hdr->kernel_addr, kernel_start_addr, kernel_size);
	memmove((void*) hdr->ramdisk_addr, (char *)(image_addr + page_size + kernel_actual), hdr->ramdisk_size);

	#if DEVICE_TREE
	if(hdr->dt_size) {
		dt_table_offset = ((uint32_t)image_addr + page_size + kernel_actual + ramdisk_actual + second_actual);
		table = (struct dt_table*) dt_table_offset;

		if (dev_tree_validate(table, hdr->page_size, &dt_hdr_size) != 0) {
			dprintf(CRITICAL, "ERROR: Cannot validate Device Tree Table \n");
			return -1;
		}

		/* Find index of device tree within device tree table */
		if(dev_tree_get_entry_info(table, &dt_entry) != 0){
			dprintf(CRITICAL, "ERROR: Getting device tree address failed\n");
			return -1;
		}

		if (is_gzip_package((unsigned char *)dt_table_offset + dt_entry.offset, dt_entry.size))
		{
			unsigned int compressed_size = 0;
			out_addr += out_len;
			out_avai_len -= out_len;
			dprintf(INFO, "decompressing dtb: start\n");
			rc = decompress((unsigned char *)dt_table_offset + dt_entry.offset,
					dt_entry.size, out_addr, out_avai_len,
					&compressed_size, &dtb_size);
			if (rc)
			{
				dprintf(CRITICAL, "decompressing dtb failed!!!\n");
				ASSERT(0);
			}

			dprintf(INFO, "decompressing dtb: done\n");
			best_match_dt_addr = out_addr;
		} else {
			best_match_dt_addr = (unsigned char *)dt_table_offset + dt_entry.offset;
			dtb_size = dt_entry.size;
		}

		/* Validate and Read device device tree in the tags_addr */
		if (check_aboot_addr_range_overlap(hdr->tags_addr, dtb_size))
		{
			dprintf(CRITICAL, "Device tree addresses overlap with aboot addresses.\n");
			return -1;
		}

		memmove((void *)hdr->tags_addr, (char *)best_match_dt_addr, dtb_size);
	} else {
		/* Validate the tags_addr */
		if (check_aboot_addr_range_overlap(hdr->tags_addr, kernel_actual))
		{
			dprintf(CRITICAL, "Device tree addresses overlap with aboot addresses.\n");
			return -1;
		}
		/*
		 * If appended dev tree is found, update the atags with
		 * memory address to the DTB appended location on RAM.
		 * Else update with the atags address in the kernel header
		 */
		void *dtb;
		dtb = dev_tree_appended((void*)(image_addr + page_size),
					hdr->kernel_size, dtb_offset,
					(void *)hdr->tags_addr);
		if (!dtb) {
			dprintf(CRITICAL, "ERROR: Appended Device Tree Blob not found\n");
			return -1;
		}
	}
	#endif

	if (boot_into_recovery && !device.is_unlocked && !device.is_tampered)
		target_load_ssd_keystore();

unified_boot:

	boot_linux((void *)hdr->kernel_addr, (void *)hdr->tags_addr,
		   (const char *)hdr->cmdline, board_machtype(),
		   (void *)hdr->ramdisk_addr, hdr->ramdisk_size);

	return 0;
}

int boot_linux_from_flash(void)
{
	struct boot_img_hdr *hdr = (void*) buf;
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned offset = 0;

	unsigned char *image_addr = 0;
	unsigned kernel_actual;
	unsigned ramdisk_actual;
	unsigned imagesize_actual;
	unsigned second_actual = 0;
	
#if DEVICE_TREE
	struct dt_table *table;
	struct dt_entry dt_entry;
	unsigned dt_table_offset;
	uint32_t dt_actual;
	uint32_t dt_hdr_size;
	unsigned int dtb_size = 0;
	unsigned char *best_match_dt_addr = NULL;
#endif
	if (target_is_emmc_boot()) {
		hdr = (struct boot_img_hdr *)EMMC_BOOT_IMG_HEADER_ADDR;
		if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
			dprintf(CRITICAL, "ERROR: Invalid boot image header\n");
			return -1;
		}
		goto continue_boot;
	}

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		dprintf(CRITICAL, "ERROR: Partition table not found\n");
		return -1;
	}

	if(!boot_into_recovery)
	{
	        ptn = ptable_find(ptable, "boot");

	        if (ptn == NULL) {
		        dprintf(CRITICAL, "ERROR: No boot partition found\n");
		        return -1;
	        }
	}
	else
	{
	        ptn = ptable_find(ptable, "recovery");
	        if (ptn == NULL) {
		        dprintf(CRITICAL, "ERROR: No recovery partition found\n");
		        return -1;
	        }
	}

	if (flash_read(ptn, offset, buf, page_size)) {
		dprintf(CRITICAL, "ERROR: Cannot read boot image header\n");
		return -1;
	}

	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		dprintf(CRITICAL, "ERROR: Invalid boot image header\n");
		return -1;
	}

	if (hdr->page_size != page_size) {
		dprintf(CRITICAL, "ERROR: Invalid boot image pagesize. Device pagesize: %d, Image pagesize: %d\n",page_size,hdr->page_size);
		return -1;
	}

	/* ensure commandline is terminated */
	hdr->cmdline[BOOT_ARGS_SIZE-1] = 0;

	/*
	 * Update the kernel/ramdisk/tags address if the boot image header
	 * has default values, these default values come from mkbootimg when
	 * the boot image is flashed using fastboot flash:raw
	 */
	update_ker_tags_rdisk_addr(hdr, false);

	/* Get virtual addresses since the hdr saves physical addresses. */
	hdr->kernel_addr = VA((addr_t)(hdr->kernel_addr));
	hdr->ramdisk_addr = VA((addr_t)(hdr->ramdisk_addr));
	hdr->tags_addr = VA((addr_t)(hdr->tags_addr));

	kernel_actual  = ROUND_TO_PAGE(hdr->kernel_size,  page_mask);
	ramdisk_actual = ROUND_TO_PAGE(hdr->ramdisk_size, page_mask);

	/* Check if the addresses in the header are valid. */
	if (check_aboot_addr_range_overlap(hdr->kernel_addr, kernel_actual) ||
		check_aboot_addr_range_overlap(hdr->ramdisk_addr, ramdisk_actual))
	{
		dprintf(CRITICAL, "kernel/ramdisk addresses overlap with aboot addresses.\n");
		return -1;
	}

#ifndef DEVICE_TREE
		if (check_aboot_addr_range_overlap(hdr->tags_addr, MAX_TAGS_SIZE))
		{
			dprintf(CRITICAL, "Tags addresses overlap with aboot addresses.\n");
			return -1;
		}
#endif

	/* Authenticate Kernel */
	if(target_use_signed_kernel() && (!device.is_unlocked))
	{
		image_addr = (unsigned char *)target_get_scratch_address();
		offset = 0;

#if DEVICE_TREE
		dt_actual = ROUND_TO_PAGE(hdr->dt_size, page_mask);
		imagesize_actual = (page_size + kernel_actual + ramdisk_actual + dt_actual);

		if (check_aboot_addr_range_overlap(hdr->tags_addr, hdr->dt_size))
		{
			dprintf(CRITICAL, "Device tree addresses overlap with aboot addresses.\n");
			return -1;
		}
#else
		imagesize_actual = (page_size + kernel_actual + ramdisk_actual);
#endif

		dprintf(INFO, "AAAAALoading (%s) image (%d): start\n",
			(!boot_into_recovery ? "boot" : "recovery"),imagesize_actual);
		bs_set_timestamp(BS_KERNEL_LOAD_START);

		/* Read image without signature */
		if (flash_read(ptn, offset, (void *)image_addr, imagesize_actual))
		{
			dprintf(CRITICAL, "ERROR: Cannot read boot image\n");
				return -1;
		}

		dprintf(INFO, "AAAAALoading (%s) image (%d): done\n",
			(!boot_into_recovery ? "boot" : "recovery"), imagesize_actual);
		bs_set_timestamp(BS_KERNEL_LOAD_DONE);

		offset = imagesize_actual;
		/* Read signature */
		if (flash_read(ptn, offset, (void *)(image_addr + offset), page_size))
		{
			dprintf(CRITICAL, "ERROR: Cannot read boot image signature\n");
			return -1;
		}

		verify_signed_bootimg((uint32_t)image_addr, imagesize_actual);

		/* Move kernel and ramdisk to correct address */
		memmove((void*) hdr->kernel_addr, (char*) (image_addr + page_size), hdr->kernel_size);
		memmove((void*) hdr->ramdisk_addr, (char*) (image_addr + page_size + kernel_actual), hdr->ramdisk_size);
#if DEVICE_TREE
		if(hdr->dt_size != 0) {

			dt_table_offset = ((uint32_t)image_addr + page_size + kernel_actual + ramdisk_actual + second_actual);

			table = (struct dt_table*) dt_table_offset;

			if (dev_tree_validate(table, hdr->page_size, &dt_hdr_size) != 0){
				dprintf(CRITICAL, "ERROR: Cannot validate Device Tree Table \n");
				return -1;
			}

			/* Find index of device tree within device tree table */
			if(dev_tree_get_entry_info(table, &dt_entry) != 0){
				dprintf(CRITICAL, "ERROR: Getting device tree address failed\n");
				return -1;
			}

			/* Validate and Read device device tree in the "tags_add */
			if (check_aboot_addr_range_overlap(hdr->tags_addr, dt_entry.size)){
				dprintf(CRITICAL, "Device tree addresses overlap with aboot addresses.\n");
				return -1;
			}

			best_match_dt_addr = (unsigned char *)table + dt_entry.offset;
			dtb_size = dt_entry.size;
			memmove((void *)hdr->tags_addr, (char *)best_match_dt_addr, dtb_size);
		}
#endif

		/* Make sure everything from scratch address is read before next step!*/
		if(device.is_tampered)
		{
			write_device_info_flash(&device);
		}
#if USE_PCOM_SECBOOT
		set_tamper_flag(device.is_tampered);
#endif
	}
	else
	{
		offset = page_size;
		
        kernel_actual = ROUND_TO_PAGE(hdr->kernel_size, page_mask);
		ramdisk_actual = ROUND_TO_PAGE(hdr->ramdisk_size, page_mask);
		second_actual = ROUND_TO_PAGE(hdr->second_size, page_mask);
		
        dprintf(INFO, "BBBBLoading (%s) image (%d): start\n",
				(!boot_into_recovery ? "boot" : "recovery"), kernel_actual + ramdisk_actual);

		bs_set_timestamp(BS_KERNEL_LOAD_START);

		if (UINT_MAX - offset < kernel_actual)
		{
			dprintf(CRITICAL, "ERROR: Integer overflow in boot image header %s\t%d\n",__func__,__LINE__);
			return -1;
		}
		if (flash_read(ptn, offset, (void *)hdr->kernel_addr, kernel_actual)) {
			dprintf(CRITICAL, "ERROR: Cannot read kernel image\n");
			return -1;
		}
		offset += kernel_actual;
		if (UINT_MAX - offset < ramdisk_actual)
		{
			dprintf(CRITICAL, "ERROR: Integer overflow in boot image header %s\t%d\n",__func__,__LINE__);
			return -1;
		}
		if (flash_read(ptn, offset, (void *)hdr->ramdisk_addr, ramdisk_actual)) {
			dprintf(CRITICAL, "ERROR: Cannot read ramdisk image\n");
			return -1;
		}

		offset += ramdisk_actual;

		dprintf(INFO, "BBBBBLoading (%s) image (%d): done\n",
				(!boot_into_recovery ? "boot" : "recovery"), kernel_actual + ramdisk_actual);

		bs_set_timestamp(BS_KERNEL_LOAD_DONE);

		if(hdr->second_size != 0) {
			if (UINT_MAX - offset < second_actual)
			{
				dprintf(CRITICAL, "ERROR: Integer overflow in boot image header %s\t%d\n",__func__,__LINE__);
				return -1;
			}
			offset += second_actual;
			/* Second image loading not implemented. */
			ASSERT(0);
		}

#if DEVICE_TREE
		if(hdr->dt_size != 0) {

			/* Read the device tree table into buffer */
			if(flash_read(ptn, offset, (void *) dt_buf, page_size)) {
				dprintf(CRITICAL, "ERROR: Cannot read the Device Tree Table\n");
				return -1;
			}

			table = (struct dt_table*) dt_buf;

			if (dev_tree_validate(table, hdr->page_size, &dt_hdr_size) != 0) {
				dprintf(CRITICAL, "ERROR: Cannot validate Device Tree Table \n");
				return -1;
			}

			table = (struct dt_table*) memalign(CACHE_LINE, dt_hdr_size);
			if (!table)
				return -1;

			/* Read the entire device tree table into buffer */
			if(flash_read(ptn, offset, (void *)table, dt_hdr_size)) {
				dprintf(CRITICAL, "ERROR: Cannot read the Device Tree Table\n");
				return -1;
			}


			/* Find index of device tree within device tree table */
			if(dev_tree_get_entry_info(table, &dt_entry) != 0){
				dprintf(CRITICAL, "ERROR: Getting device tree address failed\n");
				return -1;
			}

			/* Validate and Read device device tree in the "tags_add */
			if (check_aboot_addr_range_overlap(hdr->tags_addr, dt_entry.size))
			{
				dprintf(CRITICAL, "Device tree addresses overlap with aboot addresses.\n");
				return -1;
			}

			/* Read device device tree in the "tags_add */
			if(flash_read(ptn, offset + dt_entry.offset,
						 (void *)hdr->tags_addr, dt_entry.size)) {
				dprintf(CRITICAL, "ERROR: Cannot read device tree\n");
				return -1;
			}
		}
#endif

	}
continue_boot:

	/* TODO: create/pass atags to kernel */

	boot_linux((void *)hdr->kernel_addr, (void *)hdr->tags_addr,
		   (const char *)hdr->cmdline, board_machtype(),
		   (void *)hdr->ramdisk_addr, hdr->ramdisk_size);

	return 0;
}

void write_device_info_mmc(device_info *dev)
{
	unsigned long long ptn = 0;
	unsigned long long size;
	int index = INVALID_PTN;
	uint32_t blocksize;
	uint8_t lun = 0;
	uint32_t ret = 0;

	if (devinfo_present)
		index = partition_get_index("devinfo");
	else
		index = partition_get_index("aboot");

	ptn = partition_get_offset(index);
	if(ptn == 0)
	{
		return;
	}

	lun = partition_get_lun(index);
	mmc_set_lun(lun);

	size = partition_get_size(index);

	blocksize = mmc_get_device_blocksize();

	if (devinfo_present)
		ret = mmc_write(ptn, blocksize, (void *)info_buf);
	else
		ret = mmc_write((ptn + size - blocksize), blocksize, (void *)info_buf);
	if (ret)
	{
		dprintf(CRITICAL, "ERROR: Cannot write device info\n");
		ASSERT(0);
	}
}

void read_device_info_mmc(struct device_info *info)
{
	unsigned long long ptn = 0;
	unsigned long long size;
	int index = INVALID_PTN;
	uint32_t blocksize;
	uint32_t ret  = 0;

	if ((index = partition_get_index("devinfo")) < 0)
	{
		devinfo_present = false;
		index = partition_get_index("aboot");
	}

	ptn = partition_get_offset(index);
	if(ptn == 0)
	{
		return;
	}

	mmc_set_lun(partition_get_lun(index));

	size = partition_get_size(index);

	blocksize = mmc_get_device_blocksize();

	if (devinfo_present)
		ret = mmc_read(ptn, (void *)info_buf, blocksize);
	else
		ret = mmc_read((ptn + size - blocksize), (void *)info_buf, blocksize);
	if (ret)
	{
		dprintf(CRITICAL, "ERROR: Cannot read device info\n");
		ASSERT(0);
	}
}

void write_device_info_flash(device_info *dev)
{
	struct device_info *info = memalign(PAGE_SIZE, ROUNDUP(BOOT_IMG_MAX_PAGE_SIZE, PAGE_SIZE));
	struct ptentry *ptn;
	struct ptable *ptable;
	if(info == NULL)
	{
		dprintf(CRITICAL, "Failed to allocate memory for device info struct\n");
		ASSERT(0);
	}
	info_buf = info;
	ptable = flash_get_ptable();
	if (ptable == NULL)
	{
		dprintf(CRITICAL, "ERROR: Partition table not found\n");
		return;
	}

	ptn = ptable_find(ptable, "devinfo");
	if (ptn == NULL)
	{
		dprintf(CRITICAL, "ERROR: No devinfo partition found\n");
			return;
	}

	memcpy(info, dev, sizeof(device_info));

	if (flash_write(ptn, 0, (void *)info_buf, page_size))
	{
		dprintf(CRITICAL, "ERROR: Cannot write device info\n");
			return;
	}
	free(info);
}

static int read_allow_oem_unlock(device_info *dev)
{
	unsigned offset;
	int index;
	unsigned long long ptn;
	unsigned long long ptn_size;
	unsigned blocksize = mmc_get_device_blocksize();
	STACKBUF_DMA_ALIGN(buf, blocksize);

	index = partition_get_index(frp_ptns[0]);
	if (index == INVALID_PTN)
	{
		index = partition_get_index(frp_ptns[1]);
		if (index == INVALID_PTN)
		{
			dprintf(CRITICAL, "Neither '%s' nor '%s' partition found\n", frp_ptns[0],frp_ptns[1]);
			return -1;
		}
	}

	ptn = partition_get_offset(index);
	ptn_size = partition_get_size(index);
	offset = ptn_size - blocksize;

	if (mmc_read(ptn + offset, (void *)buf, blocksize))
	{
		dprintf(CRITICAL, "Reading MMC failed\n");
		return -1;
	}

	/*is_allow_unlock is a bool value stored at the LSB of last byte*/
	is_allow_unlock = buf[blocksize-1] & 0x01;
	return 0;
}

static int write_allow_oem_unlock(bool allow_unlock)
{
	unsigned offset;
	int index;
	unsigned long long ptn;
	unsigned long long ptn_size;
	unsigned blocksize = mmc_get_device_blocksize();
	STACKBUF_DMA_ALIGN(buf, blocksize);

	index = partition_get_index(frp_ptns[0]);
	if (index == INVALID_PTN)
	{
		index = partition_get_index(frp_ptns[1]);
		if (index == INVALID_PTN)
		{
			dprintf(CRITICAL, "Neither '%s' nor '%s' partition found\n", frp_ptns[0],frp_ptns[1]);
			return -1;
		}
	}

	ptn = partition_get_offset(index);
	ptn_size = partition_get_size(index);
	offset = ptn_size - blocksize;

	if (mmc_read(ptn + offset, (void *)buf, blocksize))
	{
		dprintf(CRITICAL, "Reading MMC failed\n");
		return -1;
	}

	/*is_allow_unlock is a bool value stored at the LSB of last byte*/
	buf[blocksize-1] = allow_unlock;
	if (mmc_write(ptn + offset, blocksize, buf))
	{
		dprintf(CRITICAL, "Writing MMC failed\n");
		return -1;
	}

	return 0;
}

void read_device_info_flash(device_info *dev)
{
	struct device_info *info = memalign(PAGE_SIZE, ROUNDUP(BOOT_IMG_MAX_PAGE_SIZE, PAGE_SIZE));
	struct ptentry *ptn;
	struct ptable *ptable;
	if(info == NULL)
	{
		dprintf(CRITICAL, "Failed to allocate memory for device info struct\n");
		ASSERT(0);
	}
	info_buf = info;
	ptable = flash_get_ptable();
	if (ptable == NULL)
	{
		dprintf(CRITICAL, "ERROR: Partition table not found\n");
		return;
	}

	ptn = ptable_find(ptable, "devinfo");
	if (ptn == NULL)
	{
		dprintf(CRITICAL, "ERROR: No devinfo partition found\n");
			return;
	}

	if (flash_read(ptn, 0, (void *)info_buf, page_size))
	{
		dprintf(CRITICAL, "ERROR: Cannot write device info\n");
			return;
	}

	if (memcmp(info->magic, DEVICE_MAGIC, DEVICE_MAGIC_SIZE))
	{
		memcpy(info->magic, DEVICE_MAGIC, DEVICE_MAGIC_SIZE);
		info->is_unlocked = 0;
		info->is_tampered = 0;
		write_device_info_flash(info);
	}
	memcpy(dev, info, sizeof(device_info));
	free(info);
}

void write_device_info(device_info *dev)
{
	if(target_is_emmc_boot())
	{
		struct device_info *info = memalign(PAGE_SIZE, ROUNDUP(BOOT_IMG_MAX_PAGE_SIZE, PAGE_SIZE));
		if(info == NULL)
		{
			dprintf(CRITICAL, "Failed to allocate memory for device info struct\n");
			ASSERT(0);
		}
		info_buf = info;
		memcpy(info, dev, sizeof(struct device_info));

#if USE_RPMB_FOR_DEVINFO
		if (is_secure_boot_enable()) {
			if((write_device_info_rpmb((void*) info, PAGE_SIZE)) < 0)
				ASSERT(0);
		}
		else
			write_device_info_mmc(info);
#else
		write_device_info_mmc(info);
#endif
		free(info);
	}
	else
	{
		write_device_info_flash(dev);
	}
}

void read_device_info(device_info *dev)
{
	if(target_is_emmc_boot())
	{
		struct device_info *info = memalign(PAGE_SIZE, ROUNDUP(BOOT_IMG_MAX_PAGE_SIZE, PAGE_SIZE));
		if(info == NULL)
		{
			dprintf(CRITICAL, "Failed to allocate memory for device info struct\n");
			ASSERT(0);
		}
		info_buf = info;

#if USE_RPMB_FOR_DEVINFO
		if (is_secure_boot_enable()) {
			if((read_device_info_rpmb((void*) info, PAGE_SIZE)) < 0)
				ASSERT(0);
		}
		else
			read_device_info_mmc(info);
#else
		read_device_info_mmc(info);
#endif

		if (memcmp(info->magic, DEVICE_MAGIC, DEVICE_MAGIC_SIZE))
		{
			memcpy(info->magic, DEVICE_MAGIC, DEVICE_MAGIC_SIZE);
			if (is_secure_boot_enable())
				info->is_unlocked = 0;
			else
				info->is_unlocked = 1;
			info->is_tampered = 0;
			info->charger_screen_enabled = 0;
			write_device_info(info);
		}
		memcpy(dev, info, sizeof(device_info));
		free(info);
	}
	else
	{
		read_device_info_flash(dev);
	}
}

void reset_device_info()
{
	dprintf(ALWAYS, "reset_device_info called.");
	device.is_tampered = 0;
	write_device_info(&device);
}

void set_device_root()
{
	dprintf(ALWAYS, "set_device_root called.");
	device.is_tampered = 1;
	write_device_info(&device);
}

#if DEVICE_TREE
int copy_dtb(uint8_t *boot_image_start, unsigned int scratch_offset)
{
	uint32 dt_image_offset = 0;
	uint32_t n;
	struct dt_table *table;
	struct dt_entry dt_entry;
	uint32_t dt_hdr_size;
	unsigned int compressed_size = 0;
	unsigned int dtb_size = 0;
	unsigned int out_avai_len = 0;
	unsigned char *out_addr = NULL;
	unsigned char *best_match_dt_addr = NULL;
	int rc;

	struct boot_img_hdr *hdr = (struct boot_img_hdr *) (boot_image_start);

	if(hdr->dt_size != 0) {
		/* add kernel offset */
		dt_image_offset += page_size;
		n = ROUND_TO_PAGE(hdr->kernel_size, page_mask);
		dt_image_offset += n;

		/* add ramdisk offset */
		n = ROUND_TO_PAGE(hdr->ramdisk_size, page_mask);
		dt_image_offset += n;

		/* add second offset */
		if(hdr->second_size != 0) {
			n = ROUND_TO_PAGE(hdr->second_size, page_mask);
			dt_image_offset += n;
		}

		/* offset now point to start of dt.img */
		table = (struct dt_table*)(boot_image_start + dt_image_offset);

		if (dev_tree_validate(table, hdr->page_size, &dt_hdr_size) != 0) {
			dprintf(CRITICAL, "ERROR: Cannot validate Device Tree Table \n");
			return -1;
		}
		/* Find index of device tree within device tree table */
		if(dev_tree_get_entry_info(table, &dt_entry) != 0){
			dprintf(CRITICAL, "ERROR: Getting device tree address failed\n");
			return -1;
		}

		best_match_dt_addr = (unsigned char *)boot_image_start + dt_image_offset + dt_entry.offset;
		if (is_gzip_package(best_match_dt_addr, dt_entry.size))
		{
			out_addr = (unsigned char *)target_get_scratch_address() + scratch_offset;
			out_avai_len = target_get_max_flash_size() - scratch_offset;
			dprintf(INFO, "decompressing dtb: start\n");
			rc = decompress(best_match_dt_addr,
					dt_entry.size, out_addr, out_avai_len,
					&compressed_size, &dtb_size);
			if (rc)
			{
				dprintf(CRITICAL, "decompressing dtb failed!!!\n");
				ASSERT(0);
			}

			dprintf(INFO, "decompressing dtb: done\n");
			best_match_dt_addr = out_addr;
		} else {
			dtb_size = dt_entry.size;
		}
		/* Validate and Read device device tree in the "tags_add */
		if (check_aboot_addr_range_overlap(hdr->tags_addr, dtb_size))
		{
			dprintf(CRITICAL, "Device tree addresses overlap with aboot addresses.\n");
			return -1;
		}

		/* Read device device tree in the "tags_add */
		memmove((void*) hdr->tags_addr, (void *)best_match_dt_addr, dtb_size);
	} else
		return -1;

	/* Everything looks fine. Return success. */
	return 0;
}
#endif

void cmd_boot(const char *arg, void *data, unsigned sz)
{
#ifdef MDTP_SUPPORT
	static bool is_mdtp_activated = 0;
#endif /* MDTP_SUPPORT */
	unsigned kernel_actual;
	unsigned ramdisk_actual;
	uint32_t image_actual;
	uint32_t dt_actual = 0;
	uint32_t sig_actual = SIGNATURE_SIZE;
	struct boot_img_hdr *hdr = NULL;
	struct kernel64_hdr *kptr = NULL;
	char *ptr = ((char*) data);
	int ret = 0;
	uint8_t dtb_copied = 0;
	unsigned int out_len = 0;
	unsigned int out_avai_len = 0;
	unsigned char *out_addr = NULL;
	uint32_t dtb_offset = 0;
	unsigned char *kernel_start_addr = NULL;
	unsigned int kernel_size = 0;
	unsigned int scratch_offset = 0;

#if VERIFIED_BOOT
	if(target_build_variant_user() && !device.is_unlocked)
	{
		fastboot_fail("unlock device to use this command");
		return;
	}
#endif

	if (sz < sizeof(hdr)) {
		fastboot_fail("invalid bootimage header");
		return;
	}

	hdr = (struct boot_img_hdr *)data;

	/* ensure commandline is terminated */
	hdr->cmdline[BOOT_ARGS_SIZE-1] = 0;

	if(target_is_emmc_boot() && hdr->page_size) {
		page_size = hdr->page_size;
		page_mask = page_size - 1;
	}

	kernel_actual = ROUND_TO_PAGE(hdr->kernel_size, page_mask);
	ramdisk_actual = ROUND_TO_PAGE(hdr->ramdisk_size, page_mask);
#if DEVICE_TREE
	dt_actual = ROUND_TO_PAGE(hdr->dt_size, page_mask);
#endif

	image_actual = ADD_OF(page_size, kernel_actual);
	image_actual = ADD_OF(image_actual, ramdisk_actual);
	image_actual = ADD_OF(image_actual, dt_actual);

	if (target_use_signed_kernel() && (!device.is_unlocked))
		image_actual = ADD_OF(image_actual, sig_actual);

	/* sz should have atleast raw boot image */
	if (image_actual > sz) {
		fastboot_fail("bootimage: incomplete or not signed");
		return;
	}

	/* Handle overflow if the input image size is greater than
	 * boot image buffer can hold
	 */
#if VERIFIED_BOOT
	if ((target_get_max_flash_size() - (image_actual - sig_actual)) < page_size)
	{
		fastboot_fail("booimage: size is greater than boot image buffer can hold");
		return;
	}
#endif

	/* Verify the boot image
	 * device & page_size are initialized in aboot_init
	 */
	if (target_use_signed_kernel() && (!device.is_unlocked))
		/* Pass size excluding signature size, otherwise we would try to
		 * access signature beyond its length
		 */
		verify_signed_bootimg((uint32_t)data, (image_actual - sig_actual));

#ifdef MDTP_SUPPORT
	else
	{
		/* fastboot boot is not allowed when MDTP is activated */
		mdtp_ext_partition_verification_t ext_partition;

		if (!is_mdtp_activated) {
			ext_partition.partition = MDTP_PARTITION_NONE;
			mdtp_fwlock_verify_lock(&ext_partition);
		}
	}

	mdtp_activated(&is_mdtp_activated);
	if(is_mdtp_activated){
		dprintf(CRITICAL, "fastboot boot command is not available.\n");
		return;
	}
#endif /* MDTP_SUPPORT */

	/*
	 * Check if the kernel image is a gzip package. If yes, need to decompress it.
	 * If not, continue booting.
	 */
	if (is_gzip_package((unsigned char *)(data + page_size), hdr->kernel_size))
	{
		out_addr = (unsigned char *)target_get_scratch_address();
		out_addr = (unsigned char *)(out_addr + image_actual + page_size);
		out_avai_len = target_get_max_flash_size() - image_actual - page_size;
		dprintf(INFO, "decompressing kernel image: start\n");
		ret = decompress((unsigned char *)(ptr + page_size),
				hdr->kernel_size, out_addr, out_avai_len,
				&dtb_offset, &out_len);
		if (ret)
		{
			dprintf(CRITICAL, "decompressing image failed!!!\n");
			ASSERT(0);
		}

		dprintf(INFO, "decompressing kernel image: done\n");
		kptr = (struct kernel64_hdr *)out_addr;
		kernel_start_addr = out_addr;
		kernel_size = out_len;
	} else {
		kptr = (struct kernel64_hdr*)((char *)data + page_size);
		kernel_start_addr = (unsigned char *)((char *)data + page_size);
		kernel_size = hdr->kernel_size;
	}

	/*
	 * Update the kernel/ramdisk/tags address if the boot image header
	 * has default values, these default values come from mkbootimg when
	 * the boot image is flashed using fastboot flash:raw
	 */
	update_ker_tags_rdisk_addr(hdr, IS_ARM64(kptr));

	/* Get virtual addresses since the hdr saves physical addresses. */
	hdr->kernel_addr = VA(hdr->kernel_addr);
	hdr->ramdisk_addr = VA(hdr->ramdisk_addr);
	hdr->tags_addr = VA(hdr->tags_addr);

	kernel_size  = ROUND_TO_PAGE(kernel_size,  page_mask);
	/* Check if the addresses in the header are valid. */
	if (check_aboot_addr_range_overlap(hdr->kernel_addr, kernel_size) ||
		check_aboot_addr_range_overlap(hdr->ramdisk_addr, ramdisk_actual))
	{
		dprintf(CRITICAL, "kernel/ramdisk addresses overlap with aboot addresses.\n");
		return;
	}

#if DEVICE_TREE
	scratch_offset = image_actual + page_size + out_len;
	/* find correct dtb and copy it to right location */
	ret = copy_dtb(data, scratch_offset);

	dtb_copied = !ret ? 1 : 0;
#else
	if (check_aboot_addr_range_overlap(hdr->tags_addr, MAX_TAGS_SIZE))
	{
		dprintf(CRITICAL, "Tags addresses overlap with aboot addresses.\n");
		return;
	}
#endif

	/* Load ramdisk & kernel */
	memmove((void*) hdr->ramdisk_addr, ptr + page_size + kernel_actual, hdr->ramdisk_size);
	memmove((void*) hdr->kernel_addr, (char*) (kernel_start_addr), kernel_size);

#if DEVICE_TREE
	if (check_aboot_addr_range_overlap(hdr->tags_addr, kernel_actual))
	{
		dprintf(CRITICAL, "Tags addresses overlap with aboot addresses.\n");
		return;
	}

	/*
	 * If dtb is not found look for appended DTB in the kernel.
	 * If appended dev tree is found, update the atags with
	 * memory address to the DTB appended location on RAM.
	 * Else update with the atags address in the kernel header
	 */
	if (!dtb_copied) {
		void *dtb;
		dtb = dev_tree_appended((void*)(ptr + page_size),
					hdr->kernel_size, dtb_offset,
					(void *)hdr->tags_addr);
		if (!dtb) {
			fastboot_fail("dtb not found");
			return;
		}
	}
#endif

	fastboot_okay("");
	fastboot_stop();

	boot_linux((void*) hdr->kernel_addr, (void*) hdr->tags_addr,
		   (const char*) hdr->cmdline, board_machtype(),
		   (void*) hdr->ramdisk_addr, hdr->ramdisk_size);
}

void cmd_erase_nand(const char *arg, void *data, unsigned sz)
{
	struct ptentry *ptn;
	struct ptable *ptable;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		fastboot_fail("partition table doesn't exist");
		return;
	}

	ptn = ptable_find(ptable, arg);
	if (ptn == NULL) {
		fastboot_fail("unknown partition name");
		return;
	}
    
    dprintf(INFO, "@Ramos, arg(%s)erase nand partition:%s, size=%d", arg, ptn->name, ptn->length);
	if (flash_erase(ptn)) {
		fastboot_fail("failed to erase partition");
		return;
	}
	fastboot_okay("");
}


void cmd_erase_mmc(const char *arg, void *data, unsigned sz)
{
	unsigned long long ptn = 0;
	unsigned long long size = 0;
	int index = INVALID_PTN;
	uint8_t lun = 0;

#if VERIFIED_BOOT
	if(!strcmp(arg, KEYSTORE_PTN_NAME))
	{
		if(!device.is_unlocked)
		{
			fastboot_fail("unlock device to erase keystore");
			return;
		}
	}
#endif

	index = partition_get_index(arg);
	ptn = partition_get_offset(index);
	size = partition_get_size(index);

	if(ptn == 0) {
		fastboot_fail("Partition table doesn't exist\n");
		return;
	}

	lun = partition_get_lun(index);
	mmc_set_lun(lun);

	if (platform_boot_dev_isemmc())
	{
		if (mmc_erase_card(ptn, size)) {
			fastboot_fail("failed to erase partition\n");
			return;
		}
	} else {
		BUF_DMA_ALIGN(out, DEFAULT_ERASE_SIZE);
		size = partition_get_size(index);
		if (size > DEFAULT_ERASE_SIZE)
			size = DEFAULT_ERASE_SIZE;

		/* Simple inefficient version of erase. Just writing
	       0 in first several blocks */
		if (mmc_write(ptn , size, (unsigned int *)out)) {
			fastboot_fail("failed to erase partition");
			return;
		}
	}
	fastboot_okay("");
}

void cmd_erase(const char *arg, void *data, unsigned sz)
{
#if VERIFIED_BOOT
	if (target_build_variant_user())
	{
		if(!device.is_unlocked && !device.is_verified)
		{
			fastboot_fail("device is locked. Cannot erase");
			return;
		}
		if(!device.is_unlocked && device.is_verified)
		{
			if(!boot_verify_flash_allowed(arg))
			{
				fastboot_fail("cannot flash this partition in verified state");
				return;
			}
		}
	}
#endif

	if(target_is_emmc_boot())
		cmd_erase_mmc(arg, data, sz);
	else
		cmd_erase_nand(arg, data, sz);
}

static uint32_t aboot_get_secret_key()
{
	/* 0 is invalid secret key, update this implementation to return
	 * device specific unique secret key
	 */
	return 0;
}

void cmd_flash_mmc_img(const char *arg, void *data, unsigned sz)
{
	unsigned long long ptn = 0;
	unsigned long long size = 0;
	int index = INVALID_PTN;
	char *token = NULL;
	char *pname = NULL;
	char *sp;
	uint8_t lun = 0;
	bool lun_set = false;

	token = strtok_r((char *)arg, ":", &sp);
	pname = token;
	token = strtok_r(NULL, ":", &sp);
	if(token)
	{
		lun = atoi(token);
		mmc_set_lun(lun);
		lun_set = true;
	}

	if (pname)
	{
		if (!strncmp(pname, "frp-unlock", strlen("frp-unlock")))
		{
			if (!aboot_frp_unlock(pname, data, sz))
			{
				fastboot_info("FRP unlock successful");
				fastboot_okay("");
			}
			else
				fastboot_fail("Secret key is invalid, please update the bootloader with secret key");

			return;
		}

		if (!strcmp(pname, "partition"))
		{
			dprintf(INFO, "Attempt to write partition image.\n");
			if (write_partition(sz, (unsigned char *) data)) {
				fastboot_fail("failed to write partition");
				return;
			}
		}
		else
		{
#if VERIFIED_BOOT
			if(!strcmp(pname, KEYSTORE_PTN_NAME))
			{
				if(!device.is_unlocked)
				{
					fastboot_fail("unlock device to flash keystore");
					return;
				}
				if(!boot_verify_validate_keystore((unsigned char *)data,sz))
				{
					fastboot_fail("image is not a keystore file");
					return;
				}
			}
#endif
			index = partition_get_index(pname);
			ptn = partition_get_offset(index);
			if(ptn == 0) {
				fastboot_fail("partition table doesn't exist");
				return;
			}

			if (!strcmp(pname, "boot") || !strcmp(pname, "recovery")) {
				if (memcmp((void *)data, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
					fastboot_fail("image is not a boot image");
					return;
				}
			}

			if(!lun_set)
			{
				lun = partition_get_lun(index);
				mmc_set_lun(lun);
			}

			size = partition_get_size(index);
			if (ROUND_TO_PAGE(sz,511) > size) {
				fastboot_fail("size too large");
				return;
			}
			else if (mmc_write(ptn , sz, (unsigned int *)data)) {
				fastboot_fail("flash write failure");
				return;
			}
		}
	}
	fastboot_okay("");
	return;
}

void cmd_flash_meta_img(const char *arg, void *data, unsigned sz)
{
	int i, images;
	meta_header_t *meta_header;
	img_header_entry_t *img_header_entry;

	meta_header = (meta_header_t*) data;
	img_header_entry = (img_header_entry_t*) (data+sizeof(meta_header_t));

	images = meta_header->img_hdr_sz / sizeof(img_header_entry_t);

	for (i=0; i<images; i++) {

		if((img_header_entry[i].ptn_name == NULL) ||
			(img_header_entry[i].start_offset == 0) ||
			(img_header_entry[i].size == 0))
			break;

		cmd_flash_mmc_img(img_header_entry[i].ptn_name,
					(void *) data + img_header_entry[i].start_offset,
					img_header_entry[i].size);
	}

	if (!strncmp(arg, "bootloader", strlen("bootloader")))
	{
		strlcpy(device.bootloader_version, TARGET(BOARD), MAX_VERSION_LEN);
		strlcat(device.bootloader_version, "-", MAX_VERSION_LEN);
		strlcat(device.bootloader_version, meta_header->img_version, MAX_VERSION_LEN);
	}
	else
	{
		strlcpy(device.radio_version, TARGET(BOARD), MAX_VERSION_LEN);
		strlcat(device.radio_version, "-", MAX_VERSION_LEN);
		strlcat(device.radio_version, meta_header->img_version, MAX_VERSION_LEN);
	}

	write_device_info(&device);
	fastboot_okay("");
	return;
}

void cmd_flash_mmc_sparse_img(const char *arg, void *data, unsigned sz)
{
	unsigned int chunk;
	uint64_t chunk_data_sz;
	uint32_t *fill_buf = NULL;
	uint32_t fill_val;
	sparse_header_t *sparse_header;
	chunk_header_t *chunk_header;
	uint32_t total_blocks = 0;
	unsigned long long ptn = 0;
	unsigned long long size = 0;
	int index = INVALID_PTN;
	uint32_t i;
	uint8_t lun = 0;
	/*End of the sparse image address*/
	uint32_t data_end = (uint32_t)data + sz;

	index = partition_get_index(arg);
	ptn = partition_get_offset(index);
	if(ptn == 0) {
		fastboot_fail("partition table doesn't exist");
		return;
	}

	size = partition_get_size(index);

	lun = partition_get_lun(index);
	mmc_set_lun(lun);

	if (sz < sizeof(sparse_header_t)) {
		fastboot_fail("size too low");
		return;
	}

	/* Read and skip over sparse image header */
	sparse_header = (sparse_header_t *) data;

	if (((uint64_t)sparse_header->total_blks * (uint64_t)sparse_header->blk_sz) > size) {
		fastboot_fail("size too large");
		return;
	}

	data += sizeof(sparse_header_t);

	if (data_end < (uint32_t)data) {
		fastboot_fail("buffer overreads occured due to invalid sparse header");
		return;
	}

	if(sparse_header->file_hdr_sz != sizeof(sparse_header_t))
	{
		fastboot_fail("sparse header size mismatch");
		return;
	}

	dprintf (SPEW, "=== Sparse Image Header ===\n");
	dprintf (SPEW, "magic: 0x%x\n", sparse_header->magic);
	dprintf (SPEW, "major_version: 0x%x\n", sparse_header->major_version);
	dprintf (SPEW, "minor_version: 0x%x\n", sparse_header->minor_version);
	dprintf (SPEW, "file_hdr_sz: %d\n", sparse_header->file_hdr_sz);
	dprintf (SPEW, "chunk_hdr_sz: %d\n", sparse_header->chunk_hdr_sz);
	dprintf (SPEW, "blk_sz: %d\n", sparse_header->blk_sz);
	dprintf (SPEW, "total_blks: %d\n", sparse_header->total_blks);
	dprintf (SPEW, "total_chunks: %d\n", sparse_header->total_chunks);

	/* Start processing chunks */
	for (chunk=0; chunk<sparse_header->total_chunks; chunk++)
	{
		/* Make sure the total image size does not exceed the partition size */
		if(((uint64_t)total_blocks * (uint64_t)sparse_header->blk_sz) >= size) {
			fastboot_fail("size too large");
			return;
		}
		/* Read and skip over chunk header */
		chunk_header = (chunk_header_t *) data;
		data += sizeof(chunk_header_t);

		if (data_end < (uint32_t)data) {
			fastboot_fail("buffer overreads occured due to invalid sparse header");
			return;
		}

		dprintf (SPEW, "=== Chunk Header ===\n");
		dprintf (SPEW, "chunk_type: 0x%x\n", chunk_header->chunk_type);
		dprintf (SPEW, "chunk_data_sz: 0x%x\n", chunk_header->chunk_sz);
		dprintf (SPEW, "total_size: 0x%x\n", chunk_header->total_sz);

		if(sparse_header->chunk_hdr_sz != sizeof(chunk_header_t))
		{
			fastboot_fail("chunk header size mismatch");
			return;
		}

		if (!sparse_header->blk_sz ){
			fastboot_fail("Invalid block size\n");
			return;
		}

		chunk_data_sz = (uint64_t)sparse_header->blk_sz * chunk_header->chunk_sz;

		/* Make sure that the chunk size calculated from sparse image does not
		 * exceed partition size
		 */
		if ((uint64_t)total_blocks * (uint64_t)sparse_header->blk_sz + chunk_data_sz > size)
		{
			fastboot_fail("Chunk data size exceeds partition size");
			return;
		}

		switch (chunk_header->chunk_type)
		{
			case CHUNK_TYPE_RAW:
			if((uint64_t)chunk_header->total_sz != ((uint64_t)sparse_header->chunk_hdr_sz +
											chunk_data_sz))
			{
				fastboot_fail("Bogus chunk size for chunk type Raw");
				return;
			}

			if (data_end < (uint32_t)data + chunk_data_sz) {
				fastboot_fail("buffer overreads occured due to invalid sparse header");
				return;
			}

			/* chunk_header->total_sz is uint32,So chunk_data_sz is now less than 2^32
			   otherwise it will return in the line above
			 */
			if(mmc_write(ptn + ((uint64_t)total_blocks*sparse_header->blk_sz),
						(uint32_t)chunk_data_sz,
						(unsigned int*)data))
			{
				fastboot_fail("flash write failure");
				return;
			}
			if(total_blocks > (UINT_MAX - chunk_header->chunk_sz)) {
				fastboot_fail("Bogus size for RAW chunk type");
				return;
			}
			total_blocks += chunk_header->chunk_sz;
			data += (uint32_t)chunk_data_sz;
			break;

			case CHUNK_TYPE_FILL:
			if(chunk_header->total_sz != (sparse_header->chunk_hdr_sz +
											sizeof(uint32_t)))
			{
				fastboot_fail("Bogus chunk size for chunk type FILL");
				return;
			}

			fill_buf = (uint32_t *)memalign(CACHE_LINE, ROUNDUP(sparse_header->blk_sz, CACHE_LINE));
			if (!fill_buf)
			{
				fastboot_fail("Malloc failed for: CHUNK_TYPE_FILL");
				return;
			}

			if (data_end < (uint32_t)data + sizeof(uint32_t)) {
				fastboot_fail("buffer overreads occured due to invalid sparse header");
				return;
			}
			fill_val = *(uint32_t *)data;
			data = (char *) data + sizeof(uint32_t);

			for (i = 0; i < (sparse_header->blk_sz / sizeof(fill_val)); i++)
			{
				fill_buf[i] = fill_val;
			}

			for (i = 0; i < chunk_header->chunk_sz; i++)
			{
				/* Make sure that the data written to partition does not exceed partition size */
				if ((uint64_t)total_blocks * (uint64_t)sparse_header->blk_sz + sparse_header->blk_sz > size)
				{
					fastboot_fail("Chunk data size for fill type exceeds partition size");
					return;
				}

				if(mmc_write(ptn + ((uint64_t)total_blocks*sparse_header->blk_sz),
							sparse_header->blk_sz,
							fill_buf))
				{
					fastboot_fail("flash write failure");
					free(fill_buf);
					return;
				}

				total_blocks++;
			}

			free(fill_buf);
			break;

			case CHUNK_TYPE_DONT_CARE:
			if(total_blocks > (UINT_MAX - chunk_header->chunk_sz)) {
				fastboot_fail("bogus size for chunk DONT CARE type");
				return;
			}
			total_blocks += chunk_header->chunk_sz;
			break;

			case CHUNK_TYPE_CRC:
			if(chunk_header->total_sz != sparse_header->chunk_hdr_sz)
			{
				fastboot_fail("Bogus chunk size for chunk type CRC");
				return;
			}
			if(total_blocks > (UINT_MAX - chunk_header->chunk_sz)) {
				fastboot_fail("bogus size for chunk CRC type");
				return;
			}
			total_blocks += chunk_header->chunk_sz;
			if ((uint32_t)data > UINT_MAX - chunk_data_sz) {
				fastboot_fail("integer overflow occured");
				return;
			}
			data += (uint32_t)chunk_data_sz;
			if (data_end < (uint32_t)data) {
				fastboot_fail("buffer overreads occured due to invalid sparse header");
				return;
			}
			break;

			default:
			dprintf(CRITICAL, "Unkown chunk type: %x\n",chunk_header->chunk_type);
			fastboot_fail("Unknown chunk type");
			return;
		}
	}

	dprintf(INFO, "Wrote %d blocks, expected to write %d blocks\n",
					total_blocks, sparse_header->total_blks);

	if(total_blocks != sparse_header->total_blks)
	{
		fastboot_fail("sparse image write failure");
	}

	fastboot_okay("");
	return;
}

void cmd_flash_mmc(const char *arg, void *data, unsigned sz)
{
	sparse_header_t *sparse_header;
	meta_header_t *meta_header;

#ifdef SSD_ENABLE
	/* 8 Byte Magic + 2048 Byte xml + Encrypted Data */
	unsigned int *magic_number = (unsigned int *) data;
	int              ret=0;
	uint32           major_version=0;
	uint32           minor_version=0;

	ret = scm_svc_version(&major_version,&minor_version);
	if(!ret)
	{
		if(major_version >= 2)
		{
			if( !strcmp(arg, "ssd") || !strcmp(arg, "tqs") )
			{
				ret = encrypt_scm((uint32 **) &data, &sz);
				if (ret != 0) {
					dprintf(CRITICAL, "ERROR: Encryption Failure\n");
					return;
				}

				/* Protect only for SSD */
				if (!strcmp(arg, "ssd")) {
					ret = scm_protect_keystore((uint32 *) data, sz);
					if (ret != 0) {
						dprintf(CRITICAL, "ERROR: scm_protect_keystore Failed\n");
						return;
					}
				}
			}
			else
			{
				ret = decrypt_scm_v2((uint32 **) &data, &sz);
				if(ret != 0)
				{
					dprintf(CRITICAL,"ERROR: Decryption Failure\n");
					return;
				}
			}
		}
		else
		{
			if (magic_number[0] == DECRYPT_MAGIC_0 &&
			magic_number[1] == DECRYPT_MAGIC_1)
			{
				ret = decrypt_scm((uint32 **) &data, &sz);
				if (ret != 0) {
					dprintf(CRITICAL, "ERROR: Invalid secure image\n");
					return;
				}
			}
			else if (magic_number[0] == ENCRYPT_MAGIC_0 &&
				magic_number[1] == ENCRYPT_MAGIC_1)
			{
				ret = encrypt_scm((uint32 **) &data, &sz);
				if (ret != 0) {
					dprintf(CRITICAL, "ERROR: Encryption Failure\n");
					return;
				}
			}
		}
	}
	else
	{
		dprintf(CRITICAL,"INVALID SVC Version\n");
		return;
	}
#endif /* SSD_ENABLE */

#if VERIFIED_BOOT
	if (target_build_variant_user())
	{
		if(!device.is_unlocked)
		{
			fastboot_fail("device is locked. Cannot flash images");
			return;
		}
		if(!device.is_unlocked && device.is_verified)
		{
			if(!boot_verify_flash_allowed(arg))
			{
				fastboot_fail("cannot flash this partition in verified state");
				return;
			}
		}
	}
#endif

	sparse_header = (sparse_header_t *) data;
	meta_header = (meta_header_t *) data;
	if (sparse_header->magic == SPARSE_HEADER_MAGIC)
		cmd_flash_mmc_sparse_img(arg, data, sz);
	else if (meta_header->magic == META_HEADER_MAGIC)
		cmd_flash_meta_img(arg, data, sz);
	else
		cmd_flash_mmc_img(arg, data, sz);
	return;
}

void cmd_updatevol(const char *vol_name, void *data, unsigned sz)
{
	struct ptentry *sys_ptn;
	struct ptable *ptable;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		fastboot_fail("partition table doesn't exist");
		return;
	}

	sys_ptn = ptable_find(ptable, "system");
	if (sys_ptn == NULL) {
		fastboot_fail("system partition not found");
		return;
	}

	sz = ROUND_TO_PAGE(sz, page_mask);
	if (update_ubi_vol(sys_ptn, vol_name, data, sz))
		fastboot_fail("update_ubi_vol failed");
	else
		fastboot_okay("");
}

void cmd_flash_nand(const char *arg, void *data, unsigned sz)
{
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned extra = 0;
	unsigned bytes_to_round_page = 0;
	unsigned rounded_size = 0;

	if((uintptr_t)data > (UINT_MAX - sz)) {
		fastboot_fail("Cannot flash: image header corrupt");
                return;
        }

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		fastboot_fail("partition table doesn't exist");
		return;
	}

	ptn = ptable_find(ptable, arg);
	if (ptn == NULL) {
		dprintf(INFO, "unknown partition name (%s). Trying updatevol\n",
				arg);
		cmd_updatevol(arg, data, sz);
		return;
	}

	if (!strcmp(ptn->name, "boot") || !strcmp(ptn->name, "recovery")) {
		if((sz > BOOT_MAGIC_SIZE) && (!memcmp((void *)data, BOOT_MAGIC, BOOT_MAGIC_SIZE))) {
			dprintf(INFO, "Verified the BOOT_MAGIC in image header  \n");
		} else {
			fastboot_fail("Image is not a boot image");
			return;
		}
	}

	if (!strcmp(ptn->name, "system")
		|| !strcmp(ptn->name, "userdata")
		|| !strcmp(ptn->name, "persist")
		|| !strcmp(ptn->name, "recoveryfs")
		|| !strcmp(ptn->name, "modem"))
		extra = 1;
	else{
		rounded_size = ROUNDUP(sz, page_size);
		bytes_to_round_page = rounded_size - sz;
		if (bytes_to_round_page) {
			if (((uintptr_t)data + sz ) > (UINT_MAX - bytes_to_round_page)) {
				fastboot_fail("Integer overflow detected");
				return;
			}
			if (((uintptr_t)data + sz + bytes_to_round_page) >
				((uintptr_t)target_get_scratch_address() + target_get_max_flash_size())) {
				fastboot_fail("Buffer size is not aligned to page_size");
				return;
			}
			else {
				memset(data + sz, 0, bytes_to_round_page);
				sz = rounded_size;
			}
		}
	}

	dprintf(INFO, "writing %d bytes to '%s'\n", sz, ptn->name);
#if 1 // def  QUECTEL_SYSTEM_BACKUP  
if(!strcmp(ptn->name,"sys_back")
	|| !strcmp(ptn->name,"tz_b")
	|| !strcmp(ptn->name,"rpm_b")
	|| !strcmp(ptn->name,"aboot_b")
	|| !strcmp(ptn->name,"boot_b")
	|| !strcmp(ptn->name,"recoveryfs_b")
	|| !strcmp(ptn->name,"qdsp6sw_b")
	|| !strcmp(ptn->name,"modem_b"))
{
	 dprintf(INFO, "flash_write 111 writing %d bytes to '%s'  extra=%d \n", sz, ptn->name,extra);
	 extra =0 ;
	if (flash_write(ptn, extra, data, sz)) {
		fastboot_fail("flash write failure");
		return;
		}
}
else
{
#endif 	
	if ((sz > UBI_MAGIC_SIZE) && (!memcmp((void *)data, UBI_MAGIC, UBI_MAGIC_SIZE))) {
		
	dprintf(INFO, "flash_ubi_img writing %d bytes to '%s'\n", sz, ptn->name);
#if 1 // def  QUECTEL_SYSTEM_BACKUP  
/******************************************************************************************
Ramos-2018/04/13: 修复下载UBI 文件系统镜像时断电容易出现下次下载失败问题
Refer to [Issue-Depot].[IS0000118][Submitter:ramos.zhang,Date:2018-04-13]
<fastboot 下载UBI 文件系统镜像时断电，下次继续下载，容易出现下次读取给分区UBI数据异常造成无法下载。>
******************************************************************************************/
	 	dprintf(INFO, "UBI Image flash_write 111 writing %d bytes to '%s'  extra=%d \n", sz, ptn->name,extra);
	 	extra =0 ;
		if (flash_write(ptn, extra, data, sz)) {
			fastboot_fail("flash write failure");
			return;
			}
#else
		if (flash_ubi_img(ptn, data, sz)) {
			fastboot_fail("flash write failure");
			return;
		}
#endif
	} else {
		
	dprintf(INFO, "flash_write 222 writing %d bytes to '%s'  extra=%d \n", sz, ptn->name,extra);
		if (flash_write(ptn, extra, data, sz)) {
			fastboot_fail("flash write failure");
			return;
		}
	}
#if 1 // def  QUECTEL_SYSTEM_BACKUP  
}
#endif	
	dprintf(INFO, "partition '%s' updated\n", ptn->name);
	fastboot_okay("");
}

void cmd_flash(const char *arg, void *data, unsigned sz)
{
#if QUECTEL_FASTBOOT
	if(!quectel_is_fastboot_entry_force()) // if the fastboot flag is not set, 
	{
		if(0 != quectel_fastboot_force_entry_flag_set())
		{
			dprintf(INFO,"[Ramos] set fastboot force flag \n");
			fastboot_fail("set fastboot force flag failed");
			return;
		}
	}
#endif
	if(target_is_emmc_boot())
		cmd_flash_mmc(arg, data, sz);
	else
		cmd_flash_nand(arg, data, sz);
}

void cmd_continue(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
	fastboot_stop();

	if (target_is_emmc_boot())
	{
		boot_linux_from_mmc();
	}
	else
	{
		boot_linux_from_flash();
	}
}

void cmd_reboot(const char *arg, void *data, unsigned sz)
{
	dprintf(INFO, "rebooting the device\n");
#if QUECTEL_FASTBOOT
	quectel_fastboot_force_entry_flag_clean();	
#endif
	fastboot_okay("");
	reboot_device(0);
}

void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz)
{
	dprintf(INFO, "rebooting the device\n");
	fastboot_okay("");
	reboot_device(FASTBOOT_MODE);
}

void cmd_oem_enable_charger_screen(const char *arg, void *data, unsigned size)
{
	dprintf(INFO, "Enabling charger screen check\n");
	device.charger_screen_enabled = 1;
	write_device_info(&device);
	fastboot_okay("");
}

void cmd_oem_disable_charger_screen(const char *arg, void *data, unsigned size)
{
	dprintf(INFO, "Disabling charger screen check\n");
	device.charger_screen_enabled = 0;
	write_device_info(&device);
	fastboot_okay("");
}

void cmd_oem_select_display_panel(const char *arg, void *data, unsigned size)
{
	dprintf(INFO, "Selecting display panel %s\n", arg);
	if (arg)
		strlcpy(device.display_panel, arg,
			sizeof(device.display_panel));
	write_device_info(&device);
	fastboot_okay("");
}

void cmd_oem_unlock(const char *arg, void *data, unsigned sz)
{
	if(!is_allow_unlock) {
		fastboot_fail("oem unlock is not allowed");
		return;
	}

	display_fbcon_message("Oem Unlock requested");
	fastboot_fail("Need wipe userdata. Do 'fastboot oem unlock-go'");
}

void cmd_oem_unlock_go(const char *arg, void *data, unsigned sz)
{
	if(!device.is_unlocked || device.is_verified)
	{
		if(!is_allow_unlock) {
			fastboot_fail("oem unlock is not allowed");
			return;
		}

		device.is_unlocked = 1;
		device.is_verified = 0;
		write_device_info(&device);

		struct recovery_message msg;
		snprintf(msg.recovery, sizeof(msg.recovery), "recovery\n--wipe_data");
		write_misc(0, &msg, sizeof(msg));

		fastboot_okay("");
		reboot_device(RECOVERY_MODE);
	}
	fastboot_okay("");
}

static int aboot_frp_unlock(char *pname, void *data, unsigned sz)
{
	int ret = 1;
	uint32_t secret_key;
	char seckey_buffer[MAX_RSP_SIZE];

	secret_key = aboot_get_secret_key();
	if (secret_key)
	{
		snprintf((char *) seckey_buffer, MAX_RSP_SIZE, "%x", secret_key);
		if (!memcmp((void *)data, (void *)seckey_buffer, sz))
		{
			is_allow_unlock = true;
			write_allow_oem_unlock(is_allow_unlock);
			ret = 0;
		}
	}
	return ret;
}

void cmd_oem_lock(const char *arg, void *data, unsigned sz)
{
	/* TODO: Wipe user data */
	if(device.is_unlocked || device.is_verified)
	{
		device.is_unlocked = 0;
		device.is_verified = 0;
		write_device_info(&device);
	}
	fastboot_okay("");
}

void cmd_oem_verified(const char *arg, void *data, unsigned sz)
{
	/* TODO: Wipe user data */
	if(device.is_unlocked || !device.is_verified)
	{
		device.is_unlocked = 0;
		device.is_verified = 1;
		write_device_info(&device);
	}
	fastboot_okay("");
}

void cmd_oem_devinfo(const char *arg, void *data, unsigned sz)
{
	char response[128];
	snprintf(response, sizeof(response), "\tDevice tampered: %s", (device.is_tampered ? "true" : "false"));
	fastboot_info(response);
	snprintf(response, sizeof(response), "\tDevice unlocked: %s", (device.is_unlocked ? "true" : "false"));
	fastboot_info(response);
	snprintf(response, sizeof(response), "\tCharger screen enabled: %s", (device.charger_screen_enabled ? "true" : "false"));
	fastboot_info(response);
	snprintf(response, sizeof(response), "\tDisplay panel: %s", (device.display_panel));
	fastboot_info(response);
	fastboot_okay("");
}

void cmd_preflash(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
}

static uint8_t logo_header[LOGO_IMG_HEADER_SIZE];

int splash_screen_check_header(logo_img_header *header)
{
	if (memcmp(header->magic, LOGO_IMG_MAGIC, 8))
		return -1;
	if (header->width == 0 || header->height == 0)
		return -1;
	return 0;
}

int splash_screen_flash()
{
	struct ptentry *ptn;
	struct ptable *ptable;
	struct logo_img_header *header;
	struct fbcon_config *fb_display = NULL;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		dprintf(CRITICAL, "ERROR: Partition table not found\n");
		return -1;
	}

	ptn = ptable_find(ptable, "splash");
	if (ptn == NULL) {
		dprintf(CRITICAL, "ERROR: splash Partition not found\n");
		return -1;
	}
	if (flash_read(ptn, 0, (void *)logo_header, LOGO_IMG_HEADER_SIZE)) {
		dprintf(CRITICAL, "ERROR: Cannot read boot image header\n");
		return -1;
	}

	header = (struct logo_img_header *)logo_header;
	if (splash_screen_check_header(header)) {
		dprintf(CRITICAL, "ERROR: Boot image header invalid\n");
		return -1;
	}

	fb_display = fbcon_display();
	if (fb_display) {
		if (header->type && (header->blocks != 0)) { // RLE24 compressed data
			uint8_t *base = (uint8_t *) fb_display->base + LOGO_IMG_OFFSET;

			/* if the logo is full-screen size, remove "fbcon_clear()" */
			if ((header->width != fb_display->width)
						|| (header->height != fb_display->height))
					fbcon_clear();

			if (flash_read(ptn + LOGO_IMG_HEADER_SIZE, 0,
				(uint32_t *)base,
				(header->blocks * 512))) {
				dprintf(CRITICAL, "ERROR: Cannot read splash image from partition\n");
				return -1;
			}
			fbcon_extract_to_screen(header, base);
			return 0;
		}

		if ((header->width > fb_display->width) || (header->height > fb_display->height)) {
			dprintf(CRITICAL, "Logo config greater than fb config. Fall back default logo\n");
			return -1;
		}

		uint8_t *base = (uint8_t *) fb_display->base;
		uint32_t fb_size = ROUNDUP(fb_display->width *
					fb_display->height *
					(fb_display->bpp / 8), 4096);
		uint32_t splash_size = ((((header->width * header->height *
					fb_display->bpp/8) + 511) >> 9) << 9);

		if (splash_size > fb_size) {
			dprintf(CRITICAL, "ERROR: Splash image size invalid\n");
			return -1;
		}

		if (flash_read(ptn + LOGO_IMG_HEADER_SIZE, 0,
			(uint32_t *)base,
			((((header->width * header->height * fb_display->bpp/8) + 511) >> 9) << 9))) {
			fbcon_clear();
			dprintf(CRITICAL, "ERROR: Cannot read splash image from partition\n");
			return -1;
		}
	}

	return 0;
}

int splash_screen_mmc()
{
	int index = INVALID_PTN;
	unsigned long long ptn = 0;
	struct fbcon_config *fb_display = NULL;
	struct logo_img_header *header;
	uint32_t blocksize, realsize, readsize;
	uint8_t *base;

	index = partition_get_index("splash");
	if (index == 0) {
		dprintf(CRITICAL, "ERROR: splash Partition table not found\n");
		return -1;
	}

	ptn = partition_get_offset(index);
	if (ptn == 0) {
		dprintf(CRITICAL, "ERROR: splash Partition invalid\n");
		return -1;
	}

	mmc_set_lun(partition_get_lun(index));

	blocksize = mmc_get_device_blocksize();
	if (blocksize == 0) {
		dprintf(CRITICAL, "ERROR:splash Partition invalid blocksize\n");
		return -1;
	}

	fb_display = fbcon_display();
	if (!fb_display)
	{
		dprintf(CRITICAL, "ERROR: fb config is not allocated\n");
		return -1;
	}

	base = (uint8_t *) fb_display->base;

	if (mmc_read(ptn, (uint32_t *)(base + LOGO_IMG_OFFSET), blocksize)) {
		dprintf(CRITICAL, "ERROR: Cannot read splash image header\n");
		return -1;
	}

	header = (struct logo_img_header *)(base + LOGO_IMG_OFFSET);
	if (splash_screen_check_header(header)) {
		dprintf(CRITICAL, "ERROR: Splash image header invalid\n");
		return -1;
	}

	if (fb_display) {
		if (header->type && (header->blocks != 0)) { /* 1 RLE24 compressed data */
			base += LOGO_IMG_OFFSET;

			realsize =  header->blocks * 512;
			readsize =  ROUNDUP((realsize + LOGO_IMG_HEADER_SIZE), blocksize) - blocksize;

			/* if the logo is not full-screen size, clean screen */
			if ((header->width != fb_display->width)
						|| (header->height != fb_display->height))
				fbcon_clear();

			uint32_t fb_size = ROUNDUP(fb_display->width *
					fb_display->height *
					(fb_display->bpp / 8), 4096);

			if (readsize > fb_size) {
				dprintf(CRITICAL, "ERROR: Splash image size invalid\n");
				return -1;
			}

			if (mmc_read(ptn + blocksize, (uint32_t *)(base + blocksize), readsize)) {
				dprintf(CRITICAL, "ERROR: Cannot read splash image from partition\n");
				return -1;
			}

			fbcon_extract_to_screen(header, (base + LOGO_IMG_HEADER_SIZE));
		} else { /* 2 Raw BGR data */

			if ((header->width > fb_display->width) || (header->height > fb_display->height)) {
				dprintf(CRITICAL, "Logo config greater than fb config. Fall back default logo\n");
				return -1;
			}

			realsize =  header->width * header->height * fb_display->bpp / 8;
			readsize =  ROUNDUP((realsize + LOGO_IMG_HEADER_SIZE), blocksize) - blocksize;

			if (blocksize == LOGO_IMG_HEADER_SIZE) { /* read the content directly */
				if (mmc_read((ptn + LOGO_IMG_HEADER_SIZE), (uint32_t *)base, readsize)) {
					fbcon_clear();
					dprintf(CRITICAL, "ERROR: Cannot read splash image from partition\n");
					return -1;
				}
			} else {
				if (mmc_read(ptn + blocksize ,
						(uint32_t *)(base + LOGO_IMG_OFFSET + blocksize), readsize)) {
					dprintf(CRITICAL, "ERROR: Cannot read splash image from partition\n");
					return -1;
				}
				memmove(base, (base + LOGO_IMG_OFFSET + LOGO_IMG_HEADER_SIZE), realsize);
			}
		}
	}

	return 0;
}

int fetch_image_from_partition()
{
	if (target_is_emmc_boot()) {
		return splash_screen_mmc();
	} else {
		return splash_screen_flash();
	}
}

/* Get the size from partiton name */
static void get_partition_size(const char *arg, char *response)
{
	uint64_t ptn = 0;
	uint64_t size;
	int index = INVALID_PTN;

	index = partition_get_index(arg);

	if (index == INVALID_PTN)
	{
		dprintf(CRITICAL, "Invalid partition index\n");
		return;
	}

	ptn = partition_get_offset(index);

	if(!ptn)
	{
		dprintf(CRITICAL, "Invalid partition name %s\n", arg);
		return;
	}

	size = partition_get_size(index);

	snprintf(response, MAX_RSP_SIZE, "\t 0x%llx", size);
	return;
}

/*
 * Publish the partition type & size info
 * fastboot getvar will publish the required information.
 * fastboot getvar partition_size:<partition_name>: partition size in hex
 * fastboot getvar partition_type:<partition_name>: partition type (ext/fat)
 */
static void publish_getvar_partition_info(struct getvar_partition_info *info, uint8_t num_parts)
{
	uint8_t i;

	for (i = 0; i < num_parts; i++) {
		get_partition_size(info[i].part_name, info[i].size_response);

		if (strlcat(info[i].getvar_size, info[i].part_name, MAX_GET_VAR_NAME_SIZE) >= MAX_GET_VAR_NAME_SIZE)
		{
			dprintf(CRITICAL, "partition size name truncated\n");
			return;
		}
		if (strlcat(info[i].getvar_type, info[i].part_name, MAX_GET_VAR_NAME_SIZE) >= MAX_GET_VAR_NAME_SIZE)
		{
			dprintf(CRITICAL, "partition type name truncated\n");
			return;
		}

		/* publish partition size & type info */
		fastboot_publish((const char *) info[i].getvar_size, (const char *) info[i].size_response);
		fastboot_publish((const char *) info[i].getvar_type, (const char *) info[i].type_response);
	}
}

/* register commands and variables for fastboot */
void aboot_fastboot_register_commands(void)
{
	int i;

	struct fastboot_cmd_desc cmd_list[] = {
											/* By default the enabled list is empty. */
											{"", NULL},
											/* move commands enclosed within the below ifndef to here
											 * if they need to be enabled in user build.
											 */
#ifndef DISABLE_FASTBOOT_CMDS
											/* Register the following commands only for non-user builds */
											{"flash:", cmd_flash},
											{"erase:", cmd_erase},
											{"boot", cmd_boot},
											{"continue", cmd_continue},
											{"reboot", cmd_reboot},
											{"reboot-bootloader", cmd_reboot_bootloader},
											{"oem unlock", cmd_oem_unlock},
											{"oem unlock-go", cmd_oem_unlock_go},
											{"oem lock", cmd_oem_lock},
											{"oem verified", cmd_oem_verified},
											{"oem device-info", cmd_oem_devinfo},
											{"preflash", cmd_preflash},
											{"oem enable-charger-screen", cmd_oem_enable_charger_screen},
											{"oem disable-charger-screen", cmd_oem_disable_charger_screen},
											{"oem select-display-panel", cmd_oem_select_display_panel},
#if UNITTEST_FW_SUPPORT
											{"oem run-tests", cmd_oem_runtests},
#endif
#endif
										  };

	int fastboot_cmds_count = sizeof(cmd_list)/sizeof(cmd_list[0]);
	for (i = 1; i < fastboot_cmds_count; i++)
		fastboot_register(cmd_list[i].name,cmd_list[i].cb);

	/* publish variables and their values */
	fastboot_publish("product",  TARGET(BOARD));
	fastboot_publish("kernel",   "lk");
	fastboot_publish("serialno", sn_buf);

	/*
	 * partition info is supported only for emmc partitions
	 * Calling this for NAND prints some error messages which
	 * is harmless but misleading. Avoid calling this for NAND
	 * devices.
	 */
	if (target_is_emmc_boot())
		publish_getvar_partition_info(part_info, ARRAY_SIZE(part_info));

	/* Max download size supported */
	snprintf(max_download_size, MAX_RSP_SIZE, "\t0x%x",
			target_get_max_flash_size());
	fastboot_publish("max-download-size", (const char *) max_download_size);
	/* Is the charger screen check enabled */
	snprintf(charger_screen_enabled, MAX_RSP_SIZE, "%d",
			device.charger_screen_enabled);
	fastboot_publish("charger-screen-enabled",
			(const char *) charger_screen_enabled);
	snprintf(panel_display_mode, MAX_RSP_SIZE, "%s",
			device.display_panel);
	fastboot_publish("display-panel",
			(const char *) panel_display_mode);
	fastboot_publish("version-bootloader", (const char *) device.bootloader_version);
	fastboot_publish("version-baseband", (const char *) device.radio_version);
}

#if QUECTEL_FASTBOOT
/* author:	sam
 * date:	2014/09/19
 * purpose:	CTL+C key, enter the FastBoot
 */
int quectel_fource_boot(void)
{
	char ch, fh = 0;
	int i;
	dprintf(CRITICAL,"CTL+C key, enter the FastBoot\n");
	for (i = 2; i != 0; --i)
	{
		if (!getc(&ch))
		{
			dprintf(CRITICAL,"%s ch: %x\n", __func__, ch);
			if (ch == 0x03) ++fh;
			else fh = 0;
			if (fh >= 1) return 1;
		}
		mdelay(10);
	}
	return 0;
}	
#endif

#if MMC_SDHCI_SUPPORT
int quectel_f_mount(void);
unsigned quectel_f_read(const char *fname, void *buff, unsigned size);
void target_sdc_init(void);
void target_sdc_uninit(void);

static void quectel_cmd_flash_sd(const char *ptn, const char *file) {
	char update_file[256];
	unsigned f_size;
	void *buff = target_get_scratch_address();
	unsigned size = target_get_max_flash_size();

	snprintf(update_file, sizeof(update_file), "0:\\update\\%s", file);
	f_size = quectel_f_read(update_file, buff, size);
	if (f_size)
		cmd_flash_nand(ptn, buff, f_size);
};

static void quectel_sd_fastboot(void) {
	target_sdc_init();
	
	if (target_mmc_device() && quectel_f_mount() == 0) {
		quectel_cmd_flash_sd("sbl", "sbl1.mbn");
		//quectel_cmd_flash_sd("mibib", "partition.mbn");
		quectel_cmd_flash_sd("tz", "tz.mbn");
		quectel_cmd_flash_sd("rpm", "rpm.mbn");
		//quectel_cmd_flash_sd("aboot", "appsboot.mbn");
		quectel_cmd_flash_sd("boot", "mdm9607-perf-boot.img");
		quectel_cmd_flash_sd("modem", "NON-HLOS.ubi");
		quectel_cmd_flash_sd("recovery", "mdm9607-perf-boot.img");
		quectel_cmd_flash_sd("recoveryfs", "mdm-perf-recovery-image-mdm9607-perf.ubi");
		quectel_cmd_flash_sd("sys_back", "mdm9607-perf-sysfs.ubi");	
		quectel_cmd_flash_sd("system", "mdm9607-perf-sysfs.ubi");
	}

	target_sdc_uninit();
}
#endif

//add by hertz.zhou 2017.02.23 
#if 1
#include <platform/gpio.h>

#define CMD_BUFFER_LEN	64
int cmd_buffer_cnt = 0;
char cmd_buffer[CMD_BUFFER_LEN];

struct gpio_test_s{
	uint32_t mod_pin;
	uint32_t bb_gpio;
};

//the table which include the pins to be tested
struct gpio_test_s gpio_test_table[] = {
	{1, 25},		//WAKEUP_IN
	{2, 10},		//AP_READY
	{3, 42},		//SLEEP_IND
	{4, 11},		//W_DISABLE#
	{13, 34},		//USIM_PRESENCE
	{23, 26},		//SD1_INS_DET
	{24, 76},		//PCM_IN
	{25, 77},		//PCM_OUT
	{26, 79},		//PCM_SYNC
	{27, 78},		//PCM_CLK
	{37, 22},		//SPI_CS
	{38, 20},		//SPI_MOSI
	{39, 21},		//SPI_MISO
	{40, 23},		//SPI_CLK
	{41, 7},		//I2C_SCL
	{42, 6},		//I2C_SDA
	{62, 75},		//RI
	{63, 4},		//DCD
	{64, 3},		//CTS
	{65, 2},		//RTS
	{66, 5},		//DTR
	{67, 0},		//TXD
	{68, 1},		//RXD
};

const int gpio_test_table_size = sizeof(gpio_test_table) / sizeof(struct gpio_test_s);

int gpio_is_in_table(uint32_t gpio)
{
	int ret = 0;
	int i;

	for(i = 0; i < gpio_test_table_size; i++)
	{
		if(gpio_test_table[i].mod_pin == gpio)
		{
			ret = 1;
			break;
		}
	}

	return ret;
}

uint32_t gpio_mod_pin_mapto_bb(uint32_t mod_pin)
{
	int i;

	for(i = 0; i < gpio_test_table_size; i++)
	{
		if(gpio_test_table[i].mod_pin == mod_pin)
		{
			return gpio_test_table[i].bb_gpio;
		}
	}

	return 0;
}
/*
 *you should input,for example ,"GPIO,23",to test if pin23 is ok
 and if ok ,the response will be "OKIO,23"
 * */
void gpio_test_mode(void)
{
	char ch;
	uint32_t gpio, enable, i;
	
	cmd_buffer_cnt = 0;
	memset(cmd_buffer, 0, CMD_BUFFER_LEN);

	while(1)
	{
		ch = uart_getc(0, 1);
		putc(ch);

		if(ch == 0x0a)//change line
		{
			continue;
		}
		
		if(ch == 0x0d)//if ch is enter
		{	
			if(memcmp(cmd_buffer, "GPIO,", 5))//if the input start with "GPIO,"
			{
				printf("%s: %d\n", __func__, __LINE__);
				goto error;
			}
			
			gpio = 0;
			enable = 0;
			i = 5;
			while(isdigit(cmd_buffer[i]))//get the pin number.example "GPIO,23"
			{
				gpio *= 10;
				gpio += cmd_buffer[i] - '0';
				i++;
			}

			if(gpio == 999)//exit test
			{
				break;
			}

			i++;
			enable += cmd_buffer[i] - '0';

			if((gpio != 0 && !gpio_is_in_table(gpio)) || (enable != 0 && enable !=1))
			{
				printf("%s: %d\n", __func__, __LINE__);
				goto error;
			}
			
			if(!gpio)
			{
				for(i = 0; i < gpio_test_table_size; i++)//test all pins in the table 
				{
					gpio_tlmm_config(gpio_test_table[i].bb_gpio, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA, GPIO_DISABLE);
					dsb();
					gpio_set_val(gpio_test_table[i].bb_gpio, enable);
					dsb();
				}
			}
			//the special pin to be test
			else
			{
				gpio = gpio_mod_pin_mapto_bb(gpio);
				gpio_tlmm_config(gpio, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA, GPIO_DISABLE);
				dsb();
				gpio_set_val(gpio, enable);
				dsb();
			}
			
			cmd_buffer_cnt = 0;
			memset(cmd_buffer, 0, CMD_BUFFER_LEN);
			printf("OK\r\n");
			continue;
error:
			printf("ERROR\r\n");
			cmd_buffer_cnt = 0;
			memset(cmd_buffer, 0, CMD_BUFFER_LEN);
		}
		else
		{
			cmd_buffer[cmd_buffer_cnt++] = ch;
			if(cmd_buffer_cnt == CMD_BUFFER_LEN)
			{
				cmd_buffer_cnt = 0;
			}
		}
	}

	for(i = 0; i < gpio_test_table_size; i++)
	{
		gpio_tlmm_config(gpio_test_table[i].bb_gpio, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA, GPIO_DISABLE);
		dsb();
	}
}
#endif
//end hertz.zhou 2017.02.23


void aboot_init(const struct app_descriptor *app)
{
	unsigned reboot_mode = 0;
	unsigned usb_init = 0;
	unsigned sz = 0;
	char ch;
	int fh = 0;
	char diag_cnt = 0;
	int i;

	/* Setup page size information for nv storage */
	if (target_is_emmc_boot())
	{
		page_size = mmc_page_size();
		page_mask = page_size - 1;
	}
	else
	{
		page_size = flash_page_size();
		page_mask = page_size - 1;
	}

	ASSERT((MEMBASE + MEMSIZE) > MEMBASE);

	read_device_info(&device);
	read_allow_oem_unlock(&device);

	/* Display splash screen if enabled */
#if DISPLAY_SPLASH_SCREEN
#if NO_ALARM_DISPLAY
	if (!check_alarm_boot()) {
#endif
		dprintf(SPEW, "Display Init: Start\n");
#if ENABLE_WBC
		/* Wait if the display shutdown is in progress */
		while(pm_app_display_shutdown_in_prgs());
		if (!pm_appsbl_display_init_done())
			target_display_init(device.display_panel);
		else
			display_image_on_screen();
#else
		target_display_init(device.display_panel);
#endif
		dprintf(SPEW, "Display Init: Done\n");
#if NO_ALARM_DISPLAY
	}
#endif
#endif

	target_serialno((unsigned char *) sn_buf);
	dprintf(SPEW,"serial number: %s\n",sn_buf);

	memset(display_panel_buf, '\0', MAX_PANEL_BUF_SIZE);
#if QUECTEL_FASTBOOT
	/* this function use for check fastboot download,
	if fastboot download not completely, the system must entry fastboot again for download*/
	if(quectel_is_fastboot_entry_force())// check fastboot download flag.
	{
		goto fastboot;
	}
#endif
	
//add hertz.zhou 2017.02.23
#define	QUECTEL_FASTBOOT	1
#if QUECTEL_FASTBOOT
	
/*
 * input CTRL+C,and choose PINTEST mode or fastboot mode
 * when the test ended goto exit_pintest_mode
 * */
	printf("CTRL+C: enter instruction mode\n");
#ifndef QUECTEL_FLASH_SUPPORT_1G
	printf("RECOVERY,");
#endif
	printf("PINTEST OR FASTBOOT\n\n\n\n");

	for (i = 10; i != 0; --i)
	{
		if (!getc(&ch) && ch == 0x03)
		{
			printf("PINTEST: test gpio(s) connectivity.\n");
			printf("fastboot: set fastboot mode.\n");
#ifndef QUECTEL_FLASH_SUPPORT_1G
			printf("recovery: set recovery mode.\n");
#endif
			while(1)
			{
				ch = uart_getc(0, 1);
				putc(ch);

				if((ch >= 'A' && ch <= 'Z')
				 ||(ch >= 'a' && ch <= 'z')
				 || ch == 0x0d)
				{
					if(ch == 0x0d)
					{
						if(strlen("PINTEST") == cmd_buffer_cnt
							&& !memcmp(cmd_buffer, "PINTEST", cmd_buffer_cnt))
						{
							printf("PIN TEST MODE\r\n", __func__);
							gpio_test_mode();
							goto exit_pintest_mode;
						}
						else if(strlen("fastboot") == cmd_buffer_cnt
							&& !memcmp(cmd_buffer, "fastboot", cmd_buffer_cnt))
						{
							printf("%s: going to fastboot mode.\n", __func__);
							goto fastboot;
						}
                        // add by len for manual into recovery, 2018-1-18
#ifndef QUECTEL_FLASH_SUPPORT_1G
						else if(strlen("recovery") == cmd_buffer_cnt
                            && !memcmp(cmd_buffer, "recovery", cmd_buffer_cnt))
                        {
                            boot_into_recovery = 1;
                            strcpy(quectel_cmdline," recovery=1");
                            printf("%s: boot to recoveryfs.\n", __func__);
                            goto normal_boot;
                        }
#endif
						//add end

					}
					else
					{
						cmd_buffer[cmd_buffer_cnt++] = ch;
						if(cmd_buffer_cnt == CMD_BUFFER_LEN)
						{
							cmd_buffer_cnt = 0;
						}
					}
				}
				else
				{
					cmd_buffer_cnt = 0;
				}
			}
		}
		
		printf("%s char: %c\n", __func__, ch);
		mdelay(10);
	}
exit_pintest_mode:
#endif
//end hertz.zhou 2017.02.23

#if 1 //def QUECTEL_MODEM_RESTORE_UBI
#ifdef QUECTEL_ALL_RESTORE
	if(QUECTEL_ALL_RESTORE_BEGIN == Ql_check_RestoreFlag())
	{
		/*if found the module need all(system,modem,boot) resore,we set a Allrestoreing flag in other block .
		 and we clean this flag after all partitin restore completely. this flag is usred for all(system,modem,boot) partitino
		 restore is independent. 
		*/
		Ql_Set_AllRestoring_Flag(1); //set flag
	}
	else
	{
		if((1 == Ql_check_AllRestoring_Flag()) && (QUECTEL_RESTOREFLAG_NONE == g_Restore_stage))
		{
			/* if the AllRestoeing flag is not clean, but the restores flag is empty.
				that means the restores flag is lost by sometimes. maybe is lost when system(modme) restore ok, 
				and clean the system(modem) restore flag, but the power is shutdown, this case restores flags lost.
				So maybe the system( or modem or boot) are not restored, and we don't know which oen is. 
				So we need set the all	restore flags in here !!!!!!!				*/

			Ql_Set_Restore_Flags(1,1,1,1);

		}
	}
#endif
	if(1 == g_QuecBackupInfo.linuxfs_restore_flag)// linux rootfs restore
	{
		dprintf(CRITICAL,"@Ramos Restore the linux FS  start  now\n");
		Ql_Restore_partition("sys_back", "system");
	}
	if(1 == g_QuecBackupInfo.cefs_restore_flag)  // modem cefs restore
	{
		dprintf(CRITICAL,"@Ramos Restore the CEFS start  now\n");
		Ql_Restore_partition("sys_rev", "efs2");
	}

	if(1 == g_QuecBackupInfo.modem_restore_flag)// modem ubi restore
	{
		dprintf(CRITICAL,"@Ramos Restore the modem start  now\n");
		Ql_Restore_partition("qdsp6sw_b", "modem");
	}
	if(1 == g_QuecBackupInfo.recovery_restore_flag)
	{
		dprintf(CRITICAL,"@Darren Restore the recovery FS  start  now\n");
		Ql_Restore_partition("recoveryfs_b", "recoveryfs");
	}
	if(1 == g_QuecBackupInfo.image_restoring_flag)
	{
		dprintf(CRITICAL,"@Darren Restore the boot image start  now\n");
		Ql_Restore_partition("recovery", "boot");	
	}

	#ifdef QUECTEL_ALL_RESTORE
	if(1 == g_AllRestoring_flag)
	{
		/* all(system,modem,boot) restore completely, clean the AllRestoreing Flag */
		Ql_Set_AllRestoring_Flag(0); //clean flag
	}
	#endif

#endif

	/*
	 * Check power off reason if user force reset,
	 * if yes phone will do normal boot.
	 */
	if (is_user_force_reset())
		goto normal_boot;

	/* Check if we should do something other than booting up */
	if (keys_get_state(KEY_VOLUMEUP) && keys_get_state(KEY_VOLUMEDOWN))
	{
		dprintf(ALWAYS,"dload mode key sequence detected\n");
		if (set_download_mode(EMERGENCY_DLOAD))
		{
			dprintf(CRITICAL,"dload mode not supported by target\n");
		}
		else
		{
			reboot_device(DLOAD);
			dprintf(CRITICAL,"Failed to reboot into dload mode\n");
		}
		boot_into_fastboot = true;
	}
	if (!boot_into_fastboot)
	{
		if (keys_get_state(KEY_HOME) || keys_get_state(KEY_VOLUMEUP))
			boot_into_recovery = 1;
		if (!boot_into_recovery &&
			(keys_get_state(KEY_BACK) || keys_get_state(KEY_VOLUMEDOWN)))
			boot_into_fastboot = true;
	}
	#if NO_KEYPAD_DRIVER
	if (fastboot_trigger())
		boot_into_fastboot = true;
	#endif

#if USE_PON_REBOOT_REG
	reboot_mode = check_hard_reboot_mode();
#else
	reboot_mode = check_reboot_mode();
#endif
	if (reboot_mode == RECOVERY_MODE)
	{
        boot_into_recovery = 1;
	}
	else if(reboot_mode == FASTBOOT_MODE)
	{
		boot_into_fastboot = true;
	}
	else if(reboot_mode == ALARM_BOOT)
	{
		boot_reason_alarm = true;
	}

#if MMC_SDHCI_SUPPORT
	quectel_sd_fastboot();
#endif
normal_boot:
	if (!boot_into_fastboot)
	{
		if (target_is_emmc_boot())
		{
			if(emmc_recovery_init())
				dprintf(ALWAYS,"error in emmc_recovery_init\n");
			if(target_use_signed_kernel())
			{
				if((device.is_unlocked) || (device.is_tampered))
				{
				#ifdef TZ_TAMPER_FUSE
					set_tamper_fuse_cmd();
				#endif
				#if USE_PCOM_SECBOOT
					set_tamper_flag(device.is_tampered);
				#endif
				}
			}

			boot_linux_from_mmc();
		}
		else
		{
			recovery_init();
			fota_info_check();
	#if USE_PCOM_SECBOOT
		if((device.is_unlocked) || (device.is_tampered))
			set_tamper_flag(device.is_tampered);
	#endif
			boot_linux_from_flash();
		}
		dprintf(CRITICAL, "ERROR: Could not do normal boot. Reverting "
			"to fastboot mode.\n");
	}
#if QUECTEL_FASTBOOT
/* author:	sam
 * date:	2014/09/19
 * purpose:	CTL+C key, enter the FastBoot
 */
fastboot:
#endif
	/* We are here means regular boot did not happen. Start fastboot. */

	/* register aboot specific fastboot commands */
	aboot_fastboot_register_commands();

	/* dump partition table for debug info */
	partition_dump();

	/* initialize and start fastboot */
	fastboot_init(target_get_scratch_address(), target_get_max_flash_size());
}

uint32_t get_page_size()
{
	return page_size;
}

/*
 * Calculated and save hash (SHA256) for non-signed boot image.
 *
 * @param image_addr - Boot image address
 * @param image_size - Size of the boot image
 *
 * @return int - 0 on success, negative value on failure.
 */
static int aboot_save_boot_hash_mmc(uint32_t image_addr, uint32_t image_size)
{
	unsigned int digest[8];
#if IMAGE_VERIF_ALGO_SHA1
	uint32_t auth_algo = CRYPTO_AUTH_ALG_SHA1;
#else
	uint32_t auth_algo = CRYPTO_AUTH_ALG_SHA256;
#endif

	target_crypto_init_params();
	hash_find((unsigned char *) image_addr, image_size, (unsigned char *)&digest, auth_algo);

	save_kernel_hash_cmd(digest);
	dprintf(INFO, "aboot_save_boot_hash_mmc: imagesize_actual size %d bytes.\n", (int) image_size);

	return 0;
}

APP_START(aboot)
	.init = aboot_init,
APP_END
