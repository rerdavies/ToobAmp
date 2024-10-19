/*
 *   Copyright (c) 2024 Robin E. R. Davies
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
#include <iostream>
#include <stdexcept>
#include <cstdint>

// Prevent intellisense errors in VSCode, VStudio when compiling for aarch64
#if __INTELLISENSE__
#undef __ARM_NEON
#undef __ARM_NEON__
#endif

#define gassert(COND,...) \
if (!(COND)) { throw std::runtime_error("Assert faiiled: " #COND);}

// Reduce warnings for NueralAmpModelerCore files.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "NAM/dsp.h"
#include "NAM/wavenet.h"
#include "namFixes/wavenet_t.h"
#include "namFixes/dsp_ex.h"


#pragma GCC diagnostic pop

#include <random>

using namespace std;
using namespace nam;
using namespace nam::wavenet;
using namespace Eigen;


bool approxEqual(float v1, float v2)
{
    float denom = std::max(std::abs(v1),std::abs(v2));
    if (denom < 1.0) denom = 1.0;
    double e = std::abs(v1-v2)/denom;
    return e < 1E-5;
}
std::random_device rd;
std::mt19937 gen(rd());

float Random(float min, float max) {
    std::uniform_real_distribution dis(min,max);
    return dis(gen);
}    

std::vector<float> makeWeights(size_t size)
{
    std::vector<float> result(size);
    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = Random(-1,1);
    }
    return result;
}

template <typename T1, typename T2>
inline void SetRandomWeights(T1&v1,T2&v2,size_t nWeights = 60)
{
        auto weights = makeWeights(nWeights);
        auto iWeights1 = weights.begin();
        v1.set_weights_(iWeights1);
        auto nWeights1 = iWeights1-weights.begin();

        auto iWeights2 = weights.begin();
        v2.set_weights_(iWeights2);
        auto nWeights2 = iWeights2-weights.begin();

        gassert(nWeights1 == nWeights2);
        //cout << "nWeights: " << nWeights1 << endl;


}

template<typename Derived1, typename Derived2>
inline void CompareOutputs(MatrixBase<Derived1>&left, MatrixBase<Derived2>&right)
{
    gassert(left.rows() == right.rows());
    gassert(left.cols() == right.cols());

    for (int r = 0; r < left.rows(); ++r)
    {
        for (int c = 0; c < left.cols(); ++c)
        {
            float lval = left(r,c);
            float rval = right(r,c);
            gassert(approxEqual(lval,rval));
        }
    }

}

template<typename Derived>
void SetRandomInput(MatrixBase<Derived> &matrix)
{
    for (int r = 0; r < matrix.rows(); ++r)
    {
        for (int c = 0; c < matrix.cols(); ++c)
        {
            matrix(r,c) = Random(-1,1);
        }
    }
}


template <int IN_ROWS, int OUT_ROWS,bool BIAS>
inline void TestConv1x1() {
    auto weights = makeWeights(100);

    bool bias = BIAS;

    Conv1x1_T<IN_ROWS,OUT_ROWS> conv1x1_t {bias};
    Conv1x1 conv1x1 {(int)IN_ROWS,(int)OUT_ROWS,bias};


    SetRandomWeights(conv1x1_t,conv1x1,500);

    Matrix<float,IN_ROWS,FIXED_BUFFER_SIZE_T> input;
    SetRandomInput(input);
    Matrix<float,OUT_ROWS, FIXED_BUFFER_SIZE_T> output;

    conv1x1_t.template process<FIXED_BUFFER_SIZE_T>(input,output);

    MatrixXf inputF;
    inputF.resize(input.rows(),input.cols());
    inputF = input;

    MatrixXf outputF;
    outputF.resize(output.rows(),output.cols());

    conv1x1.process(inputF,outputF);

    CompareOutputs(output,outputF);

}
void TestConv1x1()
{
    cout << "//// Conv1x1" << endl;

    TestConv1x1<8,16,false>();    
    TestConv1x1<8,16,true>();    
    TestConv1x1<16,8,false>();    
    TestConv1x1<16,8,true>();    
}


template <int IN_ROWS, int OUT_ROWS,bool BIAS>
inline void TestConv1D() {

    for (int dilation = 1; dilation < 8; dilation *= 2)
    {
        constexpr int KERNEL_SIZE = 3;

        bool bias = BIAS;

        Conv1D_T<IN_ROWS,OUT_ROWS,FIXED_BUFFER_SIZE_T,KERNEL_SIZE> conv1D_t;
        conv1D_t.set_size_(IN_ROWS,OUT_ROWS,KERNEL_SIZE,bias,dilation);
        Conv1D conv1D;
        conv1D.set_size_(IN_ROWS,OUT_ROWS,KERNEL_SIZE,BIAS,dilation);

        SetRandomWeights(conv1D_t,conv1D,1000);



        Eigen::Matrix<float,IN_ROWS,Eigen::Dynamic> input;
        input.resize(IN_ROWS,512);
        SetRandomInput(input);
        MatrixXf inputF;
        inputF.resize(input.rows(),input.cols());
        inputF = input;


        for (int iOffset = 128; iOffset < 140; ++iOffset)
        {
            Matrix<float,OUT_ROWS, FIXED_BUFFER_SIZE_T> output;
            conv1D_t.process_(input,output,iOffset,FIXED_BUFFER_SIZE_T,0);

            MatrixXf outputF;
            outputF.resize(output.rows(),output.cols());

            conv1D.process_(inputF,outputF,iOffset,(int)FIXED_BUFFER_SIZE_T,0);

            CompareOutputs(output,outputF);
        }
    }
}

void TestConv1D()
{
    cout << "//// Conv1D" << endl;
    TestConv1D<8,16,false>();    
    TestConv1D<8,16,true>();    
    TestConv1D<16,8,false>();    
    TestConv1D<16,8,true>();    
    TestConv1D<1,8,true>();    
    TestConv1D<8,1,true>();    
}

template <int IN_ROWS, int OUT_ROWS,bool BIAS>
inline void Test_DilatedConv() {

    for (int dilation = 1; dilation < 8; dilation *= 2)
    {
        constexpr int KERNEL_SIZE = 3;

        bool bias = BIAS;

        _DilatedConv_T<IN_ROWS,OUT_ROWS,FIXED_BUFFER_SIZE_T,KERNEL_SIZE> _dilatedConv_t {bias,dilation};
        _DilatedConv _dilatedConv{(int)IN_ROWS,(int)OUT_ROWS,(int)KERNEL_SIZE,bias,dilation};

        SetRandomWeights(_dilatedConv_t,_dilatedConv,1000);



        Eigen::Matrix<float,IN_ROWS,Eigen::Dynamic> input;
        input.resize(IN_ROWS,512);
        SetRandomInput(input);
        MatrixXf inputF;
        inputF.resize(input.rows(),input.cols());
        inputF = input;


        for (int iOffset = 128; iOffset < 140; ++iOffset)
        {
            Matrix<float,OUT_ROWS, FIXED_BUFFER_SIZE_T> output;
            _dilatedConv_t.process_(input,output,iOffset,FIXED_BUFFER_SIZE_T,0);

            MatrixXf outputF;
            outputF.resize(output.rows(),output.cols());

            _dilatedConv.process_(inputF,outputF,iOffset,(int)FIXED_BUFFER_SIZE_T,0);

            CompareOutputs(output,outputF);
        }
    }
}

void Test_DilatedConv()
{
    cout << "//// _DilatedConv" << endl;
    Test_DilatedConv<8,16,false>();    
    Test_DilatedConv<8,16,true>();    
    Test_DilatedConv<16,8,false>();    
    Test_DilatedConv<16,8,true>();    
    Test_DilatedConv<1,8,true>();    
    Test_DilatedConv<8,1,true>();    
}

template <size_t INPUT_SIZE,size_t HEAD_SIZE, size_t CHANNELS,size_t KERNEL_SIZE>
inline void Test_LayerArray(const LayerArrayParams&layerArrayParams)
{
    constexpr size_t CONDITION_SIZE = 1;
    using LayerArray0_T = _LayerArray_T<INPUT_SIZE,HEAD_SIZE, CHANNELS, KERNEL_SIZE>;

    // constexpr int ConditionSize = (int)CONDITION_SIZE;
    // constexpr int HeadSize = (int)HEAD_SIZE;
    // constexpr int Channels = (int)CHANNELS;
    // constexpr int KernelSize = (int)KERNEL_SIZE;

    LayerArray0_T layerArray_T;
    layerArray_T.initialize(
      layerArrayParams.input_size, layerArrayParams.condition_size, layerArrayParams.head_size,
      layerArrayParams.channels, layerArrayParams.kernel_size, layerArrayParams.dilations,
      layerArrayParams.activation, layerArrayParams.gated, layerArrayParams.head_bias);

    _LayerArray layerArray {
        layerArrayParams.input_size, layerArrayParams.condition_size, layerArrayParams.head_size,
      layerArrayParams.channels, layerArrayParams.kernel_size, layerArrayParams.dilations,
      layerArrayParams.activation, layerArrayParams.gated, layerArrayParams.head_bias};

    SetRandomWeights(layerArray_T,layerArray,10000);

    layerArray_T.set_num_frames_(FIXED_BUFFER_SIZE_T);
    layerArray.set_num_frames_(FIXED_BUFFER_SIZE_T);

    Eigen::Matrix<float,INPUT_SIZE,FIXED_BUFFER_SIZE_T> layer_inputs;
    Eigen::Matrix<float,CONDITION_SIZE,FIXED_BUFFER_SIZE_T> condition;
    Eigen::Matrix<float,CHANNELS,FIXED_BUFFER_SIZE_T> head_inputs;
    Eigen::Matrix<float,CHANNELS,FIXED_BUFFER_SIZE_T> layer_outputs;
    Eigen::Matrix<float,HEAD_SIZE,FIXED_BUFFER_SIZE_T> head_outputs;

    Eigen::MatrixXf layer_inputs_F;
    layer_inputs_F.resize(layer_inputs.rows(),layer_inputs.cols());

    Eigen::MatrixXf  condition_F;
    condition_F.resize(condition.rows(),condition.cols());
    Eigen::MatrixXf head_inputs_F;
    head_inputs_F.resize(head_inputs.rows(),head_inputs.cols());

    Eigen::MatrixXf  layer_outputs_F;
    layer_outputs_F.resize(layer_outputs.rows(),layer_outputs.cols());
    Eigen::MatrixXf  head_outputs_F;
    head_outputs_F.resize(head_outputs.rows(),head_outputs.cols());

    for (size_t i = 0; i < 10; ++i)
    {
        SetRandomInput(layer_inputs);
        layer_inputs_F = layer_inputs;
        SetRandomInput(condition);
        condition_F = condition;
        SetRandomInput(head_inputs);
        head_inputs_F = head_inputs;

        layerArray_T.prepare_for_frames_(FIXED_BUFFER_SIZE_T);
        layerArray_T.process_(
            layer_inputs,
            condition,
            head_inputs,
            layer_outputs,
            head_outputs
        );
        layerArray_T.advance_buffers_(FIXED_BUFFER_SIZE_T);


        layerArray.prepare_for_frames_(FIXED_BUFFER_SIZE_T);
        layerArray.process_(
            layer_inputs_F,
            condition_F,
            head_inputs_F,
            layer_outputs_F,
            head_outputs_F
        );
        layerArray.advance_buffers_(FIXED_BUFFER_SIZE_T);

        CompareOutputs(layer_outputs,layer_outputs_F);
        CompareOutputs(head_outputs,head_outputs_F);


    }
}

std::vector<LayerArrayParams> layerArrayParams {
    LayerArrayParams{
        1,1,8,16,3,
        std::vector<int>{1,2,4,8,16,32,64,128,512},
        "Tanh",
        false,
        false},
    LayerArrayParams{
        16,1,1,8,3,
        std::vector<int>{1,2,4,8,16,32,64,128,512},
        "Tanh",
        false,
        false},
};

void Test_LayerArray()
{
    cout << "//// _LayerArray" << endl;

    constexpr size_t CONDITION_SIZE = 1, HEAD_SIZE=8,CHANNELS=16,KERNEL_SIZE=3;

    Test_LayerArray<CONDITION_SIZE,HEAD_SIZE, CHANNELS, KERNEL_SIZE>(layerArrayParams[0]);
    Test_LayerArray<CHANNELS, CONDITION_SIZE, HEAD_SIZE,KERNEL_SIZE>(layerArrayParams[1]);

}


template <size_t INPUT_SIZE,size_t HEAD_SIZE,size_t CHANNELS,size_t KERNEL_SIZE>
void Test_Layer(bool gated, int dilation)
{
    _Layer_T<INPUT_SIZE,HEAD_SIZE,CHANNELS,KERNEL_SIZE> layer_t;
    layer_t.initialize(
        INPUT_SIZE,CHANNELS,KERNEL_SIZE,
        dilation,"Tanh",gated);
    
    _Layer layer(
        INPUT_SIZE,CHANNELS,KERNEL_SIZE,
        dilation,"Tanh",gated);

    SetRandomWeights(layer_t,layer,5000);

    layer_t.set_num_frames_(FIXED_BUFFER_SIZE_T);
    layer.set_num_frames_(FIXED_BUFFER_SIZE_T);


    Eigen::Matrix<float,CHANNELS,Eigen::Dynamic> input;
    input.resize(input.rows(),1024);
    Eigen::Matrix<float,INPUT_SIZE,FIXED_BUFFER_SIZE_T> condition;
    Eigen::Matrix<float,CHANNELS,FIXED_BUFFER_SIZE_T> head_input;
    Eigen::Matrix<float,CHANNELS,Eigen::Dynamic> output;
    output.resize(output.rows(),1024);
    output.setZero();

    SetRandomInput(input);
    SetRandomInput(condition);


    MatrixXf input_x;
    MatrixXf condition_x;
    MatrixXf head_input_x;
    MatrixXf output_x; 

    input_x.resize(input.rows(),input.cols());
    input_x = input;
    condition_x.resize(condition.rows(),condition.cols());
    condition_x = condition;
    head_input_x.resize(head_input.rows(),head_input.cols());
    output_x.resize(output.rows(),output.cols());
    output_x.setZero();


    layer_t.set_num_frames_(FIXED_BUFFER_SIZE_T);
    layer.set_num_frames_(FIXED_BUFFER_SIZE_T);


    for (size_t i = 0; i < 1; ++i)
    {
        SetRandomInput(head_input);
        head_input_x = head_input;

        layer_t.process_(input,condition,head_input,output,512,0);

        layer.process_(input_x,condition_x,head_input_x,output_x,512,0);
        CompareOutputs(head_input,head_input_x);
        CompareOutputs(output,output_x);
    }
}

void Test_Layer()
{
    cout << "//// _Layer" << endl;

    Test_Layer<1,8,16,3>(false,2);

    Test_Layer<1,8,16,3>(true,1);
    Test_Layer<1,8,16,3>(false,1);
    Test_Layer<1,8,16,3>(false,1);
    Test_Layer<1,8,16,3>(true,2);

}

void TestDsp()
{
    cout << "//// DSP" << endl;

    std::vector<float> inputData = makeWeights(3000);

    namespace fs = std::filesystem;

    fs::path presetPath = "/var/pipedal/audio_uploads/NeuralAmpModels/Fender Twin Pack/Tim R Fender TwinVerb Norm Bright.nam";
    if (!fs::exists(presetPath)) return;



    std::unique_ptr<nam::DSP> dsp =  nam::get_dsp_ex(presetPath, 32,32);
    std::vector<float> dspOutput;
    dspOutput.resize(inputData.size());
    for (size_t i = 0; i+32 <= inputData.size(); i += 32)
    {
        dsp->process(inputData.data()+i,dspOutput.data()+i,32);
        dsp->finalize_(32);

    }

    std::unique_ptr<nam::DSP> originalDsp =  nam::get_dsp_ex(presetPath, -2,-2);
    std::vector<float> originalDspOutput;
    originalDspOutput.resize(inputData.size()); 
    for (size_t i = 0; i+32 <= inputData.size(); i += 32)
    {
        originalDsp->process(inputData.data()+i,originalDspOutput.data()+i,32);
        originalDsp->finalize_(32);
    }


    std::unique_ptr<nam::DSP> bufferedDsp =  nam::get_dsp_ex(presetPath, -1,-1);
    std::vector<float> bufferedDspOutput;
    bufferedDspOutput.resize(inputData.size());

    for (size_t i = 0; i+17 <= inputData.size(); i += 17)
    {
        bufferedDsp->process(inputData.data()+i,bufferedDspOutput.data()+i,17);
        bufferedDsp->finalize_(17);
    }


    for (size_t i = 0; i < inputData.size()-32; ++i)
    {
        gassert(approxEqual(dspOutput[i],originalDspOutput[i]));
    }

    for (size_t i = 0; i < inputData.size()-32-32; ++i)
    {
        gassert(approxEqual(originalDspOutput[i],bufferedDspOutput[i+FIXED_BUFFER_SIZE_T]));
    }

}

int main(void)
{
    cout << "WaveNet_T Unit Test" << endl;
    Test_DilatedConv();
    TestConv1D();
    TestConv1x1();
    Test_Layer();
    Test_LayerArray();
    TestDsp();
    cout << "//// " << endl;
    cout << "Success." << endl;
    return EXIT_SUCCESS;
}


