/**
 * Copyright 2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nnacl/pack.h"
#include <string.h>
#include <stdlib.h>

void PackWeightFp32(float *weight_data, ConvParameter *conv_param, float *packed_weight, int oc_block,
                    int oc_block_num) {
  // original weight format : ohwi
  if (oc_block_num == 0) {
    return;
  }
  int kernel_h = conv_param->kernel_h_;
  int kernel_w = conv_param->kernel_w_;
  int in_channel = conv_param->input_channel_;
  int out_channel = conv_param->output_channel_;
  int ic4 = UP_DIV(in_channel, C4NUM);
  int kernel_plane = kernel_h * kernel_w;
  int pack_weight_size = oc_block * oc_block_num * ic4 * C4NUM * kernel_plane;

  int unit_size = oc_block * C4NUM;
  const int block_size = pack_weight_size / oc_block_num;

  for (int m = 0; m < kernel_plane; m++) {
    int kernel_plane_stride = m * in_channel;
    int packed_kernel_plane_stride = m * unit_size * ic4;
    for (int i = 0; i < ic4; i++) {
      int channel_block_stride = kernel_plane_stride + i * C4NUM;
      int packed_channel_block_size = packed_kernel_plane_stride + i * unit_size;
      int ic_remainder = in_channel - i * C4NUM;
      int real_ic_num = ic_remainder < C4NUM ? ic_remainder : C4NUM;
      for (int h = 0; h < real_ic_num; h++) {
        int block_stride = channel_block_stride + h;
        int packed_block_stride = packed_channel_block_size + h * oc_block;
        for (int j = 0; j < oc_block_num; j++) {
          int kernel_block_stride = block_stride + j * oc_block * kernel_plane * in_channel;
          int packed_kernel_block_size = packed_block_stride + j * block_size;
          int oc_remainder = out_channel - j * oc_block;
          int real_oc_num = oc_remainder < oc_block ? oc_remainder : oc_block;
          for (int k = 0; k < real_oc_num; k++) {
            float *origin_data_ptr = weight_data + kernel_block_stride + k * kernel_plane * in_channel;
            float *packed_data_ptr = packed_weight + packed_kernel_block_size + k;
            *packed_data_ptr = *origin_data_ptr;
          }
        }  // kernel block loop
      }    // inchannel block loop
    }      // channel block loop
  }        // kernel plane loop
}

void PackWeightKHWToHWKFp32(const void *src, void *dst, int plane, int channel) {
  return PackNCHWToNHWCFp32(src, dst, 1, plane, channel);
}

void PackWeightInt8(int8_t *weight_data, ConvParameter *conv_param, int8_t *packed_weight, int32_t *weight_sum) {
  // original weight format : ohwi
  int kernel_h = conv_param->kernel_h_;
  int kernel_w = conv_param->kernel_w_;
  int in_channel = conv_param->input_channel_;
  int out_channel = conv_param->output_channel_;
  int oc4 = UP_DIV(out_channel, C4NUM);
  int ic4 = UP_DIV(in_channel, C4NUM);
  int kernel_plane = kernel_h * kernel_w;
  int plane_c4 = UP_DIV(kernel_plane, C4NUM);
  int pack_weight_size = oc4 * C4NUM * ic4 * C4NUM * plane_c4 * C4NUM;
  int block_size = pack_weight_size / oc4;

  for (int m = 0; m < kernel_plane; m++) {
    int kernel_plane_stride = m * in_channel;
    int plane_block = m / C4NUM;
    int plane_res = m % C4NUM;
    int packed_kernel_plane_stride = plane_block * C4NUM * C4NUM * ic4 * C4NUM + plane_res * C4NUM;
    for (int i = 0; i < ic4; i++) {
      int channel_block_stride = kernel_plane_stride + i * C4NUM;
      int packed_channel_block_size = packed_kernel_plane_stride + i * C4NUM * C4NUM * C4NUM;
      int ic_remainder = in_channel - i * C4NUM;
      int real_ic_num = ic_remainder < C4NUM ? ic_remainder : C4NUM;
      for (int h = 0; h < real_ic_num; h++) {
        int block_stride = channel_block_stride + h;
        int packed_block_stride = packed_channel_block_size + h;
        for (int j = 0; j < oc4; j++) {
          int kernel_block_stride = block_stride + j * C4NUM * kernel_plane * in_channel;
          int packed_kernel_block_size = packed_block_stride + j * block_size;
          int oc_remainder = out_channel - j * C4NUM;
          int real_oc_num = oc_remainder < C4NUM ? oc_remainder : C4NUM;
          for (int k = 0; k < real_oc_num; k++) {
            int8_t *origin_data_ptr = weight_data + kernel_block_stride + k * kernel_plane * in_channel;
            int8_t *packed_data_ptr = packed_weight + packed_kernel_block_size + k * C4NUM * C4NUM;
            *packed_data_ptr = origin_data_ptr[0];
            // value of weight must between [-127, 127]
            if (packed_data_ptr[0] == -128) {
              packed_data_ptr[0] = -127;
            }
            weight_sum[j * C4NUM + k] += (int32_t)packed_data_ptr[0];
          }
        }  // kernel block loop
      }    // inchannel block loop
    }      // channel block loop
  }        // kernel plane loop
}

void PackWeightInt8Opt(int8_t *weight_data, ConvParameter *conv_param, int8_t *packed_weight, int32_t *weight_sum) {
  // original weight format : ohwi
  int kernel_h = conv_param->kernel_h_;
  int kernel_w = conv_param->kernel_w_;
  int in_channel = conv_param->input_channel_;
  int out_channel = conv_param->output_channel_;
  int oc4 = UP_DIV(out_channel, C4NUM);
  int ic4 = UP_DIV(in_channel, C4NUM);
  int kernel_plane = kernel_h * kernel_w;
  int pack_weight_size = oc4 * ic4 * C4NUM * C4NUM * kernel_plane;
  int unit_size = C4NUM * C4NUM;
  int block_size = pack_weight_size / oc4;

  for (int m = 0; m < kernel_plane; m++) {
    int kernel_plane_stride = m * in_channel;
    int packed_kernel_plane_stride = m * unit_size * ic4;
    for (int i = 0; i < ic4; i++) {
      int channel_block_stride = kernel_plane_stride + i * C4NUM;
      int packed_channel_block_size = packed_kernel_plane_stride + i * unit_size;
      int ic_remainder = in_channel - i * C4NUM;
      int real_ic_num = ic_remainder < C4NUM ? ic_remainder : C4NUM;
      for (int h = 0; h < real_ic_num; h++) {
        int block_stride = channel_block_stride + h;
        int packed_block_stride = packed_channel_block_size + h;
        for (int j = 0; j < oc4; j++) {
          int kernel_block_stride = block_stride + j * C4NUM * kernel_plane * in_channel;
          int packed_kernel_block_size = packed_block_stride + j * block_size;
          int oc_remainder = out_channel - j * C4NUM;
          int real_oc_num = oc_remainder < C4NUM ? oc_remainder : C4NUM;
          for (int k = 0; k < real_oc_num; k++) {
            int8_t *origin_data_ptr = weight_data + kernel_block_stride + k * kernel_plane * in_channel;
            int8_t *packed_data_ptr = packed_weight + packed_kernel_block_size + k * C4NUM;
            *packed_data_ptr = origin_data_ptr[0];
            if (packed_data_ptr[0] == -128) {
              packed_data_ptr[0] = -127;
            }
            weight_sum[j * C4NUM + k] += (int32_t)(packed_data_ptr[0]);
          }
        }  // kernel block loop
      }    // inchannel block loop
    }      // channel block loop
  }        // kernel plane loop
}

void Conv1x1InputPack(const void *src_ptr, void *dst_ptr, ConvParameter *conv_param, int data_size) {
  /* support nhwc */
  char *src = (char *)src_ptr;
  char *dst = (char *)dst_ptr;
  for (int dst_h = 0; dst_h < conv_param->output_h_; dst_h++) {
    int src_h = dst_h * conv_param->stride_h_ - conv_param->pad_u_;
    if (src_h < 0 || src_h >= conv_param->input_h_) {
      continue;
    }
    const char *src_h_ptr = src + src_h * conv_param->input_w_ * conv_param->input_channel_ * data_size;
    char *dst_h_ptr = dst + dst_h * conv_param->output_w_ * conv_param->input_channel_ * data_size;
    for (int dst_w = 0; dst_w < conv_param->output_w_; dst_w++) {
      int src_w = dst_w * conv_param->stride_w_ - conv_param->pad_l_;
      if (src_w < 0 || src_w >= conv_param->input_w_) {
        continue;
      }
      memcpy(dst_h_ptr + dst_w * conv_param->input_channel_ * data_size,
             src_h_ptr + src_w * conv_param->input_channel_ * data_size, conv_param->input_channel_ * data_size);
    }
  }
  return;
}

void Pack1x1WeightFp32(const float *weight_data, float *packed_weight, ConvParameter *conv_param) {
  int c4 = UP_ROUND(conv_param->input_channel_, C4NUM);
  for (int ic = 0; ic < conv_param->input_channel_; ic++) {
    for (int oc = 0; oc < conv_param->output_channel_; oc++) {
      int oc4mod = oc % 4;
      int oc4div = oc / 4;
      int dst_index = oc4div * c4 * C4NUM + ic * C4NUM + oc4mod;
      int src_index = oc * conv_param->input_channel_ + ic;
      packed_weight[dst_index] = weight_data[src_index];
    }
  }
  return;
}

void PackInputSum16x4PerLayer(const int8_t *src, int32_t *dst, int32_t filter_zp, size_t row4, size_t col16) {
  /* optimize normal -> same layout */
#ifdef ENABLE_ARM64
  asm volatile(
    "mov x10, %[src] \n"
    "mov x11, %[dst] \n"
    "dup v15.4s, %w[filter_zp]  \n"

    "mov x0, #0 \n"
    "1: \n"
    "cmp x0, %[row4] \n"
    "beq 4f \n"
    "add x0, x0, #4\n"
    "dup v10.4s, wzr \n"
    "mov x2, #0 \n"

    "2: \n"
    "cmp x2, %[col16] \n"
    "beq 3f \n"
    "add x2, x2, #16\n"

    "ld1 {v0.16b}, [x10], #16\n"
    "ld1 {v1.16b}, [x10], #16\n"
    "ld1 {v2.16b}, [x10], #16\n"
    "ld1 {v3.16b}, [x10], #16\n"

    "saddlp v4.8h, v0.16b \n"
    "saddlp v5.8h, v1.16b \n"
    "saddlp v6.8h, v2.16b \n"
    "saddlp v7.8h, v3.16b \n"

    "saddlp v0.4S, v4.8h \n"
    "saddlp v1.4S, v5.8h \n"
    "saddlp v2.4S, v6.8h \n"
    "saddlp v3.4S, v7.8h \n"

    "addv s4, v0.4S \n"
    "addv s5, v1.4S \n"
    "addv s6, v2.4S \n"
    "addv s7, v3.4S \n"

    "mov v0.s[0], v4.s[0] \n"
    "mov v0.s[1], v5.s[0] \n"
    "mov v0.s[2], v6.s[0] \n"
    "mov v0.s[3], v7.s[0] \n"

    "add v10.4s, v10.4s, v0.4s \n"
    "b 2b\n"

    "3: \n"
    "mul v10.4s, v10.4s, v15.4s \n"
    "st1 {v10.4s}, [x11], #16 \n"
    "beq 1b \n"

    "4: \n"

    :
    : [ dst ] "r"(dst), [ src ] "r"(src), [ row4 ] "r"(row4), [ col16 ] "r"(col16), [ filter_zp ] "r"(filter_zp)
    : "x0", "x1", "x2", "x3", "x10", "x11", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v10", "v15");
#else
  for (int r = 0; r < row4; r++) {
    int32_t tmp_value = 0;
    for (int c = 0; c < col16; c++) {
      int r4div = r / C4NUM, r4mod = r % C4NUM, c16div = c / C16NUM, c16mod = c % C16NUM;
      int src_index = r4div * C4NUM * col16 + c16div * C16NUM * C4NUM + r4mod * C16NUM + c16mod;
      tmp_value += src[src_index];
    }
    dst[r] = tmp_value * filter_zp;
  }
#endif
  return;
}

void PackInputSum16x4Int8(const int8_t *input_value, int32_t *input_sum, size_t input_channel, size_t output_channel,
                          size_t plane_size, ConvParameter *conv_param) {
  size_t hw4 = UP_ROUND(plane_size, C4NUM);
  size_t ic16 = UP_ROUND(input_channel, C16NUM);
  if (conv_param->conv_quant_arg_.filter_arg_num_ == 1) {
    PackInputSum16x4PerLayer(input_value, input_sum, conv_param->conv_quant_arg_.filter_quant_args_[0].zp_, hw4, ic16);
  } else {
    for (int ri = 0; ri < plane_size; ri++) {
      int ri4div = ri / C4NUM, ri4mod = ri % C4NUM;
      for (int ci = 0; ci < output_channel; ci++) {
        int32_t tmp_sum_value = 0;
        int ci4div = ci / C4NUM, ci4mod = ci % C4NUM;
        int32_t filter_zp = conv_param->conv_quant_arg_.filter_quant_args_[ci].zp_;
        for (int di = 0; di < input_channel; di++) {
          size_t di16div = di / C16NUM, di16mod = di % C16NUM;
          int src_index = ri4div * C4NUM * ic16 + di16div * C16NUM * C4NUM + ri4mod * C16NUM + di16mod;
          tmp_sum_value += input_value[src_index];
        }
        int dst_index = ci4div * C4NUM * hw4 + ri * C4NUM + ci4mod;
        input_sum[dst_index] = tmp_sum_value * filter_zp;
      }
    }
  }
  return;
}

void PackInputSum8x4Int8(const int8_t *input_value, int32_t *input_sum, size_t input_channel, size_t output_channel,
                         size_t plane_size, ConvParameter *conv_param) {
  size_t hw8 = UP_ROUND(plane_size, C8NUM);
  size_t ic4 = UP_ROUND(input_channel, C4NUM);
  if (conv_param->conv_quant_arg_.filter_arg_num_ == 1) {
    for (int r = 0; r < hw8; r++) {
      int32_t tmp_value = 0;
      for (int c = 0; c < ic4; c++) {
        int r8div = r / C8NUM, r8mod = r % C8NUM, c4div = c / C4NUM, c4mod = c % C4NUM;
        int src_index = r8div * C8NUM * ic4 + c4div * C8NUM * C4NUM + r8mod * C4NUM + c4mod;
        tmp_value += input_value[src_index];
      }
      input_sum[r] = tmp_value * conv_param->conv_quant_arg_.filter_quant_args_[0].zp_;
    }
  } else {
    for (int ri = 0; ri < plane_size; ri++) {
      int ri8div = ri / C8NUM, ri8mod = ri % C8NUM;
      for (int ci = 0; ci < output_channel; ci++) {
        int32_t tmp_sum_value = 0;
        int ci8div = ci / C8NUM, ci8mod = ci % C8NUM;
        int32_t filter_zp = conv_param->conv_quant_arg_.filter_quant_args_[ci].zp_;
        for (int di = 0; di < input_channel; di++) {
          size_t di4div = di / C4NUM, di4mod = di % C4NUM;
          int src_index = ri8div * C8NUM * ic4 + di4div * C8NUM * C4NUM + ri8mod * C4NUM + di4mod;
          tmp_sum_value += input_value[src_index];
        }
        int dst_index = ci8div * C8NUM * hw8 + ri * C8NUM + ci8mod;
        input_sum[dst_index] = tmp_sum_value * filter_zp;
      }
    }
  }
  return;
}

void Im2ColPackUnitFp32(const float *input_data, ConvParameter *conv_param, float *packed_input, int real_cal_num,
                        int block_index) {
  // input format : nhwc
  int kernel_h = conv_param->kernel_h_;
  int kernel_w = conv_param->kernel_w_;
  int stride_h = conv_param->stride_h_;
  int stride_w = conv_param->stride_w_;
  int pad_h = conv_param->pad_u_;
  int pad_w = conv_param->pad_l_;
  int dilation_h = conv_param->dilation_h_;
  int dilation_w = conv_param->dilation_w_;
  int in_channel = conv_param->input_channel_;
  int in_h = conv_param->input_h_;
  int in_w = conv_param->input_w_;
  int out_w = conv_param->output_w_;
  int ic4 = UP_DIV(in_channel, C4NUM);
  memset(packed_input, 0, kernel_h * kernel_w * ic4 * C4NUM * TILE_NUM * sizeof(float));

  for (int i = 0; i < real_cal_num; i++) {
    int block_start = block_index + i;
    int input_h = block_start / out_w * stride_h - pad_h;
    int input_w = block_start % out_w * stride_w - pad_w;
    int input_stride = input_h * in_w * ic4 * C4NUM + input_w * ic4 * C4NUM;
    int kh_s = MSMAX(0, UP_DIV(-input_h, dilation_h));
    int kh_e = MSMIN(kernel_h, UP_DIV(in_h - input_h, dilation_h));
    int kw_s = MSMAX(0, UP_DIV(-input_w, dilation_w));
    int kw_e = MSMIN(kernel_w, UP_DIV(in_w - input_w, dilation_w));
    for (int j = kh_s; j < kh_e; j++) {
      int input_y_stride = j * dilation_h * in_w * ic4 * C4NUM + input_stride;
      for (int n = kw_s; n < kw_e; n++) {
        int input_x_stride = input_y_stride + n * dilation_w * ic4 * C4NUM;
        int input_plane_offset = (j * kernel_w + n) * C8NUM * C4NUM * ic4 + i * C4NUM;
        for (int m = 0; m < ic4; m++) {
          int channel_block_stride = input_x_stride + m * C4NUM;
          int channel_block_offset = input_plane_offset + m * C8NUM * C4NUM;
#ifdef ENABLE_NEON
          vst1q_f32(packed_input + channel_block_offset, vld1q_f32(input_data + channel_block_stride));
#else
          for (int k = 0; k < C4NUM; ++k) {
            (packed_input + channel_block_offset)[k] = (input_data + channel_block_stride)[k];
          }
#endif
        }  // channel_block loop
      }    // kernel_w loop
    }      // kernel_h loop
  }        // tile num loop
}

void Im2ColPackUnitInt8(const int8_t *input_data, int8_t *packed_input, int real_cal_num, int block_index,
                        int32_t *input_sum, ConvParameter *conv_param) {
  // input format : nhwc
  int tile_num = conv_param->tile_num_;
  QuantArg *filter_arg = conv_param->conv_quant_arg_.filter_quant_args_;
  int kernel_h = conv_param->kernel_h_;
  int kernel_w = conv_param->kernel_w_;
  int stride_h = conv_param->stride_h_;
  int stride_w = conv_param->stride_w_;
  int pad_h = conv_param->pad_u_;
  int pad_w = conv_param->pad_l_;
  int dilation_h = conv_param->dilation_h_;
  int dilation_w = conv_param->dilation_w_;
  int in_channel = conv_param->input_channel_;
  int in_h = conv_param->input_h_;
  int in_w = conv_param->input_w_;
  int ic4 = UP_DIV(in_channel, C4NUM);
  int oc4 = UP_DIV(conv_param->output_channel_, C4NUM);
  int out_w = conv_param->output_w_;

  for (int i = 0; i < real_cal_num; i++) {
    int block_start = block_index + i;
    int input_h = block_start / out_w * stride_h - pad_h;
    int input_w = block_start % out_w * stride_w - pad_w;
    int input_cal_num_offset = i * C4NUM * C4NUM;
    int32_t input_accumulator = 0;
    for (int j = 0; j < kernel_h; j++) {
      int input_y = input_h + j * dilation_h;
      if (input_y < 0 || input_y >= in_h) {
        continue;
      }
      int input_y_stride = input_y * in_w * ic4 * C4NUM;
      for (int n = 0; n < kernel_w; n++) {
        int input_x = input_w + n * dilation_w;
        if (input_x < 0 || input_x >= in_w) {
          continue;
        }
        int input_x_stride = input_y_stride + input_x * ic4 * C4NUM;
        int plane_c4_block = (j * kernel_w + n) / C4NUM;
        int plane_c4_res = (j * kernel_w + n) % C4NUM;
        int input_plane_offset =
          plane_c4_block * tile_num * C4NUM * C4NUM * ic4 + plane_c4_res * C4NUM + input_cal_num_offset;
        for (int m = 0; m < ic4; m++) {
          int channel_block_stride = input_x_stride + m * C4NUM;
          int channel_block_offset = input_plane_offset + m * tile_num * C4NUM * C4NUM;
          (packed_input + channel_block_offset)[0] = (input_data + channel_block_stride)[0];
          (packed_input + channel_block_offset)[1] = (input_data + channel_block_stride)[1];
          (packed_input + channel_block_offset)[2] = (input_data + channel_block_stride)[2];
          (packed_input + channel_block_offset)[3] = (input_data + channel_block_stride)[3];
          input_accumulator += (packed_input + channel_block_offset)[0];
          input_accumulator += (packed_input + channel_block_offset)[1];
          input_accumulator += (packed_input + channel_block_offset)[2];
          input_accumulator += (packed_input + channel_block_offset)[3];
        }  // channel_block loop
      }    // kernel_w loop
    }      // kernel_h loop
    if (!(conv_param->conv_quant_arg_.asymmetric_ & FILTER_ASYMMETRIC)) {
      continue;
    } else if ((conv_param->conv_quant_arg_.asymmetric_ & FILTER_ASYMMETRIC) &&
               (conv_param->conv_quant_arg_.per_channel_ & FILTER_PER_CHANNEL)) {
      int cal_num_offset = i * oc4 * C4NUM;
      for (int l = 0; l < conv_param->output_channel_; ++l) {
        input_sum[cal_num_offset + l] = input_accumulator * filter_arg[l].zp_;
      }
    } else if ((conv_param->conv_quant_arg_.asymmetric_ & FILTER_ASYMMETRIC) &&
               !(conv_param->conv_quant_arg_.per_channel_ & FILTER_PER_CHANNEL)) {
      input_sum[i] = input_accumulator * filter_arg[0].zp_;
    }
  }  // tile num loop
}

void Im2ColPackUnitInt8Opt(const int8_t *input_data, int8_t *packed_input, int real_cal_num, int block_index,
                           int32_t *input_sum, ConvParameter *conv_param) {
  // input format : nhwc
  int tile_num = conv_param->tile_num_;
  QuantArg *filter_arg = conv_param->conv_quant_arg_.filter_quant_args_;
  int kernel_h = conv_param->kernel_h_;
  int kernel_w = conv_param->kernel_w_;
  int stride_h = conv_param->stride_h_;
  int stride_w = conv_param->stride_w_;
  int pad_h = conv_param->pad_u_;
  int pad_w = conv_param->pad_l_;
  int dilation_h = conv_param->dilation_h_;
  int dilation_w = conv_param->dilation_w_;
  int in_channel = conv_param->input_channel_;
  int in_h = conv_param->input_h_;
  int in_w = conv_param->input_w_;
  int ic4 = UP_DIV(in_channel, C4NUM);
  int oc4 = UP_DIV(conv_param->output_channel_, C4NUM);
  int out_w = conv_param->output_w_;
  int block_size = kernel_h * kernel_w;

  for (int i = 0; i < real_cal_num; i++) {
    int block_start = block_index + i;
    int input_h = block_start / out_w * stride_h - pad_h;
    int input_w = block_start % out_w * stride_w - pad_w;
    for (int j = 0; j < kernel_h; j++) {
      int input_y = input_h + j * dilation_h;
      if (input_y < 0 || input_y >= in_h) {
        continue;
      }
      int input_y_stride = input_y * in_w * ic4 * C4NUM;
      for (int n = 0; n < kernel_w; n++) {
        int input_x = input_w + n * dilation_w;
        if (input_x < 0 || input_x >= in_w) {
          continue;
        }
        int input_x_stride = input_y_stride + input_x * ic4 * C4NUM;
        int input_plane_offset = (j * kernel_w + n) * tile_num * C4NUM * ic4 + i * C4NUM;
        for (int m = 0; m < ic4; m++) {
          int channel_block_stride = input_x_stride + m * C4NUM;
          int channel_block_offset = input_plane_offset + m * tile_num * C4NUM;
          for (int k = 0; k < C4NUM; k++) {
            (packed_input + channel_block_offset)[k] = (input_data + channel_block_stride)[k];
          }
        }  // channel_block loop
      }    // kernel_w loop
    }      // kernel_h loop
    int32_t input_accumulator = 0;
    for (int j = 0; j < block_size; j++) {
      int block_offset = j * tile_num * ic4 * C4NUM + i * C4NUM;
      for (int c = 0; c < ic4; c++) {
        int ic4_offset = block_offset + c * tile_num * C4NUM;
        for (int k = 0; k < C4NUM; ++k) {
          input_accumulator += (packed_input + ic4_offset)[k];
        }
      }
    }
    if (!(conv_param->conv_quant_arg_.asymmetric_ & FILTER_ASYMMETRIC)) {
      continue;
    } else if ((conv_param->conv_quant_arg_.asymmetric_ & FILTER_ASYMMETRIC) &&
               (conv_param->conv_quant_arg_.per_channel_ & FILTER_PER_CHANNEL)) {
      int cal_num_offset = i * oc4 * C4NUM;
      for (int l = 0; l < conv_param->output_channel_; ++l) {
        input_sum[cal_num_offset + l] = input_accumulator * filter_arg[l].zp_;
      }
    } else if ((conv_param->conv_quant_arg_.asymmetric_ & FILTER_ASYMMETRIC) &&
               !(conv_param->conv_quant_arg_.per_channel_ & FILTER_PER_CHANNEL)) {
      input_sum[i] = input_accumulator * filter_arg[0].zp_;
    }
  }  // tile num loop
}

void PackInputToC8Int8(const int8_t *input_data, int16_t *packed_input, ConvParameter *conv_param) {
  int in_batch = conv_param->input_batch_;
  int in_channel = conv_param->input_channel_;
  int in_h = conv_param->input_h_;
  int in_w = conv_param->input_w_;
  int ic8 = UP_DIV(in_channel, C8NUM);

  for (int b = 0; b < in_batch; b++) {
    int src_batch_offset = b * in_channel * in_h * in_w;
    int dst_batch_offset = b * ic8 * C8NUM * in_h * in_w;
    for (int c = 0; c < in_channel; c++) {
      int ic8_block = c / C8NUM;
      int ic8_res = c % C8NUM;
      int src_c_offset = src_batch_offset + c;
      int dst_c_offset = dst_batch_offset + ic8_block * C8NUM * in_h * in_w + ic8_res;
      for (int k = 0; k < in_w * in_h; k++) {
        int src_plane_offset = src_c_offset + k * in_channel;
        int dst_plane_offset = dst_c_offset + k * C8NUM;
        (packed_input + dst_plane_offset)[0] = (int16_t)(input_data + src_plane_offset)[0];
      }
    }
  }
}

void PackWeightToC8Int8(const int8_t *origin_weight_data, int16_t *packed_weight_data, ConvParameter *conv_param) {
  // origin weight format : ohwi
  int input_channel = conv_param->input_channel_;
  int ic8 = UP_DIV(input_channel, C8NUM);
  int output_channel = conv_param->output_channel_;
  QuantArg *filter_zp = conv_param->conv_quant_arg_.filter_quant_args_;
  int kernel_plane = conv_param->kernel_h_ * conv_param->kernel_w_;

  for (int k = 0; k < kernel_plane; k++) {
    int src_kernel_offset = k * input_channel;
    int dst_kernel_offset = k * C8NUM;
    for (int o = 0; o < output_channel; o++) {
      int32_t zp;
      if (conv_param->conv_quant_arg_.filter_arg_num_ == 1) {
        zp = filter_zp[0].zp_;
      } else {
        zp = filter_zp[o].zp_;
      }
      int src_oc_offset = src_kernel_offset + o * kernel_plane * input_channel;
      int dst_oc_offset = dst_kernel_offset + o * ic8 * kernel_plane * C8NUM;
      for (int i = 0; i < input_channel; i++) {
        int c8_block_num = i / C8NUM;
        int c8_block_rem = i % C8NUM;
        int src_ic_offset = src_oc_offset + i;
        int dst_ic_offset = dst_oc_offset + c8_block_num * kernel_plane * C8NUM + c8_block_rem;
        (packed_weight_data + dst_ic_offset)[0] = (int16_t)((origin_weight_data + src_ic_offset)[0] - zp);
      }
    }
  }
}

void PackNHWCToNC4HW4Fp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_oc_offset = b * plane * channel;
    int dst_oc_offset = b * plane * c4 * C4NUM;
    for (int k = 0; k < plane; k++) {
      int src_kernel_offset = src_oc_offset + k * channel;
      int dst_kernel_offset = dst_oc_offset + k * C4NUM;
      for (int i = 0; i < channel; i++) {
        int c4_block_num = i / C4NUM;
        int c4_block_rem = i % C4NUM;
        int src_ic_offset = src_kernel_offset + i;
        int dst_ic_offset = dst_kernel_offset + c4_block_num * plane * C4NUM + c4_block_rem;
        ((float *)dst + dst_ic_offset)[0] = ((float *)src + src_ic_offset)[0];
      }
    }
  }
}

void PackNCHWToNC4HW4Fp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * channel;
    int dst_offset = b * plane * c4 * C4NUM;
    for (int c = 0; c < channel; c++) {
      int c4_block_num = c / C4NUM;
      int c4_block_rem = c % C4NUM;
      int src_c_offset = src_offset + c * plane;
      int dst_c_offset = dst_offset + c4_block_num * plane * C4NUM;
      for (int k = 0; k < plane; k++) {
        int src_kernel_offset = src_c_offset + k;
        int dst_kernel_offset = dst_c_offset + C4NUM * k + c4_block_rem;
        ((float *)dst + dst_kernel_offset)[0] = ((float *)src + src_kernel_offset)[0];
      }
    }
  }
}

void PackNHWCToNHWC4Fp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  int nhwc4_batch_unit_offset = c4 * C4NUM * plane;
  int ic_remainder_ = channel % C4NUM;
  if (ic_remainder_ != 0) {
    int nhwc4_batch_offset = 0;
    for (int b = 0; b < batch; b++) {
      int batch_offset = b * channel * plane;
      for (int i = 0; i < plane; i++) {
        memcpy((float *)dst + nhwc4_batch_offset + i * c4 * C4NUM, (float *)src + batch_offset + i * channel,
               channel * sizeof(float));
      }
      nhwc4_batch_offset += nhwc4_batch_unit_offset;
    }
  } else {
    size_t ori_input_size = batch * plane * channel * sizeof(float);
    memcpy((float *)dst, (float *)src, ori_input_size);
  }
}

void PackNHWC4ToNHWCFp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  int ic_remainder_ = channel % C4NUM;
  if (ic_remainder_ != 0) {
    int nhwc_batch_unit_offset = channel * plane;
    for (int b = 0; b < batch; b++) {
      int batch_offset = b * c4 * C4NUM * plane;
      for (int i = 0; i < plane; i++) {
        memcpy((float *)dst + b * nhwc_batch_unit_offset + i * channel, (float *)src + batch_offset + i * c4 * C4NUM,
               channel * sizeof(float));
      }
    }
  } else {
    size_t ori_input_size = batch * plane * channel * sizeof(float);
    memcpy((float *)dst, (float *)src, ori_input_size);
  }
}

void PackNCHWToNHWC4Fp32(const void *src, void *dst, int batch, int plane, int channel) {
  int nhwc4_batch_offset = 0;
  int c4 = UP_DIV(channel, C4NUM);
  int nhwc4_batch_unit_offset = c4 * C4NUM * plane;

  for (int b = 0; b < batch; b++) {
    int batch_offset = b * channel * plane;
    for (int c = 0; c < channel; c++) {
      int src_c_offset = batch_offset + c * plane;
      int dst_c_offset = nhwc4_batch_offset + c;
      for (int i = 0; i < plane; i++) {
        int src_plane_offset = src_c_offset + i;
        int dst_plane_offset = dst_c_offset + i * c4 * C4NUM;
        ((float *)dst)[dst_plane_offset] = ((float *)src)[src_plane_offset];
      }
    }
    nhwc4_batch_offset += nhwc4_batch_unit_offset;
  }
}

void PackNC4HW4ToNHWC4Fp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * c4 * C4NUM;
    int dst_offset = b * plane * channel;
    for (int c = 0; c < channel; c++) {
      int c4_block_num = c / C4NUM;
      int c4_block_res = c % C4NUM;
      int src_c_offset = src_offset + c4_block_num * plane * C4NUM + c4_block_res;
      int dst_c_offset = dst_offset + c4_block_num * C4NUM + c4_block_res;
      for (int k = 0; k < plane; k++) {
        int src_kernel_offset = src_c_offset + k * C4NUM;
        int dst_kernel_offset = dst_c_offset + k * c4 * C4NUM;
        ((float *)dst + dst_kernel_offset)[0] = ((float *)src + src_kernel_offset)[0];
      }
    }
  }
}

void PackNC4HW4ToNHWCFp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * c4 * C4NUM;
    int dst_offset = b * plane * channel;
    for (int k = 0; k < plane; k++) {
      int src_kernel_offset = src_offset + k * C4NUM;
      int dst_kernel_offset = dst_offset + k * channel;
      for (int c = 0; c < c4 - 1; c++) {
        int src_c_offset = src_kernel_offset + c * plane * C4NUM;
        int dst_c_offset = dst_kernel_offset + c * C4NUM;
#ifdef ENABLE_NEON
        vst1q_f32((float *)dst + dst_c_offset, vld1q_f32((float *)src + src_c_offset));
#else
        ((float *)dst + dst_c_offset)[0] = ((float *)src + src_c_offset)[0];
        ((float *)dst + dst_c_offset)[1] = ((float *)src + src_c_offset)[1];
        ((float *)dst + dst_c_offset)[2] = ((float *)src + src_c_offset)[2];
        ((float *)dst + dst_c_offset)[3] = ((float *)src + src_c_offset)[3];
#endif
      }
      // res part
      int res_c = channel - (c4 - 1) * C4NUM;
      for (int i = 0; i < res_c; i++) {
        int src_res_c_offset = src_kernel_offset + (c4 - 1) * C4NUM * plane + i;
        int dst_res_c_offset = dst_kernel_offset + (c4 - 1) * C4NUM + i;
        ((float *)dst + dst_res_c_offset)[0] = ((float *)src + src_res_c_offset)[0];
      }
    }
  }
}

void PackNC4HW4ToNHWCReluFp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * c4 * C4NUM;
    int dst_offset = b * plane * channel;
    for (int k = 0; k < plane; k++) {
      int src_kernel_offset = src_offset + k * C4NUM;
      int dst_kernel_offset = dst_offset + k * channel;
      for (int c = 0; c < c4 - 1; c++) {
        int src_c_offset = src_kernel_offset + c * plane * C4NUM;
        int dst_c_offset = dst_kernel_offset + c * C4NUM;
#ifdef ENABLE_NEON
        float32x4_t input_ptr = vld1q_f32((float *)src + src_c_offset);
        float32x4_t zero = vdupq_n_f32(0);
        input_ptr = vmaxq_f32(zero, input_ptr);
        vst1q_f32((float *)dst + dst_c_offset, input_ptr);
#else
        for (int i = 0; i < C4NUM; ++i) {
          float input_data = ((float *)src + src_c_offset)[i];
          input_data = input_data < 0 ? 0 : input_data;
          ((float *)dst + dst_c_offset)[i] = input_data;
        }
#endif
      }
      // res part
      int res_c = channel - (c4 - 1) * C4NUM;
      for (int i = 0; i < res_c; i++) {
        int src_res_c_offset = src_kernel_offset + (c4 - 1) * C4NUM * plane + i;
        int dst_res_c_offset = dst_kernel_offset + (c4 - 1) * C4NUM + i;
        float input_data = ((float *)src + src_res_c_offset)[0];
        input_data = input_data < 0 ? 0 : input_data;
        ((float *)dst + dst_res_c_offset)[0] = input_data;
      }
    }
  }
}

void PackNC4HW4ToNHWCRelu6Fp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * c4 * C4NUM;
    int dst_offset = b * plane * channel;
    for (int k = 0; k < plane; k++) {
      int src_kernel_offset = src_offset + k * C4NUM;
      int dst_kernel_offset = dst_offset + k * channel;
      for (int c = 0; c < c4 - 1; c++) {
        int src_c_offset = src_kernel_offset + c * plane * C4NUM;
        int dst_c_offset = dst_kernel_offset + c * C4NUM;
#ifdef ENABLE_NEON
        float32x4_t input_ptr = vld1q_f32((float *)src + src_c_offset);
        float32x4_t zero = vdupq_n_f32(0);
        float32x4_t six = vdupq_n_f32(6);
        input_ptr = vmaxq_f32(zero, input_ptr);
        input_ptr = vminq_f32(six, input_ptr);
        vst1q_f32((float *)dst + dst_c_offset, input_ptr);
#else
        for (int i = 0; i < C4NUM; ++i) {
          float input_data = ((float *)src + src_c_offset)[i];
          input_data = input_data < 0 ? 0 : input_data;
          input_data = input_data > 6 ? 6 : input_data;
          ((float *)dst + dst_c_offset)[i] = input_data;
        }
#endif
      }
      // res part
      int res_c = channel - (c4 - 1) * C4NUM;
      for (int i = 0; i < res_c; i++) {
        int src_res_c_offset = src_kernel_offset + (c4 - 1) * C4NUM * plane + i;
        int dst_res_c_offset = dst_kernel_offset + (c4 - 1) * C4NUM + i;
        float input_data = ((float *)src + src_res_c_offset)[0];
        input_data = input_data < 0 ? 0 : input_data;
        input_data = input_data > 6 ? 6 : input_data;
        ((float *)dst + dst_res_c_offset)[0] = input_data;
      }
    }
  }
}

void PackNC4HW4ToNHWCPreluFp32(const void *src, void *dst, const void *slope, int batch, int plane, int channel) {}

void PackNC4HW4ToNCHWFp32(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * c4 * C4NUM;
    int dst_offset = b * plane * channel;
    for (int c = 0; c < channel; c++) {
      int c4_block_num = c / C4NUM;
      int c4_block_res = c % C4NUM;
      int src_c_offset = src_offset + c4_block_num * plane * C4NUM + c4_block_res;
      int dst_c_offset = dst_offset + c * plane;
      for (int k = 0; k < plane; k++) {
        int src_kernel_offset = src_c_offset + k * C4NUM;
        int dst_kernel_offset = dst_c_offset + k;
        ((float *)dst + dst_kernel_offset)[0] = ((float *)src + src_kernel_offset)[0];
      }
    }
  }
}

void PackNHWCToC8HWN8Fp32(const void *src, void *dst, int batch, int plane, int channel) {
  for (int n = 0; n < batch; n++) {
    for (int hw = 0; hw < plane; hw++) {
      for (int c = 0; c < channel; c++) {
        int c8div = c / C8NUM;
        int c8mod = c % C8NUM;
        int src_index = n * plane * channel + hw * channel + c;
        int dst_index = c8div * batch * plane * C8NUM + hw * batch * C8NUM + n * C8NUM + c8mod;
        ((float *)dst)[dst_index] = ((float *)src)[src_index];
      }
    }
  }
  return;
}

void PackNHWCToNHWC4Int8(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  int nhwc4_batch_unit_offset = c4 * C4NUM * plane;
  int ic_remainder_ = channel % C4NUM;
  if (ic_remainder_ != 0) {
    int nhwc4_batch_offset = 0;
    for (int b = 0; b < batch; b++) {
      int batch_offset = b * channel * plane;
      for (int i = 0; i < plane; i++) {
        memcpy((int8_t *)dst + nhwc4_batch_offset + i * c4 * C4NUM, (int8_t *)src + batch_offset + i * channel,
               channel);
      }
      nhwc4_batch_offset += nhwc4_batch_unit_offset;
    }
  } else {
    size_t ori_input_size = batch * plane * channel;
    memcpy((int8_t *)dst, (int8_t *)src, ori_input_size);
  }
}

void PackNHWC4ToNHWCInt8(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  int nhwc4_batch_unit_offset = c4 * C4NUM * plane;
  int ic_remainder_ = channel % C4NUM;
  if (ic_remainder_ != 0) {
    for (int b = 0; b < batch; b++) {
      int batch_offset = b * channel * plane;
      int nhwc4_batch_offset = b * nhwc4_batch_unit_offset;
      for (int i = 0; i < plane; i++) {
        memcpy((int8_t *)dst + batch_offset + i * channel, (int8_t *)src + nhwc4_batch_offset + i * c4 * C4NUM,
               channel);
      }
    }
  } else {
    size_t ori_input_size = batch * plane * channel;
    memcpy((int8_t *)dst, (int8_t *)src, ori_input_size);
  }
}

void PackNCHWToNHWC4Int8(const void *src, void *dst, int batch, int plane, int channel) {
  int nhwc4_batch_offset = 0;
  int c4 = UP_DIV(channel, C4NUM);
  int nhwc4_batch_unit_offset = c4 * C4NUM * plane;

  for (int b = 0; b < batch; b++) {
    int batch_offset = b * channel * plane;
    for (int c = 0; c < channel; c++) {
      int src_c_offset = batch_offset + c * plane;
      int dst_c_offset = nhwc4_batch_offset + c;
      for (int i = 0; i < plane; i++) {
        int src_plane_offset = src_c_offset + i;
        int dst_plane_offset = dst_c_offset + i * c4 * C4NUM;
        ((uint8_t *)dst)[dst_plane_offset] = ((uint8_t *)src)[src_plane_offset];
      }
    }
    nhwc4_batch_offset += nhwc4_batch_unit_offset;
  }
}

void PackNC4HW4ToNHWC4Int8(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * c4 * C4NUM;
    int dst_offset = b * plane * channel;
    for (int c = 0; c < channel; c++) {
      int c4_block_num = c / C4NUM;
      int c4_block_res = c % C4NUM;
      int src_c_offset = src_offset + c4_block_num * plane * C4NUM + c4_block_res;
      int dst_c_offset = dst_offset + c4_block_num * C4NUM + c4_block_res;
      for (int k = 0; k < plane; k++) {
        int src_kernel_offset = src_c_offset + k * C4NUM;
        int dst_kernel_offset = dst_c_offset + k * c4 * C4NUM;
        ((uint8_t *)dst + dst_kernel_offset)[0] = ((uint8_t *)src + src_kernel_offset)[0];
      }
    }
  }
}

void PackNC4HW4ToNHWCInt8(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * c4 * C4NUM;
    int dst_offset = b * plane * channel;
    for (int k = 0; k < plane; k++) {
      int src_kernel_offset = src_offset + k * C4NUM;
      int dst_kernel_offset = dst_offset + k * channel;
      for (int c = 0; c < c4 - 1; c++) {
        int src_c_offset = src_kernel_offset + c * plane * C4NUM;
        int dst_c_offset = dst_kernel_offset + c * C4NUM;
        ((int8_t *)dst + dst_c_offset)[0] = ((int8_t *)src + src_c_offset)[0];
        ((int8_t *)dst + dst_c_offset)[1] = ((int8_t *)src + src_c_offset)[1];
        ((int8_t *)dst + dst_c_offset)[2] = ((int8_t *)src + src_c_offset)[2];
        ((int8_t *)dst + dst_c_offset)[3] = ((int8_t *)src + src_c_offset)[3];
      }
      // res part
      int res_c = channel - (c4 - 1) * C4NUM;
      for (int i = 0; i < res_c; i++) {
        int src_res_c_offset = src_kernel_offset + (c4 - 1) * C4NUM * plane + i;
        int dst_res_c_offset = dst_kernel_offset + (c4 - 1) * C4NUM + i;
        ((int8_t *)dst + dst_res_c_offset)[0] = ((int8_t *)src + src_res_c_offset)[0];
      }
    }
  }
}

void PackNC4HW4ToNCHWInt8(const void *src, void *dst, int batch, int plane, int channel) {
  int c4 = UP_DIV(channel, C4NUM);
  for (int b = 0; b < batch; b++) {
    int src_offset = b * plane * c4 * C4NUM;
    int dst_offset = b * plane * channel;
    for (int c = 0; c < channel; c++) {
      int c4_block_num = c / C4NUM;
      int c4_block_res = c % C4NUM;
      int src_c_offset = src_offset + c4_block_num * plane * C4NUM + c4_block_res;
      int dst_c_offset = dst_offset + c * plane;
      for (int k = 0; k < plane; k++) {
        int src_kernel_offset = src_c_offset + k * C4NUM;
        int dst_kernel_offset = dst_c_offset + k;
        ((uint8_t *)dst + dst_kernel_offset)[0] = ((uint8_t *)src + src_kernel_offset)[0];
      }
    }
  }
}

void PackNHWCToC8HWN8Int8(const void *src, void *dst, int batch, int plane, int channel) {
  for (int n = 0; n < batch; n++) {
    for (int hw = 0; hw < plane; hw++) {
      for (int c = 0; c < channel; c++) {
        int c8div = c / C8NUM;
        int c8mod = c % C8NUM;
        int src_index = n * plane * channel + hw * channel + c;
        int dst_index = c8div * batch * plane * C8NUM + hw * batch * C8NUM + n * C8NUM + c8mod;
        ((int8_t *)dst)[dst_index] = ((int8_t *)src)[src_index];
      }
    }
  }
  return;
}

void PackNHWCToNC8HW8Int8(const void *src, void *dst, int batch, int plane, int channel) {
  int c8 = UP_DIV(channel, C8NUM);
  for (int b = 0; b < batch; b++) {
    int src_oc_offset = b * plane * channel;
    int dst_oc_offset = b * plane * c8 * C8NUM;
    for (int k = 0; k < plane; k++) {
      int src_kernel_offset = src_oc_offset + k * channel;
      int dst_kernel_offset = dst_oc_offset + k * C8NUM;
      for (int i = 0; i < channel; i++) {
        int c8_block_num = i / C8NUM;
        int c8_block_rem = i % C8NUM;
        int src_ic_offset = src_kernel_offset + i;
        int dst_ic_offset = dst_kernel_offset + c8_block_num * plane * C8NUM + c8_block_rem;
        ((int8_t *)dst + dst_ic_offset)[0] = ((int8_t *)src + src_ic_offset)[0];
      }
    }
  }
}

void PackNCHWToNHWCInt8(const void *src, void *dst, int batch, int plane, int channel) {
  for (int n = 0; n < batch; n++) {
    for (int c = 0; c < channel; c++) {
      for (int hw = 0; hw < plane; hw++) {
        int nhwc_index = n * channel * plane + hw * channel + c;
        int nchw_index = n * channel * plane + c * plane + hw;
        ((int8_t *)(dst))[nhwc_index] = ((const int8_t *)(src))[nchw_index];
      }
    }
  }
  return;
}

void PackNHWCToNCHWFp32(const void *src, void *dst, int batches, int plane, int channel) {
  int hw8 = plane / C8NUM * C8NUM;
  int c8 = channel / C8NUM * C8NUM;
  int batch = plane * channel;
  for (int n = 0; n < batches; n++) {
    const float *src_batch = (const float *)src + n * batch;
    float *dst_batch = (float *)dst + n * batch;
    int hw = 0;
    for (; hw < hw8; hw += C8NUM) {
      int c = 0;
      for (; c < c8; c += C8NUM) {
        const float *src_ptr = src_batch + hw * channel + c;
        float *dst_ptr = dst_batch + c * plane + hw;
#ifdef ENABLE_ARM64
        size_t srcStride = channel * sizeof(float);
        size_t dstStride = plane * sizeof(float);
        asm volatile(
          "mov x10, %[src_ptr]\n"
          "mov x11, %[dst_ptr]\n"

          "ld1 {v0.4s, v1.4s}, [x10], %[srcStride]\n"
          "ld1 {v2.4s, v3.4s}, [x10], %[srcStride]\n"

          "zip1 v8.4s, v0.4s, v2.4s\n"
          "zip2 v9.4s, v0.4s, v2.4s\n"
          "zip1 v12.4s, v1.4s, v3.4s\n"
          "zip2 v13.4s, v1.4s, v3.4s\n"

          "ld1 {v4.4s, v5.4s}, [x10], %[srcStride]\n"
          "ld1 {v6.4s, v7.4s}, [x10], %[srcStride]\n"

          "zip1 v10.4s, v4.4s, v6.4s\n"
          "zip2 v11.4s, v4.4s, v6.4s\n"
          "zip1 v14.4s, v5.4s, v7.4s\n"
          "zip2 v15.4s, v5.4s, v7.4s\n"

          "ld1 {v0.4s, v1.4s}, [x10], %[srcStride]\n"
          "ld1 {v2.4s, v3.4s}, [x10], %[srcStride]\n"

          "trn1 v16.2d, v8.2d, v10.2d\n"
          "trn2 v18.2d, v8.2d, v10.2d\n"
          "trn1 v20.2d, v9.2d, v11.2d\n"
          "trn2 v22.2d, v9.2d, v11.2d\n"

          "ld1 {v4.4s, v5.4s}, [x10], %[srcStride]\n"
          "ld1 {v6.4s, v7.4s}, [x10], %[srcStride]\n"

          "trn1 v24.2d, v12.2d, v14.2d\n"
          "trn2 v26.2d, v12.2d, v14.2d\n"
          "trn1 v28.2d, v13.2d, v15.2d\n"
          "trn2 v30.2d, v13.2d, v15.2d\n"

          "zip1 v8.4s, v0.4s, v2.4s\n"
          "zip2 v9.4s, v0.4s, v2.4s\n"
          "zip1 v12.4s, v1.4s, v3.4s\n"
          "zip2 v13.4s, v1.4s, v3.4s\n"

          "zip1 v10.4s, v4.4s, v6.4s\n"
          "zip2 v11.4s, v4.4s, v6.4s\n"
          "zip1 v14.4s, v5.4s, v7.4s\n"
          "zip2 v15.4s, v5.4s, v7.4s\n"

          "trn1 v17.2d, v8.2d, v10.2d\n"
          "trn2 v19.2d, v8.2d, v10.2d\n"
          "trn1 v21.2d, v9.2d, v11.2d\n"
          "trn2 v23.2d, v9.2d, v11.2d\n"

          "trn1 v25.2d, v12.2d, v14.2d\n"
          "trn2 v27.2d, v12.2d, v14.2d\n"
          "trn1 v29.2d, v13.2d, v15.2d\n"
          "trn2 v31.2d, v13.2d, v15.2d\n"

          "st1 {v16.4s, v17.4s}, [x11], %[dstStride]\n"
          "st1 {v18.4s, v19.4s}, [x11], %[dstStride]\n"
          "st1 {v20.4s, v21.4s}, [x11], %[dstStride]\n"
          "st1 {v22.4s, v23.4s}, [x11], %[dstStride]\n"
          "st1 {v24.4s, v25.4s}, [x11], %[dstStride]\n"
          "st1 {v26.4s, v27.4s}, [x11], %[dstStride]\n"
          "st1 {v28.4s, v29.4s}, [x11], %[dstStride]\n"
          "st1 {v30.4s, v31.4s}, [x11], %[dstStride]\n"

          :
          :
          [ dst_ptr ] "r"(dst_ptr), [ src_ptr ] "r"(src_ptr), [ srcStride ] "r"(srcStride), [ dstStride ] "r"(dstStride)
          : "x10", "x11", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14",
            "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
            "v30", "v31");
#else
        for (int tr = 0; tr < C8NUM; tr++) {
          for (int tc = 0; tc < C8NUM; tc++) {
            dst_ptr[tc * plane + tr] = src_ptr[tr * channel + tc];
          }
        }
#endif
      }
      for (; c < channel; c++) {
        const float *src_ptr = src_batch + hw * channel + c;
        float *dst_ptr = dst_batch + c * plane + hw;
        for (size_t i = 0; i < C8NUM; i++) {
          dst_ptr[i] = src_ptr[i * channel];
        }
      }
    }
    for (; hw < plane; hw++) {
      const float *src_ptr = src_batch + hw * channel;
      float *dst_ptr = dst_batch + hw;
      for (size_t i = 0; i < channel; i++) {
        dst_ptr[i * plane] = src_ptr[i];
      }
    }
  }
  return;
}

void PackNHWCToNCHWInt8(const void *src, void *dst, int batch, int plane, int channel) {
  for (int n = 0; n < batch; n++) {
    for (int c = 0; c < channel; c++) {
      for (int hw = 0; hw < plane; hw++) {
        int nhwc_index = n * channel * plane + hw * channel + c;
        int nchw_index = n * channel * plane + c * plane + hw;
        ((int8_t *)dst)[nchw_index] = ((int8_t *)src)[nhwc_index];
      }
    }
  }
  return;
}

void PackNCHWToNHWCFp32(const void *src, void *dst, int batch, int plane, int channel) {
  return PackNHWCToNCHWFp32(src, dst, batch, channel, plane);
}

void MatrixPackUnit(const float *src, float *dst, size_t row, size_t col, size_t src_stride, size_t dst_stride) {
  size_t copy_size = row * C4NUM * sizeof(float);
  for (int c = 0; c < col; c++) {
    memcpy(dst + c * dst_stride, src + c * src_stride, copy_size);
  }
}

void MatrixPack(const float *src, float *dst, int row, int ic4, int stride) {
  int row4mod = row % 4;
  int row4div = row / 4;

  for (int i = 0; i < row4div; i++) {
    MatrixPackUnit(src + i * 4 * 4, dst + i * 4 * ic4 * 4, 4, ic4, stride, 16);
  }

  if (row4mod > 0) {
    MatrixPackUnit(src + row4div * 4 * 4, dst + row4div * 4 * ic4 * 4, row4mod, ic4, stride, row4mod * 4);
  }
  return;
}

void PackDepthwiseInt8Input(const int8_t *src, int16_t *dst, const ConvParameter *conv_param) {
  int input_zp = conv_param->conv_quant_arg_.input_quant_args_[0].zp_;
  int ic4 = UP_DIV(conv_param->input_channel_, C4NUM);
  int unit = conv_param->input_h_ * conv_param->input_w_;

  for (int b = 0; b < conv_param->input_batch_; b++) {
    const int8_t *src_b = src + b * unit * conv_param->input_channel_;
    int16_t *dst_b = dst + b * unit * ic4 * C4NUM;
    for (int k = 0; k < unit; k++) {
      const int8_t *src_k = src_b + k * conv_param->input_channel_;
      int16_t *dst_k = dst_b + k * ic4 * C4NUM;
      for (int c = 0; c < conv_param->input_channel_; c++) {
        dst_k[c] = (int16_t)(src_k[c] - input_zp);
      }
    }
  }
}

void PackDepthwiseInt8Weight(const int8_t *origin_weight, int16_t *packed_weight_, int plane, int channel,
                             ConvQuantArg *quant_qrg) {
  int weight_zp = quant_qrg->filter_quant_args_[0].zp_;
  for (int c = 0; c < channel; c++) {
    if (quant_qrg->per_channel_ & FILTER_PER_CHANNEL) {
      weight_zp = quant_qrg->filter_quant_args_[c].zp_;
    }
    int c4_block_num = c / C4NUM;
    int c4_block_rem = c % C4NUM;
    const int8_t *src_c = origin_weight + c * plane;
    int16_t *dst_c = packed_weight_ + c4_block_num * plane * C4NUM;
    for (int k = 0; k < plane; k++) {
      const int8_t *src_kernel = src_c + k;
      int16_t *dst_kernel = dst_c + C4NUM * k + c4_block_rem;
      *dst_kernel = (int16_t)(src_kernel[0] - weight_zp);
    }
  }
}
