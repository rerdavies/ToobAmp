#pragma once

#include <algorithm>
#include <iostream>
#include <math.h>

// Prevent intellisense errors in VSCode, VStudio when compiling for aarch64
#if __INTELLISENSE__
#undef __ARM_NEON
#undef __ARM_NEON__
#undef __AVX__
#endif

#ifndef NOINLINE
#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif
#endif

#define WNT_ASSERT(x)                                    \
  if (!(x))                                              \
  {                                                      \
    std::cout << "Assert failed: " << (#x) << std::endl; \
    throw std::runtime_error(#x);                        \
  }

#include <Eigen/Dense>

template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
inline nam::wavenet::_DilatedConv_T<IN_ROWS, OUT_ROWS, OUT_COLUMNS, KERNEL_SIZE>::_DilatedConv_T(
    const int in_channels, const int out_channels, const int kernel_size,
    const int bias, const int dilation)
{
  WNT_ASSERT(in_channels == IN_ROWS);
  WNT_ASSERT(out_channels == OUT_ROWS);
  WNT_ASSERT(kernel_size == KERNEL_SIZE);

  this->set_size_(in_channels, out_channels, kernel_size, bias, dilation);
}


template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
inline void nam::wavenet::_DilatedConv_T<IN_ROWS, OUT_ROWS, OUT_COLUMNS, KERNEL_SIZE>::initialize(
      const int bias,
      const int dilation)
{
  this->set_size_(IN_ROWS, OUT_ROWS, KERNEL_SIZE, bias, dilation);
  
}


template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
nam::wavenet::_DilatedConv_T<IN_ROWS, OUT_ROWS, OUT_COLUMNS, KERNEL_SIZE>::_DilatedConv_T(
    const int bias,
    const int dilation) : _DilatedConv_T((int)IN_ROWS, (int)OUT_ROWS, (int)KERNEL_SIZE, bias, dilation)
{
}



//////////// _LayerArray_T /////////////////////////////

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>

inline void nam::wavenet::_Layer_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::process_(
    Eigen::Matrix<float, CHANNELS, Eigen::Dynamic> &input,
    const Eigen::Matrix<float, CONDITION_SIZE, FIXED_BUFFER_SIZE_T> &condition,
    Eigen::Matrix<float, CHANNELS, FIXED_BUFFER_SIZE_T> &head_input,
    Eigen::Matrix<float, CHANNELS, Eigen::Dynamic> &output,
    const long i_start,
    const long j_start)
{
  const long ncols = condition.cols();

  if (!this->_gated)
  {
    // Input dilated conv
    this->_conv_ungated.process_(input, this->_z_ungated, i_start, ncols, 0);
    // Mix-in condition
    this->_input_mixin_ungated.template process<FIXED_BUFFER_SIZE_T>(condition, _tmpMixin_ungated);
    this->_z_ungated += _tmpMixin_ungated;

    apply_activation<CHANNELS, FIXED_BUFFER_SIZE_T>(this->_activation, this->_z_ungated);

    head_input += this->_z_ungated;
    output.middleCols(j_start, ncols) = input.middleCols(i_start, ncols) + this->_1x1.template process<FIXED_BUFFER_SIZE_T>(this->_z_ungated);
  }
  else
  {

    // Input dilated conv
    this->_conv_gated.process_(input, this->_z_gated, i_start, ncols, 0);
    // Mix-in condition
    this->_input_mixin_gated.template process<FIXED_BUFFER_SIZE_T>(condition, _tmpMixin_gated);
    this->_z_gated += _tmpMixin_gated;

    constexpr int channels = (int)CHANNELS;

    // this->_activation->apply(this->_z_gated.topRows(channels));
    apply_activation_to_block(this->_activation, this->_z_gated.topRows(channels));

    // activations::Activation::get_activation("Sigmoid")->apply(this->_z.block(channels, 0, channels, this->_z.cols()));
    apply_activation_to_block(activations::Activation::get_activation("Sigmoid"), this->_z_gated.bottomRows(channels));

    this->_z_gated.topRows(channels).array() *= this->_z_gated.bottomRows(channels).array();
    // this->_z.topRows(channels) = this->_z.topRows(channels).cwiseProduct(
    //   this->_z.bottomRows(channels)
    // );

    head_input += this->_z_gated.topRows(channels);
    output.middleCols(j_start, ncols) = input.middleCols(i_start, ncols) + this->_1x1.template process_block<FIXED_BUFFER_SIZE_T, typeof(_z_gated.topRows(1))>(this->_z_gated.topRows(channels));
  }
}

#ifndef LAYER_ARRAY_BUFFER_SIZE
#define LAYER_ARRAY_BUFFER_SIZE 65536
#endif

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::initialize(
    const int input_size, const int condition_size, const int head_size,
    const int channels, const int kernel_size, const std::vector<int> &dilations,
    const std::string activation, const bool gated, const bool head_bias)
// : _rechannel(input_size, channels, false)
// , _head_rechannel(channels, head_size, head_bias)
{

  _rechannel.initialize(false);
  _head_rechannel.initialize(head_bias);
  ///

  this->_layers.resize(dilations.size());
  for (size_t i = 0; i < dilations.size(); i++)
    this->_layers[i].initialize(
        condition_size,
        channels,
        kernel_size,
        dilations[i],
        activation,
        gated);
  const long receptive_field = this->_get_receptive_field();
  for (size_t i = 0; i < dilations.size(); i++)
  {
    this->_layer_buffers.push_back(
        BufferMatrix());
    // Eigen::MatrixXf(channels, LAYER_ARRAY_BUFFER_SIZE + receptive_field - 1)
    _layer_buffers[i].resize(CHANNELS, LAYER_ARRAY_BUFFER_SIZE + receptive_field - 1);
    this->_layer_buffers[i].setZero();
  }
  _last_layer_buffer.resize(CHANNELS, FIXED_BUFFER_SIZE_T);
  _last_layer_buffer.setZero();

  this->_buffer_start = this->_get_receptive_field() - 1;
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_Layer_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::set_weights_(std::vector<float>::iterator &weights)
{
  if (_gated)
  {
    this->_conv_gated.set_weights_(weights);
    this->_input_mixin_gated.set_weights_(weights);
  }
  else
  {
    this->_conv_ungated.set_weights_(weights);
    this->_input_mixin_ungated.set_weights_(weights);
  }
  this->_1x1.set_weights_(weights);
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
void nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::_rewind_buffers_()
// Consider wrapping instead...
// Can make this smaller--largest dilation, not receptive field!
{
  const long start = this->_get_receptive_field() - 1;
  for (size_t i = 0; i < this->_layer_buffers.size(); i++)
  {
    const long d = (this->_layers[i].get_kernel_size() - 1) * this->_layers[i].get_dilation();
    this->_layer_buffers[i].middleCols(start - d, d) = this->_layer_buffers[i].middleCols(this->_buffer_start - d, d);
  }
  this->_buffer_start = start;
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::set_weights_(std::vector<float>::iterator &weights)
{
  this->_rechannel.set_weights_(weights);
  for (size_t i = 0; i < this->_layers.size(); i++)
    this->_layers[i].set_weights_(weights);
  this->_head_rechannel.set_weights_(weights);
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_Layer_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::set_num_frames_(const long num_frames)
{
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::advance_buffers_(const int num_frames)
{
  this->_buffer_start += num_frames;
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline long nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::_get_receptive_field() const
{
  // TODO remove this and use get_receptive_field() instead!
  long res = 1;
  for (size_t i = 0; i < this->_layers.size(); i++)
    res += (this->_layers[i].get_kernel_size() - 1) * this->_layers[i].get_dilation();
  return res;
}

// Always 1 less than _get_receptive_field ??
// template<size_t INPUT_SIZE,size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
// inline long nam::wavenet::_LayerArray_T<INPUT_SIZE,HEAD_SIZE,CHANNELS,KERNEL_SIZE>::get_receptive_field() const
// {
//   long result = 0;
//   for (size_t i = 0; i < this->_layers.size(); i++)
//     result += this->_layers[i].get_dilation() * (this->_layers[i].get_kernel_size() - 1);
//   return result;
// }

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::process_(
    Eigen::Matrix<float, INPUT_SIZE, FIXED_BUFFER_SIZE_T> &layer_inputs,
    const Eigen::Matrix<float, CONDITION_SIZE, FIXED_BUFFER_SIZE_T> &condition,
    Eigen::Matrix<float, CHANNELS, FIXED_BUFFER_SIZE_T> &head_inputs,
    Eigen::Matrix<float, CHANNELS, FIXED_BUFFER_SIZE_T> &layer_outputs,
    Eigen::Matrix<float, HEAD_SIZE, FIXED_BUFFER_SIZE_T> &head_outputs)
{
  Eigen::Matrix<float, CHANNELS, FIXED_BUFFER_SIZE_T> tInput;
  this->_rechannel.template process<FIXED_BUFFER_SIZE_T>(layer_inputs, tInput);
  this->_layer_buffers[0].middleCols(this->_buffer_start, layer_inputs.cols()) = tInput;
  if (this->_layers.size() == 1)
  {
    throw std::runtime_error("Not implemented");
    // this->_layers[0].process_(
    //     this->_layer_buffers[0],
    //     condition,
    //     head_inputs,
    //     layer_outputs,
    //     this->_buffer_start,
    //     0);
  }
  else
  {
    size_t lastLayer = this->_layers.size() - 1;
    for (size_t i = 0; i < lastLayer; ++i)
    {
      this->_layers[i].process_(
          this->_layer_buffers[i],
          condition,
          head_inputs,
          this->_layer_buffers[i + 1],
          this->_buffer_start,
          this->_buffer_start);
    }
    this->_layers[lastLayer].process_(
        this->_layer_buffers[lastLayer],
        condition,
        head_inputs,
        _last_layer_buffer,
        this->_buffer_start,
        0);
  }
  WNT_ASSERT(layer_outputs.cols() == _last_layer_buffer.cols());
  layer_outputs = _last_layer_buffer;
  this->_head_rechannel.template process<FIXED_BUFFER_SIZE_T>(head_inputs, head_outputs);
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::prepare_for_frames_(const long num_frames)
{
  // Example:
  // _buffer_start = 0
  // num_frames = 64
  // buffer_size = 64
  // -> this will write on indices 0 through 63, inclusive.
  // -> No illegal writes.
  // -> no rewind needed.
  if (this->_buffer_start + num_frames > this->_get_buffer_size())
    this->_rewind_buffers_();
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::set_num_frames_(const long num_frames)
{
  // Wavenet checks for unchanged num_frames; if we made it here, there's
  // something to do.
  if (LAYER_ARRAY_BUFFER_SIZE - num_frames < this->_get_receptive_field())
  {
    std::stringstream ss;
    ss << "Asked to accept a buffer of " << num_frames << " samples, but the buffer is too short ("
       << LAYER_ARRAY_BUFFER_SIZE << ") to get out of the recptive field (" << this->_get_receptive_field()
       << "); copy errors could occur!\n";
    throw std::runtime_error(ss.str().c_str());
  }
  for (size_t i = 0; i < this->_layers.size(); i++)
    this->_layers[i].set_num_frames_(num_frames);
}

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline long nam::wavenet::_LayerArray_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::_get_channels() const
{
  return this->_layers.size() > 0 ? this->_layers[0].get_channels() : 0;
}

// Head =======================================================================

inline nam::wavenet::_Head_T::_Head_T(const int input_size, const int num_layers, const int channels, const std::string activation)
    : _channels(channels), _head(num_layers > 0 ? channels : input_size, 1, true), _activation(activations::Activation::get_activation(activation))
{
  WNT_ASSERT(num_layers > 0);
  int dx = input_size;
  for (int i = 0; i < num_layers; i++)
  {
    this->_layers.push_back(Conv1x1(dx, i == num_layers - 1 ? 1 : channels, true));
    dx = channels;
    if (i < num_layers - 1)
      this->_buffers.push_back(Eigen::MatrixXf());
  }
}

inline void nam::wavenet::_Head_T::set_weights_(std::vector<float>::iterator &weights)
{
  for (size_t i = 0; i < this->_layers.size(); i++)
    this->_layers[i].set_weights_(weights);
}

inline void nam::wavenet::_Head_T::process_(Eigen::MatrixXf &inputs, Eigen::MatrixXf &outputs)
{
  const size_t num_layers = this->_layers.size();
  this->_apply_activation_(inputs);
  if (num_layers == 1)
  {
    this->_layers[0].process(inputs, outputs);
  }
  else
  {
    this->_layers[0].process(inputs, this->_buffers[0]);
    for (size_t i = 1; i < num_layers; i++)
    { // WNT_ASSERTed > 0 layers
      this->_apply_activation_(this->_buffers[i - 1]);
      if (i < num_layers - 1)
        this->_layers[i].process(this->_buffers[i - 1], this->_buffers[i]);
      else
        this->_layers[i].process(this->_buffers[i - 1], outputs);
    }
  }
}

inline void nam::wavenet::_Head_T::set_num_frames_(const long num_frames)
{
  for (size_t i = 0; i < this->_buffers.size(); i++)
  {
    if (this->_buffers[i].rows() == this->_channels && this->_buffers[i].cols() == num_frames)
      continue; // Already has correct size
    this->_buffers[i].resize(this->_channels, num_frames);
    this->_buffers[i].setZero();
  }
}

inline void nam::wavenet::_Head_T::_apply_activation_(Eigen::MatrixXf &x)
{
  this->_activation->apply(x);
}

// WaveNet_T ====================================================================


template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>::set_weights_(std::vector<float> &weights)
{
  std::vector<float>::iterator it = weights.begin();
  _layer_array_0.set_weights_(it);
  _layer_array_1.set_weights_(it);

  // this->_head.set_params_(it);
  this->_head_scale = *(it++);
  if (it != weights.end())
  {
    std::stringstream ss;
    for (size_t i = 0; i < weights.size(); i++)
      if (weights[i] == *it)
      {
        ss << "Weight mismatch: assigned " << i + 1 << " weights, but " << weights.size() << " were provided.";
        throw std::runtime_error(ss.str().c_str());
      }
    ss << "Weight mismatch: provided " << weights.size() << " weights, but the model expects more.";
    throw std::runtime_error(ss.str().c_str());
  }
}

template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>::_advance_buffers_(const int num_frames)
{
  _layer_array_0.advance_buffers_(num_frames);
  _layer_array_1.advance_buffers_(num_frames);
}

template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>::_prepare_for_frames_(const long num_frames)
{
  _layer_array_0.prepare_for_frames_(num_frames);
  _layer_array_1.prepare_for_frames_(num_frames);
}

template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>::_set_condition_array(NAM_SAMPLE *input, const int num_frames)
{
  for (int j = 0; j < num_frames; j++)
  {
    this->_condition(0, j) = input[j];
  }
}

template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
NOINLINE inline void nam::wavenet::WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>::process_frame(NAM_SAMPLE *input, NAM_SAMPLE *output)
{
  this->_set_num_frames_(FIXED_BUFFER_SIZE_T);
  this->_prepare_for_frames_(FIXED_BUFFER_SIZE_T);
  this->_set_condition_array(input, FIXED_BUFFER_SIZE_T);

  // Main layer arrays:
  // Layer-to-layer
  // Sum on head output
  this->_head_0.setZero();

  _layer_array_0.process_(
      this->_condition,
      this->_condition,
      this->_head_0,
      this->_layer_array_output_0,
      this->_head_1);
  _layer_array_1.process_(
      this->_layer_array_output_0,
      this->_condition,
      this->_head_1,
      this->_layer_array_output_1,
      _head_2);

  // this->_head.process_(
  //   this->_head_input,
  //   this->_head_output
  //);
  //  Copy to required output array
  //  Hack: apply head scale here; revisit when/if I activate the head.
  //  WNT_ASSERT(this->_head_output.rows() == 1);

  WNT_ASSERT(this->_head_2.rows() == 1);

  for (int s = 0; s < FIXED_BUFFER_SIZE_T; s++)
  {
    float out = this->_head_scale * _head_2(0, s);
    output[s] = out;
  }
  _advance_buffers_(FIXED_BUFFER_SIZE_T);
}

template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>::process(NAM_SAMPLE *input, NAM_SAMPLE *output, const int num_frames)
{
  int frames = num_frames;

  if ((frames & (frames - 1)) != 0 || frames < 32) // guard against hosts who misrepresent what they're going to do.
  {
    _no_buffer_required = false;
  }
  if (_no_buffer_required)
  {
    // i/o is always a multiple of FIXED_BUFFER_SIZE_T: process in-place with no delay.
    while (frames != 0)
    {
      process_frame(input, output);

      input += FIXED_BUFFER_SIZE_T;
      output += FIXED_BUFFER_SIZE_T;
      frames -= FIXED_BUFFER_SIZE_T;
    }
  }
  else
  {
    // i/o is avariable, or not a multiple of FIXED_BUFFER_SIZE_T: assemble data into buffers of FIXED_BUFFER_SIZE_T
    // introduces FIXED_BUFFER_SIZE_T samples of delay.
    while (frames != 0)
    {
      int this_time = std::min(frames, (int)(FIXED_BUFFER_SIZE_T - bufferIndex));
      if (this_time == 0)
      {
        process_frame(input_buffer, output_buffer);
        bufferIndex = 0;
        this_time = std::min(frames, (int)FIXED_BUFFER_SIZE_T);
      }
      for (int i = 0; i < this_time; ++i)
      {
        input_buffer[bufferIndex] = *input++;
        *output++ = output_buffer[bufferIndex];
        ++bufferIndex;
      }
      frames -= this_time;
    }
  }
}

template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>::_set_num_frames_(const long num_frames)
{
  if (num_frames == this->_num_frames)
    return;

  // this->_condition.resize(this->_get_condition_dim(), num_frames);

  // _head_0.resize(_head_0.rows(),num_frames);
  // _head_1.resize(_head_1.rows(),num_frames);
  // _head_2.resize(_head_2.rows(),num_frames);

  // _layer_array_output_0.resize(_layer_array_output_0.rows(),num_frames);
  // _head_output.resize(this->_head_output.rows(), num_frames);
  this->_head_output.setZero();

  this->_layer_array_0.set_num_frames_(num_frames);
  this->_layer_array_1.set_num_frames_(num_frames);

  // this->_head.set_num_frames_(num_frames);
  this->_num_frames = num_frames;
}

template <size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline nam::wavenet::WaveNet_T<HEAD_SIZE, CHANNELS, KERNEL_SIZE>::WaveNet_T(const std::vector<nam::wavenet::LayerArrayParams> &layer_array_params,
                                                                            const float head_scale, const bool with_head, std::vector<float> weights,
                                                                            const double expected_sample_rate, bool no_buffer_required)
    : DSP(expected_sample_rate), _num_frames(0), _head_scale(head_scale), _no_buffer_required(no_buffer_required)
{
  for (size_t i = 0; i < FIXED_BUFFER_SIZE_T; ++i)
  {
    input_buffer[i] = 0;
    output_buffer[i] = 0;
  }
  bufferIndex = 0;

  _layer_array_output_0.resize(CHANNELS, FIXED_BUFFER_SIZE_T);
  _layer_array_output_1.resize(LayerArray1_T::Channels, FIXED_BUFFER_SIZE_T);

  if (with_head)
    throw std::runtime_error("Head not implemented!");

  _layer_array_0.initialize(
      layer_array_params[0].input_size, layer_array_params[0].condition_size, layer_array_params[0].head_size,
      layer_array_params[0].channels, layer_array_params[0].kernel_size, layer_array_params[0].dilations,
      layer_array_params[0].activation, layer_array_params[0].gated, layer_array_params[0].head_bias);

  _layer_array_1.initialize(
      layer_array_params[1].input_size, layer_array_params[1].condition_size, layer_array_params[1].head_size,
      layer_array_params[1].channels, layer_array_params[1].kernel_size, layer_array_params[1].dilations,
      layer_array_params[1].activation, layer_array_params[1].gated, layer_array_params[1].head_bias);

  this->set_weights_(weights);

  mPrewarmSamples = 1;
  mPrewarmSamples += _layer_array_0._get_receptive_field() + 1;
  mPrewarmSamples += _layer_array_1._get_receptive_field() + 1;

  // round up to next fixed-buffer boundarary.
  mPrewarmSamples += (mPrewarmSamples + FIXED_BUFFER_SIZE_T - 1) / FIXED_BUFFER_SIZE_T * FIXED_BUFFER_SIZE_T;

}

/////////////////////////////////////////////////

template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
inline void nam::wavenet::Conv1D_T<IN_ROWS, OUT_ROWS, OUT_COLUMNS, KERNEL_SIZE>::set_weights_(
    std::vector<float>::iterator &weights)
{
  if (this->_weight.size() > 0)
  {
    const long out_channels = this->_weight[0].rows();
    const long in_channels = this->_weight[0].cols();
    // Crazy ordering because that's how it gets flattened.
    for (auto i = 0; i < out_channels; i++)
      for (auto j = 0; j < in_channels; j++)
        for (size_t k = 0; k < this->_weight.size(); k++)
          this->_weight[k](i, j) = *(weights++);
  }
  if (_do_bias)
  {
    for (long i = 0; i < this->_bias.size(); i++)
      this->_bias(i) = *(weights++);
  }
}

template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
inline void nam::wavenet::Conv1D_T<IN_ROWS, OUT_ROWS, OUT_COLUMNS, KERNEL_SIZE>::set_size_(
    const int in_channels, const int out_channels, const int kernel_size, const bool do_bias,
    const int _dilation)
{
  WNT_ASSERT(in_channels == IN_ROWS && out_channels == OUT_ROWS);
  WNT_ASSERT(kernel_size == KERNEL_SIZE);
  WNT_ASSERT(_weight.size() == KERNEL_SIZE);
  WNT_ASSERT(_weight[0].rows() == out_channels);
  WNT_ASSERT(_weight[0].cols() == in_channels);

  // this->_weight.resize(KERNEL_SIZE);
  // for (size_t i = 0; i < this->_weight.size(); i++)
  //   this->_weight[i].resize(out_channels,
  //                           in_channels); // y = Ax, input array (C,L)
  this->_do_bias = do_bias;
  this->_dilation = _dilation;
}

template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::Conv1D_T<IN_ROWS, OUT_ROWS, OUT_CHANNELS, KERNEL_SIZE>::set_size_and_weights_(const int in_channels, const int out_channels, const int kernel_size,
                                                                                                        const int _dilation, const bool do_bias, std::vector<float>::iterator &weights)
{
  this->set_size_(in_channels, out_channels, kernel_size, do_bias, _dilation);
  this->set_weights_(weights);
}

template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
inline void nam::wavenet::Conv1D_T<IN_ROWS, OUT_ROWS, OUT_COLUMNS, KERNEL_SIZE>::process_(
    const Eigen::MatrixXf &input,
    Eigen::Matrix<float, OUT_ROWS, OUT_COLUMNS> &output, const long i_start, const long ncols,
    const long j_start) const
{
  WNT_ASSERT(input.rows() == IN_ROWS);
  WNT_ASSERT(ncols == OUT_COLUMNS);
  WNT_ASSERT(j_start == 0);

  // This is the clever part ;)
  for (size_t k = 0; k < this->_weight.size(); k++)
  {
    const long offset = this->_dilation * (k + 1 - this->_weight.size());
    if (k == 0)
      output = this->_weight[k] * input.middleCols(i_start + offset, ncols);
    else
      output += this->_weight[k] * input.middleCols(i_start + offset, ncols);
  }
  if (_do_bias > 0)
    // output.middleCols(j_start, ncols).colwise() += this->_bias;
    output.colwise() += this->_bias;
}

template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
inline void nam::wavenet::Conv1D_T<IN_ROWS, OUT_ROWS, OUT_COLUMNS, KERNEL_SIZE>::process_(
    const Eigen::Matrix<float, IN_ROWS, Eigen::Dynamic> &input,
    Eigen::Matrix<float, OUT_ROWS, OUT_COLUMNS> &output, const long i_start, const long ncols,
    const long j_start) const
{
  WNT_ASSERT(ncols == OUT_COLUMNS);
  WNT_ASSERT(j_start == 0);
  // This is the clever part ;)
  for (size_t k = 0; k < this->_weight.size(); k++)
  {
    const long offset = this->_dilation * (k + 1 - this->_weight.size());
    if (k == 0)
      output = this->_weight[k] * input.middleCols(i_start + offset, ncols);
    else
      output += this->_weight[k] * input.middleCols(i_start + offset, ncols);
  }
  if (_do_bias > 0)
    output.colwise() += this->_bias;
}

template <size_t IN_ROWS, size_t OUT_ROWS, size_t OUT_COLUMNS, size_t KERNEL_SIZE>
inline long nam::wavenet::Conv1D_T<IN_ROWS, OUT_ROWS, OUT_COLUMNS, KERNEL_SIZE>::get_num_weights() const
{
  long num_weights = _do_bias ? this->_bias.size() : 0;
  for (size_t i = 0; i < this->_weight.size(); i++)
    num_weights += this->_weight[i].size();
  return num_weights;
}

///////////////// _Layer_T /////////////////////////

template <size_t INPUT_SIZE, size_t HEAD_SIZE, size_t CHANNELS, size_t KERNEL_SIZE>
inline void nam::wavenet::_Layer_T<INPUT_SIZE, HEAD_SIZE, CHANNELS, KERNEL_SIZE>::initialize(
    const int condition_size, const int channels, const int kernel_size, const int dilation,
    const std::string activation, const bool gated)
{
  WNT_ASSERT(condition_size == CONDITION_SIZE);
  WNT_ASSERT(channels == CHANNELS);
  WNT_ASSERT(kernel_size == KERNEL_SIZE);

  this->_dilation = dilation;
  this->_gated = gated;
  this->_activation = activations::Activation::get_activation(activation);
  this->_conv_gated.initialize(true,dilation);
  this->_conv_ungated.initialize(true,dilation);

}

///////// Conv1x1_T ////////////////

template <size_t IN_CHANNELS, size_t OUT_CHANNELS>
inline void nam::wavenet::Conv1x1_T<IN_CHANNELS, OUT_CHANNELS>::initialize(const int in_channels, const int out_channels, const bool _bias)
{
  this->_do_bias = _bias;

  WNT_ASSERT(in_channels == IN_CHANNELS && out_channels == OUT_CHANNELS);
  WNT_ASSERT(_weight.rows() == OUT_CHANNELS && _weight.cols() == IN_CHANNELS);
  // // this->_weight.resize(out_channels, in_channels);
  // if (_bias)
  //   this->_bias.resize(out_channels);
}

template <size_t IN_CHANNELS, size_t OUT_CHANNELS>
inline void nam::wavenet::Conv1x1_T<IN_CHANNELS, OUT_CHANNELS>::set_weights_(std::vector<float>::iterator &weights)
{
  for (int i = 0; i < this->_weight.rows(); i++)
    for (int j = 0; j < this->_weight.cols(); j++)
      this->_weight(i, j) = *(weights++);
  if (this->_do_bias)
    for (int i = 0; i < this->_bias.size(); i++)
      this->_bias(i) = *(weights++);
}

