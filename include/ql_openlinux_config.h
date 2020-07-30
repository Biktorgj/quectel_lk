/**
 * @file        quectel_openlinux_config.h
 * @brief       Quectel BRANCH MACROs Define.
 * @author      Running.qian
 * @copyright   Copyright (c) 2017-2020 @ Quectel Wireless Solutions Co., Ltd.
 */

#ifndef QL_OPENLINUX_CONFIG_H
#define QL_OPENLINUX_CONFIG_H

/******************  OPENLINUX MACRO DEFINE, Priority Level: Medium1 ***********************************///

#if defined(QL_G_OPENLINUX_MODE)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   0

/******************* SPECIFIC FUNCS MACRO  ********************************/

#else
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/
#endif

#endif //#ifdef  QL_G_BRANCH_CONFIG_H