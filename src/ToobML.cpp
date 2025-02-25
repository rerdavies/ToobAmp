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
#include "ss.hpp"
#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"

#include "LsNumerics/Denorms.hpp"
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


#ifndef __clang__ // GCC-only pragma
#pragma GCC diagnostic ignored "-Waggressive-loop-optimizations"
#endif

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

#define TOOB_ML_PATCH_VERSION 1
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
template <int N_INPUTS, int HIDDEN_SIZE = 20>
class MlModelInstance: public ToobMlModel
{
private:
    RTNeural::ModelT<float, N_INPUTS, 1,
        RTNeural::LSTMLayerT<float, N_INPUTS, HIDDEN_SIZE>,
        RTNeural::DenseT<float, HIDDEN_SIZE, 1>> model;

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
		for (size_t i = 0; i < lstm_bias_ih.size(); ++i) 
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

	const auto& modelData = jsonModel.model_data();
	if (modelData.model() != "SimpleRNN")
	{
		throw MLException(SS("Unsupported model. model=" <<modelData.model()));
	}
	if (modelData.unit_type() !="LSTM")
	{
		throw MLException(SS("Unsupported model. unit_type=" <<modelData.unit_type()));
	}
	if (modelData.num_layers() != 1)
	{
		throw MLException(SS("Unsupported model. num_layers=" <<modelData.num_layers()));
	}
	if (modelData.unit_type() !="LSTM")
	{
		throw MLException(SS("Unsupported model. unit_type=" <<modelData.unit_type()));
	}
	if (jsonModel.model_data().hidden_size() == 20)
	{
		switch (jsonModel.model_data().input_size())
		{
		case 1:
			return new MlModelInstance<1,20>(jsonModel);
		case 2:
			return new MlModelInstance<2,20>(jsonModel);
		case 3:
			return new MlModelInstance<3,20>(jsonModel);

		default:
			throw MLException("Invalid model");
			break;
		}
 	} else if (jsonModel.model_data().hidden_size() == 40)
	{
		switch (jsonModel.model_data().input_size())
		{
		case 1:
			return new MlModelInstance<1,40>(jsonModel);
		case 2:
			return new MlModelInstance<2,40>(jsonModel);
		case 3:
			return new MlModelInstance<3,40>(jsonModel);

		default:
			throw MLException("Invalid model");
			break;
		}
	} else {
		throw MLException(SS("Unsupported model. hidden_size=" << jsonModel.model_data().hidden_size()));
			
	}
}


}// namespace.




uint64_t timeMs();


ToobML::ToobML(double _rate,
	const char* _bundle_path,
	const LV2_Feature* const* features)
	: 
	Lv2PluginWithState(_rate,_bundle_path,features),
	loadWorker(this),
	deleteWorker(this),
	rate(_rate),
	filterResponse(),
	bundle_path(_bundle_path),
	programNumber(0)
{
	currentModel.reserve(1024);
	urids.Map(this);
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
	try {
		delete pCurrentModel;
	} catch(const std::exception&)
	{
	}
	try {
		delete pPendingLoad;
	} catch (const std::exception&) {}
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
	
	dcBlocker.setup(this->rate,30,1);
	responseChanged = true;
	frameTime = 0;
	this->baxandallToneStack.Reset();
	this->sagProcessor.Reset();

	delete pCurrentModel;
	pCurrentModel = nullptr;

	modelValue = *(modelData);
	LoadModelIndex();

	if (modelValue == -1) // created from a fresh instance? then we're v1.
	{
		version = TOOB_ML_PATCH_VERSION;
	}
	if (version == 0)
	{
		// loaded from an old ve? Convert the modelValue to a path.
		auto index = (int)modelValue;
		if (modelFiles.size() != 0)
		{
			if (index < 0)
			{
				index = 0;
			}
			if  ((size_t)index >= this->modelFiles.size())
			{
				index = this->modelFiles.size()-1;
			}
			this->currentModel = modelFiles[index];
			pCurrentModel = LoadModel(currentModel);
			loadWorker.SetFileName(this->currentModel.c_str());
			this->modelChanged = false;
			version = TOOB_ML_PATCH_VERSION;
		}
	} else {
		this->currentModel  = loadWorker.GetFileName();
		this->pCurrentModel = LoadModel(this->currentModel);
		this->modelChanged = false;
	}
	modelPatchGetRequested	= true;



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

ToobMlModel* ToobML::LoadModel(const std::string&fileName)
{
	if (fileName == "")
	{
		return nullptr;
	}
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


	lv2_atom_forge_frame_time(&outputForge, 0);

	LV2_Atom_Forge_Frame objectFrame;
	LV2_Atom_Forge_Ref   set =
		lv2_atom_forge_object(&outputForge, &objectFrame, 0, urids.patch__Set);

    lv2_atom_forge_key(&outputForge, urids.patch__property);		
	lv2_atom_forge_urid(&outputForge, urids.property__frequencyResponseVector);
	lv2_atom_forge_key(&outputForge, urids.patch__value);

	LV2_Atom_Forge_Frame vectorFrame;
	lv2_atom_forge_vector_head(&outputForge, &vectorFrame, sizeof(float), urids.atom__Float);

	lv2_atom_forge_float(&outputForge,30.0f);
	lv2_atom_forge_float(&outputForge,20000.0f);
	lv2_atom_forge_float(&outputForge,20.0f);
	lv2_atom_forge_float(&outputForge,-20.0f);


	for (int i = 0; i < filterResponse.RESPONSE_BINS; ++i)
	{
		// lv2_atom_forge_float(&outputForge,filterResponse.GetFrequency(i));
		lv2_atom_forge_float(&outputForge,filterResponse.GetResponse(i));
	}
	lv2_atom_forge_pop(&outputForge, &vectorFrame);

	lv2_atom_forge_pop(&outputForge, &objectFrame);
	return set;
}


void ToobML::SetProgram(uint8_t programNumber)
{
	this->programNumber = programNumber;
}
const char* StringFromAtomPath(const LV2_Atom *atom)
{
    // LV2 declaration is insufficient to locate the body.
    typedef struct
    {
        LV2_Atom atom;   /**< Atom header. */
        const char c[1]; /* Contents (a null-terminated UTF-8 string) follow here. */
    } LV2_Atom_String_x;

    const char *p = ((const LV2_Atom_String_x *)atom)->c;
	return p;
}

void ToobML::SetModel(const char *szFileName)
{
	bool changed = loadWorker.SetFileName(szFileName);
	if (changed)
	{
		this->modelChanged = true;
	}
	
}
void ToobML::OnPatchSet(LV2_URID propertyUrid, const LV2_Atom*atom)
{
    if (propertyUrid == urids.ml__modelFile)
    {
		SetModel(StringFromAtomPath(atom));
	}
}

void ToobML::OnMidiCommand(int , int , int )
{
}

void ToobML::OnPatchGet(LV2_URID propertyUrid)
{
	if (propertyUrid == urids.property__frequencyResponseVector)
	{
        this->responsePatchGetRequested = true; // 
	} else if (propertyUrid == urids.ml__modelFile)
	{
		this->modelPatchGetRequested = true;
	}

}

void ToobML::AsyncLoadComplete(const std::string&path, ToobMlModel *pNewModel)
{
	asyncState = AsyncState::Loaded;
	if (pNewModel == nullptr && !path.empty()) // failed. do nothing.
	{
		asyncState = AsyncState::Idle;
		return;
	}
	this->currentModel = path;
	this->modelPatchGetRequested = true; 
	this->pPendingLoad = pNewModel;
	this->gainEnable = pNewModel != nullptr && pNewModel->IsGainEnabled() ? 1.0f: 0.0f;
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
			if (oldModel == nullptr)
			{
				this->asyncState = AsyncState::Idle;
			} else {
				AsyncDelete(oldModel);
			}
			masterDezipper.To(this->master,MODEL_FADE_RATE);
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



inline void ToobML::UpdateFilter()
{
	baxandallToneStack.Design(bassValue,midValue,trebleValue);

	bypassToneFilter = bassValue == 0.5 && midValue == 0.5 && trebleValue == 0.5;
}
void ToobML::Run(uint32_t n_samples)
{
	using namespace LsNumerics;
	auto oldFpState  = disable_denorms();
	restore_denorms(oldFpState);
	BeginAtomOutput(notifyOut);


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
		// it chanaged. Must have loaded a legacy preset.
		modelValue = *modelData;
		if (modelValue >= 0)
		{
			// legacy settings. convert to new style.
			LegacyLoad((size_t)modelValue);

			masterDezipper.To(0,MODEL_FADE_RATE);
			
		}
	}
	sagProcessor.UpdateControls();


	HandleAsyncLoad(); // trasnfer in a freshly loaded model if one is ready.
	if (asyncState == AsyncState::Idle && modelChanged)
	{
		modelChanged = false;
		loadWorker.StartRequest();
	}
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
		val = dcBlocker.filter(sagProcessor.TickOutput(val));
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
    if (this->responsePatchGetRequested)
    {
        this->responsePatchGetRequested = false;
        this->updateSampleDelay = 0;
        this->updateMs = 0;
        WriteFrequencyResponse();
    }
	if (this->modelPatchGetRequested)
	{
		this->modelPatchGetRequested = false;
		this->PutPatchPropertyPath(0,urids.ml__modelFile,currentModel.c_str());
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

	restore_denorms(oldFpState);

}

std::string ToobML::UnmapFilename(const LV2_Feature *const *features, const std::string &fileName)
{
    // const LV2_State_Make_Path *makePath = GetFeature<LV2_State_Make_Path>(features, LV2_STATE__makePath);
    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath && fileName.length() != 0)
    {
        char *result = mapPath->abstract_path(mapPath->handle, fileName.c_str());
        std::string t = result;
        if (freePath)
        {
            freePath->free_path(freePath->handle, result);
        }
        else
        {
            free(result);
        }
        return t;
    }
    else
    {
        return fileName;
    }
}

void ToobML::SaveLv2Filename(
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    const LV2_Feature *const *features,
    LV2_URID urid,
    const std::string &filename_)
{
    std::string fileName = UnmapFilename(features, filename_);
    auto status = store(handle,
                        urid,
                        fileName.c_str(),
                        fileName.length() + 1,
                        fileName.length() == 0 ? urids.atom__String : urids.atom__Path,
                        LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    if (status != LV2_State_Status::LV2_STATE_SUCCESS)
    {
        LogError(SS("`State property save failed. (" << status << ")"));
        return;
    }
}

LV2_State_Status
ToobML::OnSaveLv2State(
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
	SaveLv2Filename(
		store, handle, features,
		urids.ml__modelFile,
		this->currentModel);
	float version = TOOB_ML_PATCH_VERSION;
    auto status = store(handle,
                        urids.ml__version,
                        &version,
                        sizeof(version),
                        urids.atom__Float,
                        LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    if (status != LV2_State_Status::LV2_STATE_SUCCESS)
    {
        LogError(SS("State property save failed. (" << status << ")"));
        return status;
    }

    return LV2_State_Status::LV2_STATE_SUCCESS;
}


std::string ToobML::MapFilename(
    const LV2_Feature *const *features,
    const std::string &input)
{
    const LV2_State_Map_Path *mapPath = GetFeature<LV2_State_Map_Path>(features, LV2_STATE__mapPath);
    const LV2_State_Free_Path *freePath = GetFeature<LV2_State_Free_Path>(features, LV2_STATE__freePath);

    if (mapPath == nullptr || input.length() == 0)
    {
        return input;
    }
    else
    {
        char *t = mapPath->absolute_path(mapPath->handle, input.c_str());
        std::string result = t;
        if (freePath)
        {
            freePath->free_path(freePath->handle, t);
        }
        else
        {
            free(t);
        }
        return result;
    }
}



LV2_State_Status
ToobML::OnRestoreLv2State(
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    size_t size;
    uint32_t type;
    uint32_t myFlags;

    // PublishResourceFiles(features);

	const void *data = retrieve(
		handle, urids.ml__modelFile, &size, &type, &myFlags);
	if (data)
	{
		if (type != this->urids.atom__Path && type != this->urids.atom__String)
		{
			return LV2_State_Status::LV2_STATE_ERR_BAD_TYPE;
		}
		std::string input((const char *)data);
		if (type == this->urids.atom__Path)
		{
			input = MapFilename(features,input);
		}
		this->modelChanged = this->loadWorker.SetFileName(input.c_str());
	}
	data = retrieve(
		handle,urids.ml__version,&size,&type,&myFlags);
	if (data != nullptr && size == sizeof(float) && type == urids.atom__Float)
	{
		this->version = (int)*(float*)data;
	}
    return LV2_State_Status::LV2_STATE_SUCCESS;
}

void ToobML::LegacyLoad(size_t patchNumber)
{
	if (patchNumber >= 0 ||patchNumber < this->modelFiles.size())
	{
		std::string filename = this->modelFiles[patchNumber];
		this->modelChanged = this->loadWorker.SetFileName(filename.c_str());
		this->version = 1;
		this->modelPatchGetRequested = true;
	}
}
		