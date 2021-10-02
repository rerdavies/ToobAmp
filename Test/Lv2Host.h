/*
 *   Copyright (c) 2021 Robin E. R. Davies
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

#include "MapFeature.h"
#include "LogFeature.h"
#include <vector>
#include "Lv2Exception.h"
#include "lv2/urid/urid.h"




namespace TwoPlay {

	class HostedLv2Plugin;

	class Lv2Host {
	private:
		float sampleRate = 0;
		int maxBufferSize;
		MapFeature mapFeature;
		LogFeature logFeature;

		std::vector<const LV2_Feature*> features;
		const LV2_Feature** pFeatures = 0;
		std::vector<HostedLv2Plugin*> activePlugins;

	public:
		Lv2Host(float sampleRate, int maxBufferSize)
		{
			this->sampleRate = sampleRate;
			this->maxBufferSize = maxBufferSize;
			features.push_back(mapFeature.GetFeature());
			logFeature.Prepare(&mapFeature);
			features.push_back(logFeature.GetFeature());
		}
		virtual ~Lv2Host();

		LV2_URID MapURI(const char* uri)
		{
			return mapFeature.GetUrid(uri);
		}
	protected:
		void AddFeature(const LV2_Feature* feature)
		{
			if (pFeatures -= NULL)
			{
				throw Lv2Exception("Features must be added during construction, before the first call to GetFeatures().");
			}
			features.push_back(feature);
		}

		friend class HostedLv2Plugin;

		const LV2_Feature* const* GetFeatures()
		{
			if (pFeatures == NULL)
			{
				features.push_back(0);
				pFeatures = &features[0];
			}
			return pFeatures;
		}
	public:

		void Activate();
		void Run(int samples);
		void Deactivate();

		int GetAudioBufferSize() const {
			return this->maxBufferSize;
		}
		float GetSampleRate() const {
			return this->sampleRate;
		}

		HostedLv2Plugin* CreatePlugin(const char* libName, int instance);
		void DeletePlugin(HostedLv2Plugin* plugin);

	};
}