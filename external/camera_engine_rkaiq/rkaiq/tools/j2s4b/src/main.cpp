/* Copyright (C)
 * 2022 - WuQiang xianlee.wu@rock-chips.com
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "cJSON.h"
#include "cJSON_Utils.h"
#include "j2s4b/j2s.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <stdbool.h>
#include <stddef.h>
#include <string>
#include "RkAiqsceneManager.h"
#include "RkAiqVersion.h"

long read_file_all(void **rdata, const char *filename) {
  FILE *f = NULL;
  long len = 0;
  char *data = NULL;

  /* open in read binary mode */
  f = fopen(filename, "rb");
  /* get the length */
  fseek(f, 0, SEEK_END);
  len = ftell(f);
  fseek(f, 0, SEEK_SET);

  data = (char *)malloc(len);

  fread(data, 1, len, f);
  fclose(f);

  *rdata = data;

  return len;
}

int write_file_all(const char *fpath, void *data, size_t len) {
  FILE *ofp = NULL;

  ofp = fopen(fpath, "wb+");
  if (!ofp) {
    return -1;
  }

  fwrite(data, 1, len, ofp);

  fclose(ofp);

  return 0;
}

size_t j2s_root_struct_size(j2s_ctx *ctx) {
  j2s_struct *root_struct = NULL;
  j2s_obj *obj = NULL;
  int obj_index = -1;
  uint32_t size;

  root_struct = &ctx->structs[ctx->root_index];
  obj_index = root_struct->child_index;

  while (obj_index >= 0) {
    obj = &ctx->objs[obj_index];
    if (obj->flags & J2S_FLAG_POINTER) {
      size = sizeof(void*);
    } else {
      size = obj->base_elem_size;
    }
    printf("[%s] struct size:%u %u\n", obj->name, obj->base_elem_size, size);
    obj_index = obj->next_index;
  }

  return (size_t)j2s_struct_size(ctx, ctx->root_index);
}

int j2s_scan_map(uint8_t *data, size_t len) {
  size_t map_len = *(size_t *)(data + (len - sizeof(size_t)));
  size_t map_offset = *(size_t *)(data + (len - sizeof(size_t) * 2));
  map_index_t *map_addr = NULL;
  size_t map_index = 0;

  map_addr = (map_index_t *)(data + map_offset);

  for (map_index = 0; map_index < map_len; map_index++) {
    map_index_t tmap = (map_addr[map_index]);
    void **dst_obj_addr = (void **)(data + (size_t)tmap.dst_offset);
    *dst_obj_addr = data + (uintptr_t)tmap.ptr_offset;
  }

  return 0;
}

int show_usage(int argc, char *argv[]) {
  int ret = 0;
  j2s_ctx ctx;
  j2s_init(&ctx);

  if (argc < 2) {
    printf("Rockchip json/binary iq converter tool\n");
    printf("Copyright (C) Rockchip 2022\n");
    printf("- build hash: %s\n", GIT_VERSION);
    printf("- build date: %s\n", __DATE__);
    printf("- magic code: %d\n", ctx.magic);
    printf("\nUsage:\n");
    printf("  json to bin:  %s <input.json> <output.bin>\n", argv[0]);
    printf("  bin to json:  %s -d <input.bin> [output.json]\n", argv[0]);
    printf("  bin to json:  %s --dump <input.bin> [output.json]\n", argv[0]);
    printf("\n  -d, --dump    Convert binary IQ file to JSON format\n");
    printf("  -c, --check  Verify bin matches json (json to bin mode only)\n");
    printf("\e[33mexample: %s sc4336.json sc4336.bin\e[0m\n", argv[0]);
    printf("\e[33mexample: %s -d sc4336.bin sc4336_output.json\e[0m\n", argv[0]);
    printf("\e[33mexample: %s -d sc4336.bin\e[0m (output to stdout)\n", argv[0]);
    printf("\nNotice:\e[31m YOUR AIQ LIBRARY VERSION MUST MATCH %s \e[0m\n",
           GIT_VERSION);
    printf("       \e[31m Or will lead to unpredictable errors!\e[0m\n");
    ret = -1;
  }

  j2s_deinit(&ctx);

  return ret;
}

// bin to json mode
static int bin_to_json_mode(int argc, char *argv[]) {
  void *bin_file_data = NULL;
  size_t bin_file_size = 0;
  cJSON *root_json = NULL;
  j2s_ctx ctx;

  if (argc < 3) {
    printf("Error: missing input bin file\n");
    return -1;
  }

  const char *bin_file = argv[2];
  const char *output_file = (argc > 3) ? argv[3] : NULL;

  printf("[bin to json] input: %s\n", bin_file);

  bin_file_size = read_file_all(&bin_file_data, bin_file);
  if (!bin_file_data || bin_file_size <= 0) {
    printf("Error: failed to read bin file: %s\n", bin_file);
    return -1;
  }
  printf("[bin to json] read %zu bytes\n", bin_file_size);

  // scan and fix pointers
  j2s_scan_map((uint8_t *)bin_file_data, bin_file_size);

  j2s_init(&ctx);
  root_json = j2s_root_struct_to_json(&ctx, bin_file_data);

  if (!root_json) {
    printf("Error: failed to convert bin to json\n");
    j2s_deinit(&ctx);
    free(bin_file_data);
    return -1;
  }

  // Extract and print version info from product_info
  cJSON *product_info = RkCam_cJSON_GetObjectItem(root_json, "product_info");
  if (product_info) {
    cJSON *iq_struct_mark_json = RkCam_cJSON_GetObjectItem(product_info, "iq_struct_mark");
    cJSON *chip_name_json = RkCam_cJSON_GetObjectItem(product_info, "chip_name");
    cJSON *iq_ver_json = RkCam_cJSON_GetObjectItem(product_info, "iq_ver");

    const char *bin_iq_struct_mark = iq_struct_mark_json ? iq_struct_mark_json->valuestring : "N/A";
    const char *bin_chip_name = chip_name_json ? chip_name_json->valuestring : "N/A";
    const char *bin_iq_ver = iq_ver_json ? iq_ver_json->valuestring : "N/A";

    printf("\n========== Version Info ==========\n");
    printf("RK_AIQ_VERSION: %s\n", RK_AIQ_VERSION);
    printf("RK_AIQ_IQ_HEAD_VERSION: %s\n", RK_AIQ_IQ_HEAD_VERSION);
    printf("----------------------------------\n");
    printf("Bin file info:\n");
    printf("  Chip Name:     %s\n", bin_chip_name);
    printf("  IQ Ver:        %s\n", bin_iq_ver);
    printf("  IQ Struct Mark: %s\n", bin_iq_struct_mark);
    printf("----------------------------------\n");
    printf("Header defines:\n");
    printf("  CHIP_NAME:      %s\n", CHIP_NAME);
    printf("  IQ_STRUCT_MARK: %s\n", IQ_STRUCT_MARK);
    printf("----------------------------------\n");

    // Compare versions
    bool mark_match = (strcmp(bin_iq_struct_mark, IQ_STRUCT_MARK) == 0);
    bool chip_match = (strcmp(bin_chip_name, CHIP_NAME) == 0);

    printf("Version Check:\n");
    printf("  IQ Struct Mark: %s\n", mark_match ? "\e[32mMatch\e[0m" : "\e[31mMismatch\e[0m");
    printf("  Chip Name:       %s\n", chip_match ? "\e[32mMatch\e[0m" : "\e[31mMismatch\e[0m");
    printf("==================================\n\n");

    if (!mark_match || !chip_match) {
      printf("\e[33mWarning: Version mismatch may cause issues!\e[0m\n\n");
    }
  }

  // output
  if (output_file) {
    FILE *fp = fopen(output_file, "w");
    if (fp) {
      char *json_str = RkCam_cJSON_Print(root_json);
      fputs(json_str, fp);
      fclose(fp);
      free(json_str);
      printf("[bin to json] output: %s\n", output_file);
    } else {
      printf("Error: failed to write output file: %s\n", output_file);
    }
  } else {
    // output to stdout
    char *json_str = RkCam_cJSON_Print(root_json);
    puts(json_str);
    free(json_str);
  }

  j2s_deinit(&ctx);
  RkCam_cJSON_Delete(root_json);
  free(bin_file_data);

  printf("[done]\n");
  return 0;
}

int main(int argc, char *argv[]) {
  // check for dump mode first
  if (argc >= 2 && (strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--dump") == 0)) {
    return bin_to_json_mode(argc, argv);
  }

  if (0 != show_usage(argc, argv)) {
    return 0;
  }

  void *struct_ptr = NULL;
  void *file_data = NULL;
  j2s_ctx j2s4b_ctx;
  j2s_struct *root_struct = NULL;
  int root_size = 0;
  void *bin_file_data = NULL;
  size_t bin_file_size = 0;
  cJSON *root_json = NULL;
  cJSON *base_json = NULL;
  cJSON *full_json = NULL;

  read_file_all(&file_data, argv[1]);

  j2s_init(&j2s4b_ctx);

  root_size = j2s_root_struct_size(&j2s4b_ctx);

  root_struct = &j2s4b_ctx.structs[j2s4b_ctx.root_index];

  printf("[root] struct info:[%s][%d]\n", root_struct->name,
         root_struct->child_index);

  base_json = RkCam_cJSON_Parse((char *)file_data);
  if (!base_json) {
    return -1;
  }

  RkAiqsceneManager::mergeMultiSceneIQ(base_json);

#ifndef RKAIQ_J2S4B_DEV
  full_json = RkCam_cJSON_Parse(RkCam_cJSON_Print(base_json));
#else
  if (file_data)
    free(file_data);
  file_data = NULL;
  full_json = base_json;
#endif

  j2s_json_to_bin_root(&j2s4b_ctx, full_json,
                       &struct_ptr, root_size, argv[2]);

  j2s_deinit(&j2s4b_ctx);

#ifndef RKAIQ_J2S4B_DEV
  bin_file_size = read_file_all(&bin_file_data, argv[2]);

  j2s_scan_map((uint8_t *)bin_file_data, bin_file_size);

  j2s_init(&j2s4b_ctx);
  root_json = j2s_root_struct_to_json(&j2s4b_ctx, bin_file_data);

  int cret =
      RkCam_cJSON_Compare(RkCam_cJSON_Parse((char *)file_data), root_json, 1);

  printf("[Check] bin struct match input json: %s\n",
         !cret ? "\e[32mYes\e[0m" : "No");

  printf("[done] ...\n");

  j2s_deinit(&j2s4b_ctx);

  if (file_data) {
    free(file_data);
    file_data = NULL;
  }
  if (bin_file_data) {
    free(bin_file_data);
    bin_file_data = NULL;
  }
  if (full_json) {
    RkCam_cJSON_Delete(full_json);
  }
#endif

  if (base_json) {
    RkCam_cJSON_Delete(base_json);
  }

  return 0;
}
