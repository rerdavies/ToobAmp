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


#include "ToobML.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"

#include <exception>
#include <fstream>
#include <stdbool.h>
#include <filesystem>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <dlfcn.h>
#endif

using namespace std;
using namespace toob;

#pragma GCC diagnostic push


#pragma GCC diagnostic ignored "-Wunknown-warning-option" //clang
#pragma GCC diagnostic ignored "-Waggressive-loop-optimizations"

#include "NeuralModel.h"

#pragma GCC diagnostic push

static constexpr float MODEL_FADE_RATE = 0.2f; // seconds.
static constexpr float MASTER_DEZIP_RATE = 0.1f; // seconds.
static constexpr float GAIN_DEZIP_RATE = 0.1f; // seconds.
static const int MAX_UPDATES_PER_SECOND = 10;
static constexpr double TRIMOUT_UPDATE_RATE_S = 0.1; // seconds.

const char* ToobML::URI= TOOB_ML_URI;

#include <limits>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include "RTNeural/RTNeural.h"
#pragma GCC diagnostic pop

namespace toob {


class MLException: public std::exception {
	std::string message;
public:
	MLException(const std::string&message)
	{
		this->message = message;
	}
	const char*what() const noexcept {
		return this->message.c_str();
	}
};
class ToobMlModel {
protected:
	ToobMlModel() { };
public:
	virtual ~ToobMlModel() {}
	static ToobMlModel*  Load(const std::string&fileName);
	virtual void Reset() = 0;
	virtual void Process(int numSamples,const float*input, float*output,float param, float param2) = 0;
	virtual float Process(float input, float param, float param2) = 0;
	virtual bool IsGainEnabled() const = 0;
	
};

static std::vector<std::vector<float> > Transpose(const std::vector<std::vector<float> > &value)
{
	size_t r = value.size();
	size_t c = value[0].size();

	std::vector<std::vector<float> > result;
	result.resize(c);
	for (size_t i = 0; i < c; ++i)
	{
		result[i].resize(r);
	}
	for (size_t ir = 0; ir < r; ++ir)
	{
		for (size_t ic = 0; ic < c; ++ic)
		{
			result[ic][ir] = value[ir][ic];
		}
	}

	return result;
}
template <int N_INPUTS>
class MlModelInstance: public ToobMlModel
{
private:
    RTNeural::ModelT<float, N_INPUTS, 1,
        RTNeural::LSTMLayerT<float, N_INPUTS, 20>,
        RTNeural::DenseT<float, 20, 1>> model;

	using FloatMatrix = std::vector<std::vector<float> >;
	float inData[3];
public:

	MlModelInstance(const NeuralModel &jsonModel)
	{
		const auto& data = jsonModel.state_dict();
    	auto& lstm = (model).template get<0>();
    	auto& dense = (model).template get<1>();

		const FloatMatrix& lstm_weights_ih = data.rec__weight_ih_l0();
		lstm.setWVals(Transpose(lstm_weights_ih));

		const FloatMatrix& lstm_weights_hh = data.rec__weight_hh_l0(); 
		lstm.setUVals(Transpose(lstm_weights_hh));

		const std::vector<float>& lstm_bias_ih = data.rec__bias_ih_l0();
		std::vector<float> lstm_bias_hh = data.rec__bias_hh_l0();
		if (lstm_bias_ih.size() != lstm_bias_hh.size())
		{
			throw MLException("Invalid model.");
		}
		for (size_t i = 0; i < 80; ++i) 
			lstm_bias_hh[i] += lstm_bias_ih[i];
		lstm.setBVals(lstm_bias_hh);

		const FloatMatrix& dense_weights = data.lin__weight();
		dense.setWeights(dense_weights);

		std::vector<float> dense_bias = data.lin__bias();
		dense.setBias(dense_bias.data());
	}
	virtual void Reset() {
		model.reset();	
	}

	virtual  bool IsGainEnabled() const { return N_INPUTS > 1; }
	virtual float Process(float input, float param, float param2) 
	{
		inData[0] = input;
		inData[1] = param;
		inData[2] = param2;
		return model.forward(inData);
	}

	virtual void Process(int numSamples,const float*input, float*output,float param, float param2) {
		inData[1] = param;
		inData[2] = param;
    	for (int i = 0; i < numSamples; ++i)
		{
			inData[0] = inData[i];
        	output[i] = model.forward(inData);
		}

	}

};


ToobMlModel* ToobMlModel::Load(const std::string&fileName)
{
	NeuralModel jsonModel;
	jsonModel.Load(fileName);
	switch (jsonModel.model_data().input_size())
	{
	case 1:
		return new MlModelInstance<1>(jsonModel);
	case 2:
		return new MlModelInstance<2>(jsonModel);
	case 3:
		return new MlModelInstance<3>(jsonModel);

	default:
		throw MLException("Invalid model");
		break;
	}

}


}// namespace.




uint64_t timeMs();


ToobML::ToobML(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2Plugin(_bundle_path,features),
	loadWorker(this),
	deleteWorker(this),
	rate(_rate),
	filterResponse(),
	bundle_path(_bundle_path),
	programNumber(0)
{
	uris.Map(this);
	lv2_atom_forge_init(&forge, map);
	this->masterDezipper.SetSampleRate(_rate);
	this->trimDezipper.SetSampleRate(_rate);
	this->gainDezipper.SetSampleRate(_rate);
	this->baxandallToneStack.SetSampleRate(_rate);
	this->sagProcessor.SetSampleRate(_rate);

	this->updateSampleDelay = (int)(_rate/MAX_UPDATES_PER_SECOND);
	this->updateMsDelay = (1000/MAX_UPDATES_PER_SECOND);
	this->trimOutputSampleRate = (int)(_rate*TRIMOUT_UPDATE_RATE_S);
}

ToobML::~ToobML()
{
	delete pCurrentModel;
	delete pPendingLoad;
}

void ToobML::ConnectPort(uint32_t port, void* data)
{
	switch ((PortId)port) {
	case PortId::BASS:
		this->bassData  = (const float*)data;
		break;
	case PortId::MID:
		this->midData  = (const float*)data;
		break;
	case PortId::TREBLE:
		this->trebleData  = (const float*)data;
		break;
	case PortId::GAIN_ENABLE:
		this->gainEnableData = (float*)data;
		if (this->gainEnableData)
		{
			*(this->gainEnableData) = gainEnable;
		}
		break;
	case PortId::TRIM:
		this->trimData  = (const float*)data;
		break;
	case PortId::TRIM_OUT:
		this->trimOutData = (float*)data;
		if (trimOutData)
		{
			*trimOutData = 0;
		}
	case PortId::GAIN:
		this->gainData = (const float*)data;
		break;
	case PortId::AMP_MODEL:
		this->modelData = (const float*)data;
		break;
	case PortId::MASTER:
		this->masterData = (const float*)data;
		break;
	case PortId::AUDIO_IN:
		this->input = (const float*)data;
		break;

	case PortId::AUDIO_OUT:
		this->output = (float*)data;
		break;
	case PortId::CONTROL_IN:
		this->controlIn = (LV2_Atom_Sequence*)data;
		break;
	case PortId::NOTIFY_OUT:
		this->notifyOut = (LV2_Atom_Sequence*)data;
		break;
	case PortId::SAG:
		this->sagProcessor.Sag.SetData(data);
		break;
	case PortId::SAGD:
		this->sagProcessor.SagD.SetData(data);
		break;
	case PortId::SAGF:
		this->sagProcessor.SagF.SetData(data);
		break;

	}
}

void ToobML::Activate()
{
	
	responseChanged = true;
	frameTime = 0;
	this->baxandallToneStack.Reset();
	this->sagProcessor.Reset();

	delete pCurrentModel;
	pCurrentModel = nullptr;

	modelValue = *(modelData);
	LoadModelIndex();

	pCurrentModel = LoadModel((size_t)(modelValue));



	// fade the new model in gradually.
	masterDb = *masterData;
	master = Db2Af(masterDb);
	masterDezipper.To(0,0);
	masterDezipper.To(master,MODEL_FADE_RATE);

	trimDb = *trimData;
	trim = Db2Af(trimDb);
	trimDezipper.To(trim,0);

	gainValue = *gainData;
	gain = gainValue * 0.1f;
	gainDezipper.To(gain,0);


	bassValue = *bassData;
	midValue = *midData;
	trebleValue = *trebleData;
	UpdateFilter();

	asyncState = AsyncState::Idle;

}

static std::filesystem::path MyDirectory()
{
	#ifdef WIN32
	  #error FIXME!
	#else
		Dl_info dl_info;
		dladdr((void*)MyDirectory,&dl_info);
		return dl_info.dli_fname;
	#endif
}


void ToobML::LoadModelIndex()
{
    auto filePath = MyDirectory().parent_path();
	filePath = filePath / "models" / "tones";

    auto indexFile = filePath / "model.index";

	std::vector<string> index;

    if (std::filesystem::exists(indexFile))
    {
        // format is one filename per line (relative to parent directory).
        // which yeilds a fixed order for files.
		std::ifstream f(indexFile);

		for (std::string line; std::getline(f,line); /**/)
		{
			index.push_back((filePath / line).string());
		}
		this->modelFiles = std::move(index);
    } else {
		this->LogError("ToobML: Can't locate model resource files.\n");
	}
}

ToobMlModel* ToobML::LoadModel(size_t index)
{
	if (modelFiles.size() == 0) 
	{
		return nullptr;
	}
	if (index >= modelFiles.size())
	{
		index = modelFiles.size()-1;
	}
	const std::string &fileName = modelFiles[index];
	try {
		ToobMlModel *result = ToobMlModel::Load(fileName);
		return result;
	} catch (std::exception &error)
	{
		this->LogError("TooblML: Failed to load model file (%s).\n",fileName.c_str());
		return nullptr;
	}
}
void ToobML::Deactivate()
{
}



float ToobML::CalculateFrequencyResponse(float f)
{
	if (bypassToneFilter) return 1;
	return baxandallToneStack.GetFrequencyResponse(f);
}


LV2_Atom_Forge_Ref ToobML::WriteFrequencyResponse()
{
	
	for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
	{
		filterResponse.SetResponse(
			i,
			this->CalculateFrequencyResponse(
				filterResponse.GetFrequency(i)
				));
	}


	lv2_atom_forge_frame_time(&forge, frameTime);

	LV2_Atom_Forge_Frame objectFrame;
	LV2_Atom_Forge_Ref   set =
		lv2_atom_forge_object(&forge, &objectFrame, 0, uris.patch__Set);

    lv2_atom_forge_key(&forge, uris.patch__property);		
	lv2_atom_forge_urid(&forge, uris.param_frequencyResponseVector);
	lv2_atom_forge_key(&forge, uris.patch__value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&forge, &vectorFrame, sizeof(float), uris.atom__float);

	lv2_atom_forge_float(&forge,30.0f);
	lv2_atom_forge_float(&forge,20000.0f);
	lv2_atom_forge_float(&forge,20.0f);
	lv2_atom_forge_float(&forge,-20.0f);


	for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
	{
		lv2_atom_forge_float(&forge,filterResponse.GetFrequency(i));
		lv2_atom_forge_float(&forge,filterResponse.GetResponse(i));
	}
	lv2_atom_forge_pop(&forge, &vectorFrame);

	lv2_atom_forge_pop(&forge, &objectFrame);
	return set;
}


void ToobML::SetProgram(uint8_t programNumber)
{
	this->programNumber = programNumber;
}


void ToobML::OnMidiCommand(int , int , int )
{
}

void ToobML::OnPatchGet(LV2_URID propertyUrid)
{
	if (propertyUrid == uris.param_frequencyResponseVector)
	{
        this->patchGet = true; // 
	}

}

void ToobML::AsyncLoad(size_t model)
{
	if (asyncState == AsyncState::Idle)
	{
		asyncState = AsyncState::Loading;
		loadWorker.Request(model);
	}
}

void ToobML::AsyncLoadComplete(size_t modelIndex, ToobMlModel *pNewModel)
{
	asyncState = AsyncState::Loaded;
	this->pendingModelIndex = modelIndex;
	this->pPendingLoad = pNewModel;
	this->gainEnable = pNewModel->IsGainEnabled() ? 1.0f: 0.0f;
	if (this->gainEnableData)
	{
		*(this->gainEnableData) = this->gainEnable;
	}
}

inline void ToobML::HandleAsyncLoad()
{
	if (asyncState == AsyncState::Loaded)
	{
		if (masterDezipper.IsComplete())
		{
			ToobMlModel *oldModel = this->pCurrentModel;
			this->pCurrentModel = this->pPendingLoad;
			this->pPendingLoad = nullptr;
			AsyncDelete(oldModel);
			if (this->pendingModelIndex == this->modelValue)
			{
				masterDezipper.To(this->master,MODEL_FADE_RATE);
			} else {
				// Run the model, but don't ramp up the volume. We'll fix this mess 
				// once the delete request completes.
			}
		}
	}
}


void ToobML::AsyncDelete(ToobMlModel*pOldModel)
{
	this->asyncState = AsyncState::Deleting;
	deleteWorker.Request(pOldModel);
}
void ToobML::AsyncDeleteComplete()
{
	this->asyncState = AsyncState::Idle;
	if (this->pendingModelIndex != this->modelValue)
	{
		// We've had (one or more) model requests since the start of the last load.
		// Restart the load.
		AsyncLoad((size_t)this->modelValue);
	}
}


ToobML::LoadWorker::~LoadWorker()
{
	delete pModelResult;
}
ToobML::DeleteWorker::~DeleteWorker()
{
	delete pModel;
}

void ToobML::DeleteWorker::OnWork() {
	delete pModel;
	pModel = nullptr;
}


// static inline double clampValue(double value)
// {
// 	if (value < 0) return 0;
// 	if (value > 1) return 1;
// 	return value;
// }

inline void ToobML::UpdateFilter()
{
	baxandallToneStack.Design(bassValue,midValue,trebleValue);

	bypassToneFilter = bassValue == 0.5 && midValue == 0.5 && trebleValue == 0.5;
}
void ToobML::Run(uint32_t n_samples)
{
	// prepare forge to write to notify output port.
	// Set up forge to write directly to notify output port.
	const uint32_t notify_capacity = this->notifyOut->atom.size;
	lv2_atom_forge_set_buffer(
		&(this->forge), (uint8_t*)(this->notifyOut), notify_capacity);

	// Start a sequence in the notify output port.
	LV2_Atom_Forge_Frame out_frame;

	lv2_atom_forge_sequence_head(&this->forge, &out_frame, uris.units__Frame);


	HandleEvents(this->controlIn);


	if (*trimData != trimDb)
	{
		trimDb = *trimData;
		trim = Db2Af(trimDb);
		trimDezipper.To(trim,MASTER_DEZIP_RATE);
	}
	if (*masterData != masterDb)
	{
		masterDb = *masterData;
		master = Db2Af(masterDb);
		masterDezipper.To(master,MASTER_DEZIP_RATE);
	}
	if (*gainData != gainValue)
	{
		gainValue = *gainData;
		gain = gainValue * 0.1f;
		gainDezipper.To(gain,GAIN_DEZIP_RATE);

	}
	if (*bassData != bassValue || *midData != midValue || *trebleData != trebleValue)
	{
		bassValue = *bassData; midValue = *midData; trebleValue = *trebleData;
		UpdateFilter();
		this->responseChanged = true;

	}
	if (*modelData != modelValue)
	{
		modelValue = *modelData;
		AsyncLoad((size_t)modelValue);
		masterDezipper.To(0,MODEL_FADE_RATE);
	}
	sagProcessor.UpdateControls();

	HandleAsyncLoad(); // trasnfer in a freshly loaded model if one is ready.

	for (uint32_t i = 0; i < n_samples; ++i)
	{
		float val = trimDezipper.Tick()*input[i];

		float absVal = std::abs(val);
		if (absVal > trimOutValue)
		{
			trimOutValue = absVal;
		}
		if (!bypassToneFilter)
		{
			val = baxandallToneStack.Tick(val);
		}

		val = val*sagProcessor.GetInputScale();
		if (this->pCurrentModel != nullptr)
		{
			val = this->pCurrentModel->Process(val,gainDezipper.Tick(),0);
		}
		val = sagProcessor.TickOutput(val);
		output[i] = val*masterDezipper.Tick();
	}
	frameTime += n_samples;

	trimOutputCount -= n_samples;
	if (trimOutputCount < 0)
	{
		trimOutputCount += trimOutputSampleRate;
		*this->trimOutData = Af2Db(trimOutValue);
		trimOutValue = 0;
	}

	if (responseChanged)
	{
		responseChanged = false;
		// delay by samples or ms, depending on whether we're connected.
		if (n_samples == 0)
		{
			updateMs = timeMs() + this->updateMsDelay;
		} else {
			this->updateSamples = this->updateSampleDelay;
		}
	}
    if (this->patchGet)
    {
        this->patchGet = false;
        this->updateSampleDelay = 0;
        this->updateMs = 0;
        WriteFrequencyResponse();
    }
	if (this->updateSamples != 0)
	{
		this->updateSamples -= n_samples;
		if (this->updateSamples <= 0 || n_samples == 0)
		{
			this->updateSamples = 0;
			WriteFrequencyResponse();
		}
	}
	if (this->updateMs != 0)
	{
		uint64_t ctime = timeMs();
		if (ctime > this->updateMs || n_samples != 0)
		{
			this->updateMs = 0;
			WriteFrequencyResponse();
		}
	}
	lv2_atom_forge_pop(&forge, &out_frame);
}

 