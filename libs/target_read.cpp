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

#include <target_read.hpp>

#include <iostream>
#include <string>
#include <sstream>
#include <facilities.hpp>

#include <io.hpp>
#include <cost_model.hpp>
#include <wm_serializer.hpp>

std::string read_field(const char *file_name, std::string field_name)
{
	size_t file_size;
	auto file = read_file<char>(file_name, &file_size, 1);
	(file.get())[file_size] = '\0';
	std::string s(file.get());

	std::stringstream ss(s);
	std::string field_start = "== ";
	bool field_found = false;
	std::stringstream res;
	const size_t max_line = 4096;
	char line[max_line];
	while (ss.getline(line, max_line)) {
		if (field_found == false && std::string(line) == ("== " + field_name)) {
			field_found = true;
		} else if (field_found == true) {
			if (std::string(line).substr(0, field_start.size()) == field_start) {
				return res.str();
			} else {
				res << (res.str().empty() ? "" : "\n") << line;
			}
		}
	}
	return res.str();
}

cost_model get_wm(const char *target, const char *encoder_name)
{
	std::string model;
	try {
		model = read_field(join_s(target, ".tgt").c_str(), encoder_name);
	} catch (...) {
		throw std::logic_error(join_s("No model ", target, " found."));
	}
	if (model.empty()) {
		throw std::logic_error(join_s(
			"No time model for the couple (target, encoder) = (",
			target,
			", ",
			encoder_name,
			")."
		));
	}
	return wm_unserialize(model);
}