/**
 * @file        quectel_branch_config.h
 * @brief       Quectel BRANCH MACROs Define.
 * @author      Running.qian
 * @copyright   Copyright (c) 2017-2020 @ Quectel Wireless Solutions Co., Ltd.
 */

#ifndef QL_BRANCH_CONFIG_H
#define QL_BRANCH_CONFIG_H

/******************  BRANCH MACRO DEFINE, Priority Level: Medium ***********************************///
//#if QL_BRANCH_R02
/******************* ReDefine FUNCS MACRO  ********************************/
//#undef    QL_FUNC_XXX
//#define   QL_FUNC_XXX     1

/******************* SPECIFIC FUNCS MACRO  ********************************/
//#define   QL_FUNC_YYY
//#endif

#if defined(QL_G_BRANCH_R02)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   0

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_BRANCH_R05)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_BRANCH_R06)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   0

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_BRANCH_R06_updat01)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   0

/******************* SPECIFIC FUNCS MACRO  ********************************/

#else
//#warning "<<<<<<<<<<<<<<<<<<Undefine BRANCH NAME>>>>>>>>>>>>>>>>>>>>>>>>>>>"
#endif

#endif //#ifdef  QL_G_BRANCH_CONFIG_H