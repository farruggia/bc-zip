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

#ifndef __FSG_CHECK_HPP
#define __FSG_CHECK_HPP

#include <memory>
#include <gtest/gtest.h>
#include <utilities.hpp>
#include <generators.hpp>
#include <common.hpp>
#include <meter_printer.hpp>
#include <sstream>


class compare_run {
private:
	text_info ti;
	cost_model cm;
public:

	compare_run(text_info ti, cost_model cm) 
		: ti(ti), cm(cm)
	{

	}

	template <typename v1_t, typename v2_t>
	std::string print_mismatch_edges(v1_t edges_1, v2_t edges_2, size_t l1, size_t l2, unsigned int pos, size_t *mismatch_count)
	{
		std::stringstream ss;
		ss << "Position = " << pos << std::endl << std::endl;
		ss << "First generator = " << l1 << " edges" << std::endl;
		ss << "Second generaotr = " << l2 << " edges" << std::endl;
		ss << "First edges:" << std::endl;
		for (auto i = 0u; i < l1; i++) {
			auto edge = edges_1[i];
			auto dst = edge.d;
			auto len = edge.ell;
			ss << "(" << dst << "," << len << ")\t";
		}
		ss << std::endl;

		ss << "Second edges:" << std::endl;
		for (auto i = 0u; i < l2; i++) {
			auto edge = edges_2[i];
			auto dst = edge.d;
			auto len = edge.ell;
			ss << "(" << dst << "," << len << ")\t";
		}
		ss << std::endl;
		(*mismatch_count)++;
		return ss.str();
	}

	template <typename gen_fac_1_t, typename gen_fac_2_t>
	void run(gen_fac_1_t &&gen_fac_1, gen_fac_2_t &&gen_fac_2)
	{
		auto file_length = ti.len;

		// Instantiate the two FSG
		auto fsg_1	= gen_fac_1.instantiate(cm);
		auto fsg_2	= gen_fac_2.instantiate(cm);

		// Let us now generate maximal edges, and see if the two methods find the same maximal edges.
		auto &m1 = fsg_1->get_edges();
		auto &m2 = fsg_2->get_edges();

		fsg_meter meter(file_length);

		size_t mismatch_count = 0;
		size_t const max_mis = 30;
		for (auto i = 0U; i <= file_length; i++) {
			std::uint32_t g1, g2;
			auto b1 = fsg_1->gen_next(&g1);
			auto b2 = fsg_2->gen_next(&g2);
			// Well, let's just test if this explodes...
			ASSERT_EQ(b1, b2);
			EXPECT_EQ(g1, g2) << print_mismatch_edges(m1, m2, g1, g2, i, &mismatch_count);
			ASSERT_LT(mismatch_count, max_mis);
//			assert(b1 == b2);
//			assert(g1 == g2);
			for (auto j = 0u; j < std::min(g1, g2); j++) {
				auto e_old = m1[j];
				auto e_new = m2[j];
				EXPECT_EQ(e_old.ell, e_new.ell) << "Position: " << i;
				auto dst_idx_1 = std::get<0>(cm.get_idx(e_old.d, e_old.ell));
				auto dst_idx_2 = std::get<0>(cm.get_idx(e_new.d, e_new.ell));
				EXPECT_EQ(dst_idx_1, dst_idx_2) << "Position: " << i;
				EXPECT_EQ(e_old.cost_id, e_new.cost_id) << "Position: " << i;
			}
			meter.new_character();
		}		
	}

	template <typename gen_fac_1, typename gen_fac_2>
	void run()
	{
		sa_cacher sc;
		run(gen_fac_1(ti, sc), gen_fac_2(ti, sc));
	}
};

#endif