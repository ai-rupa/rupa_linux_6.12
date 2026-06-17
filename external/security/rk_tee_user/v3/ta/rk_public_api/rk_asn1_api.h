/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025, Rockchip Electronics Co., Ltd.
 */

#ifndef RK_ASN1_API_H
#define RK_ASN1_API_H

#include "rk_crypto_api.h"

typedef struct ae_param {
	uint32_t key_len;
	uint8_t key[64];
	uint32_t iv_len;
	uint8_t iv[16];
	uint32_t tag_len;
	uint8_t tag[16];
} ae_param_t;

struct ssl_aes_ae_in {
	void *key;
	void *src;
	void *iv;
	void *add;
	int32_t key_len;
	int32_t src_len;
	int32_t iv_len;
	int32_t add_len;
	int32_t tag_size;
};

struct ssl_aes_ae_gcm_out {
	void *dest;
	void *tag;
	int32_t dest_len;
};

typedef enum {
	RK_ALGORITHM_AES = 1,
	RK_ALGORITHM_DES = 11,
	RK_ALGORITHM_DES3 = 21,
	RK_ALGORITHM_HASH = 31,
	RK_ALGORITHM_HMAC = 41,
	RK_ALGORITHM_RSA = 51,
	RK_ALGORITHM_DSA = 61,
	RK_ALGORITHM_EC = 71,
} rk_algorithm_t;

typedef enum {
	RK_KEY_TYPE_SYMMETRIC = 2,
	RK_KEY_TYPE_RSA = 12,
	RK_KEY_TYPE_ECDSA = 22,
} rk_key_type_t;

TEE_Result rk_rsa_pubkey_decode(uint8_t* asn1_key, uint32_t pub_len, rsa_key_t *rsa_key);
TEE_Result rk_rsa_prikey_decode(uint8_t* asn1_key, uint32_t pri_len, rsa_key_t *rsa_key);
TEE_Result rk_rsa_pubkey_encode(uint8_t* asn1_key, uint32_t *pub_len, rsa_key_t *rsa_key);
TEE_Result rk_rsa_prikey_encode(uint8_t* asn1_key, uint32_t *pub_len, rsa_key_t *rsa_key);
TEE_Result rk_rsa_pkcs8_decode(uint8_t *pkcs8_key, uint32_t pkcs8_len, rsa_key_t *rsa_key);
TEE_Result rk_rsa_x509_encode(uint8_t* asn1_key, uint32_t *pub_len, rsa_key_t *rsa_key);

TEE_Result rk_ec_prikey_decode(uint8_t *asn1_key, uint32_t pri_len, ec_key_t *ec_key);
TEE_Result rk_ec_prikey_encode(uint8_t *asn1_key, uint32_t *pri_len, ec_key_t *ec_key);
TEE_Result rk_ec_pubkey_decode(uint8_t *asn1_key, uint32_t pub_len, ec_key_t *ec_key);
TEE_Result rk_ec_pubkey_encode(uint8_t *asn1_key, uint32_t *pub_len, ec_key_t *ec_key);
TEE_Result rk_ec_pkcs8_decode(uint8_t *pkcs8_key, uint32_t pkcs8_len, ec_key_t *ec_key);
TEE_Result rk_ec_x509_encode(uint8_t* asn1_key, uint32_t *pub_len, ec_key_t *ec_key);

#endif /* RK_ASN1_API_H */
