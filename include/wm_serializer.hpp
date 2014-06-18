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

#ifndef __WM_SERIALIZER_
#define __WM_SERIALIZER_

#include <string>
#include <encoders.hpp>

std::string wm_serialize(const cost_model &wm);

cost_model wm_unserialize(std::string serialized);

cost_model wm_load(std::string encoder_name);

#endif
