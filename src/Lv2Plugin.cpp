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

#include "Lv2Plugin.h"

#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/midi/midi.h"
#include "lv2/urid/urid.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include <iostream>

using namespace TwoPlay;

static std::vector<Lv2PluginFactory> descriptorFactories;

Lv2LogLevel Lv2Plugin::logLevel = Lv2LogLevel::Error;

// Grant friend access to callback functions.

LV2_Handle
Lv2Plugin::instantiate(const LV2_Descriptor *descriptor,
                       double rate,
                       const char *bundle_path,
                       const LV2_Feature *const *features)
{

    Lv2Plugin *amp = NULL;

    for (size_t i = 0; i < descriptorFactories.size(); ++i)
    {
        if (strcmp(descriptorFactories[i].URI, descriptor->URI) == 0)
        {
            try {
            return descriptorFactories[i].createPlugin(rate, bundle_path, features);
            } catch (const std::exception &e)
            {
                // haven't establish a log feature yet. Just log as we can.
                std::cout << "Error creating plugin: " << e.what() << std::endl;
            }
        }
    }
    return (LV2_Handle)amp;
}

void Lv2Plugin::connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    plugin->ConnectPort(port, data);
}

void Lv2Plugin::activate(LV2_Handle instance)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    plugin->Activate();
}

void Lv2Plugin::run(LV2_Handle instance, uint32_t n_samples)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    plugin->Run(n_samples);
}

void Lv2Plugin::deactivate(LV2_Handle instance)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    plugin->Deactivate();
}
void Lv2Plugin::cleanup(LV2_Handle instance)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    delete plugin;
}

LV2_Worker_Status Lv2Plugin::work_response(LV2_Handle instance, uint32_t size, const void *data)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    return plugin->OnWorkResponse(size, data);
}

LV2_Worker_Status Lv2Plugin::work(
    LV2_Handle instance,
    LV2_Worker_Respond_Function respond,
    LV2_Worker_Respond_Handle handle,
    uint32_t size,
    const void *data)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    return plugin->OnWork(respond, handle, size, data);
}
LV2_State_Status Lv2Plugin::save(
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    return plugin->OnSaveLv2State(store, handle, flags, features);
}

LV2_State_Status Lv2Plugin::restore(
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features)
{
    Lv2Plugin *plugin = (Lv2Plugin *)instance;
    return plugin->OnRestoreLv2State(retrieve, handle, flags, features);
}

const void *Lv2Plugin::extension_data(const char *uri)
{
    static const LV2_State_Interface state = {save, restore};
    static const LV2_Worker_Interface worker = {work, work_response, NULL};
    if (!strcmp(uri, LV2_STATE__interface))
    {
        return &state;
    }
    else if (!strcmp(uri, LV2_WORKER__interface))
    {
        return &worker;
    }
    return NULL;
    return NULL;
}

const LV2_Descriptor *const *Lv2Plugin::CreateDescriptors(const std::vector<Lv2PluginFactory> &pluginFactories)
{
    descriptorFactories = pluginFactories;

    LV2_Descriptor **descriptors = new LV2_Descriptor *[pluginFactories.size()];
    for (size_t i = 0; i < pluginFactories.size(); ++i)
    {
        descriptors[i] = new LV2_Descriptor{pluginFactories[i].URI,
                                            Lv2Plugin::instantiate,
                                            Lv2Plugin::connect_port,
                                            Lv2Plugin::activate,
                                            Lv2Plugin::run,
                                            Lv2Plugin::deactivate,
                                            Lv2Plugin::cleanup,
                                            Lv2Plugin::extension_data};
    }
    return descriptors;
}

Lv2Plugin::Lv2Plugin(const LV2_Feature *const *features, bool hasState)
{
    this->hasState = hasState;

    this->logger.log = NULL;
    this->map = NULL;
    this->schedule = NULL;
    // Scan host features for URID map
    // clang-format off
    const char* missing = lv2_features_query(
        features,
        LV2_LOG__log, &this->logger.log, false,
        LV2_URID__map, &this->map, true,
        LV2_WORKER__schedule, &schedule, false,
        NULL);
    lv2_log_logger_set_map(&this->logger, this->map);
    if (missing) {
        lv2_log_error(&this->logger, "Missing feature <%s>\n", missing);
    } else {
        uris.Init(map);
        lv2_atom_forge_init(&this->inputForge, map);
        lv2_atom_forge_init(&this->outputForge, map);

    }

}

LV2_URID Lv2Plugin::MapURI(const char* uri)
{
    return map->map(map->handle, uri);
}


void Lv2Plugin::LogError(const char* fmt, ...)
{
    if (logLevel > Lv2LogLevel::Error) return;
    if (logger.log != NULL)
    {
        va_list va;
        va_start(va,fmt);
        logger.log->vprintf(logger.log->handle, logger.Error, fmt, va);
        va_end(va);
    }
}
void Lv2Plugin::LogWarning(const char* fmt, ...)
{
    if (logLevel > Lv2LogLevel::Warning) return;
    if (logger.log != NULL)
    {
        va_list va;
        va_start(va, fmt);
        logger.log->vprintf(logger.log->handle, logger.Warning, fmt, va);
        va_end(va);
    }

}
void Lv2Plugin::LogNote(const char* fmt, ...)
{
    if (logLevel > Lv2LogLevel::Note) return;
    if (logger.log != NULL)
    {
        va_list va;
        va_start(va, fmt);
        logger.log->vprintf(logger.log->handle, logger.Note, fmt, va);
        va_end(va);
    }

}
void Lv2Plugin::LogTrace(const char* fmt, ...)
{
    if (logLevel > Lv2LogLevel::Trace) return;
    if (logger.log != NULL)
    {
        va_list va;
        va_start(va, fmt);
        logger.log->vprintf(logger.log->handle, logger.Trace, fmt, va);
        va_end(va);
    }
}

void Lv2Plugin::HandleEvents(LV2_Atom_Sequence*controlInput)
{
    LV2_ATOM_SEQUENCE_FOREACH (controlInput, ev) {

        /* Update current frame offset to this event's time.  This is stored in
            the instance because it is used for sychronous worker event
            execution.  This allows a sample load event to be executed with
            sample accuracy when running in a non-realtime context (such as
            exporting a session). */
        // frame_offset = ev->time.frames;

        if (lv2_atom_forge_is_object_type(&inputForge, ev->body.type)) {
            const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
            if (obj->body.otype == uris.patch_Set) {
                // Get the property and value of the set message
                const LV2_Atom* property = NULL;
                const LV2_Atom* value    = NULL;

                lv2_atom_object_get(
                    obj,
                    uris.patch_property, &property,
                    uris.patch_value,    &value,
                    0);

                if (!property)
                {
                    this->LogTrace("Patch Set message with no property\n");
                }
                else if (property->type != uris.atom_URID)
                {
                        this->LogTrace("Patch Set property is not a URID\n");
                } else {
                    LV2_URID key = ((const LV2_Atom_URID *)property)->body;
                    OnPatchSet(key,value);
                }
            }
            else if (obj->body.otype == uris.patch_Get)
            {
                // Get the property and value of the set message
                const LV2_Atom* property = NULL;

                lv2_atom_object_get(
                    obj,
                    uris.patch_property, &property,
                    0);
                if (property != NULL && property->type == uris.atom_URID)
                {
                    LV2_URID propertyUrid = ((LV2_Atom_URID*)property)->body;
                    OnPatchGet(propertyUrid,obj);

                }
            }
        }
    }
}

void Lv2Plugin::WorkerAction::Request()
{
    if (pPlugin->schedule)
    {
        pPlugin->schedule->schedule_work(
            pPlugin->schedule->handle,
            sizeof(pThis), &pThis); // must be POD!
    }
    else
    {
        // no scheduler. do it synchronously.
        OnWork();
        OnResponse();
    }
}


void Lv2Plugin::WorkerAction::Work(LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle handle)
{
    OnWork();
    respond(handle, sizeof(pThis), &pThis);
}
void Lv2Plugin::WorkerAction::Response()
{
    OnResponse();
}

void Lv2Plugin::WorkerActionWithCleanup::CleanupWorker::OnWork()
{
    pThis->OnCleanup();
}
void Lv2Plugin::WorkerActionWithCleanup::CleanupWorker::OnResponse()
{
    pThis->OnCleanupComplete();
}

Lv2Plugin::WorkerActionWithCleanup::CleanupWorker::CleanupWorker(Lv2Plugin *plugin, WorkerActionWithCleanup *pThis) : WorkerAction(plugin), pThis(pThis)
{
}



void Lv2Plugin::PatchPutString(int64_t frameTime,LV2_URID propertyUrid, const char*value)
{
    lv2_atom_forge_frame_time(&outputForge, frameTime);
    size_t len = strlen(value);

    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&outputForge, &frame, 0, uris.patch_Set);

    lv2_atom_forge_key(&outputForge, uris.patch_property);
    lv2_atom_forge_urid(&outputForge, propertyUrid);
    lv2_atom_forge_key(&outputForge, uris.patch_value);
    lv2_atom_forge_string(&outputForge, value, len);
    lv2_atom_forge_pop(&outputForge, &frame);

}
void Lv2Plugin::PatchPutPath(int64_t frameTime,LV2_URID propertyUrid, const char*value)
{
    lv2_atom_forge_frame_time(&outputForge, frameTime);
    size_t len = strlen(value);

    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&outputForge, &frame, 0, uris.patch_Set);

    lv2_atom_forge_key(&outputForge, uris.patch_property);
    lv2_atom_forge_urid(&outputForge, propertyUrid);
    lv2_atom_forge_key(&outputForge, uris.patch_value);
    lv2_atom_forge_path(&outputForge, value, len);
    lv2_atom_forge_pop(&outputForge, &frame);

}
void Lv2Plugin::PatchPut(int64_t frameTime,LV2_URID propertyUrid, float value)
{
    lv2_atom_forge_frame_time(&outputForge, frameTime);

    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&outputForge, &frame, 0, uris.patch_Set);

    lv2_atom_forge_key(&outputForge, uris.patch_property);
    lv2_atom_forge_urid(&outputForge, propertyUrid);
    lv2_atom_forge_key(&outputForge, uris.patch_value);
    lv2_atom_forge_float(&outputForge, value);
    lv2_atom_forge_pop(&outputForge, &frame);

}
void Lv2Plugin::PatchPut(int64_t frameTime,LV2_URID propertyUrid, size_t count, float *values)
{
    lv2_atom_forge_frame_time(&outputForge, frameTime);

    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&outputForge, &frame, 0, uris.patch_Set);

    lv2_atom_forge_key(&outputForge, uris.patch_property);
    lv2_atom_forge_urid(&outputForge, propertyUrid);
    lv2_atom_forge_key(&outputForge, uris.patch_value);
    lv2_atom_forge_vector(&outputForge, 
        sizeof(float),
        uris.atom_float,
        (uint32_t)count,(void*)values);
    lv2_atom_forge_pop(&outputForge, &frame);

}
void Lv2Plugin::PatchPut(int64_t frameTime,LV2_URID propertyUrid, double value)
{
    lv2_atom_forge_frame_time(&outputForge, frameTime);

    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&outputForge, &frame, 0, uris.patch_Set);

    lv2_atom_forge_key(&outputForge, uris.patch_property);
    lv2_atom_forge_urid(&outputForge, propertyUrid);
    lv2_atom_forge_key(&outputForge, uris.patch_value);
    lv2_atom_forge_double(&outputForge, value);
    lv2_atom_forge_pop(&outputForge, &frame);

}
void Lv2Plugin::PatchPut(int64_t frameTime,LV2_URID propertyUrid, int32_t value)
{
    lv2_atom_forge_frame_time(&outputForge, frameTime);

    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&outputForge, &frame, 0, uris.patch_Set);

    lv2_atom_forge_key(&outputForge, uris.patch_property);
    lv2_atom_forge_urid(&outputForge, propertyUrid);
    lv2_atom_forge_key(&outputForge, uris.patch_value);
    lv2_atom_forge_int(&outputForge, value);
    lv2_atom_forge_pop(&outputForge, &frame);

}
void Lv2Plugin::PatchPut(int64_t frameTime,LV2_URID propertyUrid, int64_t value)
{
    lv2_atom_forge_frame_time(&outputForge, frameTime);

    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_object(&outputForge, &frame, 0, uris.patch_Set);

    lv2_atom_forge_key(&outputForge, uris.patch_property);
    lv2_atom_forge_urid(&outputForge, propertyUrid);
    lv2_atom_forge_key(&outputForge, uris.patch_value);
    lv2_atom_forge_long(&outputForge, value);
    lv2_atom_forge_pop(&outputForge, &frame);

}
