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

#ifndef __IOS_
#define __IOS_

#include <stdexcept>
#include <fstream>
#include <string>
#include <memory>

class ioexception : public std::runtime_error {
public:
    explicit ioexception(const char *what) : std::runtime_error(what) { }
};

void open_file(std::ifstream &file, const char *name);

void open_file(std::ofstream &file, const char *name);

std::streamoff file_length(std::ifstream &f) throw (ioexception);

template <typename T>
void read_some(std::ifstream &f, T *data, std::streamsize length) throw (ioexception)
{
    f.read(reinterpret_cast<char*>(data), sizeof(T) * length);
}

template <typename T>
void read_data(std::ifstream &f, T *data, std::streamsize length) throw (ioexception)
{
    f.read(reinterpret_cast<char*>(data), sizeof(T) * length);
    if (f.bad() || f.fail())
        throw ioexception("Failed to read file");
}

template <typename T>
void read_file(std::ifstream &f, T *data, std::streamsize length) throw (ioexception)
{
    f.seekg(0, std::ios_base::beg);
    read_data(f, data, length);
}

template <typename T>
void write_file(std::ofstream &f, T *data, std::streamsize length) throw (ioexception)
{
    f.write(reinterpret_cast<char*>(data), sizeof(T) * length);
    if (f.bad() || f.fail())
        throw ioexception("Failed to write on file");
}

template <typename T>
std::unique_ptr<T[]> read_file(const char *name, size_t *size = nullptr, unsigned int extra = 0)
{
    std::ifstream file;
    open_file(file, name);
    size_t l_size = file_length(file);
    if (size != nullptr) {
        *size = l_size;
    }
	std::unique_ptr<T[]> to_ret(new T[l_size + extra]);
    std::fill(to_ret.get(), to_ret.get() + l_size + extra, T());
	read_file(file, to_ret.get(), static_cast<std::streamsize>(l_size));
    return std::move(to_ret);
}

#endif
