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

#include <graph_cache.hpp>
#include <optimal_parser.hpp>
#include <solution_getter.hpp>

#include <iostream>
#include <string>
#include <write_parsing.hpp>
#include <cm_factory.hpp>
#include <target_read.hpp>
#include <gtest/gtest.h>
#include <chrono>

// TEST ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class cost_model_id : public ::testing::Test {
protected:

	static void SetUpTestCase()
	{

	}

	virtual void SetUp()
	{

	}

public:
	static cm_factory cm_fact;

	static void set_cm(cost_model cm_, cost_model w_cm_)
	{
		cm_fact = cm_factory(cm_, w_cm_);
	}

};

cm_factory cost_model_id::cm_fact;

TEST_F(cost_model_id, all)
{
	auto sha_cost = cm_fact.cost().id();
	auto sha_weight = cm_fact.weight().id();
	auto sha_l1 = cm_fact.lambda(1.0).id();
	auto sha_l2 = cm_fact.lambda(2.0).id();
	auto sha_l11 = cm_fact.lambda(1.0).id();

	ASSERT_EQ(sha_l1, sha_l11);
	ASSERT_NE(sha_cost, sha_weight);
	ASSERT_NE(sha_cost, sha_l1);
	ASSERT_NE(sha_cost, sha_l2);
	ASSERT_NE(sha_cost, sha_l11);
	ASSERT_NE(sha_weight, sha_l1);
	ASSERT_NE(sha_weight, sha_l2);
	ASSERT_NE(sha_weight, sha_l11);
	ASSERT_NE(sha_l1, sha_l2);
	ASSERT_NE(sha_l2, sha_l11);

	ASSERT_EQ(sha_cost, cm_fact.cost().id());
	ASSERT_EQ(cost_model().id(), "");
}


int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  if (argc < 3) {
  	std::cerr << "ERROR: target file, encoder" << std::endl;
  	exit(1);
  }
  char *target_file = *++argv;
  char *encoder_name = *++argv;
  cost_model cost = encoders_().get_cm(encoder_name);
  cost_model weight = get_wm(target_file, encoder_name);
  cost_model_id::set_cm(cost, weight);
  return RUN_ALL_TESTS();
}