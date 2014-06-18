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

#include <string>
#include <array>

#include <encoders.hpp>
#include <common.hpp>
#include <cost_model.hpp>

namespace lzopt {

namespace hybrid {

const unsigned int masks[] = {0x3F, 0x3FFF, 0x3FFFFF, 0x3FFFFFFF};
const unsigned int l_masks[] = {0x7F, 0x7FFF};

}

}

namespace soda09 {

std::array<unsigned int, 7> soda09_dst::cost_classes =
{
	0U,
	16384U,
	278528U,
	2375680U,
	19152896U,
	153370624U,
	1227112448U
};

std::array<unsigned int, 6> soda09_dst::decode_mask =
{
	(1U << 14U) - 1,
	(1U << 18U) - 1,
	(1U << 21U) - 1,
	(1U << 24U) - 1,
	(1U << 27U) - 1,
	(1U << 30U) - 1
};

std::array<unsigned int, 6> soda09_dst::binary_width =
{
	14U, 18U, 21U, 24U, 27U, 30U
};

std::array<unsigned int, 16> soda09_len::cost_classes =
{
	0U,
	8U,
	16U,
	24U,
	32U,
	48U,
	64U,
	80U,
	112U,
	176U,
	304U,
	560U,
	1072U,
	2096U,
	4144U,
	1052720U
};

std::array<unsigned int, 15> soda09_len::binary_width =
{
	3U,
	3U,
	3U,
	3U,
	4U,
	4U,
	4U,
	5U,
	6U,
	7U,
	8U,
	9U,
	10U,
	11U,
	20U
};

std::array<unsigned int, 15> soda09_len::decode_mask =
{
	(1U << 3U) - 1,
	(1U << 3U) - 1,
	(1U << 3U) - 1,
	(1U << 3U) - 1,
	(1U << 4U) - 1,
	(1U << 4U) - 1,
	(1U << 4U) - 1,
	(1U << 5U) - 1,
	(1U << 6U) - 1,
	(1U << 7U) - 1,
	(1U << 8U) - 1,
	(1U << 9U) - 1,
	(1U << 10U) - 1,
	(1U << 11U) - 1,
	(1U << 20U) - 1,
};

}

namespace nibble {

std::array<unsigned int, 11> class_desc::cost_classes =
{
	0U,
	8U,
	72U,
	584U,
	4680U,
	37448U,
	299592U,
	2396744U,
	19173960U,
	153391688U,
	1227133512U,
};

std::array<unsigned int, 10> class_desc::binary_width =
{
	3U, 6U, 9U, 12U, 15U, 18U, 21U, 24U, 27U, 30U
};

std::array<unsigned int, 10> class_desc::decode_mask =
{
	(1U << 3U) - 1,
	(1U << 6U) - 1,
	(1U << 9U) - 1,
	(1U << 12U) - 1,
	(1U << 15U) - 1,
	(1U << 18U) - 1,
	(1U << 21U) - 1,
	(1U << 24U) - 1,
	(1U << 27U) - 1,
	(1U << 30U) - 1,
};

}
