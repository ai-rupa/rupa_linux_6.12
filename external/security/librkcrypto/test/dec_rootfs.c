/*
 * Copyright (c) 2022 Rockchip Electronics Co. Ltd.
 */
#include "dec_rootfs.h"
#include "c_model.h"
#include "cmode_adapter.h"
#include "rkcrypto_core.h"
#include "rkcrypto_core_int.h"
#include "rkcrypto_mem.h"
#include "rkcrypto_otp_key.h"
#include "rkcrypto_trace.h"
#include "tee_client_api.h"
#include "test_utils.h"
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

// #define DEBUG

// uint8_t otp_key[32] = {
// 	0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
// 	0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
// 	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
// 	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
// };

#define CHUNK_SIZE (1 * 1024 * 1024)

#ifndef RK_CRYPTO_SERVICE_UUID
#define RK_CRYPTO_SERVICE_UUID                                                 \
	{                                                                          \
		0x0cacdb5d, 0x4fea, 0x466c, {                                          \
			0x97, 0x16, 0x3d, 0x54, 0x16, 0x52, 0x83, 0x0f                     \
		}                                                                      \
	}
#endif

#ifndef CRYPTO_SERVICE_CMD_OEM_OTP_KEY_CIPHER
#define CRYPTO_SERVICE_CMD_OEM_OTP_KEY_CIPHER 0x00000001
#endif

RK_RES dec_rootfs(char *device_path, unsigned long img_size) {
	int fd;
	char *src_buffer;
	TEEC_Context contex;
	RK_RES res = RK_CRYPTO_ERR_GENERIC;
	TEEC_Session session;
	uint32_t error_origin = 0;
	TEEC_UUID uuid = RK_CRYPTO_SERVICE_UUID;
	TEEC_SharedMemory sm;
	TEEC_Operation operation;

	res = TEEC_InitializeContext(NULL, &contex);
	if (res != TEEC_SUCCESS) {
		E_TRACE("TEEC_InitializeContext failed with code TEEC res= 0x%x", res);
		return res;
	}

	res = TEEC_OpenSession(&contex, &session, &uuid, TEEC_LOGIN_PUBLIC, NULL,
		NULL, &error_origin);
	if (res != TEEC_SUCCESS) {
		E_TRACE("TEEC_Opensession failed with code TEEC res= 0x%x origin 0x%x", res,
			error_origin);
		TEEC_FinalizeContext(&contex);
		return res;
	}

	sm.size = CHUNK_SIZE;
	sm.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	res = TEEC_AllocateSharedMemory(&contex, &sm);
	if (res != TEEC_SUCCESS) {
		E_TRACE("AllocateSharedMemory ERR! TEEC res= 0x%x", res);
		TEEC_CloseSession(&session);
		TEEC_FinalizeContext(&contex);
		return res;
	}

#ifdef DEBUG
	printf("device_path = %s\n", device_path);
#endif

	// 打开设备文件
	fd = open(device_path, O_RDONLY);
	if (fd == -1) {
		perror("open device file failed\n");
		return -1;
	}

#ifdef DEBUG
	printf("img size = %lu\n", img_size);
#endif
	src_buffer = mmap(NULL, img_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (src_buffer == MAP_FAILED) {
		perror("mmap device failed");
		close(fd);
		return -1;
	}

	close(fd);

	char tmp_path[256];
	snprintf(tmp_path, sizeof(tmp_path), "/tmp/dec.img");

	FILE *tmp_file = fopen(tmp_path, "wb");
	if (tmp_file == NULL) {
		perror("fopen tmp file failed");
		munmap(src_buffer, img_size);
		return -1;
	}
	fwrite(src_buffer, 1, img_size, tmp_file);
	fclose(tmp_file);
	munmap(src_buffer, img_size);

#ifdef DEBUG
	char md5_command[512];
	snprintf(md5_command, sizeof(md5_command), "md5sum %s", tmp_path);
	system(md5_command);
#endif

	int tmp_fd = open(tmp_path, O_RDWR | O_CREAT, 0777);
	if (tmp_fd == -1) {
		perror("open tmp file failed");
		close(fd);
		return -1;
	}

	// 查找空闲loop设备
	struct loop_info64 lo_info;
	int loop_fd = -1;
	for (int i = 0; i < 8; i++) {
		char loop_path[32];
		snprintf(loop_path, sizeof(loop_path), "/dev/loop%d", i);
		loop_fd = open(loop_path, O_RDWR);
		if (loop_fd == -1)
			continue;
		if (ioctl(loop_fd, LOOP_GET_STATUS64, &lo_info) == -1) {
			printf("get loop device: %s\n", loop_path);
			break;
		}
		close(loop_fd);
	}
	if (loop_fd == -1) {
		perror("no free loop device\n");
		return -1;
	}

	// 绑定loop设备到临时文件
	ioctl(loop_fd, LOOP_SET_FD, tmp_fd);

	// res = rk_write_oem_otp_key(RK_OEM_OTP_KEY1,
	// 			   otp_key, sizeof(otp_key));
	// printf("write otp key 0. res:%d\n", res);

	rk_cipher_config cipher_cfg;
	memset(&cipher_cfg, 0x00, sizeof(cipher_cfg));
	cipher_cfg.algo = RK_ALGO_AES;
	cipher_cfg.mode = RK_CIPHER_MODE_ECB;
	cipher_cfg.operation = RK_OP_CIPHER_DEC;
	cipher_cfg.key_len = 32;

	size_t offset = 0;
	while (offset < img_size) {
		size_t chunk_size =
			(img_size - offset) < CHUNK_SIZE ? (img_size - offset) : CHUNK_SIZE;

		src_buffer = mmap(NULL, chunk_size, PROT_READ | PROT_WRITE, MAP_SHARED,
			loop_fd, offset);
		if (src_buffer == MAP_FAILED) {
			perror("mmap chunk failed");
			close(loop_fd);
			close(tmp_fd);
			return -1;
		}

		memcpy(sm.buffer, src_buffer, chunk_size);

		memset(&operation, 0, sizeof(TEEC_Operation));
		operation.params[0].value.a = RK_OEM_OTP_KEY0;
		operation.params[1].tmpref.buffer = &cipher_cfg;
		operation.params[1].tmpref.size = sizeof(rk_cipher_config);
		operation.params[2].memref.parent = &sm;
		operation.params[2].memref.offset = 0;
		operation.params[2].memref.size = chunk_size;

		operation.paramTypes =
			TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_MEMREF_TEMP_INPUT,
				TEEC_MEMREF_PARTIAL_INOUT, TEEC_NONE);

		// 解密当前块
		res = TEEC_InvokeCommand(&session, CRYPTO_SERVICE_CMD_OEM_OTP_KEY_CIPHER,
			&operation, &error_origin);
		if (res != TEEC_SUCCESS) {
			E_TRACE("InvokeCommand ERR! TEEC res= 0x%x, error_origin= 0x%x", res,
				error_origin);
		} else {
#ifdef DEBUG
			printf("cipher_virt sm.size = %lu\n", chunk_size);
#endif
		}

		memcpy(src_buffer, sm.buffer, chunk_size);

		// 同步数据到文件
		if (msync(src_buffer, chunk_size, MS_SYNC) == -1) {
			perror("msync chunk failed");
			munmap(sm.buffer, chunk_size);
			close(loop_fd);
			close(tmp_fd);
			return -1;
		}

		// 解除映射
		if (munmap(src_buffer, chunk_size) == -1) {
			perror("munmap chunk failed");
		}

		offset += chunk_size;
	}

#ifdef DEBUG
	// 解密后的文件的 MD5 值
	char final_md5_command[512];
	snprintf(final_md5_command, sizeof(final_md5_command), "md5sum %s", tmp_path);
	system(final_md5_command);
#endif

	// unlink(tmp_path);  // 删除临时文件
	TEEC_ReleaseSharedMemory(&sm);

	TEEC_CloseSession(&session);

	TEEC_FinalizeContext(&contex);
	return 0; // 成功返回
}
