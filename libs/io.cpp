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

#include "io.hpp"

void open_file(std::ifstream &file, const char *name)
{
    file.open(name, std::ios::binary | std::ios::in);
}

void open_file(std::ofstream &file, const char *name)
{
    file.open(name, std::ios::binary | std::ios::out | std::ios::trunc);
}

std::streamoff file_length(std::ifstream &f) throw (ioexception)
{
    auto cur_pos = f.tellg();
    f.seekg(0, std::ios_base::end);
    auto end = f.tellg();
    f.seekg(cur_pos);
    if (f.bad() || f.fail())
        throw ioexception("Failed to open file");
    return end;
}
