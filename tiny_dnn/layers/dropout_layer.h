/*
    Copyright (c) 2013, Taiga Nomi and the respective contributors
    All rights reserved.

    Use of this source code is governed by a BSD-style license that can be found
    in the LICENSE file.
*/
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "tiny_dnn/layers/layer.h"
#include "tiny_dnn/util/util.h"

namespace tiny_dnn {

/**
 * applies dropout to the input
 **/
class dropout_layer : public layer {
 public:
  /**
   * @param in_dim       [in] number of elements of the input
   * @param dropout_rate [in] (0-1) fraction of the input units to be dropped
   * @param phase        [in] initial state of the dropout
   **/
  dropout_layer(size_t in_dim,
                float_t dropout_rate,
                net_phase phase = net_phase::train)
    : layer({vector_type::data}, {vector_type::data}),
      phase_(phase),
      dropout_rate_(dropout_rate),
      scale_(float_t(1) / (float_t(1) - dropout_rate_)),
      in_size_(in_dim) {
    mask_.resize(1, std::vector<uint8_t>(in_dim));
    clear_mask();
  }

  dropout_layer(const dropout_layer &obj) = default;
  virtual ~dropout_layer() {}

  dropout_layer(dropout_layer &&obj) = default;
  dropout_layer &operator=(const dropout_layer &obj) = default;
  dropout_layer &operator=(dropout_layer &&obj) = default;

  void set_dropout_rate(float_t rate) {
    dropout_rate_ = rate;
    scale_        = float_t(1) / (float_t(1) - dropout_rate_);
  }

  float_t dropout_rate() const { return dropout_rate_; }

  ///< number of incoming connections for each output unit
  size_t fan_in_size() const override { return 1; }

  ///< number of outgoing connections for each input unit
  size_t fan_out_size() const override { return 1; }

  std::vector<index3d<size_t>> in_shape() const override {
    return {index3d<size_t>(in_size_, 1, 1)};
  }

  std::vector<index3d<size_t>> out_shape() const override {
    return {index3d<size_t>(in_size_, 1, 1)};
  }

  void back_propagation(const std::vector<Tensor<> *> &in_data,
                        const std::vector<Tensor<> *> &out_data,
                        std::vector<Tensor<> *> &out_grad,
                        std::vector<Tensor<> *> &in_grad) override {
    Tensor<> &prev_delta       = *in_grad[0];
    const Tensor<> &curr_delta = *out_grad[0];

    CNN_UNREFERENCED_PARAMETER(in_data);
    CNN_UNREFERENCED_PARAMETER(out_data);

    for_i(prev_delta.shape()[0], [&](size_t sample) {
      // assert(prev_delta[sample].size() == curr_delta[sample].size());
      // assert(mask_[sample].size() == prev_delta[sample].size());
      size_t sz = prev_delta.shape()[1];
      for (size_t i = 0; i < sz; ++i) {
        prev_delta.host_at(sample, i) =
          mask_[sample][i] * curr_delta.host_at(sample, i);
      }
    });
  }

  void forward_propagation(const std::vector<Tensor<> *> &in_data,
                           std::vector<Tensor<> *> &out_data) override {
    const Tensor<> &in = *in_data[0];
    Tensor<> &out      = *out_data[0];
    out.reshape(in.shape());
    const size_t sample_count = in.shape()[0];

    if (mask_.size() < sample_count) {
      mask_.resize(sample_count, mask_[0]);
    }

    for_i(sample_count, [&](size_t sample) {
      std::vector<uint8_t> &mask = mask_[sample];

      const auto in_vec = in.subView(TensorSingleIndex(sample), TensorAll());
      auto out_vec      = out.subView(TensorSingleIndex(sample), TensorAll());

      const size_t num = in_vec.shape()[0];

      if (phase_ == net_phase::train) {
        for (size_t i = 0; i < num; i++) mask[i] = bernoulli(dropout_rate_);

        for (size_t i        = 0; i < num; i++)
          out_vec.host_at(i) = mask[i] * scale_ * in_vec.host_at(i);
      } else {
        for (size_t i = 0; i < num; i++) out_vec.host_at(i) = in_vec.host_at(i);
      }
    });
  }

  /**
   * set dropout-context (training-phase or test-phase)
   **/
  void set_context(net_phase ctx) override { phase_ = ctx; }

  std::string layer_type() const override { return "dropout"; }

  // currently used by tests only
  const std::vector<uint8_t> &get_mask(size_t sample_index) const {
    return mask_[sample_index];
  }

  void clear_mask() {
    for (auto &sample : mask_) {
      std::fill(sample.begin(), sample.end(), 0);
    }
  }

  friend struct serialization_buddy;

 private:
  net_phase phase_;
  float_t dropout_rate_;
  float_t scale_;
  size_t in_size_;
  std::vector<std::vector<uint8_t>> mask_;
};

}  // namespace tiny_dnn
