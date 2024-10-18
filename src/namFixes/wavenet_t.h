#pragma once

/*
  Reimplments the Wavenet NAM model without using dynamic buffers.

  Yeilds ~15% performance improvement.
*/

#if __INTELLISENSE__
  #undef __ARM_NEON
  #undef __ARM_NEON__
#endif

#include <string>
#include <vector>
#include <array>

#include "json.hpp"
#include <Eigen/Dense>

#include "NAM/dsp.h"

// Prevent intellisense errors in VSCode, VStudio when compiling for aarch64


namespace nam
{
namespace wavenet
{

constexpr size_t FIXED_BUFFER_SIZE_T = 32;

// Rework the initialization API slightly. Merge w/ dsp.h later.


template<size_t ROWS, size_t COLUMNS>
void apply_activation(activations::Activation*activation,Eigen::Matrix<float,ROWS,COLUMNS>&matrix)
{
  activation->apply(matrix.data(),matrix.size());
}
template<typename T>
void apply_activation_to_block(activations::Activation*activation,T block) 
{

  for (int c = 0; c < block.cols(); ++c)
  {
    float *mem = &block.coeffRef(0,c);
    activation->apply(mem,block.rows());
  }
}

template <size_t IN_CHANNELS, size_t OUT_CHANNELS>
class Conv1x1_T
{
public:


  Conv1x1_T(): _do_bias(false) {}
  Conv1x1_T(const bool _bias) : _do_bias(_bias) { }

  void initialize(const bool _bias) { _do_bias = _bias; }
  void initialize(const int in_channels, const int out_channels, const bool _bias);
  
  void set_weights_(std::vector<float>::iterator& weights);

  long get_out_channels() const { return OUT_CHANNELS; };

  template <size_t IN_COLS>
  void process(const Eigen::Matrix<float, IN_CHANNELS, IN_COLS>& input,
               Eigen::Matrix<float, OUT_CHANNELS, IN_COLS>& output)
  {
    if (this->_do_bias)
    {
      // (this->_weight * input).colwise() + this->_bias (with no temporary allocations)
      Eigen::Matrix<float, OUT_CHANNELS, IN_COLS> _tmpMul = (this->_weight * input);

      output.noalias() = _tmpMul.colwise() + this->_bias;
    }
    else {
      output.noalias() = this->_weight * input;
    }
  }
  template <size_t IN_COLUMNS, typename T>
  Eigen::Matrix<float, OUT_CHANNELS, IN_COLUMNS> process_block(
    T input) const // yyy remove me
  {
    if (this->_do_bias)
    {
      // (this->_weight * input).colwise() + this->_bias (with no temporary allocations)
      Eigen::Matrix<float, OUT_CHANNELS, IN_COLUMNS> _tmpMul = (this->_weight * input);

      return _tmpMul.colwise() + this->_bias;
    }
    else {
      return this->_weight * input;
    }
    
  }


  template <size_t IN_COLUMNS>
  Eigen::Matrix<float, OUT_CHANNELS, IN_COLUMNS> process(
    const Eigen::Matrix<float, IN_CHANNELS, IN_COLUMNS>& input) const // yyy remove me
  {
    if (this->_do_bias)
    {
      // (this->_weight * input).colwise() + this->_bias (with no temporary allocations)
      Eigen::Matrix<float, OUT_CHANNELS, IN_COLUMNS> _tmpMul = (this->_weight * input);

      return _tmpMul.colwise() + this->_bias;
    }
    else {
      return this->_weight * input;
    }
    
  }
private:
  // this->_weight.resize(out_channels, in_channels);
  Eigen::Matrix<float, OUT_CHANNELS, IN_CHANNELS> _weight;
  //   this->_bias.resize(out_channels);
  Eigen::Vector<float, OUT_CHANNELS> _bias;
  bool _do_bias;
};

template <size_t IN_ROWS,size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
class Conv1D_T
{
public:
  Conv1D_T() { this->_dilation = 1; };
  void set_weights_(std::vector<float>::iterator& weights);
  void set_size_(const int in_channels, const int out_channels, const int kernel_size, const bool do_bias,
                 const int _dilation);
  void set_size_and_weights_(
    const int in_channels, const int out_channels, const int kernel_size, const int _dilation,
    const bool do_bias, std::vector<float>::iterator& weights);
  // Process from input to output
  //  Rightmost indices of input go from i_start for ncols,
  //  Indices on output for from j_start (to j_start + ncols - i_start)
  void process_(  
    const Eigen::Matrix<float,IN_ROWS,Eigen::Dynamic>& input, 
    Eigen::Matrix<float,OUT_ROWS,OUT_COLUMNS>& output, 
    const long i_start, 
    const long ncols,
    const long j_start) const;
  void process_(
      const Eigen::MatrixXf& input, 
      Eigen::Matrix<float,OUT_ROWS,OUT_COLUMNS>& output, 
      const long i_start, 
      const long ncols,
      const long j_start) const;
  long get_in_channels() const { return IN_ROWS; };
  long get_kernel_size() const { return KERNEL_SIZE; };
  long get_num_weights() const;
  long get_out_channels() const { return OUT_ROWS; };
  int get_dilation() const { return this->_dilation; };

private:
  // Gonna wing this...
  // conv[kernel](cout, cin)
  std::array<Eigen::Matrix<float,OUT_ROWS,IN_ROWS>,KERNEL_SIZE> _weight;
  bool _do_bias = false;
  Eigen::Vector<float,OUT_ROWS> _bias;
  int _dilation;
};


template <size_t IN_ROWS,size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
class _DilatedConv_T : public Conv1D_T<IN_ROWS,OUT_ROWS,OUT_COLUMNS,KERNEL_SIZE>
{
public:
  _DilatedConv_T() { }
  _DilatedConv_T(
    const int bias,
    const int dilation
  );
  _DilatedConv_T(
    const int in_channels, 
    const int out_channels, 
    const int kernel_size, 
    const int bias,
    const int dilation);
};

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
class _Layer_T
{
public:
  static constexpr size_t CONDITION_SIZE = 1;

  _Layer_T() {}
  void initialize(const int condition_size, const int channels, const int kernel_size, const int dilation,
                  const std::string activation, const bool gated);
  void set_weights_(std::vector<float>::iterator& weights);
  // :param `input`: from previous layer
  // :param `output`: to next layer
  void process_(
    Eigen::Matrix<float,CHANNELS,Eigen::Dynamic>& input, 
    const Eigen::Matrix<float,CONDITION_SIZE,FIXED_BUFFER_SIZE_T>& condition, 
    Eigen::Matrix<float,CHANNELS,FIXED_BUFFER_SIZE_T>& head_input,
    Eigen::Matrix<float,CHANNELS,Eigen::Dynamic>& output, 
    const long i_start, 
    const long j_start);
  void set_num_frames_(const long num_frames);
  long get_channels() const { return _gated ? this->_conv_gated.get_in_channels() : this->_conv.get_in_channels(); };
  int get_dilation() const { return _dilation; };
  long get_kernel_size() const { return KERNEL_SIZE; };

private:
  int _dilation;


  Eigen::Matrix<float,CHANNELS*2,FIXED_BUFFER_SIZE_T> _tmpMixin_gated;
  Eigen::Matrix<float,CHANNELS,FIXED_BUFFER_SIZE_T> _tmpMixin_ungated;
  // Eigen::MatrixXf _tmpConv1x1;
  // Eigen::MatrixXf _tmpTopRows;


  // : _conv(channels, gated ? 2 * channels : channels, kernel_size, true, dilation)
  //   , _input_mixin(condition_size, gated ? 2 * channels : channels, false)
  //   , _1x1(channels, channels, true)
  //   , _activation(activations::Activation::get_activation(activation))
  //   , _gated(gated){};

  bool _gated = false;
  // The dilated convolution at the front of the block
  // : _conv(channels, gated ? 2 * channels : channels, kernel_size, true, dilation)
  _DilatedConv_T<CHANNELS,CHANNELS, FIXED_BUFFER_SIZE_T, KERNEL_SIZE> _conv_ungated{true,1};
  _DilatedConv_T<CHANNELS,CHANNELS*2,FIXED_BUFFER_SIZE_T, KERNEL_SIZE> _conv_gated{true,1};
  // Input mixin
  //   _input_mixin(condition_size, gated ? 2 * channels : channels, false)
  Conv1x1_T<CONDITION_SIZE,CHANNELS> _input_mixin_ungated{false};
  Conv1x1_T<CONDITION_SIZE, CHANNELS*2> _input_mixin_gated{false};

  // The post-activation 1x1 convolution
  //   , _1x1(channels, channels, true)
  Conv1x1_T<CHANNELS,CHANNELS> _1x1 {true};
  // The internal state
  Eigen::Matrix<float,CHANNELS*2,FIXED_BUFFER_SIZE_T> _z_gated;
  Eigen::Matrix<float,CHANNELS,FIXED_BUFFER_SIZE_T> _z_ungated;
  

  activations::Activation* _activation = nullptr;
};

// An array of layers with the same channels, kernel sizes, activations.

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
class _LayerArray_T
{
public:
  static constexpr size_t CONDITION_SIZE = 1;

  static constexpr size_t LayerArrayBufferSize = 65536;

  static constexpr size_t InputSize = INPUT_SIZE;
  static constexpr size_t HeadSize = HEAD_SIZE;
  static constexpr size_t Channels = CHANNELS;
  static constexpr size_t KernelSize = KERNEL_SIZE;

  _LayerArray_T() {}
  void initialize(const int input_size, const int condition_size, const int head_size, const int channels,
                  const int kernel_size, const std::vector<int>& dilations, const std::string activation,
                  const bool gated, const bool head_bias);

  void advance_buffers_(const int num_frames);

  // Preparing for frames:
  // Rewind buffers if needed
  // Shift index to prepare
  //
  void prepare_for_frames_(const long num_frames);

  // All arrays are "short".
  void process_( //yyx
    Eigen::Matrix<float,INPUT_SIZE,FIXED_BUFFER_SIZE_T>& layer_inputs, 
    const Eigen::Matrix<float,CONDITION_SIZE,FIXED_BUFFER_SIZE_T>& condition,
    Eigen::Matrix<float,CHANNELS,FIXED_BUFFER_SIZE_T>& head_inputs, 
    Eigen::Matrix<float,CHANNELS,FIXED_BUFFER_SIZE_T>& layer_outputs,
    Eigen::Matrix<float,HEAD_SIZE,FIXED_BUFFER_SIZE_T>& head_outputs
  );
  void set_num_frames_(const long num_frames);
  void set_weights_(std::vector<float>::iterator& it);

  // "Zero-indexed" receptive field.
  // E.g. a 1x1 convolution has a z.i.r.f. of zero.
  //long get_receptive_field() const;

  inline long _get_receptive_field() const;

private:


  std::vector<_Layer_T<INPUT_SIZE,HEAD_SIZE, CHANNELS, KERNEL_SIZE>> _layers;


  long _buffer_start;
  // The rechannel before the layers
  // : _rechannel(input_size, channels, false)
  Conv1x1_T<INPUT_SIZE,CHANNELS> _rechannel{false};

  // Buffers in between layers.
  // buffer [i] is the input to layer [i].
  // the last layer outputs to a short array provided by outside.
  using BufferMatrix = Eigen::Matrix<float,CHANNELS,Eigen::Dynamic>;

  std::vector<BufferMatrix> _layer_buffers;
  BufferMatrix _last_layer_buffer;
  // The layer objects

  Eigen::MatrixXf _tmpConv1x1Process;
  Eigen::MatrixXf _tmpHeadProcess;

  // Rechannel for the head`
  // , _head_rechannel(channels, head_size, head_bias)

  Conv1x1_T<CHANNELS,HEAD_SIZE> _head_rechannel;

  long _get_buffer_size() const { return this->_layer_buffers.size() > 0 ? this->_layer_buffers[0].cols() : 0; };
  long _get_channels() const;
  // "One-indexed" receptive field
  // TODO remove!
  // E.g. a 1x1 convolution has a o.i.r.f. of one.
  void _rewind_buffers_();
};

// The head module
// [Act->Conv] x L
class _Head_T
{
public:
  _Head_T(const int input_size, const int num_layers, const int channels, const std::string activation);
  void set_weights_(std::vector<float>::iterator& weights);
  // NOTE: the head transforms the provided input by applying a nonlinearity
  // to it in-place!
  void process_(Eigen::MatrixXf& inputs, Eigen::MatrixXf& outputs);
  void set_num_frames_(const long num_frames);

private:
  int _channels;
  std::vector<Conv1x1> _layers;
  Conv1x1 _head;
  activations::Activation* _activation;

  // Stores the outputs of the convs *except* the last one, which goes in
  // The array `outputs` provided to .process_()
  std::vector<Eigen::MatrixXf> _buffers;

  // Apply the activation to the provided array, in-place
  void _apply_activation_(Eigen::MatrixXf& x);
};

// The main WaveNet model
template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE = 3>
class WaveNet_T : public DSP
{
public:
  static constexpr size_t CONDITION_SIZE = 1;

  WaveNet_T(const std::vector<LayerArrayParams>& layer_array_params, const float head_scale, const bool with_head,
            std::vector<float> weights, const double expected_sample_rate = -1.0,bool noBufferingRequired= false);
  ~WaveNet_T() = default;

  void finalize_(const int num_frames) override;
  void set_weights_(std::vector<float>& weights);

private:
  bool _no_buffer_required = false;
  long _num_frames = 0;;

  int bufferIndex = 0;
  float input_buffer[FIXED_BUFFER_SIZE_T];
  float output_buffer[FIXED_BUFFER_SIZE_T];


  using LayerArray0_T = _LayerArray_T<CONDITION_SIZE,HEAD_SIZE, CHANNELS, KERNEL_SIZE>;
  using LayerArray1_T = _LayerArray_T<CHANNELS, CONDITION_SIZE, HEAD_SIZE,KERNEL_SIZE>;

  // Their outputs

  Eigen::Matrix<float, CHANNELS, FIXED_BUFFER_SIZE_T> _layer_array_output_0;
  Eigen::Matrix<float, LayerArray1_T::Channels,FIXED_BUFFER_SIZE_T> _layer_array_output_1;

  // Head _head;


  LayerArray0_T _layer_array_0;
  LayerArray1_T _layer_array_1;

  // Element-wise arrays:
  Eigen::Matrix<float,1,FIXED_BUFFER_SIZE_T> _condition;
  // One more than total layer arrays
  Eigen::Matrix<float, LayerArray0_T::Channels, FIXED_BUFFER_SIZE_T> _head_0;
  Eigen::Matrix<float, LayerArray0_T::HeadSize, FIXED_BUFFER_SIZE_T> _head_1;
  Eigen::Matrix<float, LayerArray1_T::HeadSize, FIXED_BUFFER_SIZE_T> _head_2;

  float _head_scale;
  Eigen::Matrix<float, 1, FIXED_BUFFER_SIZE_T> _head_output;

  void _advance_buffers_(const int num_frames);
  void _prepare_for_frames_(const long num_frames);
  void process(NAM_SAMPLE* input, NAM_SAMPLE* output, const int num_frames) override;

  void process_frame(NAM_SAMPLE* input, NAM_SAMPLE* output);

  virtual int _get_condition_dim() const { return 1; };
  // Fill in the "condition" array that's fed into the various parts of the net.
  virtual void _set_condition_array(NAM_SAMPLE* input, const int num_frames);
  // Ensure that all buffer arrays are the right size for this num_frames
  void _set_num_frames_(const long num_frames);
};

template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE = 3>
class WaveNetFactory_T
{
public:
  bool matches(std::vector<wavenet::LayerArrayParams>& layerParams)
  {
    if (layerParams.size() != 2)
      return false;
    if (layerParams[0].input_size != 1 || layerParams[0].condition_size != 1 || layerParams[0].head_size != HEAD_SIZE
        || layerParams[0].channels != CHANNELS || layerParams[0].kernel_size != KERNEL_SIZE)
    {
      return false;
    }
    if (layerParams[1].input_size != CHANNELS || layerParams[1].condition_size != 1 || layerParams[1].head_size != 1
        || layerParams[1].channels != HEAD_SIZE || layerParams[1].kernel_size != KERNEL_SIZE)
    {
      return false;
    }
    return true;
  }
  static const size_t head_size = HEAD_SIZE;
  static const size_t channels = CHANNELS;
  static const size_t kernel_size = KERNEL_SIZE;


  std::unique_ptr<DSP> create(const std::vector<wavenet::LayerArrayParams>& layer_array_params, float head_scale,
                              bool with_head, const std::vector<float>& weights, double expected_sample_rate, bool noPageFlipRequired)
  {
    return std::make_unique<WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>>(
      layer_array_params, head_scale, with_head, weights, expected_sample_rate,noPageFlipRequired);
  }
};
}; // namespace wavenet
}; // namespace nam


#include "wavenet_t.inl.h"
