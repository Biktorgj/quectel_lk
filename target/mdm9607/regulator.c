/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
 *     * Neither the name of The Linux Fundation, Inc. nor the names of its
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
 *
 */

#include <regulator.h>
#include <rpm-smd.h>
#include <bits.h>
#include <debug.h>

static uint32_t ldo13[][11]=
{
	{
		LDOA_RES_TYPE, 13,
		KEY_SOFTWARE_ENABLE, 4, GENERIC_DISABLE,
		KEY_MICRO_VOLT, 4, 0,
		KEY_CURRENT, 4, 0,
	},
	{
		LDOA_RES_TYPE, 13,
		KEY_SOFTWARE_ENABLE, 4, GENERIC_ENABLE,
		KEY_MICRO_VOLT, 4, 2850000, //<- vote for 2.85V
		KEY_CURRENT, 4, 40,
	},
};

void regulator_enable(uint32_t enable)
{
	if (enable & REG_LDO13)
		rpm_send_data(&ldo13[GENERIC_ENABLE][0], 36, RPM_REQUEST_TYPE);
}

void regulator_disable(uint32_t enable)
{
	if (enable & REG_LDO13)
		rpm_send_data(&ldo13[GENERIC_DISABLE][0], 36,RPM_REQUEST_TYPE);
}
