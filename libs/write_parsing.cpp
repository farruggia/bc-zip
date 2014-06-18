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

#include <write_parsing.hpp>
#include <encoders.hpp>

void write_parsing(std::vector<edge_t> &sol, text_info ti, std::string file_name, std::string encoder_name)
{
	writer_factory fact;
	encoders_().instantiate<base_writer, writer_factory>(encoder_name, fact)->write(file_name, ti, sol);
}

class base_p_writer {
public:
	virtual compressed_file write(const std::vector<edge_t> &sol, size_t parsing_length, text_info ti) = 0;
};

template <typename enc_>
class true_p_writer : public base_p_writer {
public:
	compressed_file write(const std::vector<edge_t> &sol, size_t parsing_length, text_info ti)
	{
		return write_parsing<enc_>(sol, parsing_length, ti);
	}
};

class w_fact {
public:
	template <typename enc_>
	std::unique_ptr<base_p_writer> get_instance() const
	{
		return make_unique<true_p_writer<enc_>>();
	}
};

compressed_file write_parsing(const std::vector<edge_t> &sol, text_info ti, std::string encoder_name, cost_model space_cm)
{
	// (1) get parsing length
	auto p_len = parsing_length<size_t>(ITERS(sol), space_cm);
	// (2) get writer object
	w_fact fact;
	auto w_obj = encoders_().instantiate<base_p_writer, w_fact>(encoder_name, fact);
	// (3) return parsing
	return w_obj->write(sol, p_len, ti);
}

compressed_file write_parsing(const std::vector<edge_t> &sol, text_info ti, std::string encoder_name)
{
	cost_model space_cm = encoders_().get_cm(encoder_name);
	return write_parsing(sol, ti, encoder_name, space_cm);
}

void write_parsing(
	std::string enc_name, const std::vector<edge_t> &sol, size_t parsing_length, text_info ti, byte *output
)
{
	generic_parsing_writer func(sol, parsing_length, ti, output);
	encoders_().call(enc_name, func);
}