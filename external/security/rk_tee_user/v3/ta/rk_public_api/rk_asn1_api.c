/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025, Rockchip Electronics Co., Ltd.
 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <tee_api_defines.h>
#include <tee_api_defines_extensions.h>
#include <utee_defines.h>
#include <util.h>
#include "rk_asn1_api.h"

#define ASN1_DEBUG 0

#define ASN1_INTEGER        (uint8_t)0x02
#define ASN1_BIT_STRING     (uint8_t)0x03
#define ASN1_OCT_STRING     (uint8_t)0x04
#define ASN1_NULL           (uint8_t)0x05
#define ASN1_OBJ_IDENTIFIER (uint8_t)0x06
#define ASN1_SEQUENCE       (uint8_t)0x30
#define ASN1_CONTEXT0       (uint8_t)0xA0
#define ASN1_CONTEXT1       (uint8_t)0xA1

typedef enum {
	PKCS_TYPE_1 = 0,
	PKCS_TYPE_8
} pkcs_type;

static const uint8_t secp224r1[] = {0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x21};
static const uint8_t prime256v1[] = {0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
static const uint8_t secp384r1[] = {0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22};
static const uint8_t secp521r1[] = {0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23};
static const uint8_t sm2p256[] = {0x06, 0x08, 0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x82, 0x2d};
static const uint8_t asn1_oid_ec_pub[] = {0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};

typedef struct {
	uint8_t *data;          //the buffer of data
	uint16_t data_max_size; //buffser size
	uint16_t *data_len;     //valid length of data
	uint8_t tag;            //ASN1 data type
	uint8_t need_plus;      //to identify weather the data is a positive number
	uint16_t reserve;       //padding byte of this struct,no use
} asn1_object_t;

TEE_Result rk_rsa_pkcs8_head_decode(uint8_t* pkcs8_key, uint32_t pkcs8_len, uint32_t *head_size);

static uint32_t asn1_parse_len(uint8_t *len_field, uint32_t *len)
{
	uint32_t offset = 0;
	uint32_t i, len_count = 0;
	if (len_field == NULL || len == NULL)
		return 0;

	*len = 0;

	//length field should less than 4 byte, or you can't put length into uint32_t
	if (*len_field > 0x84 || *len_field == 0x80)
		return 0;

	if (*len_field < 0x80) {
		//if the first byte is less than 0x80, it is use 1Byte to describe
		*len = *len_field;
		offset = 1;
	} else {
		len_count = *len_field - 0x80;
		offset = len_count + 1;
		for (i = 0; i < len_count; i++) {
			*len |= (len_field[i + 1] << ((len_count - 1 - i) * 8));
		}
	}

	return offset;
}

static TEE_Result asn1_compose_len(uint32_t len, uint8_t *field, uint32_t *field_len)
{
	uint8_t tmp_field[4], i, j;
	if (field == NULL || field_len == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	if (len < 0x80) {
		*field = len;
		*field_len = 1;
	} else {
		tmp_field[0] = (len >> 24) & 0xff;
		tmp_field[1] = (len >> 16) & 0xff;
		tmp_field[2] = (len >> 8) & 0xff;
		tmp_field[3] = len & 0xff;

		for (i = 0; i < sizeof(tmp_field); i++) {
			if (tmp_field[i] == 0x00)
				continue;

			for (j = 0; j < sizeof(tmp_field) - i; j++) {
				field[j + 1] = tmp_field[j + i];
			}
			break;
		}
		field[0] = 0X80 + sizeof(tmp_field) - i;
		*field_len = sizeof(tmp_field) - i + 1;
	}

	return TEE_SUCCESS;
}

static TEE_Result asn1_get_object(uint8_t *in, uint8_t tag, uint8_t need_plus, uint32_t *data_offset,
                                  uint32_t *data_len)
{
	uint8_t *p = in;
	uint32_t offset = 0, len;

	if (in == NULL || data_offset == NULL || data_len == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	*data_len = 0;

	if (tag != *p) {
		EMSG("Expct tag = %02x, Actual tag = %02x", tag, *p);
		return TEE_ERROR_BAD_FORMAT;
	}

	p++; //ship tag
	offset = asn1_parse_len(p, &len);
	if (offset == 0) {
		EMSG("asn1_parse_len error");
		return TEE_ERROR_BAD_FORMAT;
	}
	p += offset; //point to value field

	//if the first Byte is bigger or equal to 0x80,then should padding 0x00 as sign byte
	if (need_plus && *p == 0x00 && *(p + 1) >= 0x80) {
		//adjust the offset and len, to skip padding Byte(0x00)
		len--;
		offset++;
	}

	*data_len = len;
	*data_offset = offset + 1;

	return TEE_SUCCESS;
}

static TEE_Result asn1_set_object(uint8_t *in, uint32_t in_len, uint8_t tag, uint8_t need_plus, uint8_t *out,
                                  uint32_t out_max, uint32_t *out_len)
{
	TEE_Result res;
	uint8_t *pout = out;
	uint32_t field_len;
	uint8_t tmp_field[5];

	if (in == NULL || out == NULL || out_len == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	*out_len = 0;

	//padding tag field
	if (out_max < 1) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	*pout = tag;
	pout++;
	out_max--;

	//padding length field
	if (need_plus && *in >= 0x80) {
		res = asn1_compose_len(in_len + 1, tmp_field, &field_len);
	} else {
		res = asn1_compose_len(in_len, tmp_field, &field_len);
	}

	if (res != TEE_SUCCESS) {
		EMSG("asn1_compose_len error");
		return res;
	}

	if (out_max < field_len) {
		EMSG("out_max = %d, field_len = %d", out_max, field_len);
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(pout, tmp_field, field_len);
	pout += field_len;
	out_max -= field_len;

	//padding value field
	if (need_plus && *in >= 0x80) {
		if (out_max < 1) {
			res = TEE_ERROR_SHORT_BUFFER;
			goto exit;
		}
		*pout = 0x00;
		pout++;
		out_max--;
	}

	if (out_max < in_len) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(pout, in, in_len);
	pout += in_len;

	*out_len = pout - out;

exit:
	return res;
}

TEE_Result rk_rsa_pkcs8_head_decode(uint8_t *pkcs8_key, uint32_t pkcs8_len, uint32_t *head_size)
{
	TEE_Result res;
	uint8_t *parse_point = pkcs8_key;
	uint32_t data_len, offset;
	const uint8_t pkcs8_algo_rsa[] = {0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00};

	if (pkcs8_key == NULL || head_size == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	*head_size = 0;

	//The first tag must be ASN1_SEQUENCE
	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key is not a SEQUENCE");
		goto exit;
	}

	if (offset + data_len != pkcs8_len) {
		EMSG("asn1_key data_len is not match. asn1_len=%d, offset+data_len = %d", pkcs8_len, offset + data_len);
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += offset;

	//parsing version filed
	res = asn1_get_object(parse_point, ASN1_INTEGER, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("parsing version field error");
		goto exit;
	}
	parse_point += offset;
	parse_point += data_len;

	//parsing algorithm indendifer sequence
	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("parsing algorithm indentifer sequence error");
		goto exit;
	}
	parse_point += offset;

	if (TEE_MemCompare(pkcs8_algo_rsa, parse_point, data_len)) {
		EMSG("this is not a rsa private key");
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += data_len;

	//parsing PKCS1 field
	res = asn1_get_object(parse_point, ASN1_OCT_STRING, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("parsing PKCS1 field ASN1_OCT_STRING error");
		goto exit;
	}
	parse_point += offset;

	*head_size = parse_point - pkcs8_key;
exit:
	return res;
}

TEE_Result rk_rsa_pkcs8_decode(uint8_t *pkcs8_key, uint32_t pkcs8_len, rsa_key_t *rsa_key)
{
	TEE_Result res;
	uint32_t head_size = 0;

	res = rk_rsa_pkcs8_head_decode(pkcs8_key, pkcs8_len, &head_size);
	if (res != TEE_SUCCESS) {
		EMSG("decode pkcs8 head error");
		goto exit;
	}

	res = rk_rsa_prikey_decode(pkcs8_key + head_size, pkcs8_len - head_size, rsa_key);
	if (res != TEE_SUCCESS) {
		EMSG("decode pkcs1 key error");
		goto exit;
	}
exit:
	return res;
}

TEE_Result rk_rsa_pubkey_decode(uint8_t *asn1_key, uint32_t pub_len, rsa_key_t *rsa_key)
{
	TEE_Result res;
	uint8_t *parse_point = asn1_key;
	uint32_t data_len, offset;

	if (asn1_key == NULL || rsa_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;
	TEE_MemFill(rsa_key, 0x00, sizeof(*rsa_key));

	//The first tag must be ASN1_SEQUENCE
	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key is not a SEQUENCE");
		goto exit;
	}

	if (offset + data_len != pub_len) {
		EMSG("asn1_key data_len is not match. pub_len=%d, offset+data_len = %d", pub_len, offset + data_len);
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += offset;

	//start parsing
	res = asn1_get_object(parse_point, ASN1_INTEGER, 1, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key is not a ASN1_INTEGER.");
		goto exit;
	}
	parse_point += offset;

	//check whether the buffer is big enough
	if (data_len > sizeof(rsa_key->n)) {
		EMSG("output keybuf is too short, need %dByte, actual %zu", data_len, sizeof(rsa_key->n));
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(rsa_key->n, parse_point, data_len);
	rsa_key->key_len = data_len;
	parse_point += data_len;

	res = asn1_get_object(parse_point, ASN1_INTEGER, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key is not a ASN1_INTEGER.");
		goto exit;
	}

	parse_point += offset;

	if (data_len > sizeof(rsa_key->e)) {
		EMSG("output keybuf is too short, need %dByte, actual %zu", data_len, sizeof(rsa_key->n));
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}
	TEE_MemMove(rsa_key->e, parse_point, data_len);
	rsa_key->e_len = data_len;
	parse_point += data_len;

#if ASN1_DEBUG
	EMSG("rk_rsa_pubkey_decode ok! key_len = %d", rsa_key->key_len);
#endif

exit:
	return res;
}

TEE_Result rk_rsa_prikey_decode(uint8_t *asn1_key, uint32_t pri_len, rsa_key_t *rsa_key)
{
	TEE_Result res;
	uint8_t version[1] = {0};
	uint8_t *parse_point = NULL;
	uint16_t ver_len = 0;
	uint32_t data_len, offset;
	uint32_t i;
	asn1_object_t object_list[] = {
	    {version,     sizeof(version),     &ver_len,          ASN1_INTEGER, 0},
	    {rsa_key->n,  sizeof(rsa_key->n),  &rsa_key->key_len, ASN1_INTEGER, 1},
	    {rsa_key->e,  sizeof(rsa_key->e),  &rsa_key->e_len,   ASN1_INTEGER, 0},
	    {rsa_key->d,  sizeof(rsa_key->d),  &rsa_key->d_len,   ASN1_INTEGER, 1},
	    {rsa_key->p,  sizeof(rsa_key->p),  &rsa_key->p_len,   ASN1_INTEGER, 1},
	    {rsa_key->q,  sizeof(rsa_key->q),  &rsa_key->q_len,   ASN1_INTEGER, 1},
	    {rsa_key->dp, sizeof(rsa_key->dp), &rsa_key->dp_len,  ASN1_INTEGER, 1},
	    {rsa_key->dq, sizeof(rsa_key->dq), &rsa_key->dq_len,  ASN1_INTEGER, 1},
	    {rsa_key->iq, sizeof(rsa_key->iq), &rsa_key->iq_len,  ASN1_INTEGER, 1},
	};

	if (asn1_key == NULL || rsa_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;
	TEE_MemFill(rsa_key, 0x00, sizeof(*rsa_key));

	parse_point = asn1_key;

	//The first tag must be ASN1_SEQUENCE
	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("pub_asn1 is not a SEQUENCE");
		goto exit;
	}

	if (offset + data_len != pri_len) {
		EMSG("asn1_key data_len is not match. pri_len=%d, offset+data_len = %d", pri_len, offset + data_len);
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += offset;

	for (i = 0; i < sizeof(object_list) / sizeof(object_list[0]); i++) {
		res = asn1_get_object(parse_point, object_list[i].tag, object_list[i].need_plus, &offset, &data_len);
		if (res != TEE_SUCCESS) {
			EMSG("pub_asn1 is not a ASN1_INTEGER.");
			goto exit;
		}
		parse_point += offset;

		//check whether the buffer is big enough
		if (data_len > object_list[i].data_max_size) {
			EMSG("output keybuf is too short, need %dByte, actual %d", data_len, object_list[i].data_max_size);
			res = TEE_ERROR_SHORT_BUFFER;
			goto exit;
		}

		*(object_list[i].data_len) = (uint16_t)data_len;

		TEE_MemMove(object_list[i].data, parse_point, data_len);
		parse_point += data_len;
	}

#if ASN1_DEBUG
	EMSG("rk_rsa_prikey_decode ok! key_len = %d", rsa_key->key_len);
#endif

exit:
	return res;
}

TEE_Result rk_rsa_pubkey_encode(uint8_t *asn1_key, uint32_t *pub_len, rsa_key_t *rsa_key)
{
	TEE_Result res;
	uint8_t tmp_field[8];
	uint32_t total_len = 0, tmp_len = 0, out_max;

	if (asn1_key == NULL || pub_len == NULL || rsa_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	out_max = *pub_len;

	//padding n
	res = asn1_set_object(rsa_key->n, rsa_key->key_len, ASN1_INTEGER, 1, asn1_key + total_len, out_max, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set rsa_key->n object error!");
		goto exit;
	}
	total_len += tmp_len;
	out_max -= tmp_len;

	//padding e
	res = asn1_set_object(rsa_key->e, rsa_key->e_len, ASN1_INTEGER, 0, asn1_key + total_len, out_max, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set rsa_key->e object error!");
		goto exit;
	}
	total_len += tmp_len;
	out_max -= tmp_len;

	//add SEQUENCE info in head
	res = asn1_compose_len(total_len, tmp_field, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set asn1_compose_len error!");
		goto exit;
	}

	if (out_max < tmp_len + 1) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(asn1_key + tmp_len + 1, asn1_key, total_len);
	*asn1_key = ASN1_SEQUENCE;
	total_len++;
	TEE_MemMove(asn1_key + 1, tmp_field, tmp_len);
	total_len += tmp_len;

	*pub_len = total_len;
exit:
	return res;
}

TEE_Result rk_rsa_prikey_encode(uint8_t *asn1_key, uint32_t *pub_len, rsa_key_t *rsa_key)
{
	TEE_Result res;
	uint8_t tmp_field[5];
	uint32_t total_len = 0, tmp_len = 0;
	uint8_t version[1] = {0};
	uint16_t ver_len = 1;
	uint32_t i, out_max;
	asn1_object_t object_list[] = {
	    {version,     sizeof(version),     &ver_len,          ASN1_INTEGER, 0},
	    {rsa_key->n,  sizeof(rsa_key->n),  &rsa_key->key_len, ASN1_INTEGER, 1},
	    {rsa_key->e,  sizeof(rsa_key->e),  &rsa_key->e_len,   ASN1_INTEGER, 0},
	    {rsa_key->d,  sizeof(rsa_key->d),  &rsa_key->d_len,   ASN1_INTEGER, 1},
	    {rsa_key->p,  sizeof(rsa_key->p),  &rsa_key->p_len,   ASN1_INTEGER, 1},
	    {rsa_key->q,  sizeof(rsa_key->q),  &rsa_key->q_len,   ASN1_INTEGER, 1},
	    {rsa_key->dp, sizeof(rsa_key->dp), &rsa_key->dp_len,  ASN1_INTEGER, 1},
	    {rsa_key->dq, sizeof(rsa_key->dq), &rsa_key->dq_len,  ASN1_INTEGER, 1},
	    {rsa_key->iq, sizeof(rsa_key->iq), &rsa_key->iq_len,  ASN1_INTEGER, 1},
	};

	if (asn1_key == NULL || pub_len == NULL || rsa_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	out_max = *pub_len;

	for (i = 0; i < sizeof(object_list) / sizeof(object_list[0]); i++) {
		res = asn1_set_object(object_list[i].data, *(object_list[i].data_len), object_list[i].tag,
		                      object_list[i].need_plus, asn1_key + total_len, out_max, &tmp_len);
		if (res != TEE_SUCCESS) {
			EMSG("set %d object error!", i);
			goto exit;
		}
		total_len += tmp_len;
		out_max -= tmp_len;
	}

	res = asn1_compose_len(total_len, tmp_field, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set asn1_compose_len error!");
		goto exit;
	}

	if (out_max < tmp_len + 1) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(asn1_key + tmp_len + 1, asn1_key, total_len);
	*asn1_key = ASN1_SEQUENCE;
	total_len++;
	TEE_MemMove(asn1_key + 1, tmp_field, tmp_len);
	total_len += tmp_len;

	*pub_len = total_len;
exit:
	return res;
}

TEE_Result rk_rsa_x509_encode(uint8_t *asn1_key, uint32_t *pub_len, rsa_key_t *rsa_key)
{
	TEE_Result res;
	uint8_t tmp_field[8];
	uint32_t total_len = 0, tmp_len = 0, tmp_filed_len = 0, out_max;
	const unsigned char rsa_algo_seq[] = {0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
	                                      0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00};

	if (asn1_key == NULL || pub_len == NULL || rsa_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	out_max = *pub_len;
	tmp_len = out_max;

	res = rk_rsa_pubkey_encode(asn1_key, &tmp_len, rsa_key);
	if (res != TEE_SUCCESS) {
		EMSG("set rsa_key->n object error!");
		goto exit;
	}
	total_len += tmp_len;
	out_max -= tmp_len;

	//add one byte 0x00,tmp_len should add one
	res = asn1_compose_len(tmp_len + 1, tmp_field, &tmp_filed_len);
	if (res != TEE_SUCCESS) {
		EMSG("set asn1_compose_len error!");
		goto exit;
	}

	//tag + len_filed + 0x00 + pub_data
	if (out_max < tmp_filed_len + 2) {
		EMSG("buff too small!");
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(asn1_key + 2 + tmp_filed_len, asn1_key, tmp_len);
	out_max -= (2 + tmp_filed_len);
	total_len += (2 + tmp_filed_len);
	*asn1_key = ASN1_BIT_STRING;
	TEE_MemMove(asn1_key + 1, tmp_field, tmp_filed_len);
	*(asn1_key + 1 + tmp_filed_len) = 0x00;

	if (out_max < sizeof(rsa_algo_seq)) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(asn1_key + sizeof(rsa_algo_seq), asn1_key, total_len);
	TEE_MemMove(asn1_key, rsa_algo_seq, sizeof(rsa_algo_seq));

	total_len += sizeof(rsa_algo_seq);
	out_max -= sizeof(rsa_algo_seq);

	//add SEQUENCE info in head
	res = asn1_compose_len(total_len, tmp_field, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set asn1_compose_len error!");
		goto exit;
	}

	if (out_max < tmp_len + 1) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(asn1_key + tmp_len + 1, asn1_key, total_len);
	*asn1_key = ASN1_SEQUENCE;
	total_len++;
	TEE_MemMove(asn1_key + 1, tmp_field, tmp_len);
	total_len += tmp_len;

	*pub_len = total_len;
exit:
	return res;
}

TEE_Result rk_ec_x509_encode(uint8_t *asn1_key, uint32_t *pub_len, ec_key_t *ec_key)
{
	return rk_ec_pubkey_encode(asn1_key, pub_len, ec_key);
}

static TEE_Result rk_ec_prikey_sub_decode(uint8_t *asn1_key, uint32_t pri_len, ec_key_t *ec_key, pkcs_type type)
{
	TEE_Result res;
	uint8_t compress_form;
	uint8_t *parse_point = NULL;
	uint32_t data_len, offset;
	const uint8_t version = 0x01;

	if (asn1_key == NULL || ec_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	if (type != PKCS_TYPE_1 && type != PKCS_TYPE_8)
		return TEE_ERROR_BAD_PARAMETERS;

#if ASN1_DEBUG
	EMSG("asn1_key[0] == 0x%2x; asn1_key_len = %d; pkcs_type = %d", *asn1_key, pri_len, type);
#endif
	parse_point = asn1_key;

	//The first tag must be ASN1_SEQUENCE
	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key has not a SEQUENCE");
		goto exit;
	}

	if (offset + data_len != pri_len) {
		EMSG("asn1_key data_len is not match. pri_len=%d, offset+data_len = %d", pri_len, offset + data_len);
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += offset;

	//parsing version
	res = asn1_get_object(parse_point, ASN1_INTEGER, 1, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key has not a ASN1_INTEGER.");
		goto exit;
	}
	parse_point += offset;

	if (version != *parse_point) {
		EMSG("ec key version error");
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += data_len;

	//parsing private key
	res = asn1_get_object(parse_point, ASN1_OCT_STRING, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key has not a ASN1_OCT_STRING.");
		goto exit;
	}
	parse_point += offset;

	//check whether the buffer is big enough
	if (data_len > sizeof(ec_key->d)) {
		EMSG("output keybuf is too short, need %dByte, actual %zu", data_len, sizeof(ec_key->d));
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(ec_key->d, parse_point, data_len);
	ec_key->d_len = data_len;
	parse_point += data_len;

	/* maybe only contains private key, but not public key */
	if ((uint32_t)(parse_point - asn1_key) == pri_len) {
		EMSG("asn1_key skip ASN1_CONTEXT0 & ASN1_CONTEXT1.");
		ec_key->x_len = 0;
		ec_key->y_len = 0;
		goto exit;
	}

	/*******************************************************************
	 * parsing ASN1_CONTEXT0 identifier, only PKCS#1 require this field.
	 * It is optional for PKCS#8 and RFC5915.
	 ******************************************************************/
	res = asn1_get_object(parse_point, ASN1_CONTEXT0, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		if (type == PKCS_TYPE_1) {
			EMSG("asn1_key has not a ASN1_CONTEXT0.");
			goto exit;
		}

		/**********************************************************
		 * ASN1_CONTEXT0 is not compulsive requirement for PKCS#8,
		 * check ASN1_CONTEXT0 for compatible with RFC5915
		 *********************************************************/
	} else {
		parse_point += offset;

		if (0 == TEE_MemCompare(parse_point, secp224r1, sizeof(secp224r1)) && data_len == sizeof(secp224r1)) {
			ec_key->key_len = 224;
			ec_key->curve = TEE_ECC_CURVE_NIST_P224;
		} else if (0 == TEE_MemCompare(parse_point, prime256v1, sizeof(prime256v1)) && data_len == sizeof(prime256v1)) {
			ec_key->key_len = 256;
			ec_key->curve = TEE_ECC_CURVE_NIST_P256;
		} else if (0 == TEE_MemCompare(parse_point, secp384r1, sizeof(secp384r1)) && data_len == sizeof(secp384r1)) {
			ec_key->key_len = 384;
			ec_key->curve = TEE_ECC_CURVE_NIST_P384;
		} else if (0 == TEE_MemCompare(parse_point, secp521r1, sizeof(secp521r1)) && data_len == sizeof(secp521r1)) {
			ec_key->key_len = 521;
			ec_key->curve = TEE_ECC_CURVE_NIST_P521;
		} else if (0 == TEE_MemCompare(parse_point, sm2p256, sizeof(sm2p256)) && data_len == sizeof(sm2p256)) {
			ec_key->key_len = 256;
			ec_key->curve = TEE_ECC_CURVE_SM2;
		} else {
			EMSG("ec key curve not supported");
			res = TEE_ERROR_BAD_FORMAT;
			goto exit;
		}
		parse_point += data_len;
	}

	/* maybe only contains private key, but not public key CONTEXT1 is optional */
	if ((uint32_t)(parse_point - asn1_key) == pri_len) {
		EMSG("asn1_key skip ASN1_CONTEXT1.");
		ec_key->x_len = 0;
		ec_key->y_len = 0;
		goto exit;
	}

	//parsing public context
	res = asn1_get_object(parse_point, ASN1_CONTEXT1, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key has not a ASN1_CONTEXT1.");
		goto exit;
	}
	parse_point += offset;

	//parsing public key
	res = asn1_get_object(parse_point, ASN1_BIT_STRING, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key has not a ASN1_BIT_STRING.");
		goto exit;
	}
	parse_point += offset;

	//get the compress form
	parse_point++;
	compress_form = *parse_point;
	parse_point++;
	data_len -= 2;

	if (compress_form == 0x04) {
		//check whether the buffer is big enough
		if (data_len > (2 * sizeof(ec_key->x))) {
			EMSG("output keybuf is too short, need %dByte, actual %zu", data_len, 2 * sizeof(ec_key->x));
			res = TEE_ERROR_SHORT_BUFFER;
			goto exit;
		}
		//copy x data
		TEE_MemMove(ec_key->x, parse_point, data_len / 2);
		ec_key->x_len = data_len / 2;
		parse_point += data_len / 2;
		//copy y data
		TEE_MemMove(ec_key->y, parse_point, data_len / 2);
		ec_key->y_len = data_len / 2;
	} else {
		EMSG("only implement uncompress form for now!");
		res = TEE_ERROR_NOT_IMPLEMENTED;
		goto exit;
	}

#if ASN1_DEBUG
	EMSG("rk_ec_prikey_decode ok! key_len = %d", ec_key->key_len);
#endif

exit:
	return res;
}

TEE_Result rk_ec_pkcs8_decode(uint8_t *pkcs8_key, uint32_t pkcs8_len, ec_key_t *ec_key)
{
	TEE_Result res;
	uint8_t *parse_point = NULL;
	uint32_t data_len, offset;
	const uint8_t version = 0x00;

	if (pkcs8_key == NULL || ec_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	TEE_MemFill(ec_key, 0x00, sizeof(*ec_key));

#if ASN1_DEBUG
	EMSG("pkcs8_key[0] = 0x%2x; asn1_key_len = %d", *pkcs8_key, pkcs8_len);
#endif
	parse_point = pkcs8_key;

	//The first tag must be ASN1_SEQUENCE
	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("pkcs8_key has not a SEQUENCE");
		goto exit;
	}

	if (offset + data_len != pkcs8_len) {
		EMSG("pkcs8_key data_len is not match. pri_len=%d, offset+data_len = %d", pkcs8_len, offset + data_len);
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += offset;

	//parsing version
	res = asn1_get_object(parse_point, ASN1_INTEGER, 1, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("pkcs8_key has not a ASN1_INTEGER.");
		goto exit;
	}
	parse_point += offset;

	if (version != *parse_point) {
		//EMSG("ec key version error");
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += data_len;

	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("pkcs8_key has not a SEQUENCE");
		goto exit;
	}
	parse_point += offset;

	if (TEE_MemCompare(parse_point, asn1_oid_ec_pub, sizeof(asn1_oid_ec_pub))) {
		EMSG("This is not a ecPublicKey");
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}

	parse_point += sizeof(asn1_oid_ec_pub);
	data_len -= sizeof(asn1_oid_ec_pub);
	if (0 == TEE_MemCompare(parse_point, secp224r1, sizeof(secp224r1)) && data_len == sizeof(secp224r1)) {
		ec_key->key_len = 224;
		ec_key->curve = TEE_ECC_CURVE_NIST_P224;
	} else if (0 == TEE_MemCompare(parse_point, prime256v1, sizeof(prime256v1)) && data_len == sizeof(prime256v1)) {
		ec_key->key_len = 256;
		ec_key->curve = TEE_ECC_CURVE_NIST_P256;
	} else if (0 == TEE_MemCompare(parse_point, secp384r1, sizeof(secp384r1)) && data_len == sizeof(secp384r1)) {
		ec_key->key_len = 384;
		ec_key->curve = TEE_ECC_CURVE_NIST_P384;
	} else if (0 == TEE_MemCompare(parse_point, secp521r1, sizeof(secp521r1)) && data_len == sizeof(secp521r1)) {
		ec_key->key_len = 521;
		ec_key->curve = TEE_ECC_CURVE_NIST_P521;
	} else if (0 == TEE_MemCompare(parse_point, sm2p256, sizeof(sm2p256)) && data_len == sizeof(sm2p256)) {
		ec_key->key_len = 256;
		ec_key->curve = TEE_ECC_CURVE_SM2;
	} else {
		EMSG("ec key curve not supported");
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += data_len;

	//parsing private key
	res = asn1_get_object(parse_point, ASN1_OCT_STRING, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("pkcs8_key has not a ASN1_OCT_STRING.");
		goto exit;
	}
	parse_point += offset;

	res = rk_ec_prikey_sub_decode(parse_point, data_len, ec_key, PKCS_TYPE_8);
	if (res != TEE_SUCCESS) {
		EMSG("rk_ec_pkcs8_sub_decode error.");
		goto exit;
	}
#if ASN1_DEBUG
	EMSG("rk_ec_prikey_decode ok! key_len = %d", ec_key->key_len);
#endif

exit:
	return res;
}

TEE_Result rk_ec_pubkey_decode(uint8_t *asn1_key, uint32_t pub_len, ec_key_t *ec_key)
{
	TEE_Result res;
	uint8_t compress_form;
	uint8_t *parse_point = NULL;
	uint32_t data_len, offset;

	if (asn1_key == NULL || ec_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;
	TEE_MemFill(ec_key, 0x00, sizeof(*ec_key));

#if ASN1_DEBUG
	EMSG("asn1_key[0] is 0x%2x; asn1_key_len = %d", *asn1_key, pub_len);
#endif
	parse_point = asn1_key;

	//The first tag must be ASN1_SEQUENCE
	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key has not a SEQUENCE");
		goto exit;
	}

	if (offset + data_len != pub_len) {
		EMSG("asn1_key data_len is not match. pub_len=%d, offset+data_len = %d", pub_len, offset + data_len);
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += offset;

	//parsing ASN1_SEQUENCE include idenifier
	res = asn1_get_object(parse_point, ASN1_SEQUENCE, 1, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key has not a ASN1_INTEGER.");
		goto exit;
	}
	parse_point += offset;

	if (TEE_MemCompare(parse_point, asn1_oid_ec_pub, sizeof(asn1_oid_ec_pub))) {
		EMSG("This is not a ecPublicKey");
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point += sizeof(asn1_oid_ec_pub);

	if (0 == TEE_MemCompare(parse_point, secp224r1, sizeof(secp224r1))) {
		ec_key->key_len = 224;
		ec_key->curve = TEE_ECC_CURVE_NIST_P224;
	} else if (0 == TEE_MemCompare(parse_point, prime256v1, sizeof(prime256v1))) {
		ec_key->key_len = 256;
		ec_key->curve = TEE_ECC_CURVE_NIST_P256;
	} else if (0 == TEE_MemCompare(parse_point, secp384r1, sizeof(secp384r1))) {
		ec_key->key_len = 384;
		ec_key->curve = TEE_ECC_CURVE_NIST_P384;
	} else if (0 == TEE_MemCompare(parse_point, secp521r1, sizeof(secp521r1))) {
		ec_key->key_len = 521;
		ec_key->curve = TEE_ECC_CURVE_NIST_P521;
	} else if (0 == TEE_MemCompare(parse_point, sm2p256, sizeof(sm2p256))) {
		ec_key->key_len = 256;
		ec_key->curve = TEE_ECC_CURVE_SM2;
	} else {
		EMSG("ec key curve not supported");
		res = TEE_ERROR_BAD_FORMAT;
		goto exit;
	}
	parse_point = parse_point + 1 + 1 + *(parse_point + 1); //tag + len

	//parsing public key
	res = asn1_get_object(parse_point, ASN1_BIT_STRING, 0, &offset, &data_len);
	if (res != TEE_SUCCESS) {
		EMSG("asn1_key has not a ASN1_BIT_STRING.");
		goto exit;
	}
	parse_point += offset;

	//get the compress form
	parse_point++;
	compress_form = *parse_point;
	parse_point++;
	data_len -= 2;

	if (compress_form == 0x04) {
		//check whether the buffer is big enough
		if (data_len > (2 * sizeof(ec_key->x))) {
			EMSG("output keybuf is too short, need %dByte, actual %zu", data_len, 2 * sizeof(ec_key->x));
			res = TEE_ERROR_SHORT_BUFFER;
			goto exit;
		}
		//copy x data
		TEE_MemMove(ec_key->x, parse_point, data_len / 2);
		ec_key->x_len = data_len / 2;
		parse_point += data_len / 2;
		//copy y data
		TEE_MemMove(ec_key->y, parse_point, data_len / 2);
		ec_key->y_len = data_len / 2;
	} else {
		EMSG("only implement uncompress form for now!");
		res = TEE_ERROR_NOT_IMPLEMENTED;
		goto exit;
	}

#if ASN1_DEBUG
	EMSG("rk_ec_pubkey_decode ok!, key_len = %d", ec_key->key_len);
#endif

exit:
	return res;
}

TEE_Result rk_ec_prikey_decode(uint8_t *asn1_key, uint32_t pri_len, ec_key_t *ec_key)
{
	if (asn1_key == NULL || ec_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	TEE_MemFill(ec_key, 0x00, sizeof(*ec_key));
	return rk_ec_prikey_sub_decode(asn1_key, pri_len, ec_key, PKCS_TYPE_1);
}

TEE_Result rk_ec_pubkey_encode(uint8_t *asn1_key, uint32_t *pub_len, ec_key_t *ec_key)
{
	TEE_Result res;
	uint8_t tmp_field[5];
	uint32_t total_len = 0, tmp_len = 0;
	uint8_t compress_form[2] = {0x00, 0x04};
	uint32_t out_max;
	const uint8_t *object_id = NULL;
	uint32_t object_id_len = 0;
	uint32_t pubkey_len = 0;
	uint8_t pubkey_string[256] = {0}; /* max public key length */

	if (asn1_key == NULL || pub_len == NULL || ec_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

#if ASN1_DEBUG
	EMSG("key_len = %d", ec_key->key_len);
#endif

	switch (ec_key->curve) {
	case TEE_ECC_CURVE_NIST_P224:
		object_id = secp224r1;
		object_id_len = sizeof(secp224r1);
		break;
	case TEE_ECC_CURVE_NIST_P256:
		object_id = prime256v1;
		object_id_len = sizeof(prime256v1);
		break;
	case TEE_ECC_CURVE_NIST_P384:
		object_id = secp384r1;
		object_id_len = sizeof(secp384r1);
		break;
	case TEE_ECC_CURVE_NIST_P521:
		object_id = secp521r1;
		object_id_len = sizeof(secp521r1);
		break;
	case TEE_ECC_CURVE_SM2:
		object_id = sm2p256;
		object_id_len = sizeof(sm2p256);
		break;
	default:
		EMSG("unknown ec key length: %d", ec_key->key_len);
		res = TEE_ERROR_BAD_PARAMETERS;
		goto exit;
	}

	out_max = *pub_len;

	if (out_max < sizeof(asn1_oid_ec_pub) + object_id_len) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	/* set id ecPublicKey */
	TEE_MemMove(asn1_key + total_len, asn1_oid_ec_pub, sizeof(asn1_oid_ec_pub));
	total_len += sizeof(asn1_oid_ec_pub);
	out_max -= sizeof(asn1_oid_ec_pub);

	/* set id object id */
	TEE_MemMove(asn1_key + total_len, object_id, object_id_len);
	total_len += object_id_len;
	out_max -= object_id_len;

	//add SEQUENCE info in head
	res = asn1_compose_len(total_len, tmp_field, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set asn1_compose_len error!");
		goto exit;
	}

	if (out_max < tmp_len + 1) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(asn1_key + tmp_len + 1, asn1_key, total_len);
	*asn1_key = ASN1_SEQUENCE;
	total_len++;
	out_max--;
	TEE_MemMove(asn1_key + 1, tmp_field, tmp_len);
	total_len += tmp_len;
	out_max -= tmp_len;

	/* set public key object */
	TEE_MemMove(pubkey_string, compress_form, 2);
	pubkey_len += 2;
	TEE_MemMove(pubkey_string + pubkey_len, ec_key->x, ec_key->x_len);
	pubkey_len += ec_key->x_len;
	TEE_MemMove(pubkey_string + pubkey_len, ec_key->y, ec_key->y_len);
	pubkey_len += ec_key->y_len;
	res = asn1_set_object(pubkey_string, pubkey_len, ASN1_BIT_STRING, 0, asn1_key + total_len, out_max, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set private key object error!");
		goto exit;
	}
	total_len += tmp_len;
	out_max -= tmp_len;

	res = asn1_compose_len(total_len, tmp_field, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set asn1_compose_len error!");
		goto exit;
	}

	if (out_max < tmp_len + 1) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(asn1_key + tmp_len + 1, asn1_key, total_len);
	*asn1_key = ASN1_SEQUENCE;
	total_len++;
	out_max--;
	TEE_MemMove(asn1_key + 1, tmp_field, tmp_len);
	total_len += tmp_len;
	out_max -= tmp_len;

	*pub_len = total_len;

#if ASN1_DEBUG
	EMSG("rk_ec_pubkey_encode ok! asn1 key_len = %d", total_len);
#endif

exit:
	return res;
}

TEE_Result rk_ec_prikey_encode(uint8_t *asn1_key, uint32_t *pri_len, ec_key_t *ec_key)
{
	TEE_Result res;
	uint8_t tmp_field[5];
	uint32_t total_len = 0, tmp_len = 0;
	uint8_t version[1] = {0x01};
	uint8_t compress_form[2] = {0x00, 0x04};
	uint16_t ver_len = 1;
	uint32_t out_max;
	const uint8_t *object_id = NULL;
	uint32_t object_id_len = 0;
	uint32_t pubkey_string_len = 0;
	uint32_t pubkey_len = 0;
	uint8_t pubkey_string[4 + 2 + 132] = {0}; /* bit string tag&len + compress form + max public key length */
	uint8_t *pubkey = pubkey_string + 4;

	if (asn1_key == NULL || pri_len == NULL || ec_key == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

#if ASN1_DEBUG
	EMSG("key_len = %d", ec_key->key_len);
#endif

	switch (ec_key->curve) {
	case TEE_ECC_CURVE_NIST_P224:
		object_id = secp224r1;
		object_id_len = sizeof(secp224r1);
		break;
	case TEE_ECC_CURVE_NIST_P256:
		object_id = prime256v1;
		object_id_len = sizeof(prime256v1);
		break;
	case TEE_ECC_CURVE_NIST_P384:
		object_id = secp384r1;
		object_id_len = sizeof(secp384r1);
		break;
	case TEE_ECC_CURVE_NIST_P521:
		object_id = secp521r1;
		object_id_len = sizeof(secp521r1);
		break;
	case TEE_ECC_CURVE_SM2:
		object_id = sm2p256;
		object_id_len = sizeof(sm2p256);
		break;
	default:
		EMSG("unknown ec key length: %d", ec_key->key_len);
		res = TEE_ERROR_BAD_PARAMETERS;
		goto exit;
	}

	out_max = *pri_len;
	/*
	    for(i=0; i< sizeof(object_list)/sizeof(object_list[0]); i++){
	        res = asn1_set_object(object_list[i].data, *(object_list[i].data_len), object_list[i].tag,
	                        object_list[i].need_plus, asn1_key+total_len, out_max, &tmp_len);
	        if(res != TEE_SUCCESS){
	            EMSG("set %d object error!", i);
	            goto exit;
	        }
	        total_len += tmp_len;
	        out_max -= tmp_len;
	    }
	*/
	/* set version object */
	res = asn1_set_object(version, ver_len, ASN1_INTEGER, 0, asn1_key + total_len, out_max, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set version object error!");
		goto exit;
	}
	total_len += tmp_len;
	out_max -= tmp_len;

	/* set private key object */
	res = asn1_set_object(ec_key->d, ec_key->d_len, ASN1_OCT_STRING, 0, asn1_key + total_len, out_max, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set private key object error!");
		goto exit;
	}
	total_len += tmp_len;
	out_max -= tmp_len;

	/* reference to the used curve and public key is OPTIONAL */
	if (ec_key->x_len != 0 && ec_key->y_len != 0) {
		/* set object id context0 */
		res = asn1_set_object((uint8_t *)object_id, object_id_len, ASN1_CONTEXT0, 0, asn1_key + total_len, out_max,
		                      &tmp_len);
		if (res != TEE_SUCCESS) {
			EMSG("set object id error!");
			goto exit;
		}
		total_len += tmp_len;
		out_max -= tmp_len;

		/* set public key object */
		TEE_MemMove(pubkey, compress_form, 2);
		pubkey_len += 2;
		TEE_MemMove(pubkey + pubkey_len, ec_key->x, ec_key->x_len);
		pubkey_len += ec_key->x_len;
		TEE_MemMove(pubkey + pubkey_len, ec_key->y, ec_key->y_len);
		pubkey_len += ec_key->y_len;
		res = asn1_set_object(pubkey, pubkey_len, ASN1_BIT_STRING, 0, pubkey_string, sizeof(pubkey_string), &tmp_len);
		if (res != TEE_SUCCESS) {
			EMSG("set private key object error!");
			goto exit;
		}
		pubkey_string_len = tmp_len;

		/* set object id context1 */
		res = asn1_set_object(pubkey_string, pubkey_string_len, ASN1_CONTEXT1, 0, asn1_key + total_len, out_max,
		                      &tmp_len);
		if (res != TEE_SUCCESS) {
			EMSG("set private key object error!");
			goto exit;
		}
		total_len += tmp_len;
		out_max -= tmp_len;
	}

	res = asn1_compose_len(total_len, tmp_field, &tmp_len);
	if (res != TEE_SUCCESS) {
		EMSG("set asn1_compose_len error!");
		goto exit;
	}

	if (out_max < tmp_len + 1) {
		res = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}

	TEE_MemMove(asn1_key + tmp_len + 1, asn1_key, total_len);
	*asn1_key = ASN1_SEQUENCE;
	total_len++;
	TEE_MemMove(asn1_key + 1, tmp_field, tmp_len);
	total_len += tmp_len;

	*pri_len = total_len;

#if ASN1_DEBUG
	EMSG("rk_ec_prikey_encode ok! asn1 key_len = %d", total_len);
#endif

exit:
	return res;
}
