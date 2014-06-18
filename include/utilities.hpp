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

#ifndef __UTILITIES_
#define __UTILITIES_

#include <divsufsort.h>
#include <tuple>
#include <string>
#include <iostream>
#include <sstream>
#include <map>

#include <common.hpp>
#include <io.hpp>
#include <cost_model.hpp>

std::shared_ptr<std::vector<std::int32_t>> get_sa(byte *s, size_t len);

std::shared_ptr<std::vector<std::int32_t>> get_isa(const std::vector<std::int32_t> *sa);

std::vector<unsigned int> get_cost_classes(std::vector<unsigned int> dst, size_t t_len);

std::tuple<std::shared_ptr<std::uint8_t>, size_t> get_file(const char *file_name);

template <typename T>
std::unique_ptr<T[]> get_file(const char *name, size_t *file_size = nullptr)
{
	std::ifstream file;
	open_file(file, name);
	std::streamoff len = file_length(file);
	std::unique_ptr<T[]> to_ret(new T[len]);
	read_file(file, to_ret.get(), static_cast<std::streamsize>(len));
	if (file_size != nullptr){
		*file_size = len;
	}
	return to_ret;
}
/***** distances normalizer ********/
std::vector<unsigned int> normalize_dst(const std::vector<unsigned int> &dst, size_t t_len);

std::string get_cm_rep(const cost_model &cm);

/** Check solution correctness */
struct correctness_report {
	bool correct;
	unsigned int error_position;
	unsigned int error_d;
	unsigned int error_ell;

	correctness_report(bool correct, unsigned int error_pos, unsigned int error_d, unsigned int error_ell)
		: correct(correct), error_position(error_pos), error_d(error_d), error_ell(error_ell)
	{

	}
};

correctness_report check_correctness(const std::vector<edge_t> &sol, byte *start);

/****** SUFFIX ARRAY CACHER **********/
class sa_getter {
public:
	virtual std::shared_ptr<std::vector<std::int32_t>> get(byte *begin, byte *end) = 0;

	virtual ~sa_getter()
	{

	}
};

class sa_cacher : public sa_getter {
private:
	std::map<std::tuple<byte *, byte *>, std::shared_ptr<std::vector<std::int32_t>>> cache;
public:
	sa_cacher();

	std::shared_ptr<std::vector<std::int32_t>> get(byte *begin, byte *end);
};

class sa_instantiate : public sa_getter {
public:
	sa_instantiate();

	std::shared_ptr<std::vector<std::int32_t>> get(byte *begin, byte *end);
};

/**************** Distance sequence type and selector **************************/
enum distance_kind {
	GENERIC,
	ALL_SAME,
	MULTIPLE
};

distance_kind get_kind(std::vector<unsigned int> dst);

std::string kind_name(distance_kind kind);

bool compatible(distance_kind cm_kind, distance_kind gen_kind);

std::string suggest_gen(distance_kind kind);
#endif
