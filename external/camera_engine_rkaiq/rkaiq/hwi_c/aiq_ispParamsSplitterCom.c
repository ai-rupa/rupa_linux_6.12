/*
 *  Copyright (c) 2024 Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Cody Xie <cody.xie@rock-chips.com>
 */

#include "hwi_c/aiq_ispParamsSplitterCom.h"

#include "algos/ae/rk_aiq_types_ae_hw.h"
#include "hwi_c/aiq_ispParamsSplitter.h"
#include "include/common/rk_isp20_hw.h"
#include "include/common/rkisp3-config.h"
#include "include/common/rkisp32-config.h"

//#define DEBUG

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// Internal implementation to split submodule configs eg:
//  * SplitIsp2xAeLittle
//  * SplitIsp2xAeBig0
//  * SplitIsp3xAeBig0
// Then a module can be an aggregation of submodules
// for example:
// SplitAecParams
//   -> SplitIsp2xAeLittle
//   -> SplitIsp3xAeBig0

/*********************************************/
/*            Aec hwi splitter               */
/*********************************************/

void AiqIspParamsSplitter_SplitAecWeight(u8* ori_weight, u8* left_weight, u8* right_weight,
                                         WinSplitMode mode, u8 wnd_num) {
    switch (mode) {
        case LEFT_AND_RIGHT_MODE:
            for (int i = 0; i < wnd_num; i++) {
                for (int j = 0; j < wnd_num; j++) {
                    left_weight[i * wnd_num + j] = ori_weight[i * wnd_num + j / 2];
                    right_weight[i * wnd_num + j] =
                        ori_weight[i * wnd_num + j / 2 + j % 2 + wnd_num / 2];
                }
            }
            break;
        case LEFT_MODE:
        case RIGHT_MODE:
            memcpy(left_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            memcpy(right_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
        default:
            break;
    }
}

static void SplitAecSubWin(u8* subwin_en, struct isp2x_window* ori_win,
                           struct isp2x_window* left_win, struct isp2x_window* right_win,
                           Splitter_Rectangle_t left_isp_rect_,
                           Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    for (int i = 0; i < ISP2X_RAWAEBIG_SUBWIN_NUM; i++) {
        if (subwin_en[i] == 1) {  // only when subwin is enabled, should split hwi params
            if (ori_win[i].h_offs + ori_win[i].h_size <= left_isp_rect_.w) {
#ifdef DEBUG
                printf("win locate in left isp\n");
#endif
                mode[i] = LEFT_MODE;

                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = 0;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = ori_win[i].v_offs;
                right_win[i].v_size = ori_win[i].v_size;
            } else if (ori_win[i].h_offs >= right_isp_rect_.x) {
#ifdef DEBUG
                printf("win locate in right isp\n");
#endif
                mode[i] = RIGHT_MODE;

                // win only locate in right isp, actually stats of left isp would not be used
                left_win[i].h_offs = 0;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = ori_win[i].h_offs - right_isp_rect_.x;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = ori_win[i].v_offs;
                right_win[i].v_size = ori_win[i].v_size;

            } else {
#ifdef DEBUG
                printf(" win locate at left&right isp\n");
#endif
                mode[i] = LEFT_AND_RIGHT_MODE;

                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = MAX(0, ((long)left_isp_rect_.w - (long)left_win[i].h_offs));
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = left_win[i].h_offs + left_win[i].h_size - right_isp_rect_.x;
                right_win[i].h_size = MAX(0, ((long)ori_win[i].h_size - (long)left_win[i].h_size));
                right_win[i].v_offs = ori_win[i].v_offs;
                right_win[i].v_size = ori_win[i].v_size;
            }
        }
    }
}

static void SplitAecCalcBlockSize(struct isp2x_window* left_win, struct isp2x_window* right_win,
                                  u8 wnd_num, Splitter_Rectangle_t right_isp_rect_, u16* block_h) {
    bool loop_en = true;

    while (loop_en && *block_h > 0) {
        left_win->h_size  = *block_h * wnd_num;
        right_win->h_offs = (left_win->h_size + left_win->h_offs > right_isp_rect_.x)
                                ? left_win->h_size + left_win->h_offs - right_isp_rect_.x
                                : 0;

        if ((u32)(right_win->h_offs + *block_h * wnd_num) > right_isp_rect_.w - 1) {
            (*block_h)--;

        } else {
            loop_en = false;

            // if (*block_h % 2)
            //(*block_h)--;

            left_win->h_size  = *block_h * wnd_num;
            right_win->h_offs = (left_win->h_size + left_win->h_offs > right_isp_rect_.x)
                                    ? left_win->h_size + left_win->h_offs - right_isp_rect_.x
                                    : 0;
            right_win->h_offs = right_win->h_offs & 0xfffe;
            right_win->h_size = *block_h * wnd_num;
        }
    }
}

void SplitAecWin(struct isp2x_window* ori_win, struct isp2x_window* left_win,
                        struct isp2x_window* right_win, u8 wnd_num,
                        Splitter_Rectangle_t left_isp_rect_, Splitter_Rectangle_t right_isp_rect_,
                        WinSplitMode* mode) {
    // win only locate in left isp, actually stats of right isp would not be used
    if (ori_win->h_offs + ori_win->h_size <= left_isp_rect_.w) {
#ifdef DEBUG
        printf("win locate in left isp\n");
#endif
        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;
    } else if (ori_win->h_offs >= right_isp_rect_.x) {
#ifdef DEBUG
        printf("win locate in right isp\n");
#endif
        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = 0;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs - right_isp_rect_.x;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;

    } else {
        if ((ori_win->h_offs + ori_win->h_size / 2) <= left_isp_rect_.w &&
            right_isp_rect_.x <= (ori_win->h_offs + ori_win->h_size / 2)) {
#ifdef DEBUG
            printf(" win locate at left&right isp,and center line locate in overlapping zone!\n");
#endif
            *mode = LEFT_AND_RIGHT_MODE;

            // win center locate at overlap zone
            left_win->h_offs = ori_win->h_offs;
            left_win->v_offs = ori_win->v_offs;
            left_win->v_size = ori_win->v_size;

            right_win->v_offs = ori_win->v_offs;
            right_win->v_size = ori_win->v_size;

            u16 block_h = ori_win->h_size / (2 * wnd_num);
            SplitAecCalcBlockSize(left_win, right_win, wnd_num, right_isp_rect_, &block_h);
        } else {
#ifdef DEBUG
            printf(
                " win locate at left&right isp,but center line not locate in overlapping zone!\n");
#endif
            if ((ori_win->h_offs + ori_win->h_size / 2) < right_isp_rect_.x) {
                left_win->h_offs = ori_win->h_offs;
                left_win->v_offs = ori_win->v_offs;
                left_win->v_size = ori_win->v_size;

                right_win->v_offs = ori_win->v_offs;
                right_win->v_size = ori_win->v_size;

                u16 h_size_tmp1 = left_isp_rect_.w - ori_win->h_offs;
                u16 h_size_tmp2 = (right_isp_rect_.x - ori_win->h_offs) * 2;

                if (abs(ori_win->h_size - h_size_tmp1) < abs(ori_win->h_size - h_size_tmp2)) {
#ifdef DEBUG
                    printf("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp1);
#endif
                    *mode = LEFT_MODE;

                    ori_win->h_size = h_size_tmp1;

                    left_win->h_size = ori_win->h_size;
                    // actually stats of right isp would not be used
                    right_win->h_offs = 0;
                    right_win->h_size = ori_win->h_size;
                } else {
#ifdef DEBUG
                    printf("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp2);
#endif
                    *mode = LEFT_AND_RIGHT_MODE;

                    ori_win->h_size = h_size_tmp2;

                    u16 block_h = ori_win->h_size / (2 * wnd_num);
                    SplitAecCalcBlockSize(left_win, right_win, wnd_num, right_isp_rect_, &block_h);
                }
            } else {
                left_win->v_offs = ori_win->v_offs;
                left_win->v_size = ori_win->v_size;

                right_win->v_offs = ori_win->v_offs;
                right_win->v_size = ori_win->v_size;

                u16 h_size_tmp1 = ori_win->h_offs + ori_win->h_size - right_isp_rect_.x;
                u16 h_size_tmp2 = (ori_win->h_offs + ori_win->h_size - left_isp_rect_.w) * 2;

                if (abs(ori_win->h_size - h_size_tmp1) < abs(ori_win->h_size - h_size_tmp2)) {
#ifdef DEBUG
                    printf("correct glb.h_off %d to %u\n", ori_win->h_offs, right_isp_rect_.x);
                    printf("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp1);
#endif

                    *mode = RIGHT_MODE;

                    ori_win->h_size = h_size_tmp1;
                    ori_win->h_offs = right_isp_rect_.x;

                    right_win->h_offs = 0;
                    right_win->h_size = ori_win->h_size;

                    // actually stats of left isp would not be used
                    left_win->h_offs = 0;
                    left_win->h_size = ori_win->h_size;
                } else {
#ifdef DEBUG
                    printf("correct glb.h_off %d to %d\n", ori_win->h_offs,
                           ori_win->h_offs + ori_win->h_size -
                               (ori_win->h_offs + ori_win->h_size - left_isp_rect_.w) * 2);
                    printf("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp2);
#endif
                    *mode = LEFT_AND_RIGHT_MODE;

                    ori_win->h_offs = ori_win->h_offs + ori_win->h_size -
                                      (ori_win->h_offs + ori_win->h_size - left_isp_rect_.w) * 2;
                    ori_win->h_size  = h_size_tmp2;
                    left_win->h_offs = ori_win->h_offs;

                    u16 block_h = ori_win->h_size / (2 * wnd_num);
                    SplitAecCalcBlockSize(left_win, right_win, wnd_num, right_isp_rect_, &block_h);
                }
            }
        }
    }
}

void AiqIspParamsSplitter_SplitAecWeightForThree(u8* ori_weight,
                                         u8* left_weight, u8* middle_weight, u8* right_weight,
                                         WinSplitMode mode, u8 wnd_num) {
    switch (mode) {
        case LEFT_AND_RIGHT_MODE:
        {
            u8* merge_wgt[3] = {left_weight, middle_weight, right_weight};
            for (int i = 0; i < wnd_num; i++) {
                for (int j = 0; j < 3 * wnd_num; j++) {
                    int wgt_idx = j / wnd_num;
                    int sub_idx = j % wnd_num;
                    int ori_idx = j / 3;
                    merge_wgt[wgt_idx][i * wnd_num + sub_idx] = ori_weight[i * wnd_num + ori_idx];
                }
            }
        }
            break;
        case LEFT_MODE:
        case MIDDLE_MODE:
        case RIGHT_MODE:
            memcpy(left_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            memcpy(middle_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            memcpy(right_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            break;
        default:
            break;
    }
}

static void SplitAecSubWinForThree(u8* subwin_en,
                           struct isp2x_window* ori_win,
                           struct isp2x_window* left_win,
                           struct isp2x_window* middle_win,
                           struct isp2x_window* right_win,
                           Splitter_Rectangle_t left_isp_rect_,
                           Splitter_Rectangle_t middle_isp_rect_,
                           Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    for (int i = 0; i < ISP2X_RAWAEBIG_SUBWIN_NUM; i++) {
        if (subwin_en[i] == 1) {  // only when subwin is enabled, should split hwi params
            if (ori_win[i].h_offs + ori_win[i].h_size <= left_isp_rect_.w) {
#ifdef DEBUG
                printf("win locate in left isp\n");
#endif
                mode[i] = LEFT_MODE;

                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                middle_win[i].h_offs = 0;
                middle_win[i].h_size = ori_win[i].h_size;
                middle_win[i].v_offs = ori_win[i].v_offs;
                middle_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = 0;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = ori_win[i].v_offs;
                right_win[i].v_size = ori_win[i].v_size;
            } else if (ori_win[i].h_offs >= right_isp_rect_.x) {
#ifdef DEBUG
                printf("win locate in right isp\n");
#endif
                mode[i] = RIGHT_MODE;

                // win only locate in right isp, actually stats of left isp would not be used
                left_win[i].h_offs = 0;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                middle_win[i].h_offs = 0;
                middle_win[i].h_size = ori_win[i].h_size;
                middle_win[i].v_offs = ori_win[i].v_offs;
                middle_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = ori_win[i].h_offs - right_isp_rect_.x;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = ori_win[i].v_offs;
                right_win[i].v_size = ori_win[i].v_size;

            } else if (ori_win[i].h_offs + ori_win[i].h_size <= middle_isp_rect_.w + middle_isp_rect_.x &&
                       ori_win[i].h_offs >= middle_isp_rect_.x) {
                mode[i] = MIDDLE_MODE;

                // win only locate in right isp, actually stats of left isp would not be used
                left_win[i].h_offs = 0;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                middle_win[i].h_offs = ori_win[i].h_offs - middle_isp_rect_.x;
                middle_win[i].h_size = ori_win[i].h_size;
                middle_win[i].v_offs = ori_win[i].v_offs;
                middle_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = 0;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = ori_win[i].v_offs;
                right_win[i].v_size = ori_win[i].v_size;

            } else {
#ifdef DEBUG
                printf(" win locate at left&right isp\n");
#endif
                mode[i] = LEFT_AND_RIGHT_MODE;

                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = MAX(0, ((long)left_isp_rect_.w - (long)left_win[i].h_offs));
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                middle_win[i].h_offs = left_win[i].h_offs + left_win[i].h_size - middle_isp_rect_.x;
                middle_win[i].h_size = MAX(0, ((long)middle_isp_rect_.w - (long)middle_win[i].h_offs - (long)middle_isp_rect_.x));
                middle_win[i].v_offs = ori_win[i].v_offs;
                middle_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = middle_win[i].h_offs + middle_win[i].h_size + middle_isp_rect_.x - right_isp_rect_.x;
                right_win[i].h_size = MAX(0, ((long)ori_win[i].h_size - (long)left_win[i].h_size - (long)middle_win[i].h_size));
                right_win[i].v_offs = ori_win[i].v_offs;
                right_win[i].v_size = ori_win[i].v_size;
            }
        }
    }
}

void SplitAecWinForThree(struct isp2x_window* ori_win,
                        struct isp2x_window* left_win,
                        struct isp2x_window* middle_win,
                        struct isp2x_window* right_win, u8 wnd_num,
                        Splitter_Rectangle_t left_isp_rect_,
                        Splitter_Rectangle_t middle_isp_rect_,
                        Splitter_Rectangle_t right_isp_rect_,
                        WinSplitMode* mode) {
    // win only locate in left isp, actually stats of right isp would not be used
    if (ori_win->h_offs + ori_win->h_size <= left_isp_rect_.w) {
#ifdef DEBUG
        printf("win locate in left isp\n");
#endif
        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = 0;
        middle_win->h_size = ori_win->h_size;
        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;
    } else if (ori_win->h_offs >= right_isp_rect_.x) {
#ifdef DEBUG
        printf("win locate in right isp\n");
#endif
        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = 0;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = 0;
        middle_win->h_size = ori_win->h_size;
        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs - right_isp_rect_.x;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;

    } else if (ori_win->h_offs + ori_win->h_size <= middle_isp_rect_.w + middle_isp_rect_.x &&
               ori_win->h_offs >= middle_isp_rect_.x) {

        *mode = MIDDLE_MODE;

        left_win->h_offs = 0;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = ori_win->h_offs - middle_isp_rect_.x;
        middle_win->h_size = ori_win->h_size;
        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;
    } else {
        *mode = LEFT_AND_RIGHT_MODE;

        // win center locate at overlap zone
        left_win->h_offs = ori_win->h_offs;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;

        u16 block_h = ori_win->h_size / (3 * wnd_num);

        left_win->h_size  = block_h * wnd_num;
        middle_win->h_offs = (left_win->h_size + left_win->h_offs) > middle_isp_rect_.x ?
                                left_win->h_size + left_win->h_offs - middle_isp_rect_.x : 0;
        middle_win->h_size = block_h * wnd_num;
        right_win->h_offs = (middle_win->h_size + middle_win->h_offs + middle_isp_rect_.x > right_isp_rect_.x)
                                ? middle_win->h_size + middle_win->h_offs + middle_isp_rect_.x - right_isp_rect_.x
                                : 0;
        right_win->h_size = block_h * wnd_num;
    }
}

void AiqIspParamsSplitter_SplitAecWeightForThreeV(u8* ori_weight,
                                         u8* left_weight, u8* middle_weight, u8* right_weight,
                                         WinSplitMode mode, u8 wnd_num) {
    switch (mode) {
        case LEFT_AND_RIGHT_MODE:
        {
            u8* merge_wgt[3] = {left_weight, middle_weight, right_weight};
            for (int i = 0; i < wnd_num; i++) {
                int wgt_idx0 = 3 * i / wnd_num;
                int wgt_idx1 = (3 * i + 1) / wnd_num;
                int wgt_idx2 = (3 * i + 2) / wnd_num;
                int sub_idx0 = 3 * i % wnd_num;
                int sub_idx1 = (3 * i + 1) % wnd_num;
                int sub_idx2 = (3 * i + 2) % wnd_num;
                for (int j = 0; j < wnd_num; j++) {
                    merge_wgt[wgt_idx0][sub_idx0 * wnd_num + j] = ori_weight[i * wnd_num + j];
                    merge_wgt[wgt_idx1][sub_idx1 * wnd_num + j] = ori_weight[i * wnd_num + j];
                    merge_wgt[wgt_idx2][sub_idx2 * wnd_num + j] = ori_weight[i * wnd_num + j];
                }
            }
        }
            break;
        case LEFT_MODE:
        case MIDDLE_MODE:
        case RIGHT_MODE:
            memcpy(left_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            memcpy(middle_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            memcpy(right_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            break;
        default:
            break;
    }
}

static void SplitAecSubWinForThreeV(u8* subwin_en,
                           struct isp2x_window* ori_win,
                           struct isp2x_window* left_win,
                           struct isp2x_window* middle_win,
                           struct isp2x_window* right_win,
                           Splitter_Rectangle_t left_isp_rect_,
                           Splitter_Rectangle_t middle_isp_rect_,
                           Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    for (int i = 0; i < ISP2X_RAWAEBIG_SUBWIN_NUM; i++) {
        if (subwin_en[i] == 1) {  // only when subwin is enabled, should split hwi params
            if (ori_win[i].v_offs + ori_win[i].v_size <= left_isp_rect_.h) {
#ifdef DEBUG
                printf("win locate in left isp\n");
#endif
                mode[i] = LEFT_MODE;

                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                middle_win[i].h_offs = ori_win[i].h_offs;
                middle_win[i].h_size = ori_win[i].h_size;
                middle_win[i].v_offs = 0;
                middle_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = ori_win[i].h_offs;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = 0;
                right_win[i].v_size = ori_win[i].v_size;
            } else if (ori_win[i].v_offs >= right_isp_rect_.y) {
#ifdef DEBUG
                printf("win locate in right isp\n");
#endif
                mode[i] = RIGHT_MODE;

                // win only locate in right isp, actually stats of left isp would not be used
                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = 0;
                left_win[i].v_size = ori_win[i].v_size;

                middle_win[i].h_offs = ori_win[i].h_offs;
                middle_win[i].h_size = ori_win[i].h_size;
                middle_win[i].v_offs = 0;
                middle_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = ori_win[i].h_offs;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = ori_win[i].v_offs - right_isp_rect_.y;
                right_win[i].v_size = ori_win[i].v_size;

            } else if (ori_win[i].v_offs + ori_win[i].v_size <= middle_isp_rect_.h + middle_isp_rect_.y &&
                       ori_win[i].v_offs >= middle_isp_rect_.y) {
                mode[i] = MIDDLE_MODE;

                // win only locate in right isp, actually stats of left isp would not be used
                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = 0;
                left_win[i].v_size = ori_win[i].v_size;

                middle_win[i].h_offs = ori_win[i].h_offs;
                middle_win[i].h_size = ori_win[i].h_size;
                middle_win[i].v_offs = ori_win[i].v_offs - middle_isp_rect_.y;
                middle_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = ori_win[i].h_offs;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = 0;
                right_win[i].v_size = ori_win[i].v_size;

            } else {
#ifdef DEBUG
                printf(" win locate at left&right isp\n");
#endif
                mode[i] = LEFT_AND_RIGHT_MODE;

                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = MAX(0, ((long)left_isp_rect_.h - (long)left_win[i].v_offs));

                middle_win[i].h_offs = ori_win[i].h_offs;
                middle_win[i].h_size = ori_win[i].h_size;
                middle_win[i].v_offs = left_win[i].v_offs + left_win[i].v_size - middle_isp_rect_.y;
                middle_win[i].v_size = MAX(0, ((long)middle_isp_rect_.h - (long)middle_win[i].v_offs - (long)middle_isp_rect_.y));

                right_win[i].h_offs = ori_win[i].h_offs;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = middle_win[i].v_offs + middle_win[i].v_size + middle_isp_rect_.y - right_isp_rect_.y;
                right_win[i].v_size = MAX(0, ((long)ori_win[i].v_size - (long)left_win[i].v_size - (long)middle_win[i].v_size));
            }
        }
    }
}

void SplitAecWinForThreeV(struct isp2x_window* ori_win,
                        struct isp2x_window* left_win,
                        struct isp2x_window* middle_win,
                        struct isp2x_window* right_win, u8 wnd_num,
                        Splitter_Rectangle_t left_isp_rect_,
                        Splitter_Rectangle_t middle_isp_rect_,
                        Splitter_Rectangle_t right_isp_rect_,
                        WinSplitMode* mode) {
    // win only locate in left isp, actually stats of right isp would not be used
    if (ori_win->v_offs + ori_win->v_size <= left_isp_rect_.h) {
#ifdef DEBUG
        printf("win locate in left isp\n");
#endif
        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;
        middle_win->v_offs = 0;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = 0;
        right_win->v_size = ori_win->v_size;
    } else if (ori_win->v_offs >= right_isp_rect_.y) {
#ifdef DEBUG
        printf("win locate in right isp\n");
#endif
        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = 0;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;
        middle_win->v_offs = 0;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs - right_isp_rect_.y;
        right_win->v_size = ori_win->v_size;

    } else if (ori_win->v_offs + ori_win->v_size <= middle_isp_rect_.h + middle_isp_rect_.y &&
                ori_win->v_offs >= middle_isp_rect_.y) {
        *mode = MIDDLE_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = 0;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;
        middle_win->v_offs = ori_win->v_offs - middle_isp_rect_.y;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = 0;
        right_win->v_size = ori_win->v_size;

    } else {
        *mode = LEFT_AND_RIGHT_MODE;

        // win center locate at overlap zone
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;

        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;

        u16 block_v = ori_win->v_size / (3 * wnd_num);

        left_win->v_size  = block_v * wnd_num;
        middle_win->v_offs = (left_win->v_size + left_win->v_offs) > middle_isp_rect_.y ?
                                left_win->v_size + left_win->v_offs - middle_isp_rect_.y : 0;
        middle_win->v_size = block_v * wnd_num;
        right_win->v_offs = (middle_win->v_size + middle_win->v_offs + middle_isp_rect_.y > right_isp_rect_.y)
                                ? middle_win->v_size + middle_win->v_offs + middle_isp_rect_.y - right_isp_rect_.y
                                : 0;
        right_win->v_size = block_v * wnd_num;
    }
}

/*********************************************/
/*      other module hwi splitter            */
/*********************************************/

static void SplitAwbCalcBlockSize(struct isp2x_window* left_win, struct isp2x_window* right_win,
                                  u8 ds_awb, u8 wnd_num, Splitter_Rectangle_t right_isp_rect_,
                                  u16* block_h) {
    bool loop_en = true;

    while (loop_en && *block_h > 0) {
        left_win->h_size = (*block_h * wnd_num) << ds_awb;

        right_win->h_offs = (left_win->h_size + left_win->h_offs > right_isp_rect_.x)
                                ? left_win->h_size + left_win->h_offs - right_isp_rect_.x
                                : 0;

        if ((u32)(right_win->h_offs + left_win->h_size) > right_isp_rect_.w) {
            (*block_h)--;
        } else {
            loop_en = false;

            // if (*block_h % 2)
            //    (*block_h)--;

            left_win->h_size  = (*block_h * wnd_num) << ds_awb;
            right_win->h_offs = (left_win->h_size + left_win->h_offs > right_isp_rect_.x)
                                    ? left_win->h_size + left_win->h_offs - right_isp_rect_.x
                                    : 0;
            right_win->h_offs = right_win->h_offs & 0xfffe;
            right_win->h_size = (*block_h * wnd_num) << ds_awb;
        }
    }
}

void AiqIspParamsSplitter_SplitAwbWin(struct isp2x_window* ori_win, struct isp2x_window* left_win,
                                      struct isp2x_window* right_win, u8 ds_awb, u8 wnd_num,
                                      Splitter_Rectangle_t left_isp_rect_,
                                      Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    u16 win_ds_hsize       = ori_win->h_size >> ds_awb;
    u16 ori_win_hsize_clip = win_ds_hsize << ds_awb;

    // win only locate in left isp, actually stats of right isp would not be used
    if (ori_win->h_offs + ori_win_hsize_clip <= left_isp_rect_.w) {
        LOG1_AWB("win locate in left isp\n");

        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win_hsize_clip;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = ori_win_hsize_clip;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;
    } else if (ori_win->h_offs >= right_isp_rect_.x) {
        LOG1_AWB("win locate in right isp\n");

        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = 0;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs - right_isp_rect_.x;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;

    } else {
        if ((ori_win->h_offs + ori_win->h_size / 2) <= left_isp_rect_.w &&
            right_isp_rect_.x <= (ori_win->h_offs + ori_win->h_size / 2)) {
            LOG1_AWB(" win locate at left&right isp,and center line locate in overlapping zone!\n");

            *mode = LEFT_AND_RIGHT_MODE;

            // win center locate at overlap zone
            left_win->h_offs = ori_win->h_offs;
            left_win->v_offs = ori_win->v_offs;
            left_win->v_size = ori_win->v_size;

            right_win->v_offs = ori_win->v_offs;
            right_win->v_size = ori_win->v_size;
            // u16 block_h = ori_win->h_size / (2 * wnd_num);

            u16 block_h = win_ds_hsize / (2 * wnd_num);

            left_win->h_size = (block_h * wnd_num) << ds_awb;

            right_win->h_offs = (left_win->h_size + left_win->h_offs > right_isp_rect_.x)
                                    ? left_win->h_size + left_win->h_offs - right_isp_rect_.x
                                    : 0;
            right_win->h_offs = right_win->h_offs & 0xfffe;
            right_win->h_size = (win_ds_hsize - block_h * wnd_num) << ds_awb;
            right_win->h_size = right_win->h_offs + right_win->h_size > right_isp_rect_.w
                                    ? (right_isp_rect_.w - right_win->h_offs)
                                    : right_win->h_size;
        } else {
            LOG1_AWB(
                " win locate at left&right isp,but center line not locate in overlapping zone!\n");

            if ((ori_win->h_offs + ori_win->h_size / 2) < right_isp_rect_.x) {
                left_win->h_offs = ori_win->h_offs;
                left_win->v_offs = ori_win->v_offs;
                left_win->v_size = ori_win->v_size;

                right_win->v_offs = ori_win->v_offs;
                right_win->v_size = ori_win->v_size;

                u16 h_size_tmp1 = left_isp_rect_.w - ori_win->h_offs;
                u16 h_size_tmp2 = (right_isp_rect_.x - ori_win->h_offs) * 2;

                if (abs(ori_win_hsize_clip - h_size_tmp1) < abs(ori_win_hsize_clip - h_size_tmp2)) {
                    LOG1_AWB("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp1);

                    *mode = LEFT_MODE;

                    ori_win->h_size = h_size_tmp1;

                    left_win->h_size = ori_win->h_size;
                    // actually stats of right isp would not be used
                    right_win->h_offs = 0;
                    right_win->h_size = ori_win->h_size;
                } else {
                    LOG1_AWB("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp2);
                    *mode = LEFT_AND_RIGHT_MODE;

                    ori_win->h_size = h_size_tmp2;
                    win_ds_hsize    = ori_win->h_size >> ds_awb;

                    u16 block_h = win_ds_hsize / (2 * wnd_num);

                    SplitAwbCalcBlockSize(left_win, right_win, ds_awb, wnd_num, right_isp_rect_,
                                          &block_h);
                }
            } else {
                left_win->v_offs = ori_win->v_offs;
                left_win->v_size = ori_win->v_size;

                right_win->v_offs = ori_win->v_offs;
                right_win->v_size = ori_win->v_size;

                u16 h_size_tmp1 = ori_win->h_offs + ori_win->h_size - right_isp_rect_.x;
                u16 h_size_tmp2 = (ori_win->h_offs + ori_win->h_size - left_isp_rect_.w) * 2;

                if (abs(ori_win_hsize_clip - h_size_tmp1) < abs(ori_win_hsize_clip - h_size_tmp2)) {
                    LOG1_AWB("correct glb.h_off %d to %d\n", ori_win->h_offs, right_isp_rect_.x);
                    LOG1_AWB("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp1);

                    *mode = RIGHT_MODE;

                    ori_win->h_size = h_size_tmp1;
                    ori_win->h_offs = right_isp_rect_.x;

                    right_win->h_offs = 0;
                    right_win->h_size = ori_win->h_size;

                    // actually stats of left isp would not be used
                    left_win->h_offs = 0;
                    left_win->h_size = ori_win->h_size;
                } else {
                    LOG1_AWB("correct glb.h_off %d to %d\n", ori_win->h_offs,
                             ori_win->h_offs + ori_win->h_size -
                                 (ori_win->h_offs + ori_win->h_size - left_isp_rect_.w) * 2);
                    LOG1_AWB("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp2);

                    *mode = LEFT_AND_RIGHT_MODE;

                    ori_win->h_offs = ori_win->h_offs + ori_win->h_size -
                                      (ori_win->h_offs + ori_win->h_size - left_isp_rect_.w) * 2;
                    ori_win->h_size  = h_size_tmp2;
                    left_win->h_offs = ori_win->h_offs;

                    win_ds_hsize = ori_win->h_size >> ds_awb;

                    u16 block_h = win_ds_hsize / (2 * wnd_num);

                    SplitAwbCalcBlockSize(left_win, right_win, ds_awb, wnd_num, right_isp_rect_,
                                          &block_h);
                }
            }
        }
    }
}

void AiqIspParamsSplitter_SplitAwbMultiWin(
    struct isp2x_window* ori_win, struct isp2x_window* left_win, struct isp2x_window* right_win,
    struct isp2x_window* main_left_win, struct isp2x_window* main_right_win,
    Splitter_Rectangle_t left_isp_rect_, Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    if (ori_win->h_offs + ori_win->h_size <= main_left_win->h_offs + main_left_win->h_size) {
        LOG1_AWB("win locate in left isp\n");

        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = 0;
        right_win->v_offs = 0;
        right_win->v_size = 0;
    } else if (ori_win->h_offs >= right_isp_rect_.x + main_right_win->h_offs) {
        LOG1_AWB("win locate in right isp\n");

        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = 0;
        left_win->h_size = 0;
        left_win->v_offs = 0;
        left_win->v_size = 0;

        right_win->h_offs =
            MAX((int)main_right_win->h_offs, (int)ori_win->h_offs - (int)right_isp_rect_.x);
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;

    } else {
        LOG1_AWB(" win locate at left&right isp\n");

        *mode = LEFT_AND_RIGHT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = MAX(
            0, ((int)main_left_win->h_offs + (int)main_left_win->h_size - (int)left_win->h_offs));
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs =
            MAX((int)main_right_win->h_offs,
                (int)left_win->h_offs + (int)left_win->h_size - (int)right_isp_rect_.x);
        right_win->h_size = MAX(0, ((int)ori_win->h_size - (int)left_win->h_size));
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;
    }
}

void AiqIspParamsSplitter_SplitAecWeightVertical(u8* ori_weight, u8* left_weight, u8* right_weight,
                                                 WinSplitMode mode, u8 wnd_num) {
    switch (mode) {
        case LEFT_AND_RIGHT_MODE:
            for (int i = 0; i < wnd_num; i++) {
                for (int j = 0; j < wnd_num; j++) {
                    left_weight[i + j * wnd_num] = ori_weight[i + j / 2 * wnd_num];
                    right_weight[i + j * wnd_num] =
                        ori_weight[i + wnd_num * (j / 2 + j % 2 + wnd_num / 2)];
                }
            }
            break;
        case LEFT_MODE:
        case RIGHT_MODE:
            memcpy(left_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            memcpy(right_weight, ori_weight, wnd_num * wnd_num * sizeof(u8));
            break;
        default:
            break;
    }
}

void AiqIspParamsSplitter_SplitAwbWinThree(
    struct isp2x_window* ori_win, struct isp2x_window* left_win,
    struct isp2x_window* middle_win, struct isp2x_window* right_win,
    u8 ds_awb, u8 wnd_num,
    Splitter_Rectangle_t left_isp_rect_,
    Splitter_Rectangle_t middle_isp_rect_,
    Splitter_Rectangle_t right_isp_rect_,
    WinSplitMode* mode) {
    u16 win_ds_hsize       = ori_win->h_size >> ds_awb;
    u16 ori_win_hsize_clip = win_ds_hsize << ds_awb;

    // win only locate in left isp, actually stats of right isp would not be used
    if (ori_win->h_offs + ori_win_hsize_clip <= left_isp_rect_.w) {
#ifdef DEBUG
        printf("win locate in left isp\n");
#endif
        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win_hsize_clip;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = 0;
        middle_win->h_size = ori_win_hsize_clip;
        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = ori_win_hsize_clip;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;
    } else if (ori_win->h_offs >= right_isp_rect_.x) {
#ifdef DEBUG
        printf("win locate in right isp\n");
#endif
        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = 0;
        left_win->h_size = ori_win_hsize_clip;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = 0;
        middle_win->h_size = ori_win_hsize_clip;
        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs - right_isp_rect_.x;
        right_win->h_size = ori_win_hsize_clip;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;

    } else if (ori_win->h_offs + ori_win->h_size <= middle_isp_rect_.w + middle_isp_rect_.x &&
               ori_win->h_offs >= middle_isp_rect_.x) {

        *mode = MIDDLE_MODE;

        left_win->h_offs = 0;
        left_win->h_size = ori_win_hsize_clip;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = ori_win->h_offs - middle_isp_rect_.x;
        middle_win->h_size = ori_win_hsize_clip;
        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = ori_win_hsize_clip;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;

    } else {
        LOG1_AWB(" win locate at left&right isp,and center line locate in overlapping zone!\n");

        *mode = LEFT_AND_RIGHT_MODE;

        // win center locate at overlap zone
        left_win->h_offs = ori_win->h_offs;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;
        // u16 block_h = ori_win->h_size / (2 * wnd_num);

        u16 block_h = win_ds_hsize / (3 * wnd_num);

        left_win->h_size = (block_h * wnd_num) << ds_awb;

        middle_win->h_offs = (left_win->h_size + left_win->h_offs > middle_isp_rect_.x)
                                ? left_win->h_size + left_win->h_offs - middle_isp_rect_.x
                                : 0;
        middle_win->h_offs = middle_win->h_offs & 0xfffe;
        middle_win->h_size = (block_h * wnd_num) << ds_awb;

        right_win->h_offs = (middle_win->h_size + middle_win->h_offs + middle_isp_rect_.x > right_isp_rect_.x)
                                ? middle_win->h_size + middle_win->h_offs + middle_isp_rect_.x - right_isp_rect_.x
                                : 0;
        right_win->h_offs = right_win->h_offs & 0xfffe;
        right_win->h_size = (win_ds_hsize - 2 * block_h * wnd_num) << ds_awb;
        right_win->h_size = right_win->h_offs + right_win->h_size > right_isp_rect_.w
                                ? (right_isp_rect_.w - right_win->h_offs)
                                : right_win->h_size;
    }
}

void AiqIspParamsSplitter_SplitAwbMultiWinThree(
    struct isp2x_window* ori_win, struct isp2x_window* left_win, struct isp2x_window* middle_win, struct isp2x_window* right_win,
    struct isp2x_window* main_left_win, struct isp2x_window* main_middle_win, struct isp2x_window* main_right_win,
    Splitter_Rectangle_t left_isp_rect_, Splitter_Rectangle_t middle_isp_rect_, Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    if (ori_win->h_offs + ori_win->h_size <= main_left_win->h_offs + main_left_win->h_size) {
        LOG1_AWB("win locate in left isp\n");

        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs = 0;
        middle_win->h_size = 0;
        middle_win->v_offs = 0;
        middle_win->v_size = 0;

        right_win->h_offs = 0;
        right_win->h_size = 0;
        right_win->v_offs = 0;
        right_win->v_size = 0;
    } else if (ori_win->h_offs >= right_isp_rect_.x + main_right_win->h_offs) {
        LOG1_AWB("win locate in right isp\n");

        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = 0;
        left_win->h_size = 0;
        left_win->v_offs = 0;
        left_win->v_size = 0;

        middle_win->h_offs = 0;
        middle_win->h_size = 0;
        middle_win->v_offs = 0;
        middle_win->v_size = 0;

        right_win->h_offs =
            MAX((int)main_right_win->h_offs, (int)ori_win->h_offs - (int)right_isp_rect_.x);
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;

    } else if (ori_win->h_offs + ori_win->h_size <= middle_isp_rect_.w + middle_isp_rect_.x &&
               ori_win->h_offs >= middle_isp_rect_.x) {

        *mode = MIDDLE_MODE;

        left_win->h_offs = 0;
        left_win->h_size = 0;
        left_win->v_offs = 0;
        left_win->v_size = 0;

        middle_win->h_offs =
            MAX((int)main_middle_win->h_offs, (int)ori_win->h_offs - (int)middle_isp_rect_.x);;
        middle_win->h_size = ori_win->h_size;
        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = 0;
        right_win->v_offs = 0;
        right_win->v_size = 0;
    } else {
        LOG1_AWB(" win locate at left&right isp\n");

        *mode = LEFT_AND_RIGHT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = MAX(
            0, ((int)main_left_win->h_offs + (int)main_left_win->h_size - (int)left_win->h_offs));
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        middle_win->h_offs =
            MAX((int)main_middle_win->h_offs,
                (int)left_win->h_offs + (int)left_win->h_size - (int)middle_isp_rect_.x);
        middle_win->h_size =  MAX(0, ((int)middle_isp_rect_.w + (int)middle_isp_rect_.x - (int)middle_win->h_offs));
        middle_win->v_offs = ori_win->v_offs;
        middle_win->v_size = ori_win->v_size;

        right_win->h_offs =
            MAX((int)main_right_win->h_offs,
                (int)middle_win->h_offs + (int)middle_win->h_size + (int)middle_isp_rect_.x - (int)right_isp_rect_.x);
        right_win->h_size = MAX(0, ((int)ori_win->h_size - (int)left_win->h_size - (int)middle_win->h_size));
        right_win->v_offs = ori_win->v_offs;
        right_win->v_size = ori_win->v_size;
    }
}

void AiqIspParamsSplitter_SplitAwbWinThreeV(
    struct isp2x_window* ori_win, struct isp2x_window* left_win,
    struct isp2x_window* middle_win, struct isp2x_window* right_win,
    u8 ds_awb, u8 wnd_num,
    Splitter_Rectangle_t left_isp_rect_,
    Splitter_Rectangle_t middle_isp_rect_,
    Splitter_Rectangle_t right_isp_rect_,
    WinSplitMode* mode) {
    u16 win_ds_vsize       = ori_win->v_size >> ds_awb;
    u16 ori_win_vsize_clip = win_ds_vsize << ds_awb;

    // win only locate in left isp, actually stats of right isp would not be used
    if (ori_win->v_offs + ori_win_vsize_clip <= left_isp_rect_.h) {
#ifdef DEBUG
        printf("win locate in left isp\n");
#endif
        *mode = LEFT_MODE;

        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win_vsize_clip;
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;

        middle_win->v_offs = 0;
        middle_win->v_size = ori_win_vsize_clip;
        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;

        right_win->v_offs = 0;
        right_win->v_size = ori_win_vsize_clip;
        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
    } else if (ori_win->v_offs >= right_isp_rect_.y) {
#ifdef DEBUG
        printf("win locate in right isp\n");
#endif
        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->v_offs = 0;
        left_win->v_size = ori_win_vsize_clip;
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;

        middle_win->v_offs = 0;
        middle_win->v_size = ori_win_vsize_clip;
        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;

        right_win->v_offs = ori_win->v_offs - right_isp_rect_.y;
        right_win->v_size = ori_win_vsize_clip;
        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;

    } else if (ori_win->v_offs + ori_win->v_size <= middle_isp_rect_.h + middle_isp_rect_.y &&
               ori_win->v_offs >= middle_isp_rect_.y) {

        *mode = MIDDLE_MODE;

        left_win->v_offs = 0;
        left_win->v_size = ori_win_vsize_clip;
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;

        middle_win->v_offs = ori_win->v_offs - middle_isp_rect_.y;
        middle_win->v_size = ori_win_vsize_clip;
        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;

        right_win->v_offs = 0;
        right_win->v_size = ori_win_vsize_clip;
        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;

    } else {
        LOG1_AWB(" win locate at left&right isp,and center line locate in overlapping zone!\n");

        *mode = LEFT_AND_RIGHT_MODE;

        // win center locate at overlap zone
        left_win->v_offs = ori_win->v_offs;
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;

        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        // u16 block_h = ori_win->v_size / (2 * wnd_num);

        u16 block_h = win_ds_vsize / (3 * wnd_num);

        left_win->v_size = (block_h * wnd_num) << ds_awb;

        middle_win->v_offs = (left_win->v_size + left_win->v_offs > middle_isp_rect_.y)
                                ? left_win->v_size + left_win->v_offs - middle_isp_rect_.y
                                : 0;
        middle_win->v_offs = middle_win->v_offs & 0xfffe;
        middle_win->v_size = (block_h * wnd_num) << ds_awb;

        right_win->v_offs = (middle_win->v_size + middle_win->v_offs + middle_isp_rect_.y > right_isp_rect_.y)
                                ? middle_win->v_size + middle_win->v_offs + middle_isp_rect_.y - right_isp_rect_.y
                                : 0;
        right_win->v_offs = right_win->v_offs & 0xfffe;
        right_win->v_size = (win_ds_vsize - 2 * block_h * wnd_num) << ds_awb;
        right_win->v_size = right_win->v_offs + right_win->v_size > right_isp_rect_.h
                                ? (right_isp_rect_.h - right_win->v_offs)
                                : right_win->v_size;
    }
}

void AiqIspParamsSplitter_SplitAwbMultiWinThreeV(
    struct isp2x_window* ori_win, struct isp2x_window* left_win, struct isp2x_window* middle_win, struct isp2x_window* right_win,
    struct isp2x_window* main_left_win, struct isp2x_window* main_middle_win, struct isp2x_window* main_right_win,
    Splitter_Rectangle_t left_isp_rect_, Splitter_Rectangle_t middle_isp_rect_, Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    if (ori_win->v_offs + ori_win->v_size <= main_left_win->v_offs + main_left_win->v_size) {
        LOG1_AWB("win locate in left isp\n");

        *mode = LEFT_MODE;

        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;

        middle_win->v_offs = 0;
        middle_win->v_size = 0;
        middle_win->h_offs = 0;
        middle_win->h_size = 0;

        right_win->v_offs = 0;
        right_win->v_size = 0;
        right_win->h_offs = 0;
        right_win->h_size = 0;
    } else if (ori_win->v_offs >= right_isp_rect_.y + main_right_win->v_offs) {
        LOG1_AWB("win locate in right isp\n");

        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->v_offs = 0;
        left_win->v_size = 0;
        left_win->h_offs = 0;
        left_win->h_size = 0;

        middle_win->v_offs = 0;
        middle_win->v_size = 0;
        middle_win->h_offs = 0;
        middle_win->h_size = 0;

        right_win->v_offs =
            MAX((int)main_right_win->v_offs, (int)ori_win->v_offs - (int)right_isp_rect_.y);
        right_win->v_size = ori_win->v_size;
        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;

    } else if (ori_win->v_offs + ori_win->v_size <= middle_isp_rect_.h + middle_isp_rect_.y &&
               ori_win->v_offs >= middle_isp_rect_.y) {

        *mode = MIDDLE_MODE;

        left_win->v_offs = 0;
        left_win->v_size = 0;
        left_win->h_offs = 0;
        left_win->h_size = 0;

        middle_win->v_offs =
            MAX((int)main_middle_win->v_offs, (int)ori_win->v_offs - (int)middle_isp_rect_.y);;
        middle_win->v_size = ori_win->v_size;
        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;

        right_win->v_offs = 0;
        right_win->v_size = 0;
        right_win->h_offs = 0;
        right_win->h_size = 0;
    } else {
        LOG1_AWB(" win locate at left&right isp\n");

        *mode = LEFT_AND_RIGHT_MODE;

        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = MAX(
            0, ((int)main_left_win->v_offs + (int)main_left_win->v_size - (int)left_win->v_offs));
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;

        middle_win->v_offs =
            MAX((int)main_middle_win->v_offs,
                (int)left_win->v_offs + (int)left_win->v_size - (int)middle_isp_rect_.y);
        middle_win->v_size =  MAX(0, ((int)middle_isp_rect_.h + (int)middle_isp_rect_.y - (int)middle_win->v_offs));
        middle_win->h_offs = ori_win->h_offs;
        middle_win->h_size = ori_win->h_size;

        right_win->v_offs =
            MAX((int)main_right_win->v_offs,
                (int)middle_win->v_offs + (int)middle_win->v_size + (int)middle_isp_rect_.y - (int)right_isp_rect_.y);
        right_win->v_size = MAX(0, ((int)ori_win->v_size - (int)left_win->v_size - (int)middle_win->v_size));
        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
    }
}

static void SplitAecCalcBlockSizeVertical(struct isp2x_window* left_win,
                                          struct isp2x_window* right_win, u8 wnd_num,
                                          Splitter_Rectangle_t right_isp_rect_, u16* block_v) {
    bool loop_en = true;

    while (loop_en && *block_v > 0) {
        left_win->v_size  = *block_v * wnd_num;
        right_win->v_offs = (left_win->v_size + left_win->v_offs > right_isp_rect_.y)
                                ? left_win->v_size + left_win->v_offs - right_isp_rect_.y
                                : 0;

        if ((u32)(right_win->v_offs + *block_v * wnd_num) > right_isp_rect_.h - 1) {
            (*block_v)--;

        } else {
            loop_en = false;

            // if (*block_h % 2)
            //(*block_h)--;

            left_win->v_size  = *block_v * wnd_num;
            right_win->v_offs = (left_win->v_size + left_win->v_offs > right_isp_rect_.y)
                                    ? left_win->v_size + left_win->v_offs - right_isp_rect_.y
                                    : 0;
            right_win->v_offs = right_win->v_offs & 0xfffe;
            right_win->v_size = *block_v * wnd_num;
        }
    }
}

static void SplitAecSubWinVertical(u8* subwin_en, struct isp2x_window* ori_win,
                                   struct isp2x_window* left_win, struct isp2x_window* right_win,
                                   Splitter_Rectangle_t left_isp_rect_,
                                   Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    for (int i = 0; i < ISP2X_RAWAEBIG_SUBWIN_NUM; i++) {
        if (subwin_en[i] == 1) {  // only when subwin is enabled, should split hwi params
            if (ori_win[i].v_offs + ori_win[i].v_size <= left_isp_rect_.h) {
#ifdef DEBUG
                printf("win locate in left isp\n");
#endif
                mode[i] = LEFT_MODE;

                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = ori_win[i].h_offs;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = 0;
                right_win[i].v_size = ori_win[i].v_size;
            } else if (ori_win[i].v_offs >= right_isp_rect_.y) {
#ifdef DEBUG
                printf("win locate in right isp\n");
#endif
                mode[i] = RIGHT_MODE;

                // win only locate in right isp, actually stats of left isp would not be used
                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = 0;
                left_win[i].v_size = ori_win[i].v_size;

                right_win[i].h_offs = ori_win[i].h_offs;
                right_win[i].h_size = ori_win[i].h_size;
                right_win[i].v_offs = ori_win[i].v_offs - right_isp_rect_.y;
                right_win[i].v_size = ori_win[i].v_size;

            } else {
#ifdef DEBUG
                printf(" win locate at left&right isp\n");
#endif
                mode[i] = LEFT_AND_RIGHT_MODE;

                left_win[i].h_offs = ori_win[i].h_offs;
                left_win[i].h_size = ori_win[i].h_size;
                left_win[i].v_offs = ori_win[i].v_offs;
                left_win[i].v_size = MAX(0, ((long)left_isp_rect_.h - (long)left_win[i].v_offs));

                right_win[i].h_offs = right_win[i].h_offs;
                right_win[i].h_size = right_win[i].h_size;
                right_win[i].v_offs = left_win[i].v_offs + left_win[i].v_size - right_isp_rect_.y;
                right_win[i].v_size = MAX(0, ((long)ori_win[i].v_size - (long)left_win[i].v_size));
            }
        }
    }
}

void SplitAecWinVertical(struct isp2x_window* ori_win, struct isp2x_window* left_win,
                         struct isp2x_window* right_win, u8 wnd_num,
                         Splitter_Rectangle_t left_isp_rect_,
                         Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    // win only locate in left isp, actually stats of right isp would not be used
    if (ori_win->v_offs + ori_win->v_size <= left_isp_rect_.h) {
#ifdef DEBUG
        printf("win locate in left isp\n");
#endif
        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = 0;
        right_win->v_size = ori_win->v_size;
    } else if (ori_win->v_offs >= right_isp_rect_.y) {
#ifdef DEBUG
        printf("win locate in right isp\n");
#endif
        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = 0;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs - right_isp_rect_.y;
        right_win->v_size = ori_win->v_size;

    } else {
        if ((ori_win->v_offs + ori_win->v_size / 2) <= left_isp_rect_.h &&
            right_isp_rect_.y <= (ori_win->v_offs + ori_win->v_size / 2)) {
#ifdef DEBUG
            printf(" win locate at left&right isp,and center line locate in overlapping zone!\n");
#endif
            *mode = LEFT_AND_RIGHT_MODE;

            // win center locate at overlap zone
            left_win->v_offs = ori_win->v_offs;
            left_win->h_offs = ori_win->h_offs;
            left_win->h_size = ori_win->h_size;

            right_win->h_offs = ori_win->h_offs;
            right_win->h_size = ori_win->h_size;

            u16 block_v = ori_win->v_size / (2 * wnd_num);
            SplitAecCalcBlockSizeVertical(left_win, right_win, wnd_num, right_isp_rect_, &block_v);
        } else {
#ifdef DEBUG
            printf(
                " win locate at left&right isp,but center line not locate in overlapping zone!\n");
#endif
            if ((ori_win->v_offs + ori_win->v_size / 2) < right_isp_rect_.y) {
                left_win->v_offs = ori_win->v_offs;
                left_win->h_offs = ori_win->h_offs;
                left_win->h_size = ori_win->h_size;

                right_win->h_offs = ori_win->h_offs;
                right_win->h_size = ori_win->h_size;

                u16 v_size_tmp1 = left_isp_rect_.h - ori_win->v_offs;
                u16 v_size_tmp2 = (right_isp_rect_.y - ori_win->v_offs) * 2;

                if (abs(ori_win->v_size - v_size_tmp1) < abs(ori_win->v_size - v_size_tmp2)) {
#ifdef DEBUG
                    printf("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp1);
#endif
                    *mode = LEFT_MODE;

                    ori_win->v_size = v_size_tmp1;

                    left_win->v_size = ori_win->v_size;
                    // actually stats of right isp would not be used
                    right_win->v_offs = 0;
                    right_win->v_size = ori_win->v_size;
                } else {
#ifdef DEBUG
                    printf("correct glb.h_size %d to %d\n", ori_win->h_size, h_size_tmp2);
#endif
                    *mode = LEFT_AND_RIGHT_MODE;

                    ori_win->v_size = v_size_tmp2;

                    u16 block_v = ori_win->v_size / (2 * wnd_num);
                    SplitAecCalcBlockSizeVertical(left_win, right_win, wnd_num, right_isp_rect_,
                                                  &block_v);
                }
            } else {
                left_win->h_offs = ori_win->h_offs;
                left_win->h_size = ori_win->h_size;

                right_win->h_offs = ori_win->h_offs;
                right_win->h_size = ori_win->h_size;

                u16 v_size_tmp1 = ori_win->v_offs + ori_win->v_size - right_isp_rect_.y;
                u16 v_size_tmp2 = (ori_win->v_offs + ori_win->v_size - left_isp_rect_.h) * 2;

                if (abs(ori_win->v_size - v_size_tmp1) < abs(ori_win->v_size - v_size_tmp2)) {
#ifdef DEBUG
                    printf("correct glb.h_off %d to %u\n", ori_win->v_offs, right_isp_rect_.y);
                    printf("correct glb.h_size %d to %d\n", ori_win->v_size, v_size_tmp1);
#endif

                    *mode = RIGHT_MODE;

                    ori_win->v_size = v_size_tmp1;
                    ori_win->v_offs = right_isp_rect_.y;

                    right_win->v_offs = 0;
                    right_win->v_size = ori_win->v_size;

                    // actually stats of left isp would not be used
                    left_win->v_offs = 0;
                    left_win->v_size = ori_win->v_size;
                } else {
#ifdef DEBUG
                    printf("correct glb.h_off %d to %d\n", ori_win->v_offs,
                           ori_win->v_offs + ori_win->v_size -
                               (ori_win->v_offs + ori_win->v_size - left_isp_rect_.h) * 2);
                    printf("correct glb.h_size %d to %d\n", ori_win->v_size, v_size_tmp2);
#endif
                    *mode = LEFT_AND_RIGHT_MODE;

                    ori_win->v_offs = ori_win->v_offs + ori_win->v_size -
                                      (ori_win->v_offs + ori_win->v_size - left_isp_rect_.h) * 2;
                    ori_win->v_size  = v_size_tmp2;
                    left_win->v_offs = ori_win->v_offs;

                    u16 block_v = ori_win->v_size / (2 * wnd_num);
                    SplitAecCalcBlockSizeVertical(left_win, right_win, wnd_num, right_isp_rect_,
                                                  &block_v);
                }
            }
        }
    }
}

static void SplitAwbCalcBlockSizeVertical(struct isp2x_window* left_win,
                                          struct isp2x_window* right_win, u8 ds_awb, u8 wnd_num,
                                          Splitter_Rectangle_t right_isp_rect_, u16* block_v) {
    bool loop_en = true;

    while (loop_en && *block_v > 0) {
        left_win->v_size = (*block_v * wnd_num) << ds_awb;

        right_win->v_offs = (left_win->v_size + left_win->v_offs > right_isp_rect_.y)
                                ? left_win->v_size + left_win->v_offs - right_isp_rect_.y
                                : 0;

        if ((u32)(right_win->v_offs + left_win->v_size) > right_isp_rect_.h) {
            (*block_v)--;
        } else {
            loop_en = false;

            // if (*block_v % 2)
            //    (*block_v)--;

            left_win->v_size  = (*block_v * wnd_num) << ds_awb;
            right_win->v_offs = (left_win->v_size + left_win->v_offs > right_isp_rect_.y)
                                    ? left_win->v_size + left_win->v_offs - right_isp_rect_.y
                                    : 0;
            right_win->v_offs = right_win->v_offs & 0xfffe;
            right_win->v_size = (*block_v * wnd_num) << ds_awb;
        }
    }
}

void AiqIspParamsSplitter_SplitAwbWinVertical(struct isp2x_window* ori_win,
                                              struct isp2x_window* left_win,
                                              struct isp2x_window* right_win, u8 ds_awb, u8 wnd_num,
                                              Splitter_Rectangle_t left_isp_rect_,
                                              Splitter_Rectangle_t right_isp_rect_,
                                              WinSplitMode* mode) {
    u16 win_ds_vsize       = ori_win->v_size >> ds_awb;
    u16 ori_win_vsize_clip = win_ds_vsize << ds_awb;

    // win only locate in left isp, actually stats of right isp would not be used
    if (ori_win->v_offs + ori_win_vsize_clip <= left_isp_rect_.h) {
        LOG1_AWB("win locate in left isp\n");

        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win_vsize_clip;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = 0;
        right_win->v_size = ori_win_vsize_clip;
    } else if (ori_win->v_offs >= right_isp_rect_.y) {
        LOG1_AWB("win locate in right isp\n");

        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = 0;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs = ori_win->v_offs - right_isp_rect_.y;
        right_win->v_size = ori_win->v_size;

    } else {
        if ((ori_win->v_offs + ori_win->v_size / 2) <= left_isp_rect_.h &&
            right_isp_rect_.y <= (ori_win->v_offs + ori_win->v_size / 2)) {
            LOG1_AWB(" win locate at left&right isp,and center line locate in overlapping zone!\n");

            *mode = LEFT_AND_RIGHT_MODE;

            // win center locate at overlap zone
            left_win->v_offs = ori_win->v_offs;
            left_win->h_offs = ori_win->h_offs;
            left_win->h_size = ori_win->h_size;

            right_win->h_offs = ori_win->h_offs;
            right_win->h_size = ori_win->h_size;
            // u16 block_v = ori_win->v_size / (2 * wnd_num);

            u16 block_v = win_ds_vsize / (2 * wnd_num);

            left_win->v_size = (block_v * wnd_num) << ds_awb;

            right_win->v_offs = (left_win->v_size + left_win->v_offs > right_isp_rect_.y)
                                    ? left_win->v_size + left_win->v_offs - right_isp_rect_.y
                                    : 0;
            right_win->v_offs = right_win->v_offs & 0xfffe;
            right_win->v_size = (win_ds_vsize - block_v * wnd_num) << ds_awb;
            right_win->v_size = right_win->v_offs + right_win->v_size > right_isp_rect_.h
                                    ? (right_isp_rect_.h - right_win->v_offs)
                                    : right_win->v_size;
        } else {
            LOG1_AWB(
                " win locate at left&right isp,but center line not locate in overlapping zone!\n");

            if ((ori_win->v_offs + ori_win->v_size / 2) < right_isp_rect_.y) {
                left_win->v_offs = ori_win->v_offs;
                left_win->h_offs = ori_win->h_offs;
                left_win->h_size = ori_win->h_size;

                right_win->h_offs = ori_win->h_offs;
                right_win->h_size = ori_win->h_size;

                u16 v_size_tmp1 = left_isp_rect_.h - ori_win->v_offs;
                u16 v_size_tmp2 = (right_isp_rect_.y - ori_win->v_offs) * 2;

                if (abs(ori_win_vsize_clip - v_size_tmp1) < abs(ori_win_vsize_clip - v_size_tmp2)) {
                    LOG1_AWB("correct glb.v_size %d to %d\n", ori_win->v_size, v_size_tmp1);

                    *mode = LEFT_MODE;

                    ori_win->v_size = v_size_tmp1;

                    left_win->v_size = ori_win->v_size;
                    // actually stats of right isp would not be used
                    right_win->v_offs = 0;
                    right_win->v_size = ori_win->v_size;
                } else {
                    LOG1_AWB("correct glb.h_size %d to %d\n", ori_win->v_size, v_size_tmp2);
                    *mode = LEFT_AND_RIGHT_MODE;

                    ori_win->v_size = v_size_tmp2;
                    win_ds_vsize    = ori_win->v_size >> ds_awb;

                    u16 block_v = win_ds_vsize / (2 * wnd_num);

                    SplitAwbCalcBlockSizeVertical(left_win, right_win, ds_awb, wnd_num,
                                                  right_isp_rect_, &block_v);
                }
            } else {
                left_win->h_offs = ori_win->h_offs;
                left_win->h_size = ori_win->h_size;

                right_win->h_offs = ori_win->h_offs;
                right_win->h_size = ori_win->h_size;

                u16 v_size_tmp1 = ori_win->v_offs + ori_win->v_size - right_isp_rect_.y;
                u16 v_size_tmp2 = (ori_win->v_offs + ori_win->v_size - left_isp_rect_.h) * 2;

                if (abs(ori_win_vsize_clip - v_size_tmp1) < abs(ori_win_vsize_clip - v_size_tmp2)) {
                    LOG1_AWB("correct glb.v_off %d to %d\n", ori_win->v_offs, right_isp_rect_.y);
                    LOG1_AWB("correct glb.v_size %d to %d\n", ori_win->v_size, v_size_tmp1);

                    *mode = RIGHT_MODE;

                    ori_win->v_size = v_size_tmp1;
                    ori_win->v_offs = right_isp_rect_.y;

                    right_win->v_offs = 0;
                    right_win->v_size = ori_win->v_size;

                    // actually stats of left isp would not be used
                    left_win->v_offs = 0;
                    left_win->v_size = ori_win->v_size;
                } else {
                    LOG1_AWB("correct glb.v_off %d to %d\n", ori_win->v_offs,
                             ori_win->v_offs + ori_win->v_size -
                                 (ori_win->v_offs + ori_win->v_size - left_isp_rect_.h) * 2);
                    LOG1_AWB("correct glb.v_size %d to %d\n", ori_win->v_size, v_size_tmp2);

                    *mode = LEFT_AND_RIGHT_MODE;

                    ori_win->v_offs = ori_win->v_offs + ori_win->v_size -
                                      (ori_win->v_offs + ori_win->v_size - left_isp_rect_.h) * 2;
                    ori_win->v_size  = v_size_tmp2;
                    left_win->v_offs = ori_win->v_offs;

                    win_ds_vsize = ori_win->v_size >> ds_awb;

                    u16 block_v = win_ds_vsize / (2 * wnd_num);

                    SplitAwbCalcBlockSizeVertical(left_win, right_win, ds_awb, wnd_num,
                                                  right_isp_rect_, &block_v);
                }
            }
        }
    }
}

void AiqIspParamsSplitter_AiqIspParamsSplitter_SplitAwbMultiWinVertical(
    struct isp2x_window* ori_win, struct isp2x_window* left_win, struct isp2x_window* right_win,
    struct isp2x_window* main_left_win, struct isp2x_window* main_right_win,
    Splitter_Rectangle_t left_isp_rect_, Splitter_Rectangle_t right_isp_rect_, WinSplitMode* mode) {
    if (ori_win->v_offs + ori_win->v_size <= main_left_win->v_offs + main_left_win->v_size) {
        LOG1_AWB("win locate in left isp\n");

        *mode = LEFT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = ori_win->v_size;

        right_win->h_offs = 0;
        right_win->h_size = 0;
        right_win->v_offs = 0;
        right_win->v_size = 0;
    } else if (ori_win->v_offs >= right_isp_rect_.y + main_right_win->v_offs) {
        LOG1_AWB("win locate in right isp\n");

        *mode = RIGHT_MODE;

        // win only locate in right isp, actually stats of left isp would not be used
        left_win->h_offs = 0;
        left_win->h_size = 0;
        left_win->v_offs = 0;
        left_win->v_size = 0;

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs =
            MAX((int)main_right_win->v_offs, (int)ori_win->v_offs - (int)right_isp_rect_.y);
        right_win->v_size = ori_win->v_size;

    } else {
        LOG1_AWB(" win locate at left&right isp\n");

        *mode = LEFT_AND_RIGHT_MODE;

        left_win->h_offs = ori_win->h_offs;
        left_win->h_size = ori_win->h_size;
        left_win->v_offs = ori_win->v_offs;
        left_win->v_size = MAX(
            0, ((int)main_left_win->v_offs + (int)main_left_win->v_size - (int)left_win->v_offs));

        right_win->h_offs = ori_win->h_offs;
        right_win->h_size = ori_win->h_size;
        right_win->v_offs =
            MAX((int)main_right_win->v_offs,
                (int)left_win->v_offs + (int)left_win->v_size - (int)right_isp_rect_.y);
        right_win->v_size = MAX(0, ((int)ori_win->v_size - (int)left_win->v_size));
    }
}

XCamReturn AiqIspParamsSplitter_SplitRawAeLiteParams(AiqIspParamsSplitter_t* pSplit,
                                                     struct isp2x_rawaelite_meas_cfg* ori,
                                                     struct isp2x_rawaelite_meas_cfg* left,
                                                     struct isp2x_rawaelite_meas_cfg* right) {
    u8 wnd_num = 0;
    if (ori->wnd_num == 0)
        wnd_num = 1;
    else
        wnd_num = 5;

    WinSplitMode mode        = LEFT_AND_RIGHT_MODE;
    WinSplitMode sub_mode[4] = {LEFT_AND_RIGHT_MODE};

    SplitAecWin(&ori->win, &left->win, &right->win, wnd_num, pSplit->left_isp_rect_,
                pSplit->right_isp_rect_, &mode);

#ifdef DEBUG
    printf("AeLite left=%d-%d-%d-%d, right=%d-%d-%d-%d\n", left->win.h_offs, left->win.v_offs,
           left->win.h_size, left->win.v_size, right->win.h_offs, right->win.v_offs,
           right->win.h_size, right->win.v_size);
#endif

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn AiqIspParamsSplitter_SplitRawAeBigParams(AiqIspParamsSplitter_t* pSplit,
                                                    struct isp2x_rawaebig_meas_cfg* ori,
                                                    struct isp2x_rawaebig_meas_cfg* left,
                                                    struct isp2x_rawaebig_meas_cfg* right) {
    u8 wnd_num = 0;

    if (ori->wnd_num == 0)
        wnd_num = 1;
    else if (ori->wnd_num == 1)
        wnd_num = 5;
    else
        wnd_num = 15;

    WinSplitMode mode        = LEFT_AND_RIGHT_MODE;
    WinSplitMode sub_mode[4] = {LEFT_AND_RIGHT_MODE};

    SplitAecWin(&ori->win, &left->win, &right->win, wnd_num, pSplit->left_isp_rect_,
                pSplit->right_isp_rect_, &mode);
    SplitAecSubWin(ori->subwin_en, ori->subwin, left->subwin, right->subwin, pSplit->left_isp_rect_,
                   pSplit->right_isp_rect_, sub_mode);

    for (int i = 0; i < ISP2X_RAWAEBIG_SUBWIN_NUM; i++) {
        if (ori->subwin_en[i]) {
            switch (sub_mode[i]) {
                case LEFT_AND_RIGHT_MODE:
                    left->subwin_en[i]  = true;
                    right->subwin_en[i] = true;
                    break;
                case LEFT_MODE:
                    left->subwin_en[i]  = true;
                    right->subwin_en[i] = false;
                    break;
                case RIGHT_MODE:
                    left->subwin_en[i]  = false;
                    right->subwin_en[i] = true;
                    break;
                default:
                    break;
            }
        }
    }

#ifdef DEBUG
    printf("AeBig left=%d-%d-%d-%d, right=%d-%d-%d-%d\n", left->win.h_offs, left->win.v_offs,
           left->win.h_size, left->win.v_size, right->win.h_offs, right->win.v_offs,
           right->win.h_size, right->win.v_size);
#endif

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn AiqIspParamsSplitter_SplitRawHistLiteParams(AiqIspParamsSplitter_t* pSplit,
                                                       struct isp2x_rawhistlite_cfg* ori,
                                                       struct isp2x_rawhistlite_cfg* left,
                                                       struct isp2x_rawhistlite_cfg* right) {
    u8 wnd_num = 0;
    wnd_num    = 5;

    WinSplitMode mode = LEFT_AND_RIGHT_MODE;

    SplitAecWin(&ori->win, &left->win, &right->win, wnd_num, pSplit->left_isp_rect_,
                pSplit->right_isp_rect_, &mode);
    AiqIspParamsSplitter_SplitAecWeight(ori->weight, left->weight, right->weight, mode, wnd_num);

#ifdef DEBUG
    printf("HistLite left=%d-%d-%d-%d, right=%d-%d-%d-%d\n", left->win.h_offs, left->win.v_offs,
           left->win.h_size, left->win.v_size, right->win.h_offs, right->win.v_offs,
           right->win.h_size, right->win.v_size);

    for (int i = 0; i < wnd_num; i++) {
        for (int j = 0; j < wnd_num; j++) printf("%d ", left->weight[i * wnd_num + j]);
        printf("\n");
    }
    for (int i = 0; i < wnd_num; i++) {
        for (int j = 0; j < wnd_num; j++) printf("%d ", right->weight[i * wnd_num + j]);
        printf("\n");
    }
#endif

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn AiqIspParamsSplitter_SplitRawHistBigParams(AiqIspParamsSplitter_t* pSplit,
                                                      struct isp2x_rawhistbig_cfg* ori,
                                                      struct isp2x_rawhistbig_cfg* left,
                                                      struct isp2x_rawhistbig_cfg* right) {
    u8 wnd_num = 0;

    if (ori->wnd_num <= 1)
        wnd_num = 5;
    else
        wnd_num = 15;

    WinSplitMode mode = LEFT_AND_RIGHT_MODE;

    SplitAecWin(&ori->win, &left->win, &right->win, wnd_num, pSplit->left_isp_rect_,
                pSplit->right_isp_rect_, &mode);
    AiqIspParamsSplitter_SplitAecWeight(ori->weight, left->weight, right->weight, mode, wnd_num);

#ifdef DEBUG
    printf("HistBig left=%d-%d-%d-%d, right=%d-%d-%d-%d\n", left->win.h_offs, left->win.v_offs,
           left->win.h_size, left->win.v_size, right->win.h_offs, right->win.v_offs,
           right->win.h_size, right->win.v_size);

    for (int i = 0; i < wnd_num; i++) {
        for (int j = 0; j < wnd_num; j++) printf("%d ", left->weight[i * wnd_num + j]);
        printf("\n");
    }
    for (int i = 0; i < wnd_num; i++) {
        for (int j = 0; j < wnd_num; j++) printf("%d ", right->weight[i * wnd_num + j]);
        printf("\n");
    }

#endif

    return XCAM_RETURN_NO_ERROR;
}

int AiqIspParamsSplitter_AlscMatrixScale(float lrate, float rrate,
                                         unsigned short ori_matrix[], unsigned short left_matrix[],
                                         unsigned short right_matrix[], int cols, int rows) {
    int ori_col_index = 0;
    int lef_dst_index = 0;
    int rht_dst_index = 0;
    int mid_col       = cols / 2;
    int row_index     = 0;

    for (row_index = 0; row_index < rows; row_index++) {
        for (ori_col_index = 0; ori_col_index < cols; ori_col_index++) {
            if (ori_col_index < mid_col) {
                left_matrix[lef_dst_index++] = ori_matrix[row_index * cols + ori_col_index];
                left_matrix[lef_dst_index++] = (ori_matrix[row_index * cols + ori_col_index] +
                                                ori_matrix[row_index * cols + ori_col_index + 1]) /
                                               2;
            } else if (ori_col_index == mid_col) {
                left_matrix[lef_dst_index++] = (unsigned short) ((1 + lrate) * ori_matrix[row_index * cols + mid_col] -
                                                lrate * ori_matrix[row_index * cols + mid_col - 1] + 0.5);
                right_matrix[rht_dst_index++] = (unsigned short) ((1 + rrate) * ori_matrix[row_index * cols + mid_col] -
                                                rrate * ori_matrix[row_index * cols + mid_col + 1] + 0.5);
            } else {
                right_matrix[rht_dst_index++] = (ori_matrix[row_index * cols + ori_col_index] +
                                                 ori_matrix[row_index * cols + ori_col_index - 1]) /
                                                2;
                right_matrix[rht_dst_index++] = ori_matrix[row_index * cols + ori_col_index];
            }
        }
    }

    return 0;
}

int AiqIspParamsSplitter_uniformSplitAlscXtable(int w, int off, int in_size, unsigned short* size_tbl, unsigned short* pos_tbl) {
    if (in_size <= 0 || in_size > 16) {
        LOGE("invalid size(%d) for LSC SizeTbl \n", in_size);
        return -1;
    }
    if (pos_tbl == NULL || size_tbl == NULL) {
        LOGE("null pointer for pos_tbl or size_tbl\n");
        return -1;
    }
    int i = 0;
    pos_tbl[0] = off;
    for (i = 1; i < in_size + 1; i++) {
        pos_tbl[i] = (uint16_t)((i * w) / 16 - 1 + off);
    }
    for (i = 0; i < in_size; i++) {
        size_tbl[i] = pos_tbl[i + 1] - pos_tbl[i];
    }
    size_tbl[0] += 1;

    return 0;
}

static int interpolation_f(const unsigned short *x, int Num, unsigned short x0, float* k)
{
    int i;
    if (x0 <= x[0] || Num < 2)
    {
        *k = 0;
        return 0;
    }
    if (x0 >= x[Num - 1])
    {
        *k = 1;
        return Num - 2;
    }

    for (i = 0; i < Num; i++) {
        if (x0 < x[i])
            break;
    }

    if ((float)x[i] - (float)x[i-1] < 0.1)
        *k = 0;
    else
        *k = ((float)x0 - (float)x[i-1]) / ((float)x[i] - (float)x[i-1]);

    return i-1;
}

int AiqIspParamsSplitter_getAlscAlignCoef(const unsigned short* in_array,
                                        unsigned short* new_array,
                                        unsigned short* left_endpt,
                                        float* coef, int len) {
    if (in_array == NULL || new_array == NULL || left_endpt == NULL || coef == NULL || len <= 0) {
        LOGE("null pointer for in_array or new_array or left_endpt or coef or error len=%d\n", len);
        return -1;
    }
    for (int i = 0; i < len; i++) {
        left_endpt[i] = interpolation_f(in_array, len, new_array[i], &coef[i]);
    }

    return 0;
}

int AiqIspParamsSplitter_AlscMatrixInterp(unsigned short ori_matrix[],
                                         unsigned short pre_idx[],
                                         unsigned short dst_matrix[],
                                         float coef[],
                                         int cols, int rows, int axis)
{

    float f = 0;
    int idx0 = 0;
    int idx1 = 0;
    int dst_index = 0;
    if (axis == 1) { // x-direction
        for (int row_index = 0; row_index < rows; row_index++) {
            for (int col_index = 0; col_index < cols; col_index++) {
                f = coef[col_index];
                idx0 = pre_idx[col_index];
                idx1 = idx0 + 1;
                dst_matrix[dst_index++] = (1 - f) * ori_matrix[row_index * cols + idx0] + f * ori_matrix[row_index * cols + idx1];
            }
        }
    } else { // y-direction
        for (int col_index = 0; col_index < cols; col_index++) {
            dst_index = col_index;
            for (int row_index = 0; row_index < rows; row_index++) {
                f = coef[row_index];
                idx0 = pre_idx[row_index];
                idx1 = idx0 + 1;
                dst_matrix[dst_index] = (1 - f) * ori_matrix[idx0 * cols + col_index] + f * ori_matrix[idx1 * cols + col_index];
                dst_index += cols;
            }
        }
    }

    return 0;
}

static int AlscMatrixSplit(const unsigned short* ori_matrix, int cols, int rows,
                           unsigned short left[], unsigned short right[]) {
    int out_cols                = cols / 2 + cols % 2;
    int out_rows                = rows;
    int left_start_addr         = 0;
    int right_start_addr        = cols - out_cols;
    unsigned short* left_index  = left;
    unsigned short* right_index = right;

    while (out_rows--) {
        memcpy(left_index, ori_matrix + left_start_addr, out_cols * sizeof(unsigned short));
        memcpy(right_index, ori_matrix + right_start_addr, out_cols * sizeof(unsigned short));
        left_index += out_cols;
        right_index += out_cols;
        left_start_addr += cols;
        right_start_addr += cols;
    }

    return 0;
}

int AiqIspParamsSplitter_SplitAlscXtable(const unsigned short* in_array, int in_size,
                                         unsigned short* dst_left, unsigned short* dst_right) {
    int in_index    = 0;
    int left_index  = 0;
    int right_index = 0;
    for (in_index = 0; in_index < in_size; in_index++) {
        if (in_index < (in_size / 2)) {
            dst_left[left_index++] = ceil(in_array[in_index] * 1.0 / 2);
            dst_left[left_index] = in_array[in_index] - dst_left[left_index-1];
            left_index++;
        } else {
            dst_right[right_index++] = ceil(in_array[in_index] * 1.0 / 2);
            dst_right[right_index] = in_array[in_index] - dst_left[right_index-1];
            right_index++;
        }
    }

    dst_left[in_size - 1] += RKMOUDLE_UNITE_EXTEND_PIXEL;
    dst_right[0] += RKMOUDLE_UNITE_EXTEND_PIXEL;

    return 0;
}

int AiqIspParamsSplitter_LscGradUpdate(unsigned short xgrad_tbl[], unsigned short ygrad_tbl[],
                                       unsigned short x_sect_tbl[], unsigned short y_sect_tbl[],
                                       int x_sect_size, int y_sect_size) {
    const uint32_t SCALE = 1UL << 15;

    for (uint32_t i = 0; i < x_sect_size; i++) {
        unsigned short x = x_sect_tbl[i];
        if (0 < x) {
            xgrad_tbl[i] = (uint16_t)((SCALE + x / 2) / x);
        } else {
            return XCAM_RETURN_ERROR_PARAM;
        }
    }
    for (uint32_t i = 0; i < y_sect_size; i++) {
        unsigned short y = y_sect_tbl[i];
        if (0 < y) {
            ygrad_tbl[i] = (uint16_t)((SCALE + y / 2) / y);
        } else {
            return XCAM_RETURN_ERROR_PARAM;
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn AiqIspParamsSplitter_SplitRawAeLiteParamsVertical(
    AiqIspParamsSplitter_t* pSplit, struct isp2x_rawaelite_meas_cfg* ori,
    struct isp2x_rawaelite_meas_cfg* left, struct isp2x_rawaelite_meas_cfg* right) {
    u8 wnd_num = 0;
    if (ori->wnd_num == 0)
        wnd_num = 1;
    else
        wnd_num = 5;

    WinSplitMode mode        = LEFT_AND_RIGHT_MODE;
    WinSplitMode sub_mode[4] = {LEFT_AND_RIGHT_MODE};

    SplitAecWinVertical(&ori->win, &left->win, &right->win, wnd_num, pSplit->left_isp_rect_,
                        pSplit->bottom_left_isp_rect_, &mode);

#ifdef DEBUG
    printf("AeLite left=%d-%d-%d-%d, right=%d-%d-%d-%d\n", left->win.h_offs, left->win.v_offs,
           left->win.h_size, left->win.v_size, right->win.h_offs, right->win.v_offs,
           right->win.h_size, right->win.v_size);
#endif

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn AiqIspParamsSplitter_SplitRawAeBigParamsVertical(AiqIspParamsSplitter_t* pSplit,
                                                            struct isp2x_rawaebig_meas_cfg* ori,
                                                            struct isp2x_rawaebig_meas_cfg* left,
                                                            struct isp2x_rawaebig_meas_cfg* right) {
    u8 wnd_num = 0;

    if (ori->wnd_num == 0)
        wnd_num = 1;
    else if (ori->wnd_num == 1)
        wnd_num = 5;
    else
        wnd_num = 15;

    WinSplitMode mode        = LEFT_AND_RIGHT_MODE;
    WinSplitMode sub_mode[4] = {LEFT_AND_RIGHT_MODE};

    SplitAecWinVertical(&ori->win, &left->win, &right->win, wnd_num, pSplit->left_isp_rect_,
                        pSplit->bottom_left_isp_rect_, &mode);
    SplitAecSubWinVertical(ori->subwin_en, ori->subwin, left->subwin, right->subwin,
                           pSplit->left_isp_rect_, pSplit->bottom_left_isp_rect_, sub_mode);

    for (int i = 0; i < ISP2X_RAWAEBIG_SUBWIN_NUM; i++) {
        if (ori->subwin_en[i]) {
            switch (sub_mode[i]) {
                case LEFT_AND_RIGHT_MODE:
                    left->subwin_en[i]  = true;
                    right->subwin_en[i] = true;
                    break;
                case LEFT_MODE:
                    left->subwin_en[i]  = true;
                    right->subwin_en[i] = false;
                    break;
                case RIGHT_MODE:
                    left->subwin_en[i]  = false;
                    right->subwin_en[i] = true;
                    break;
                default:
                    break;
            }
        }
    }

#ifdef DEBUG
    printf("AeBig left=%d-%d-%d-%d, right=%d-%d-%d-%d\n", left->win.h_offs, left->win.v_offs,
           left->win.h_size, left->win.v_size, right->win.h_offs, right->win.v_offs,
           right->win.h_size, right->win.v_size);
#endif

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn AiqIspParamsSplitter_SplitRawHistLiteParamsVertical(
    AiqIspParamsSplitter_t* pSplit, struct isp2x_rawhistlite_cfg* ori,
    struct isp2x_rawhistlite_cfg* left, struct isp2x_rawhistlite_cfg* right) {
    u8 wnd_num = 0;
    wnd_num    = 5;

    WinSplitMode mode = LEFT_AND_RIGHT_MODE;

    SplitAecWinVertical(&ori->win, &left->win, &right->win, wnd_num, pSplit->left_isp_rect_,
                        pSplit->bottom_left_isp_rect_, &mode);
    AiqIspParamsSplitter_SplitAecWeightVertical(ori->weight, left->weight, right->weight, mode,
                                                wnd_num);

#ifdef DEBUG
    printf("HistLite left=%d-%d-%d-%d, right=%d-%d-%d-%d\n", left->win.h_offs, left->win.v_offs,
           left->win.h_size, left->win.v_size, right->win.h_offs, right->win.v_offs,
           right->win.h_size, right->win.v_size);

    for (int i = 0; i < wnd_num; i++) {
        for (int j = 0; j < wnd_num; j++) printf("%d ", left->weight[i * wnd_num + j]);
        printf("\n");
    }
    for (int i = 0; i < wnd_num; i++) {
        for (int j = 0; j < wnd_num; j++) printf("%d ", right->weight[i * wnd_num + j]);
        printf("\n");
    }
#endif

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn AiqIspParamsSplitter_SplitRawHistBigParamsVertical(AiqIspParamsSplitter_t* pSplit,
                                                              struct isp2x_rawhistbig_cfg* ori,
                                                              struct isp2x_rawhistbig_cfg* left,
                                                              struct isp2x_rawhistbig_cfg* right) {
    u8 wnd_num = 0;

    if (ori->wnd_num <= 1)
        wnd_num = 5;
    else
        wnd_num = 15;

    WinSplitMode mode = LEFT_AND_RIGHT_MODE;

    SplitAecWinVertical(&ori->win, &left->win, &right->win, wnd_num, pSplit->left_isp_rect_,
                        pSplit->bottom_left_isp_rect_, &mode);
    AiqIspParamsSplitter_SplitAecWeightVertical(ori->weight, left->weight, right->weight, mode,
                                                wnd_num);

#ifdef DEBUG
    printf("HistBig left=%d-%d-%d-%d, right=%d-%d-%d-%d\n", left->win.h_offs, left->win.v_offs,
           left->win.h_size, left->win.v_size, right->win.h_offs, right->win.v_offs,
           right->win.h_size, right->win.v_size);

    for (int i = 0; i < wnd_num; i++) {
        for (int j = 0; j < wnd_num; j++) printf("%d ", left->weight[i * wnd_num + j]);
        printf("\n");
    }
    for (int i = 0; i < wnd_num; i++) {
        for (int j = 0; j < wnd_num; j++) printf("%d ", right->weight[i * wnd_num + j]);
        printf("\n");
    }

#endif

    return XCAM_RETURN_NO_ERROR;
}

static int SplitAlscYtable(const unsigned short* in_array, int in_size, int ori_imgh,
                           unsigned short* dst_top, unsigned short* dst_bottom, int top_h,
                           int bottom_h) {
    int in_index     = 0;
    int top_index    = 0;
    int bottom_index = 0;
    for (in_index = 0; in_index < in_size; in_index++) {
        if (in_index < (in_size / 2)) {
            dst_top[top_index++] = ceil(in_array[in_index] * 1.0 / ori_imgh * top_h);
            dst_top[top_index++] = floor(in_array[in_index] * 1.0 / ori_imgh * top_h);
        } else {
            dst_bottom[bottom_index++] = ceil(in_array[in_index] * 1.0 / ori_imgh * bottom_h);
            dst_bottom[bottom_index++] = floor(in_array[in_index] * 1.0 / ori_imgh * bottom_h);
        }
    }

    dst_top[in_size - 1] += RKMOUDLE_UNITE_EXTEND_PIXEL;
    dst_bottom[0] += RKMOUDLE_UNITE_EXTEND_PIXEL;

    return 0;
}

int AiqIspParamsSplitter_AlscMatrixScaleVertical(float trate, float brate,
                                                 unsigned short ori_matrix[],
                                                 unsigned short top_matrix[],
                                                 unsigned short btm_matrix[],
                                                 int rows, int cols) {
    int ori_row_index = 0;
    int mid_row       = rows / 2;
    int col_index     = 0;

    for (col_index = 0; col_index < cols; col_index++) {
        for (ori_row_index = 0; ori_row_index < rows; ori_row_index++) {
            if (ori_row_index < mid_row) {
                top_matrix[col_index + ori_row_index * 2 * cols] =
                    ori_matrix[col_index + ori_row_index * cols];
                top_matrix[col_index + (ori_row_index * 2 + 1) * cols] =
                    (ori_matrix[col_index + ori_row_index * cols] +
                     ori_matrix[col_index + (ori_row_index + 1) * cols]) /
                    2;
            } else if (ori_row_index == mid_row) {
                top_matrix[col_index + (rows - 1) * cols] =
                        (unsigned short) ((1 + trate) * ori_matrix[col_index + mid_row * cols] -
                        trate * ori_matrix[col_index +  (mid_row - 1) * cols] + 0.5);
                btm_matrix[col_index] =
                        (unsigned short) ((1 + brate) * ori_matrix[col_index +  mid_row * cols] -
                        brate * ori_matrix[col_index + (mid_row + 1) * cols] + 0.5);
            } else {
                btm_matrix[col_index + (ori_row_index * 2 - rows) * cols] =
                    (ori_matrix[col_index + ori_row_index * cols] +
                     ori_matrix[col_index + (ori_row_index - 1) * cols]) /
                    2;
                btm_matrix[col_index + (ori_row_index * 2 - rows + 1) * cols] =
                    ori_matrix[col_index + ori_row_index * cols];
            }
        }
    }

    return 0;
}
