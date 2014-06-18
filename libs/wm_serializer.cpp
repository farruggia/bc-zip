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

#include <sstream>
#include <wm_serializer.hpp>
#include <io.hpp>

template <typename val_, typename stream_>
void write_vector(stream_ &ss, const std::vector<val_> &vector)
{
	for (auto i : vector) {
		ss << i << "\t";
	}
	ss << std::endl;
}

template <typename val_, typename stream_>
std::vector<val_> read_vector(stream_ &ss)
{
	std::vector<val_> to_ret;
	std::string line;
	std::getline(ss, line);
	std::stringstream sline(line);
	val_ v;
	while (sline >> v) {
		to_ret.push_back(v);
	}
	return to_ret;
}

template <typename stream_>
cost_matrix read_cm(stream_ &ss, size_t dsts, size_t lens) {
	cost_matrix cm(dsts, lens);
	for (auto dst_idx = 0U; dst_idx < dsts; dst_idx++) {
		std::vector<double> times = read_vector<double>(ss);
		if (times.size() != lens) {
			throw std::logic_error("Invalid cost_matrix in stream");
		}
		for (auto len_idx = 0U; len_idx < lens; len_idx++) {
			cm(dst_idx, len_idx) = times[len_idx];
		}
	}
	return cm;
}

template <typename stream_>
void write_cm(stream_ &ss, const cost_matrix &cm)
{
	for (auto dst_idx = 0U; dst_idx < cm.dsts(); dst_idx++) {
		std::vector<double> times;
		for (auto len_idx = 0U; len_idx < cm.lens(); len_idx++) {
			times.push_back(cm(dst_idx, len_idx));
		}
		write_vector(ss, times);
	}
}

std::string wm_serialize(const cost_model &wm)
{
	std::stringstream ss;
	// Write distances and lengths
	write_vector(ss, wm.get_dst());
	write_vector(ss, wm.get_len());

	// Write cost matrix (row: one distance, columns: lengths)
	write_cm(ss, wm.get_cm());

	// Write fixed and variable cost
	ss << wm.lit_cost(0) << std::endl;
	// TODO: Conversion from delta representation, TO TEST
	ss << wm.cost_per_char() + wm.lit_cost(1) - wm.lit_cost(0) << std::endl;

	// Write copy time and return
	ss << wm.cost_per_char() << std::endl;
	return ss.str();
}

cost_model wm_unserialize(std::string serialized)
{
	std::stringstream ss(serialized);

	// Read distances and lengths
	auto dsts = read_vector<unsigned int>(ss);
	auto lens = read_vector<unsigned int>(ss);

	// Read cost matrix
	cost_matrix cm = read_cm(ss, dsts.size(), lens.size());

	// Read fixed and variable cost
	double lit_fix, lit_var;
	ss >> lit_fix;
	ss >> lit_var;

	// Read copy time
	double copy_time;
	ss >> copy_time;

	// TODO: converting to delta rep, TO TEST
	lit_var -= copy_time;

	// Build the weigh_model and return it
	return cost_model(dsts, lens, cm, lit_fix, lit_var, copy_time);
}

cost_model wm_load(std::string encoder_name)
{
	std::string file_name = encoder_name + ".tmod";
	size_t file_length;
	auto file = read_file<char>(file_name.data(), &file_length);
	std::string model(file.get(), file_length);
	return wm_unserialize(model);
}
