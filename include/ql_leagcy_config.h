/**
 * @file        quectel_leagcy_config.h
 * @brief       Quectel Leagcy MACROs Define.
 * @author      Running.qian
 * @copyright   Copyright (c) 2017-2020 @ Quectel Wireless Solutions Co., Ltd.
 */

#ifndef QUECTEL_LEAGCY_CONFIG_H
#define QUECTEL_LEAGCY_CONFIG_H


#if defined (__QUECTEL_PROJECT_EC20CE_CT__)
    #define QUECTEL_CTCC_SUPPORT
    #define QUECTEL_CTCC_PRIVATE
#endif

#ifdef QUECTEL_FEATURE_OPENLINUX
    #define QUECTEL_OPEN_LINUX_SUPPORT       1
#else
    #define QUECTEL_OPEN_LINUX_SUPPORT       0
#endif

#if defined (__QUECTEL_PROJECT_EC20C_FT__) || defined (__QUECTEL_PROJECT_EC20C_FTB__) || defined (__QUECTEL_PROJECT_EC20C_FTG__) 
#define FEATURE_QUECTEL_DNS
#endif

/*** OMA-DM Features Macro Area ***/
#define QUECTEL_OMA_DM 1
#if QUECTEL_OMA_DM
#if defined(__QUECTEL_PROJECT_EC21V__) || defined(__QUECTEL_PROJECT_EC21NA__) || defined(__QUECTEL_PROJECT_EC25NA__)
    #define QUECTEL_ODM_UPDATE_AUTO
#endif

#define QUECTEL_ODM_UPDATE_AUTOREPORT     0
#endif//QUECTEL_OMA_DM


/******quectel modem rootfs backup restore function**********/
/******quectel modem rootfs backup restore function**********/
#if defined (__QUECTEL_PROJECT_EC20CE_FAG__) || defined (__QUECTEL_PROJECT_EC20C_FAG__) ||\
    defined (__QUECTEL_PROJECT_EC20CE_FDG__) || defined (__QUECTEL_PROJECT_EC20C_FDG__) \
    || defined (__QUECTEL_PROJECT_EC20C_FTG__) || defined (__QUECTEL_PROJECT_EC21C_FDG__) ||\
    defined (__QUECTEL_PROJECT_EC20CFDG_HIK__) || defined (__QUECTEL_PROJECT_EC20CE_FD__) || \
    defined (__QUECTEL_PROJECT_EC21E__) || defined (__QUECTEL_PROJECT_EC25E__) || \
    defined (__QUECTEL_PROJECT_EC21AU__) || defined (__QUECTEL_PROJECT_EC25AU__)
#define QUECTEL_MODEM_RESTORE_UBI
#endif


#define QUECTEL_MBIM_FEATURE
#ifdef QUECTEL_MBIM_FEATURE
    #define QUECTEL_MBIM_LOG
#endif

#define QUECTEL_CONTROL_ICCID_DISPLAY //lory2017/05/31 fix QC3655 if ICCID include hex A to F ,cannot display content after that
#define QUECTEL_QC_AUTOCONNECT_CTL //add by maxTANG to make qcautoconnect more reasonable //maxcodeflag20170929
    
#endif //ifdef QUECTEL_LEAGCY_CONFIG_H
