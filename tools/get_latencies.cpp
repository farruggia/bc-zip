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

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <tuple>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <limits>

#include <cstdio>
#include <cstdlib>

typedef std::chrono::duration<double, std::nano> nanosecs_t;

std::vector<std::tuple<unsigned int, nanosecs_t> > get_latencies(const char *file_name)
{
	using std::tuple;
	using std::vector;

	std::string line;
	std::ifstream input(file_name);

	vector<tuple<unsigned int, nanosecs_t>> latencies;

	while(bool(std::getline(input, line))) {
		// std::cerr << line << std::endl;
		if (line.find('"') != std::string::npos) {
			continue;
		}
		std::stringstream ss(line);
		double memory, latency;
		ss >> memory;
		ss >> latency;
		unsigned int kb_mem = memory * 1000;
		latencies.push_back(std::make_tuple(kb_mem, nanosecs_t(latency)));
	}
	return latencies;
}

std::vector<std::tuple<unsigned int, unsigned int, nanosecs_t> > find_plateaus(std::vector<std::tuple<unsigned int, nanosecs_t>> latencies)
{
	using std::tuple;
	using std::vector;

	vector<tuple<unsigned int, unsigned int, nanosecs_t>> plateaus;
	unsigned int start;
	std::vector<nanosecs_t> streak;
	const double eps				= 1.1;
	const unsigned int min_streak	= 4U;
	unsigned int penultimate_dist;
	{
		nanosecs_t latency;
		std::tie(start, latency) = latencies.front();
		streak				= {latency};
		penultimate_dist	= start;
	}
	for (auto i : latencies) {
	  unsigned int dist;
	  nanosecs_t latency;
	  nanosecs_t median = streak[streak.size() / 2];
	  std::tie(dist, latency) = i;
	  if (latency.count() / median.count() > eps) {
#ifndef NDEBUG
		  std::cerr << "D = " << dist << ", L = " << latency.count() << ", M = " << median.count() << ", END STREAK";
#endif
		  if (streak.size() >= min_streak) {
#ifndef NDEBUG
			  std::cerr << ", ADD";
#endif
			  plateaus.push_back(std::make_tuple(start, penultimate_dist, median));
		  }
#ifndef NDEBUG
		  std::cerr << std::endl;
#endif
		  start		= dist;
		  streak	= {latency};
	  } else {
#ifndef NDEBUG
		  std::cerr << "D = " << dist << ", L = " << latency.count() << ", M = " << median.count() << ", CONTINUE STREAK" << std::endl;
#endif
		  streak.push_back(latency);
	  }
	  penultimate_dist = dist;
	}
	if (streak.size() > min_streak) {
		plateaus.push_back(std::make_tuple(start, std::get<0>(latencies.back()), streak[streak.size() / 2]));
	}

#ifndef NDEBUG
	for (auto i : plateaus) {
		std::cerr << std::get<0>(i) << "\t" << std::get<1>(i) << std::endl;
	}
#endif

	return plateaus;
}

unsigned int closest_p2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

unsigned int closest(unsigned int s, unsigned int e)
{
	int mid = (s + e) / 2;
	int next = closest_p2(mid);
	std::vector<int> candidates = {next, next / 2, next / 4 * 3};
	std::sort(candidates.begin(), candidates.end(), [mid] (int v1, int v2) {
		return std::abs(v1 - mid) < std::abs(v2 - mid);
	});

	return candidates.front();
}

std::vector<std::tuple<unsigned int, unsigned int, nanosecs_t> > interpolate(std::vector<std::tuple<unsigned int, unsigned int, nanosecs_t> > plateaus)
{
	for (unsigned int i = 0; i + 1 < plateaus.size(); i++) {
		auto &s = std::get<1>(plateaus[i]);
		auto &e = std::get<0>(plateaus[i + 1]);
		s = e = closest(s, e);
	}
	return plateaus;
}

std::vector<std::tuple<unsigned int, unsigned int, nanosecs_t> > interpolate(
		std::vector<std::tuple<unsigned int, unsigned int, nanosecs_t>> plateaus,
		std::vector<std::tuple<unsigned int, nanosecs_t>> latencies
)
{
	std::get<1>(plateaus.back()) = std::get<0>(latencies.back());
	auto cur_win = plateaus.begin();
	auto next_win = cur_win + 1;
	unsigned int penultimate_dist = std::get<0>(latencies.front());
	for (auto i : latencies) {
		unsigned int dist;
		nanosecs_t lat;
		std::tie(dist, lat) = i;
		if (dist > std::get<1>(*cur_win)) {
			if (dist == std::get<1>(*next_win)) {
				std::get<0>(*next_win) = std::get<1>(*cur_win);
				cur_win++;
				next_win++;
			} else {
				auto cur_dist = std::abs(
					static_cast<int>(lat.count() - std::get<2>(*cur_win).count()));
				auto next_dist = std::abs(
					static_cast<int>(lat.count() - std::get<2>(*next_win).count()));
				if (cur_dist > next_dist) {
					std::get<1>(*cur_win) = std::get<0>(*next_win) = penultimate_dist;
					cur_win++;
					next_win++;
				}
			}
		}
		penultimate_dist = dist;
	}

#ifndef NDEBUG
	std::cerr << "Plateaus after gap-closing:" << std::endl;
	for (auto i : plateaus) {
		std::cerr << std::get<1>(i) << "\t";
	}
	std::cerr << std::endl;
#endif

	return plateaus;
}

std::vector<std::tuple<unsigned int, nanosecs_t> > fuse(std::vector<std::tuple<unsigned int, nanosecs_t> > plateaus)
{
	double tolerance = 1.5;
	for (unsigned int i = 0; i + 1 < plateaus.size(); i++) {
		if (std::get<1>(plateaus[i]) * tolerance >= std::get<1>(plateaus[i+1])) {
			std::get<0>(plateaus[i]) = std::get<0>(plateaus[i + 1]);
			auto new_end = std::copy(plateaus.begin() + i + 2, plateaus.end(), plateaus.begin() + i + 1);
			plateaus.erase(new_end);
		}
	}
	return plateaus;
}

std::vector<std::tuple<unsigned int, nanosecs_t> > memory_access_times(const char *lmbench)
{
	// TODO: assicurarsi che le finestre siano una multipla dell'altra
	auto latencies		= get_latencies(lmbench);
	auto plateaus		= find_plateaus(latencies);
	auto interpolated	= interpolate(interpolate(plateaus,latencies));
	std::vector<std::tuple<unsigned int, nanosecs_t>> to_ret(interpolated.size());
	std::transform(interpolated.begin(), interpolated.end(), to_ret.begin(), [](std::tuple<unsigned int, unsigned int, nanosecs_t> t) {
		return std::make_tuple(std::get<1>(t), std::get<2>(t));
	});
	auto &last_window = std::get<0>(to_ret.back());
	last_window = std::numeric_limits<decltype(std::get<0>(to_ret[0]) + 0)>::max();
	return fuse(to_ret);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cerr << "LMBench probe file required" << std::endl;
		exit(1);
	}
	auto latencies = memory_access_times(*++argv);
	for (auto i : latencies) {
		unsigned int memory;
		nanosecs_t latency;
		std::tie(memory, latency) = i;
		std::cout << memory << "\t" << latency.count() << std::endl;
	}
}
