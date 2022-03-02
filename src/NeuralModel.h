/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#pragma once

#include <string>
#include <vector>

#include "json.hpp"
#include "RTNeural/RTNeural.h"

namespace TwoPlay {


class ModelData {
private:
    std::string model_;
    size_t input_size_;
    size_t skip_;
    size_t output_size_;
    std::string unit_type_;
    size_t hidden_size_;
    bool bias_fl_;
public:
    const std::string&model() const { return model_; }
    size_t input_size() const { return input_size_; }
    size_t skip() const { return skip_; }
    size_t output_size() const { return output_size_; }
    const std::string& unit_type() const { return unit_type_; }
    size_t hidden_size() const { return hidden_size_; }
    bool bias_fl() const { return bias_fl_; }

    DECLARE_JSON_MAP(ModelData);

};

class StateDict {
private:
    std::vector<std::vector<float> > rec__weight_ih_l0_;
    std::vector<std::vector<float> > rec__weight_hh_l0_;
    std::vector<float> rec__bias_ih_l0_;
    std::vector<float> rec__bias_hh_l0_;
    std::vector<std::vector<float> > lin__weight_;
    std::vector<float> lin__bias_;
public:
    const std::vector<std::vector<float> > &rec__weight_ih_l0() const {return rec__weight_ih_l0_; }
    const std::vector<std::vector<float> > &rec__weight_hh_l0() const {return rec__weight_hh_l0_; };
    const std::vector<float> &rec__bias_ih_l0() const { return rec__bias_ih_l0_; }
    const std::vector<float> &rec__bias_hh_l0() const { return rec__bias_hh_l0_; }
    const std::vector<std::vector<float> > &lin__weight() const { return lin__weight_; }
    const std::vector<float> &lin__bias() const { return lin__bias_; }



    DECLARE_JSON_MAP(StateDict);

};

class NeuralModel {
private:
    ModelData model_data_;
    StateDict state_dict_;
public:
    const ModelData&model_data() const { return model_data_; }
    const StateDict&state_dict() const { return state_dict_; }

    void Load(const std::string&fileName);

    DECLARE_JSON_MAP(NeuralModel);
};

}//namespace