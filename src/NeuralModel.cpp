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

#include "NeuralModel.h"
#include <fstream>
#include <stdexcept>
#include "ss.hpp"

using namespace toob;
using namespace std;


void NeuralModel::Load(const std::string&fileName)
{
    ifstream s;
    s.open(fileName);
    if (!s.is_open())
    {
        throw std::logic_error(SS("Can't open file " << fileName));
    }
    json_reader reader(s);

    NeuralModel result;
    reader.read(this);
}


JSON_MAP_BEGIN(ModelData)
    JSON_MAP_REFERENCE(ModelData,model)
    JSON_MAP_REFERENCE(ModelData,input_size)
    JSON_MAP_REFERENCE(ModelData,skip)
    JSON_MAP_REFERENCE(ModelData,output_size)
    JSON_MAP_REFERENCE(ModelData,unit_type)
    JSON_MAP_REFERENCE(ModelData,hidden_size)
    JSON_MAP_REFERENCE(ModelData,bias_fl)
JSON_MAP_END()

JSON_MAP_BEGIN(StateDict)
    JSON_MAP_DICTIONARY_REFERENCE(StateDict,"rec.weight_ih_l0",rec__weight_ih_l0)
    JSON_MAP_DICTIONARY_REFERENCE(StateDict,"rec.weight_hh_l0",rec__weight_hh_l0)
    JSON_MAP_DICTIONARY_REFERENCE(StateDict,"rec.bias_ih_l0",rec__bias_ih_l0)
    JSON_MAP_DICTIONARY_REFERENCE(StateDict,"rec.bias_hh_l0",rec__bias_hh_l0)
    JSON_MAP_DICTIONARY_REFERENCE(StateDict,"lin.weight",lin__weight)
    JSON_MAP_DICTIONARY_REFERENCE(StateDict,"lin.bias",lin__bias)
JSON_MAP_END()

JSON_MAP_BEGIN(NeuralModel)
    JSON_MAP_REFERENCE(NeuralModel,model_data)
    JSON_MAP_REFERENCE(NeuralModel,state_dict)
JSON_MAP_END()

