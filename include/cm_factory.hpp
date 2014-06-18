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

#ifndef __CM_FACTORY_HPP
#define __CM_FACTORY_HPP

#include <cost_model.hpp>

class cm_factory {
private:
	cost_model cost_;
	cost_model weight_;
public:

	cm_factory()
	{
		
	}

	cm_factory(cost_model cost, cost_model weight);

	cost_model cost();

	cost_model weight();

	cost_model lambda(double lambda);
};

#endif