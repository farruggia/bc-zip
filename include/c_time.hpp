/**
 * Copyright 2014 Andrea Farruggia
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * 		http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef __CTIME__
#define __CTIME__
#include <sys/time.h>
#include <sys/resource.h>

struct count_time {
    static time_t clock()
    {
        time_t usertime, systime;
        struct rusage usage;

        getrusage (RUSAGE_SELF, &usage);

        usertime = usage.ru_utime.tv_sec * 1000000 + usage.ru_utime.tv_usec;
        systime = usage.ru_stime.tv_sec * 1000000 + usage.ru_stime.tv_usec;

        return usertime + systime;
    }
};

#endif
