/*
 * MIT License
 *
 * Copyright (c) 2023 Robin E. R. Davies
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "util.hpp"
#include <memory.h>
#include "ss.hpp"
#include <stdexcept>
using namespace toob;

#define USE_RTKIT 0

#if USE_RTKIT 
#include <dbus/dbus.h>
#include "rtkit.h"
#include <sys/time.h>
#include <sys/resource.h>
#endif
void toob::SetThreadName(const std::string &name)
{
    std::string threadName = "crvb_" + name;
    if (threadName.length() > 15)
    {
        threadName = threadName.substr(0, 15);
    }
    pthread_t pid = pthread_self();
    pthread_setname_np(pid, threadName.c_str());
}


int toob::SetRtThreadPriority(int schedPriority)
{
    int schedPolicy = SCHED_RR;
    int priorityMin = sched_get_priority_min(schedPolicy);
    int priorityMax = sched_get_priority_max(schedPolicy);
    // constexpr int USB_SERVICE_THREAD_PRIORITY = 5;
    if (schedPriority < priorityMin)
    {
        schedPriority = priorityMin;
    }
    if (schedPriority >= priorityMax)
    {
        throw std::logic_error(SS("Priority not allowed. Requested: " << schedPriority << "Max avilable: " << priorityMax << "."));
    }


    sched_param schedParam;
    memset(&schedParam, 0, sizeof(schedParam));
    schedParam.sched_priority = schedPriority;
    int rc = sched_setscheduler(0,schedPolicy | SCHED_RESET_ON_FORK,&schedParam);
    if (rc < 0)
    {
        throw std::logic_error(strerror(rc));
    }



#if !USE_RTKIT
    return 0;
#else
    {

    
    }
    // try using rtkit instead.

    DBusError dbusError;
    dbus_error_init(&dbusError);


    Finally fdbusError{[&dbusError] () mutable
            {
                dbus_error_free(&dbusError);
            }};

    DBusConnection *pConnection =  dbus_bus_get_private(DBUS_BUS_SYSTEM, &dbusError);
    if (pConnection == nullptr)
    {
        throw std::logic_error("Unable to set realtime priority.");
    }

    Finally f{[pConnection] 
                {
                    dbus_connection_close(pConnection);
                }};


    long long usMax = rtkit_get_rttime_usec_max(pConnection);
    if (usMax < 0)
    {
                throw std::logic_error("Unable to set realtime priority.");
    }

    struct rlimit rLimit;
    getrlimit(RLIMIT_RTTIME,&rLimit);
    rLimit.rlim_cur  = usMax;
    setrlimit(RLIMIT_RTTIME,&rLimit);


    int maxRtKitPriority = rtkit_get_max_realtime_priority(pConnection);
    if (maxRtKitPriority < 0)
    {
        throw std::logic_error(SS("rtkit_get_max_realtime_priority failed. (" << strerror(-maxRtKitPriority) << ")"));

    }

    int rc = rtkit_make_realtime(pConnection, 0, schedPriority);
    if (rc < 0)
    {
        throw std::logic_error(SS("RTKit set realtime priority failed. (" << strerror(-rc) << ")"));
    }
#endif

    return 0;
}
