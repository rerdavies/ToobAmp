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
#include <cassert>
#include "lv2/core/lv2.h"
#include "lv2/state/state.h"
#include "lv2/worker/worker.h"
#include "lv2/log/logger.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/urid/urid.h"
#include "lv2/patch/patch.h"
#include <vector>
#include <functional>


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"


namespace TwoPlay {

	class Lv2Plugin;

    enum class Lv2LogLevel
    {
        Trace = 0,
        Note = 1,
        Warning = 2,
        Error = 3,
        None = 4,
    };



	typedef Lv2Plugin* (*PFN_CREATE_PLUGIN)(double _rate,
		const char* _bundle_path,
		const LV2_Feature* const* features);

	class Lv2PluginFactory {
	public:
		const char* URI;
		PFN_CREATE_PLUGIN createPlugin;
	};




	class Lv2Plugin {
	protected:
		LV2_URID_Map* map = NULL;
	private:
		LV2_Log_Logger logger;
		LV2_Worker_Schedule* schedule = NULL;
		LV2_Atom_Forge inputForge;

        static Lv2LogLevel logLevel;




	public:
        static void SetLogLevel(Lv2LogLevel level) {
            Lv2Plugin::logLevel = level;
        }
		static const LV2_Descriptor* const* CreateDescriptors(const std::vector<Lv2PluginFactory>& pluginFactories);

		friend class Lv2Plugin_Callbacks;
	protected:
		Lv2Plugin(const LV2_Feature* const* features);

		virtual void ConnectPort(uint32_t port, void* data) = 0;
		virtual void Activate() = 0;
		virtual void Run(uint32_t n_samples) = 0;
		virtual void Deactivate() = 0;
		virtual ~Lv2Plugin() { }

		template <typename INPUT, typename OUTPUT>
		void DoInBackground(
			INPUT input,
			std::function<OUTPUT (INPUT)> action,
			std::function<void (OUTPUT)> onComplete
		);

	public:
		// Map functions.
		LV2_URID MapURI(const char* uri);

		// Log functions
		void LogError(const char* fmt, ...);
		void LogWarning(const char* fmt, ...);
		void LogNote(const char* fmt, ...);
		void LogTrace(const char* fmt, ...);

	protected:
		// State extension callbacks.
		virtual LV2_State_Status
			OnRestore(
				LV2_State_Retrieve_Function retrieve,
				LV2_State_Handle            handle,
				uint32_t                    flags,
				const LV2_Feature* const* features)
		{
			return LV2_State_Status::LV2_STATE_SUCCESS;
		}
		virtual LV2_State_Status
			OnSave(
				LV2_State_Store_Function  store,
				LV2_State_Handle          handle,
				uint32_t                  flags,
				const LV2_Feature* const* features)
		{
			return LV2_State_Status::LV2_STATE_SUCCESS;
		}

		void HandleEvents(LV2_Atom_Sequence*controlInput);

		virtual void OnPatchSet(LV2_URID propertyUrid,const LV2_Atom*value)
		{

		}

		virtual void OnPatchGet(LV2_URID propertyUrid, const LV2_Atom_Object*object)
		{
		}


		// Schedule extension callbacks.

protected:
		class WorkerActionBase {
		private:
			WorkerActionBase *pThis;
			Lv2Plugin *pPlugin;
		protected:
			WorkerActionBase(Lv2Plugin *pPlugin)
			{
				pThis = this;
				this->pPlugin = pPlugin;
			}
		public:
			void Request() {
				pPlugin->schedule->schedule_work(
					pPlugin->schedule->handle,
					sizeof(pThis),&pThis); // must be POD!
			}
			void Work(LV2_Worker_Respond_Function respond,LV2_Worker_Respond_Handle   handle)
			{
				OnWork();
				void* p = (void*)&pThis;
				respond(handle,sizeof(pThis),p);
			}
			void Response()
			{
				OnResponse();
			}
		protected:
			virtual void OnWork() = 0;
			virtual void OnResponse() = 0;
		};

		template<typename INPUT, typename OUTPUT>
		class WorkerAction: protected WorkerActionBase {
		public:

			void Request(INPUT input)
			{
				this->input = input;
				WorkerActionBase::Request();
			}
			INPUT input;
			OUTPUT output;
		private:
			virtual void OnWork() {
				output = Work(input);
			}
			virtual void OnResponse()
			{
				OnResponse(output);
			}
		protected:
			virtual OUTPUT OnWork(INPUT input) = 0;
			virtual void OnResponse(OUTPUT output) = 0;

		};
	private:
		LV2_Worker_Status OnWork(
			LV2_Worker_Respond_Function respond,
			LV2_Worker_Respond_Handle   handle,
			uint32_t                    size,
			const void* data) 
		{
			assert(size == sizeof(WorkerActionBase*));
			WorkerActionBase*pWorker = *(WorkerActionBase**)data;
			pWorker->Work(respond,handle);
			return LV2_Worker_Status::LV2_WORKER_SUCCESS;
		}

		LV2_Worker_Status OnWorkResponse(uint32_t size, const void* data)
		{
			assert(size == sizeof(WorkerActionBase*));
			WorkerActionBase*worker = *(WorkerActionBase**)data;
			worker->Response();
			return LV2_Worker_Status::LV2_WORKER_SUCCESS;
		}

	private:
		static LV2_Handle
			instantiate(const LV2_Descriptor* descriptor,
				double                    rate,
				const char* bundle_path,
				const LV2_Feature* const* features);
		static void connect_port(LV2_Handle instance, uint32_t port, void* data);

		static void activate(LV2_Handle instance);
		static void run(LV2_Handle instance, uint32_t n_samples);
		static void deactivate(LV2_Handle instance);
		static void cleanup(LV2_Handle instance);

		static LV2_Worker_Status work_response(LV2_Handle instance, uint32_t size, const void* data);

		static LV2_Worker_Status work(
			LV2_Handle                  instance,
			LV2_Worker_Respond_Function respond,
			LV2_Worker_Respond_Handle   handle,
			uint32_t                    size,
			const void* data);

		static LV2_State_Status save(
			LV2_Handle                instance,
			LV2_State_Store_Function  store,
			LV2_State_Handle          handle,
			uint32_t                  flags,
			const LV2_Feature* const* features);

		static LV2_State_Status restore(
			LV2_Handle                  instance,
			LV2_State_Retrieve_Function retrieve,
			LV2_State_Handle            handle,
			uint32_t                    flags,
			const LV2_Feature* const* features);

		static const void* extension_data(const char* uri);

		class PluginUris {

		public:

			LV2_URID patch;
			LV2_URID patch_Get;
			LV2_URID patch_Set;
			LV2_URID patch_property;
			LV2_URID patch_value;
			LV2_URID atom_URID;


			void Init(LV2_URID_Map* map)
			{
				patch = map->map(map->handle,LV2_PATCH_URI);
				patch_Get = map->map(map->handle,LV2_PATCH__Get);
				patch_Set = map->map(map->handle,LV2_PATCH__Set);
				patch_property = map->map(map->handle,LV2_PATCH__property);
				patch_value = map->map(map->handle,LV2_PATCH__value);
				atom_URID = map->map(map->handle,LV2_ATOM__URID);
			}
		};

		PluginUris uris;

	};

	template <typename INPUT, typename OUTPUT>
	void Lv2Plugin::DoInBackground(
		INPUT input,
		std::function<OUTPUT (INPUT)> action,
		std::function<void (OUTPUT)> onComplete
	) {

	}


};


#pragma GCC diagnostic pop
