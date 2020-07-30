/**
 * @file        quectel_product_config.h
 * @brief       Quectel PRODUCT MACROs Define.
 * @author      Running.qian
 * @copyright   Copyright (c) 2017-2020 @ Quectel Wireless Solutions Co., Ltd.
 */

#ifndef QL_PRODUCT_CONFIG_H
#define QL_PRODUCT_CONFIG_H

/******************  QUECTEL PRODUCT MACRO, Priority Level: Low1  ***********************************///
//#if QL_G_PRODUCT_EC20C_CFA
/******************* ReDefine FUNCS MACRO  ********************************/
//    #undef    QL_G_FUNC_XXX
//    #define   QL_G_FUNC_XXX     1

/******************* SPECIFIC FUNCS MACRO  ********************************/
//    #define   QL_G_FUNC_YYY
//#endif


#if defined(QL_G_PRODUCT_EC20C_FA)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20CE_FA)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20CE_FD)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20C_FD)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20C_FT)
/******************* ReDefine FUNCS MACRO  ********************************/
    #ifdef FEATURE_QUECTEL_DNSMASQ
    #undef FEATURE_QUECTEL_DNSMASQ
    #endif
    #define FEATURE_QUECTEL_DNSMASQ 1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21C_FD)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20CFD_HIK)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20CE_FAG)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20C_FAG)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20CE_FDG)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20C_FDG)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20C_FTG)
/******************* ReDefine FUNCS MACRO  ********************************/
    #ifdef FEATURE_QUECTEL_DNSMASQ
    #undef FEATURE_QUECTEL_DNSMASQ
    #endif
    #define FEATURE_QUECTEL_DNSMASQ 1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21C_FDG)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20CFDG_HIK)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25E)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21E)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21E_FB)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21AUT)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21AUTL)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25AUT)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25AUTL)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25AU)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21AU)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21AUV)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21A)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25A)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21AL)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21AS)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21V)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21VD)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25VD)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1


/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25V)
/******************* ReDefine FUNCS MACRO  ********************************/
    #undef  QL_G_FUNC_OMADM
    #define QL_G_FUNC_OMADM   1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20C_GW)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20CE_GW)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21CT)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21KL)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25J)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21J)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20CE_CT)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EG91E)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EG95E)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EG91EB)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EG95EB)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EG91NA)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EG95NA)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21NA)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EM05E)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25J_SB)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC21J_SB)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC25M)
/******************* ReDefine FUNCS MACRO  ********************************/

/******************* SPECIFIC FUNCS MACRO  ********************************/

#elif defined(QL_G_PRODUCT_EC20C_FTB)
/******************* ReDefine FUNCS MACRO  ********************************/
    #ifdef FEATURE_QUECTEL_DNSMASQ
    #undef FEATURE_QUECTEL_DNSMASQ
    #endif
    #define FEATURE_QUECTEL_DNSMASQ 1

/******************* SPECIFIC FUNCS MACRO  ********************************/

#else //error
//#error "<<<<<<<<<<<<<<<<<<Undefine PRODUCT NAME>>>>>>>>>>>>>>>>>>>>>>>>>>>"
#endif

#endif //#ifdef  QL_G_PRODUCT_CONFIG_H
