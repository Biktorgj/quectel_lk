/**
 * @file        quectel_FUNCS_config.h
 * @brief       Quectel FUNCS MACROs Define.
 * @author      Running.qian
 * @copyright   Copyright (c) 2017-2020 @ Quectel Wireless Solutions Co., Ltd.
 */

#ifndef  QL_FUNCS_CONFIG_H
#define  QL_FUNCS_CONFIG_H

/******************  BASE FUNCS MACRO DEFINE, Priority Level: Low2 ***********************************///
//#define QL_G_FUNC_EXAMPLE1    1
//#define QL_G_FUNC_EXAMPLE2    0

#define QL_G_FUNC_OMADM   0

//Laurence.yin-2018/04/19-QCM9XOL00004C014-P01, <[MCM-SMS] : add long sms and sns time func macro.>
#define QL_LONG_SMS_SURPORT
#define QL_TIMESTAMP_SMS_SURPORT


//Laurence.yin-2018/04/20-QCM9XOL00004C015-P01, <[DM] : add dm surport macro.>
#define QL_SCA_SMS_SURPORT
#define QL_DM_SERVICE_SURPORT

//Laurence.yin-2018/04/23-QCM9XOL00004C013-P01, <[VOICE] : add voice surport macro.>
#define QL_VOICE_SERVICE_SURPORT

#endif //QL_FUNCS_CONFIG_H
