/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <crypto_hash.h>
#include <boot_verifier.h>
#include <image_verify.h>
#include <mmc.h>
#include <oem_keystore.h>
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <partition_parser.h>
#include <rsa.h>
#include <string.h>
#include <openssl/err.h>
#include <platform.h>

#define ASN1_ENCODED_SHA256_SIZE 0x33
#define ASN1_ENCODED_SHA256_OFFSET 0x13
#define ASN1_SIGNATURE_BUFFER_SZ   mmc_page_size()

static KEYSTORE *oem_keystore;
static KEYSTORE *user_keystore;
static uint32_t dev_boot_state = RED;
char KEYSTORE_PTN_NAME[] = "keystore";

static const char *VERIFIED_FLASH_ALLOWED_PTN[] = {
	"aboot",
	"boot",
	"recovery",
	"system",
	NULL };

ASN1_SEQUENCE(AUTH_ATTR) ={
	ASN1_SIMPLE(AUTH_ATTR, target, ASN1_PRINTABLESTRING),
	ASN1_SIMPLE(AUTH_ATTR, len, ASN1_INTEGER)
} ASN1_SEQUENCE_END(AUTH_ATTR)
IMPLEMENT_ASN1_FUNCTIONS(AUTH_ATTR)

	ASN1_SEQUENCE(VERIFIED_BOOT_SIG) = {
		ASN1_SIMPLE(VERIFIED_BOOT_SIG, version, ASN1_INTEGER),
		ASN1_SIMPLE(VERIFIED_BOOT_SIG, certificate, X509),
		ASN1_SIMPLE(VERIFIED_BOOT_SIG, algor, X509_ALGOR),
		ASN1_SIMPLE(VERIFIED_BOOT_SIG, auth_attr, AUTH_ATTR),
		ASN1_SIMPLE(VERIFIED_BOOT_SIG, sig, ASN1_OCTET_STRING)
	} ASN1_SEQUENCE_END(VERIFIED_BOOT_SIG)
IMPLEMENT_ASN1_FUNCTIONS(VERIFIED_BOOT_SIG)

	ASN1_SEQUENCE(KEY) = {
		ASN1_SIMPLE(KEY, algorithm_id, X509_ALGOR),
		ASN1_SIMPLE(KEY, key_material, RSAPublicKey)
	}ASN1_SEQUENCE_END(KEY)
IMPLEMENT_ASN1_FUNCTIONS(KEY);

ASN1_SEQUENCE(KEYBAG) = {
	ASN1_SIMPLE(KEYBAG, mykey, KEY)
}ASN1_SEQUENCE_END(KEYBAG)
IMPLEMENT_ASN1_FUNCTIONS(KEYBAG)

	ASN1_SEQUENCE(KEYSTORE_INNER) = {
		ASN1_SIMPLE(KEYSTORE_INNER, version, ASN1_INTEGER),
		ASN1_SIMPLE(KEYSTORE_INNER, mykeybag, KEYBAG)
	} ASN1_SEQUENCE_END(KEYSTORE_INNER)
IMPLEMENT_ASN1_FUNCTIONS(KEYSTORE_INNER)

	ASN1_SEQUENCE(KEYSTORE) = {
		ASN1_SIMPLE(KEYSTORE, version, ASN1_INTEGER),
		ASN1_SIMPLE(KEYSTORE, mykeybag, KEYBAG),
		ASN1_SIMPLE(KEYSTORE, sig, VERIFIED_BOOT_SIG)
	} ASN1_SEQUENCE_END(KEYSTORE)
IMPLEMENT_ASN1_FUNCTIONS(KEYSTORE)

static uint32_t read_der_message_length(unsigned char* input)
{
	uint32_t len = 0;
	int pos = 0;
	uint8_t len_bytes = 1;

	/* Check if input starts with Sequence id (0X30) */
	if(input[pos] != 0x30)
		return len;
	pos++;

	/* A length of 0xAABBCCDD in DER encoded messages would be sequence of
	   following octets 0xAA, 0xBB, 0XCC, 0XDD.

	   To read length - read each octet and shift left by 1 octect before
	   reading next octet.
	*/
	/* check if short or long length form */
	if(input[pos] & 0x80)
	{
		len_bytes = (input[pos] & ~(0x80));
		pos++;
	}

	while(len_bytes)
	{
		/* Shift len by 1 octet, make sure to check unsigned int overflow */
		if (len <= (UINT_MAX >> 8))
			len <<= 8;
		else
		{
			dprintf(CRITICAL, "Error: Length exceeding max size of uintmax\n");
			return 0;
		}

		/* Read next octet */
		if (pos < (int) ASN1_SIGNATURE_BUFFER_SZ)
			len = len | input[pos];
		else
		{
			dprintf(CRITICAL, "Error: Pos index exceeding the input buffer size\n");
			return 0;
		}

		pos++; len_bytes--;
	}

	/* Add number of octets representing sequence id and length  */
	if ((UINT_MAX - pos) > len)
		len += pos;
	else
	{
		dprintf(CRITICAL, "Error: Len overflows UINT_MAX value\n");
		return 0;
	}

	return len;
}

static int add_attribute_to_img(unsigned char *ptr, AUTH_ATTR *input)
{
	return i2d_AUTH_ATTR(input, &ptr);
}

bool boot_verify_compare_sha256(unsigned char *image_ptr,
		unsigned int image_size, unsigned char *signature_ptr, RSA *rsa)
{
	int ret = -1;
	bool auth = false;
	unsigned char *plain_text = NULL;

	/* The magic numbers here are drawn from the PKCS#1 standard and are the ASN.1
	 *encoding of the SHA256 object identifier that is required for a PKCS#1
	* signature.*/
	uint8_t digest[ASN1_ENCODED_SHA256_SIZE] = {0x30, 0x31, 0x30, 0x0d, 0x06,
												0x09, 0x60, 0x86, 0x48, 0x01,
												0x65, 0x03, 0x04, 0x02, 0x01,
												0x05, 0x00, 0x04, 0x20};

	plain_text = (unsigned char *)calloc(sizeof(char), SIGNATURE_SIZE);
	if (plain_text == NULL) {
		dprintf(CRITICAL, "boot_verifier: Calloc failed during verification\n");
		goto cleanup;
	}

	/* Calculate SHA256 of image and place it into the ASN.1 structure*/
	image_find_digest(image_ptr, image_size, CRYPTO_AUTH_ALG_SHA256,
			digest + ASN1_ENCODED_SHA256_OFFSET);

	/* Find digest from the image. This performs the PKCS#1 padding checks up to
	 * but not including the ASN.1 OID and hash function check. The return value
	 * is not positive for a failure or the length of the part after the padding */
	ret = image_decrypt_signature_rsa(signature_ptr, plain_text, rsa);

	/* Make sure the length returned from rsa decrypt is same as x509 signature format
	 * otherwise the signature is invalid and we fail
	 */
	if (ret != ASN1_ENCODED_SHA256_SIZE)
	{
		dprintf(CRITICAL, "boot_verifier: Signature decrypt failed! Signature invalid = %d\n",
			ret);
		goto cleanup;
	}
	/* So plain_text contains the ASN.1 encoded hash from the signature and
	* digest contains the value that this should be for the image that we're
	* verifying, so compare them.*/

	ret = memcmp(plain_text, digest, ASN1_ENCODED_SHA256_SIZE);
	if(ret == 0)
	{
		auth = true;
#ifdef TZ_SAVE_KERNEL_HASH
		save_kernel_hash((unsigned char *) digest + ASN1_ENCODED_SHA256_OFFSET, CRYPTO_AUTH_ALG_SHA256);
#endif
	}

cleanup:
	if (plain_text != NULL)
		free(plain_text);
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_thread_state(NULL);
	return auth;

}

static bool verify_image_with_sig(unsigned char* img_addr, uint32_t img_size,
		char *pname, VERIFIED_BOOT_SIG *sig, KEYSTORE *ks)
{
	bool ret = false;
	uint32_t len;
	int shift_bytes;
	RSA *rsa = NULL;
	bool keystore_verification = false;
	int attr = 0;

	if(!strcmp(pname, "keystore"))
		keystore_verification = true;

	/* Verify target name */
	if(strncmp((char*)(sig->auth_attr->target->data), pname,
				sig->auth_attr->target->length) ||
				(strlen(pname) != (unsigned long) sig->auth_attr->target->length))
	{
		dprintf(CRITICAL,
				"boot_verifier: verification failure due to target name mismatch\n");
		goto verify_image_with_sig_error;
	}
	/* Read image size from signature */
	/* A len = 0xAABBCC (represented by 3 octets) would be stored in
	   len->data as 0X00CCBBAA and len->length as 3(octets).

	   To read len we need to left shift data to number of missing octets and
	   then change it to host long
	 */
	len = *((uint32_t*)sig->auth_attr->len->data);
	shift_bytes = sizeof(uint32_t) - sig->auth_attr->len->length;
	if(shift_bytes > 0) {
		len = len << (shift_bytes*8);
	}
	len = ntohl(len);

	/* Verify image size*/
	if(len != img_size)
	{
		dprintf(CRITICAL,
				"boot_verifier: image length is different. (%d vs %d)\n",
				len, img_size);
		goto verify_image_with_sig_error;
	}

	/* append attribute to image */
	if(!keystore_verification)
	{
		// verifying a non keystore partition
		attr = add_attribute_to_img((unsigned char*)(img_addr + img_size),
				sig->auth_attr);
		if (img_size > (UINT_MAX - attr))
		{
			dprintf(CRITICAL,"Interger overflow detected\n");
			ASSERT(0);
		}
		else img_size += attr;
	}

	/* compare SHA256SUM of image with value in signature */
	if(ks != NULL)
		rsa = ks->mykeybag->mykey->key_material;

	ret = boot_verify_compare_sha256(img_addr, img_size,
			(unsigned char*)sig->sig->data, rsa);

	if(!ret)
	{
		dprintf(CRITICAL,
				"boot_verifier: Image verification failed.\n");
	}

verify_image_with_sig_error:
	return ret;
}

static int encode_inner_keystore(unsigned char *ptr, KEYSTORE *ks)
{
	int ret = 0;
	KEYSTORE_INNER  *ks_inner = KEYSTORE_INNER_new();
	if (ks_inner == NULL)
		return ret;
	ASN1_INTEGER *tmp_version = ks_inner->version;
	KEYBAG *tmp_mykeybag = ks_inner->mykeybag;

	ks_inner->version = ks->version;
	ks_inner->mykeybag = ks->mykeybag;
	ret = i2d_KEYSTORE_INNER(ks_inner, &ptr);

	ks_inner->version = tmp_version;
	ks_inner->mykeybag = tmp_mykeybag;

	if(ks_inner != NULL)
		KEYSTORE_INNER_free(ks_inner);
	return ret;
}

static bool verify_keystore(unsigned char * ks_addr, KEYSTORE *ks)
{
	bool ret = false;
	unsigned char * ptr = ks_addr;
	uint32_t inner_len = encode_inner_keystore(ptr, ks);
	ret = verify_image_with_sig(ks_addr, inner_len, "keystore", ks->sig,
			oem_keystore);
	return ret;
}

static void read_oem_keystore()
{
	KEYSTORE *ks = NULL;
	uint32_t len = sizeof(OEM_KEYSTORE);
	const unsigned char *input = OEM_KEYSTORE;

	if(oem_keystore != NULL)
		return;

	ks = d2i_KEYSTORE(NULL, (const unsigned char **) &input, len);
	if(ks != NULL)
	{
		oem_keystore = ks;
		user_keystore = ks;
	}
}

static int read_user_keystore_ptn(uint8_t *keystore_buf)
{
	int index = INVALID_PTN;
	unsigned long long ptn = 0;

	index = partition_get_index(KEYSTORE_PTN_NAME);
	ptn = partition_get_offset(index);
	if(ptn == 0) {
		dprintf(CRITICAL, "boot_verifier: No keystore partition found\n");
		return -1;
	}

	if (mmc_read(ptn, (unsigned int *) keystore_buf, mmc_page_size())) {
		dprintf(CRITICAL, "boot_verifier: Cannot read user keystore\n");
		return -1;
	}
	return 0;
}

static void read_user_keystore(unsigned char *user_addr)
{
	unsigned char *input = user_addr;
	KEYSTORE *ks = NULL;
	uint32_t len = read_der_message_length(input);

	if(!len)
	{
		dprintf(CRITICAL, "boot_verifier: user keystore length is invalid.\n");
		return;
	}

	if (len > ASN1_SIGNATURE_BUFFER_SZ)
	{
		dprintf(CRITICAL, "boot_verifier: user keystore exceeds size signature buffer\n");
		return;
	}

	ks = d2i_KEYSTORE(NULL, (const unsigned char **)&input, len);
	if(ks != NULL)
	{
		if(verify_keystore(user_addr, ks) == false)
		{
			dprintf(CRITICAL, "boot_verifier: Keystore verification failed!\n");
			boot_verify_send_event(KEYSTORE_VERIFICATION_FAIL);
		}
		else
			dprintf(CRITICAL, "boot_verifier: Keystore verification success!\n");
		user_keystore = ks;
	}
	else
	{
		user_keystore = oem_keystore;
	}
}

uint32_t boot_verify_keystore_init()
{
	uint8_t *keystore_buf = NULL;

	/* Read OEM Keystore */
	read_oem_keystore();

	keystore_buf = memalign(ASN1_SIGNATURE_BUFFER_SZ, CACHE_LINE);
	ASSERT(keystore_buf);

	/* Read User Keystore */
	if(!read_user_keystore_ptn(keystore_buf))
		read_user_keystore((unsigned char *)keystore_buf);
	return dev_boot_state;
}

bool boot_verify_image(unsigned char* img_addr, uint32_t img_size, char *pname)
{
	bool ret = false;
	VERIFIED_BOOT_SIG *sig = NULL;
	unsigned char* sig_addr = (unsigned char*)(img_addr + img_size);
	uint32_t sig_len = 0;
	unsigned char *signature = NULL;

	if(dev_boot_state == ORANGE)
	{
		dprintf(INFO, "boot_verifier: Device is in ORANGE boot state.\n");
		dprintf(INFO, "boot_verifier: Skipping boot verification.\n");
		return false;
	}

	signature = malloc(ASN1_SIGNATURE_BUFFER_SZ);
	ASSERT(signature);

	/* Copy the signature from scratch memory to buffer */
	memcpy(signature, sig_addr, ASN1_SIGNATURE_BUFFER_SZ);
	sig_len = read_der_message_length(signature, ASN1_SIGNATURE_BUFFER_SZ);

	if(!sig_len)
	{
		dprintf(CRITICAL, "boot_verifier: Error while reading singature length.\n");
		goto verify_image_error;
	}

	if (sig_len > ASN1_SIGNATURE_BUFFER_SZ)
	{
		dprintf(CRITICAL, "boot_verifier: Signature length exceeds size signature buffer\n");
		goto verify_image_error;
	}

	if((sig = d2i_VERIFIED_BOOT_SIG(NULL, (const unsigned char **) &sig_addr, sig_len)) == NULL)
	{
		dprintf(CRITICAL,
				"boot_verifier: verification failure due to target name mismatch\n");
		goto verify_image_error;
	}

	ret = verify_image_with_sig(img_addr, img_size, pname, sig, user_keystore);

verify_image_error:
	free(signature);
	if(sig != NULL)
		VERIFIED_BOOT_SIG_free(sig);
	if(!ret)
		boot_verify_send_event(BOOT_VERIFICATION_FAIL);
	return ret;
}

void boot_verify_send_event(uint32_t event)
{
	switch(event)
	{
		case BOOT_INIT:
			dev_boot_state = GREEN;
			break;
		case KEYSTORE_VERIFICATION_FAIL:
			if(dev_boot_state == GREEN)
				dev_boot_state = YELLOW;
			break;
		case BOOT_VERIFICATION_FAIL:
			if(dev_boot_state == GREEN || dev_boot_state == YELLOW)
				dev_boot_state = RED;
			break;
		case DEV_UNLOCK:
			dev_boot_state = ORANGE;
			break;
		case USER_DENIES:
			if(dev_boot_state == YELLOW || dev_boot_state == ORANGE)
				dev_boot_state = RED;
			break;
	}
}

uint32_t boot_verify_get_state()
{
	return dev_boot_state;
}

void boot_verify_print_state()
{
	switch(dev_boot_state)
	{
		case GREEN:
			dprintf(INFO, "boot_verifier: Device is in GREEN boot state.\n");
			break;
		case ORANGE:
			dprintf(INFO, "boot_verifier: Device is in ORANGE boot state.\n");
			break;
		case YELLOW:
			dprintf(INFO, "boot_verifier: Device is in YELLOW boot state.\n");
			break;
		case RED:
			display_fbcon_message("Security Error:  This phone has been flashed with unauthorized software & is locked. Call your mobile operator for additional support.Please note that				repair/return for this issue may have additional cost.\n");

			dprintf(INFO, "boot_verifier: Device is in RED boot state.\n");
			break;
	}
}

bool boot_verify_validate_keystore(unsigned char * user_addr, unsigned sz)
{
	bool ret = false;
	unsigned char *input = user_addr;
	KEYSTORE *ks = NULL;
	uint32_t len = read_der_message_length(input, sz);
	if(!len)
	{
		dprintf(CRITICAL, "boot_verifier: keystore length is invalid.\n");
		return ret;
	}

	ks = d2i_KEYSTORE(NULL, (const unsigned char **)&input, len);
	if(ks != NULL)
	{
		ret = true;
	}
	return ret;
}

static bool check_list(const char **list, const char* entry)
{
	if(list == NULL || entry == NULL)
		return false;

	while(*list != NULL)
	{
		if(!strcmp(entry, *list))
			return true;

		list++;
	}

	return false;
}

bool boot_verify_flash_allowed(const char * entry)
{
	return check_list(VERIFIED_FLASH_ALLOWED_PTN, entry);
}

KEYSTORE *boot_gerity_get_oem_keystore()
{
	read_oem_keystore();
	return oem_keystore;
}
