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
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <numeric>
#include <limits>
#include <random>
#include <cstring>
#include <map>
#include <set>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>

#include <copy_routines.hpp>
#include <decompress.hpp>
#include <write_parsing.hpp>
#include <meter_printer.hpp>
#include <wm_serializer.hpp>

#include <phrase_reader.hpp>
#include <decompress.hpp>

#include <cstdio>
#include <stdlib.h>

typedef std::chrono::duration<double, std::nano> nanosecs_t;

std::vector<std::tuple<unsigned int, nanosecs_t> > get_latencies(const char *file_name)
{
	using std::tuple;
	using std::vector;

	std::string line;
	std::ifstream input(file_name);

	vector<tuple<unsigned int, nanosecs_t>> latencies;

	while(bool(std::getline(input, line))) {
	  std::stringstream stream(line);
	  double latency;
	  unsigned int memory; 
	  stream >> memory; // In kilobytes
	  stream >> latency; // In nanoseconds
	  latencies.push_back(std::make_tuple(memory, nanosecs_t(latency)));
	}
	return latencies;
}

template <typename copy_>
nanosecs_t bench_copy(unsigned int size)
{
	nanosecs_t sum(0);
	const size_t tries = 5;
#ifdef NDEBUG
	fsg_meter m(tries);
#endif
	for (unsigned int i = 0; i < tries; i++) {
		size_t offset = 8;
		std::vector<byte> source(size + offset);
		std::iota(source.begin(), source.end(), 0);
		copy_ copy;
		byte *dest	= source.data() + offset;
		byte *src	= source.data();
		auto t1 = std::chrono::high_resolution_clock::now();
		copy(dest, src, size);
		auto t2 = std::chrono::high_resolution_clock::now();
		sum += std::chrono::duration_cast<nanosecs_t>(t2 - t1);
#ifdef NDEBUG
		m.new_character();
#endif
	}
	return sum / tries;
}

struct parsing_t {
	std::unique_ptr<byte[]> parsing;
	size_t size;
};

template <typename value_t>
class abstract_forward_it {
public:
	virtual size_t distance() = 0;
	virtual value_t get() = 0;
	virtual void next() = 0;
	virtual bool finished() = 0;
	virtual std::unique_ptr<abstract_forward_it<value_t>> dup() = 0;
	virtual ~abstract_forward_it()
	{

	}
};

template <typename iter_t>
class iter_adaptor 
	: public abstract_forward_it<typename std::iterator_traits<iter_t>::value_type>
{
private:
	iter_t begin;
	iter_t end;
	iter_t cur;

	typedef typename std::iterator_traits<iter_t>::value_type value_type;

public:
	iter_adaptor(iter_t begin, iter_t end)
		: begin(begin), end(end), cur(begin)
	{

	}

	size_t distance()
	{
		return std::distance(cur, end);
	}

	bool finished()
	{
		return cur == end;
	}

	value_type get()
	{
		return *cur;
	}

	void next()
	{
		++cur;
	}

	std::unique_ptr<abstract_forward_it<value_type>> dup()
	{
		return make_unique<iter_adaptor<iter_t>>(cur, end);
	}
};

class uniform_literal_it 
	: public boost::iterator_facade<
		uniform_literal_it,
		edge_t,
		boost::random_access_traversal_tag
	  >
{
private:
	size_t lit_len;
	unsigned int pos;
	size_t text_len;
	mutable edge_t edge;

	void set_edge()
	{
		auto end_pos = pos + lit_len;
		end_pos = std::min<size_t>(text_len, end_pos);
		edge = edge_t(end_pos - pos);
	}

	uniform_literal_it(size_t lit_len, unsigned int pos, size_t text_len)
		: lit_len(lit_len), pos(pos), text_len(text_len)
	{
		set_edge();
	}

public:

	static uniform_literal_it start(size_t lit_len, size_t text_len)
	{
		return uniform_literal_it(lit_len, 0, text_len);
	}

	static uniform_literal_it end(size_t text_len)
	{
		return uniform_literal_it(0, text_len, text_len);
	}

private:
	friend class boost::iterator_core_access;


    void increment()
    {
    	advance(1);
    }

    void decrement()
    {
    	advance(-1);
    }

    void advance(int n)
    {

    	auto final_pos = static_cast<int>(pos) + static_cast<int>(lit_len) * n;
    	final_pos = std::max<int>(0, final_pos);
    	final_pos = std::min<int>(final_pos, text_len);
    	pos = final_pos;
    	set_edge();
    }

    std::iterator_traits<uniform_literal_it>::difference_type distance_to(uniform_literal_it const& other) const
    {
    	return std::ceil(1.0 * std::abs<int>(pos - other.pos) / lit_len);
    }

    bool equal(uniform_literal_it const& other) const
    {
        return pos == other.pos;
    }

    edge_t& dereference() const
    {
    	return edge;
    }
};

class base_dparsing {
public:
	virtual parsing_t get(abstract_forward_it<edge_t> *edges, byte *literal_buffer = nullptr) = 0;
	virtual ~base_dparsing()
	{

	}
};

template <typename enc_>
class dummy_parsing : public base_dparsing {
public:
	parsing_t get(abstract_forward_it<edge_t> *edges, byte *literal_buffer = nullptr)
	{
		// Find out the parsing length
		typedef typename enc_::encoder enc_t;
		cost_model cm = enc_t::get_cm();
		size_t p_length = 0;
		size_t d_length = 0;

		for (auto new_iter = edges->dup(); !new_iter->finished(); new_iter->next()) {
			auto i = new_iter->get();
			p_length += cm.edge_cost(i);
			d_length += i.ell;
		}

//		std::cerr << "bit length: " << p_length << std::endl;
//		std::cerr << "decompress length: " << d_length << std::endl;
//		std::cerr << "solution size: " << edges.size() << std::endl;

		// Instantiate the encoder
		typedef typename enc_::encoder enc_t;
		auto data_len = enc_t::data_len(p_length);

		size_t array_len = data_len + 8; // To avoid out-of-bound reads
		std::unique_ptr<byte[]> data_holder(new byte[array_len]);
		byte *data = data_holder.get();
		std::fill(data, data + array_len, 0U);

		enc_t enc(data, data_len);

		uint32_t nextliteral = 0;

		// Encode phrases
		// Bit 0 + single char 1 <dist, length>

		unsigned int count = 0, pos = 0;

		for (;!edges->finished(); edges->next()) {
			edge_t edge = edges->get();
			assert(!edge.invalid());
			if (edge.kind() == PLAIN) {
				// # of phrases up to next literal
				nextliteral = 0;
				auto j = edges->dup();
				j->next();
				for (; !j->finished(); nextliteral++) {
					auto cur_edge = j->get();
					if (cur_edge.kind() == PLAIN){
						break;
					} else {
						j->next();
					}
				}
				if (j->finished()) {
					nextliteral++;
				}
				enc.encode(literal_buffer, edge.ell, nextliteral);
			} else {
				assert(edge.kind() == REGULAR);
				enc.encode(edge.d, edge.ell);
			}
			count++;
			pos += edge.ell;
		}
		return { std::move(data_holder), data_len };
	}
};

class dp_factory {
public:
	template <typename enc_>
	std::unique_ptr<base_dparsing> get_instance() const
	{
		return make_unique<dummy_parsing<enc_>>();
	}
};

template <typename iter_>
parsing_t get_parsing(const char *encoder, iter_ begin, iter_ end, byte *literal_buffer = nullptr)
{
	dp_factory d_fact;
	auto dp_getter = encoders_().instantiate<base_dparsing, dp_factory>(encoder, d_fact);
	iter_adaptor<iter_> iterator(begin, end);
	return dp_getter->get(&iterator, literal_buffer);
}

struct empty_copy {
	template <typename T>
	void operator()(T *, T*, size_t)
	{

	}
};

struct time_matrix {
	cost_matrix cm;
	std::vector<unsigned int> dsts;
	std::vector<unsigned int> lens;
};

bool row_equal(boost::numeric::ublas::matrix<double> *cm, unsigned int row_1, unsigned int row_2, double eps)
{
	auto cols = cm->size2();
	for (auto col_idx = 0; col_idx < cols; col_idx++) {
		auto val_1 = (*cm)(row_1, col_idx);
		auto val_2 = (*cm)(row_2, col_idx);
		if (std::max(val_1, val_2) > (1 + eps) * std::min(val_1, val_2)) {
			return false;
		}
	}
	return true;
}

std::tuple<unsigned int, unsigned int> find_equal(
	boost::numeric::ublas::matrix<double> *cm
)
{
	auto rows = cm->size1();
	const double eps = 0.1;
	for (auto row_idx = 0U; row_idx < rows; row_idx++) {
		auto last_equal = row_idx + 1;
		while (last_equal < rows) {
			if (!row_equal(cm, row_idx, last_equal, eps)) {
				break;
			}
			++last_equal;
		}
		if (last_equal > row_idx + 1) {
			return std::make_tuple(row_idx, last_equal);
		}
	}
	return std::make_tuple(0U, 0U);
}

void collapse(
	boost::numeric::ublas::matrix<double> *cm, unsigned int first_row, unsigned int end_row,
	std::vector<unsigned int> *labels
)
{
	// Collapse matrix elements
	using namespace boost::numeric::ublas;
	auto rows_to_copy = cm->size1() - end_row;
	matrix_range<matrix<double>> src(*cm, range(end_row, cm->size1()), range (0, cm->size2()));
	matrix_range<matrix<double>> dest(
		*cm, range(first_row + 1, first_row + 1 + rows_to_copy), range (0, cm->size2())
	);
	dest = src;
	cm->resize(first_row + 1 + rows_to_copy, cm->size2());

	// Collapse labels
	auto first_src 	= std::next(labels->begin(), end_row - 1);
	auto end_src	= labels->end();
	auto first_dst	= std::next(labels->begin(), first_row);
	auto last_copied = std::copy(first_src, end_src, first_dst);
	labels->erase(last_copied, labels->end());
}

void reduce_matrix(boost::numeric::ublas::matrix<double> *cm, std::vector<unsigned int> *row_label)
{
	while (true) {
		unsigned int first, last;
		std::tie(first, last) = find_equal(cm);
		if (last <= first + 1) {
			break;
		}
		collapse(cm, first, last, row_label);
	}
}

void reduce_tm(time_matrix *tm)
{

	auto cm = (tm->cm).data();
	// Reduce rows
	reduce_matrix(&cm, &(tm->dsts));

	// Reduce columns
	cm = boost::numeric::ublas::trans(cm);
	reduce_matrix(&cm, &(tm->lens));

	// Transpose back
	cm = boost::numeric::ublas::trans(cm);
	(tm->cm).set(cm);
}

time_matrix phrase_decode_time(const char *encoder)
{

	using namespace std::chrono;

	const size_t dummy_phrases = std::mega::num;
	std::vector<std::tuple<unsigned int, unsigned int, nanosecs_t> > to_ret;

	// STEP 1: get infos about the encoder
	cost_model cm = encoders_().get_cm(encoder);
	std::vector<unsigned int> dst_info = cm.get_dst(), len_info = cm.get_len();

	// STEP 2: for each cost class, produce an hypothetical parsing of phrases falling in that cost class
	std::random_device rd;
	std::mt19937 gen(rd());

#ifdef NDEBUG
	fsg_meter progress_bar(dst_info.size() * len_info.size());
#endif

	cost_matrix cost_m(dst_info.size(), len_info.size());

	unsigned int low_dst = 1U;
	for (unsigned int i = 0; i < dst_info.size(); i++) {
		unsigned int low_len = 1U;
		unsigned int high_dst = dst_info[i];
		for (unsigned int j = 0; j < len_info.size(); j++) {
			unsigned int high_len = len_info[j];

			// Generate a fake parsing with a bunch of random integers
//			std::cerr << "DST: " << low_dst << " -> " << high_dst << std::endl;
//			std::cerr << "LEN: " << low_len << " -> " << high_len << std::endl;
//			std::cerr << "Phrase cost: " << cm.get_cost(i, j) << std::endl;
//			size_t exp_bit_length = dummy_phrases * cm.get_cost(i, j) + cm.edge_cost(edge_t(1));
//			std::cerr << "Expected bit length: " << exp_bit_length << std::endl;
//			std::cerr << "In bytes: " << (exp_bit_length + 7) / 8 << std::endl;
			assert(low_dst < high_dst);
			std::uniform_int_distribution<> dst_gen(low_dst, high_dst);
			assert(low_len < high_len);
			std::uniform_int_distribution<> len_gen(low_len, high_len);
			std::vector<edge_t> edges;
			edges.push_back(edge_t(1));
			const id_map map = cm.get_map();
			std::uint64_t orig_size = 1;
			for (unsigned int k = 0; k < dummy_phrases; k++) {
				auto id = map.wrap(j, i);
				edge_t to_add(dst_gen(gen), len_gen(gen), id);
				edges.push_back(to_add);
				orig_size += to_add.ell;
//				std::cerr << to_add.d << "\t" << to_add.ell << std::endl;
			}
//			std::cerr << std::endl;

			byte literal_buffer[] = {'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a'};
			auto p_t = get_parsing(encoder, ITERS(edges), literal_buffer);
			byte *p_parsing = p_t.parsing.get();

			// STEP 3: invoke decompression, passing an empty copier, and measure the time
			// byte first;
			auto elapsed = 1.0 * decompress_raw<empty_copy>(encoder, p_parsing, literal_buffer, orig_size);

			// STEP 4: divide time by number of phrases, and get the individual phrase time
			elapsed /= dummy_phrases;
			cost_m(i, j) = elapsed;
			low_len = high_len + 1;
#ifdef NDEBUG
			progress_bar.new_character();
#endif
		}
		low_dst = high_dst + 1;
	}
	return {cost_m, dst_info, len_info};
}

std::tuple<unsigned int, unsigned int> get_unaligned(const cost_model &cm)
{
	size_t dsts = cm.get_dst().size();
	size_t lens = cm.get_len().size();

	for (auto i = 0U; i < dsts; i++) {
		for (auto j = 0U; j < lens; j++) {
			unsigned int cost = cm.get_cost(i, j);
			if ((cost % 8) != 0) {
				return std::make_tuple(i, j);
			}
		}
	}
	return std::make_tuple(dsts, lens);

}

void print_stats(byte *parsing, size_t length, std::string encoder_name)
{
	using namespace lzopt;
	// Instantiate the phrase reader
	pr_factory fact(parsing, length);
	std::unique_ptr<i_phrase_reader> reader = encoders_().instantiate<i_phrase_reader,pr_factory>(encoder_name, fact);
	i_phrase_reader *read_ptr = reader.get();

	unsigned int phrases = 0, literals = 0, copies = 0, copy_len = 0, literal_len = 0;
	while (!read_ptr->end()) {
		++phrases;

		std::uint32_t dst, len;
		read_ptr->next(dst, len);
		if (dst > 0) {
			++copies;
			copy_len += len;
		} else {
			++literals;
			literal_len += len;
		}
	}
	std::cerr << ">> phrases = " << phrases << std::endl;
	std::cerr << ">> literals = " << literals << std::endl;
	std::cerr << ">> copies = " << copies << std::endl;
	std::cerr << ">> copy_len = " << copy_len << std::endl;
	std::cerr << ">> literal_len = " << literal_len << std::endl;
}

void write_parsing(std::string encoder_name, size_t orig_len, size_t comp_len, byte *parsing, char *file_name)
{
	auto pack_info = pack(encoder_name, orig_len, comp_len);
	byte *begin = std::get<2>(unpack(pack_info.parsing.get()));
	std::copy(parsing, parsing + comp_len, begin);
	std::ofstream file;
	open_file(file, file_name);
	write_file<byte>(file, pack_info.parsing.get(), pack_info.data_len);
}

std::uint64_t get_dec_time(std::string encoder, byte *parsing, byte *dest, size_t orig_len, size_t comp_len)
{
	// Get a temporary file
	char tmp_file[] = "./parsing-XXXXXX";
	mkstemp(tmp_file);

	std::cerr << "Temp file = " << tmp_file << std::endl;

	// Write the parsing on disk
	write_parsing(encoder, orig_len, comp_len, parsing, tmp_file);

	// Decompress it
	const int argc = 4;
	const char *argv[] = {"", "", tmp_file, "/dev/null"};
	char *m_argv[argc];

	std::vector<char> string_storage(argc + std::strlen(argv[2]) + std::strlen(argv[3]));
	auto storage_ptr = string_storage.data();

	for (auto i = 0U; i < argc; i++) {
		m_argv[i] = storage_ptr;
		storage_ptr = std::copy(argv[i], argv[i] + std::strlen(argv[i]), storage_ptr);
		*storage_ptr++ = '\0';
	}
	if (storage_ptr != string_storage.data() + string_storage.size()) {
		std::cerr << "ERROR: storage_ptr = " << storage_ptr - string_storage.data() << ", L = " << string_storage.size() << std::endl;
		exit(1);
	}

	auto to_ret = decompress_file("decompress", argc, m_argv, std::cerr);

	// Remove the file
	remove(tmp_file);

	// Return
	return to_ret;
}

/**
 * @brief Returns the couple (fixed time, variable time) of encoder's literal time cost.
 * @param encoder
 *		Encoder's name
 * @return
 *		The couple (fixed time, variable time)
 */
std::tuple<nanosecs_t, nanosecs_t> literal_decode_time(const char *encoder)
{
	// First: get encoder's literal max length
	const size_t min_lit = 4;
	const size_t one_gig = 1024 * 1024 * 1024;
	size_t max_lit_len = encoders_().get_literal_len(encoder);
	const size_t l_2 = one_gig / max_lit_len;
	const size_t l_1 = min_lit * l_2;
	const size_t min_lit_len = one_gig / l_1;

	std::vector<byte> literal_buffer(one_gig);

	std::cerr << "Max literal length = " << max_lit_len << std::endl;

	if (max_lit_len <= min_lit) {
		std::cerr << "Generating all-literal parsing" << std::endl;
		uniform_literal_it 	begin = uniform_literal_it::start(max_lit_len, one_gig), 
							end = uniform_literal_it::end(one_gig);
		auto dummy_parsing = get_parsing(encoder, begin, end, literal_buffer.data());
		print_stats(dummy_parsing.parsing.get(), one_gig, encoder);
		auto t = get_dec_time(encoder, dummy_parsing.parsing.get(), literal_buffer.data(), one_gig, dummy_parsing.size);
		// t = decompress_raw(encoder, dummy_parsing.parsing.get(), literal_buffer.data(), sixteen_mega);
		std::cerr << "T = " << std::fixed << t << " nsec (" << std::fixed << t / 1000000 << " msec)" << std::endl;
		return std::make_tuple(nanosecs_t(max_lit_len * t / one_gig), nanosecs_t(0));
	}

	// Compute a 1GiB parsing with only 1-char literals
	double t_1, t_2;
	{
		uniform_literal_it 	begin = uniform_literal_it::start(min_lit_len, one_gig), 
							end = uniform_literal_it::end(one_gig);
		auto dummy_parsing = get_parsing(encoder, begin, end, literal_buffer.data());
		print_stats(dummy_parsing.parsing.get(), one_gig, encoder);
		t_1 = get_dec_time(encoder, dummy_parsing.parsing.get(), literal_buffer.data(), one_gig, dummy_parsing.size);
		// t_1 = decompress_raw(encoder, dummy_parsing.parsing.get(), literal_buffer.data(), sixteen_mega);
		std::cerr << "T_1 = " << std::fixed << t_1 << " nsec (" << std::fixed << t_1 / 1000000 << " msec)" << std::endl;
	}

	// Compute a 1GiB parsing with maximum-length literals
	if (max_lit_len <= 1) {
		return std::make_tuple(nanosecs_t(t_1 / one_gig), nanosecs_t(0.0));
	}

	auto edges = std::ceil(1.0 * one_gig / max_lit_len);
	{
		uniform_literal_it	begin = uniform_literal_it::start(max_lit_len, one_gig), 
							end = uniform_literal_it::end(one_gig);
		auto dummy_parsing = get_parsing(encoder, begin, end, literal_buffer.data());
		// TODO: Anche qui
		print_stats(dummy_parsing.parsing.get(), one_gig, encoder);
		t_2 = get_dec_time(encoder, dummy_parsing.parsing.get(), literal_buffer.data(), one_gig, dummy_parsing.size);
		std::cerr << "T_2 = " << std::fixed << t_2 << " nsec (" << std::fixed << t_2 / 1000000 << " msec)" << std::endl;
	}

	auto fix_cost  	 = (t_1 - t_2) / (l_1 - l_2);
	auto var_cost 	 = (t_1 - l_1 * fix_cost) / one_gig;
	// auto fix_cost = (t_1 - t_2) / (one_gig - edges);
	// auto var_cost = (t_1 - one_gig * fix_cost) / one_gig;
	// auto fix_cost = ((one_gig / sixteen_mega) * t_1 - t_2) / (one_gig - gig_lits);
	// auto var_cost = (t_2 - sixteen_mega * fix_cost) / one_gig;

	return std::make_tuple(nanosecs_t(fix_cost), nanosecs_t(var_cost));
}

template <typename copier_>
nanosecs_t bmp_bench(size_t copy_length, nanosecs_t copy_time)
{
	using namespace std::chrono;
	// Measure time needed by a giant copy
	const size_t copies_no		= 10 * std::kilo::num;
	const size_t offset			= 8;
	copier_ copy;

	std::vector<byte> data(copy_length + offset);
	std::fill(data.begin(), data.end(), 0);

	// Now fracture that copy into a thousand copies
	std::cerr << "Generating copy points" << std::endl;
	std::vector<unsigned int> copies;
	std::random_device rd;
	std::uniform_int_distribution<unsigned int> dist(0, copy_length);
	for (unsigned int i = 0; i < copies_no; i++) {
		unsigned int sample;
		do {
			sample = dist(rd);
		} while (data[sample] == 1);
		data[sample] = 1;
		copies.push_back(sample);
	}
	std::sort(copies.begin(), copies.end());
	copies.push_back(copy_length);
	for (unsigned int i = copies.size() - 1; i > 0; i--) {
		copies[i] -= copies[i-1];
	}

	std::cerr << "Performing copies" << std::endl;
	auto t1 = high_resolution_clock::now();
	byte *dest = data.data() + offset;
	byte *src = data.data();
	for (auto i : copies) {
		copy(dest, src, i);
	}
	auto t2 = high_resolution_clock::now();
	nanosecs_t thousand_copies = std::chrono::duration_cast<nanosecs_t>(t2 - t1);

	std::cerr << "Performing empty copies" << std::endl;
	size_t real_copy = 0;
	t1 = high_resolution_clock::now();
	for (auto i : copies) {
		real_copy += i;
	}
	t2 = high_resolution_clock::now();
	auto empty_thousand_copies = std::chrono::duration_cast<nanosecs_t>(t2 - t1);

	nanosecs_t est_time = copy_time * copy_length;
	std::cerr << "Estimated time\t= " << est_time.count() << std::endl;
	std::cerr << "Thousand copies\t= " << thousand_copies.count() << std::endl;
	std::cerr << "Empty loop = " << empty_thousand_copies.count() << std::endl;
	std::cerr << "Real length = " << real_copy << std::endl;
	std::cerr << "Estimated copy length = " << copy_length << std::endl;
	std::cerr << "Copies = " << copies.size() << std::endl;

	if (thousand_copies < est_time + empty_thousand_copies) {
		return nanosecs_t(0);
	}

	return (thousand_copies - est_time - empty_thousand_copies) / copies_no;
}

nanosecs_t lit_pen_time(const char *encoder, nanosecs_t phrase_decode, nanosecs_t lit_fix)
{
	// Generate a parsing with a mix of 80% phrases and 20% literals
	const size_t length = 5 * std::mega::num;
	std::vector<edge_t> dummy_parsing;
	dummy_parsing.push_back(edge_t(1));
	std::random_device rd;
	std::uniform_real_distribution<double> dist(0, 1);
	unsigned int phrases = 0, literals = 0;
	while (dummy_parsing.size() < length) {
		double dice = dist(rd);
		if (dice < 0.8) {
			dummy_parsing.push_back(edge_t(1, 1, 0));
			phrases++;
		} else {
			dummy_parsing.push_back(edge_t(1));
			literals++;
		}
	}
	std::vector<byte> lit_buf(length, 0);
	auto p_t = get_parsing(encoder, ITERS(dummy_parsing), lit_buf.data());

	auto t1 = std::chrono::high_resolution_clock::now();
	decompress_raw<empty_copy>(encoder, p_t.parsing.get(), lit_buf.data(), length);
	auto t2 = std::chrono::high_resolution_clock::now();
	auto spent_time = std::chrono::duration_cast<nanosecs_t>(t2 - t1);

	auto exp_time = phrase_decode * phrases + lit_fix * literals;

	if (spent_time < exp_time) {
		return nanosecs_t(0.0);
	}
	return (spent_time - exp_time) / length;
}

cost_model get_cm(
		std::vector<std::tuple<unsigned int, nanosecs_t>> latencies, nanosecs_t copy_time,
		time_matrix phrase_times,
		nanosecs_t lit_fix, nanosecs_t lit_var,
		nanosecs_t bmp_time, nanosecs_t lit_penalty_time, double max_factor//, size_t line_boundary
)
{

	// Multiply latency distances by 1024
	{
		auto &last_latency_dst = std::get<0>(latencies.back());
		auto max_last_dst = std::numeric_limits<unsigned int>::max() / 1024;
		last_latency_dst = std::min(last_latency_dst, max_last_dst);
	}
	std::transform(ITERS(latencies), latencies.begin(),
		[] (std::tuple<unsigned int, nanosecs_t> t)
		{
			return std::make_tuple(std::get<0>(t) * 1024, std::get<1>(t));
		}
	);

	// First, prune latencies which are not covered by encoder distances
	{
		auto dst_max = phrase_times.dsts.back();
		auto ptr = std::lower_bound(
			ITERS(latencies), dst_max, 
			[] (std::tuple<unsigned int, nanosecs_t> t, unsigned int v) {
				return std::get<0>(t) < v;
			}
		);
		std::get<0>(*ptr) = dst_max;
		latencies.erase(std::next(ptr, 1), latencies.end());
	}

	// Let's understand how many distances we've got
	std::vector<unsigned int> distances = phrase_times.dsts;
	for (auto i : latencies) {
		distances.push_back(std::get<0>(i));
	}
	std::sort(ITERS(distances));
	distances.erase(std::unique(ITERS(distances)), distances.end());

	// Lengths are given by phrases and by the cache line-boundary.
	static const size_t cache_line = 64U;
	std::vector<unsigned int> lengths = phrase_times.lens;
	double step = 1.0 / 8;
	for (auto i = 1U; i < (max_factor - 1) / step; i++) {
		lengths.push_back(cache_line * step * i);
	}

	std::sort(ITERS(lengths));
	lengths.erase(std::unique(ITERS(lengths)), lengths.end());

	// Now we should fill the cost matrix
	cost_matrix cm(distances.size(), lengths.size());

	// Put the BMP time.
	for (auto i = 0U; i < cm.dsts(); i++) {
		for (auto j = 0U; j < cm.lens(); j++) {
			cm(i, j) = bmp_time.count();
		}
	}

	// first, latencies.
	unsigned int latest = 0;
//	unsigned int line_boundary_idx = std::distance(lengths.begin(), std::upper_bound(ITERS(lengths), line_boundary));
	for (auto i : latencies) {
		unsigned int now;
		nanosecs_t time;
		std::tie(now, time) = i;
		unsigned int low_dst_idx	= std::distance(distances.begin(), std::upper_bound(ITERS(distances), latest));
		unsigned int high_dst_idx	= std::distance(distances.begin(), std::lower_bound(ITERS(distances), now));
		for (unsigned int dst_idx = low_dst_idx; dst_idx <= high_dst_idx; dst_idx++) {
			// Put the single miss
			for (unsigned int len_idx = 0; len_idx < cm.lens(); len_idx++) {
				double factor = std::min<double>(max_factor, 1 + 1.0 * lengths[len_idx] / cache_line);
				cm(dst_idx, len_idx) += factor * time.count(); //(len_idx >= line_boundary_idx) ? 2 * time.count() : time.count();
				// Put the second miss for lengths greater than the line boundary
			}
		}
		latest = now;
	}

	// Second, phrase decode times.
	for (auto dst_idx = 0U; dst_idx < phrase_times.dsts.size(); dst_idx++) {
		for (auto len_idx = 0U; len_idx < phrase_times.lens.size(); len_idx++) {
			auto first_dst_idx = 0U;
			if (dst_idx > 0) {
				first_dst_idx = std::distance(
					distances.begin(), 
					std::find(ITERS(distances), phrase_times.dsts[dst_idx - 1])
				);
			}
			auto last_dst_idx = std::distance(
				distances.begin(),
				std::find(ITERS(distances), phrase_times.dsts[dst_idx])
			);

			auto first_len_idx = 0U;
			if (len_idx > 0) {
				first_len_idx = std::distance(
					lengths.begin(), 
					std::find(ITERS(lengths), phrase_times.lens[len_idx - 1])
				);
			}
			auto last_len_idx = std::distance(
				lengths.begin(),
				std::find(ITERS(lengths), phrase_times.lens[len_idx])
			);

			for (auto i = first_dst_idx; i <= last_dst_idx; i++) {
				for (auto j = first_len_idx; j <= last_len_idx; j++) {
					cm(i, j) += phrase_times.cm(dst_idx, len_idx);
				}
			}

		}
	}

	// Now we have everything to build the cost_model
	lit_fix += lit_penalty_time + bmp_time;
	lit_var -= copy_time;

	return cost_model(distances, lengths, cm, lit_fix.count(), lit_var.count(), copy_time.count());
}

int main(int argc, char **argv)
{
	using std::tuple;
	using std::vector;

	if (argc < 3) {
		std::cerr << "Latency file and encoder needed" << std::endl;
		exit(1);
	}

	const char *latency_file = *++argv;
	const char *encoder = *++argv;

	auto latencies = get_latencies(latency_file);

	std::cerr << "Memory latencies:" << std::endl;
	for (auto i : latencies) {
		unsigned int end;
		nanosecs_t latency;
		std::tie(end, latency) = i;
		std::cerr << end << "\t" << latency.count() << std::endl;
	}

	cost_model cm = encoders_().get_cm(encoder);

	// PHRASE DECODE TIME
	std::cerr << "Measuring phrase decode time." << std::endl;
	auto phrase_times = phrase_decode_time(encoder);
	reduce_tm(&phrase_times);

	for (unsigned int dst_idx = 0; dst_idx < phrase_times.dsts.size(); dst_idx++) {
		for (unsigned int len_idx = 0; len_idx < phrase_times.lens.size(); len_idx++) {
			std::cerr 	<< "D = " 	<< phrase_times.dsts[dst_idx] << ", "
						<< "L = " 	<< phrase_times.lens[len_idx] << ", "
						<< "LAT = "	<< phrase_times.cm(dst_idx, len_idx) << std::endl;
		}
	}

	// LITERAL DECODE TIME
	std::cerr << "Measuring literal decode time." << std::endl;
	nanosecs_t lit_fix, lit_var;
	std::tie(lit_fix, lit_var) = literal_decode_time(encoder);
	std::cerr << "Literal fix time = " << lit_fix.count() << ", var time = " << lit_var.count() << std::endl;


	// COPY TIME
	// const size_t copy_bench_len = 1024 * 1024 * 1024; // One gigabyte
	// auto time = bench_copy<fast_copy>(copy_bench_len);
	// auto copy_time = time / copy_bench_len;
	auto copy_time = lit_var;

	// COPY ENTER TIME
	// Strategia: prendi il tempo per copia di un byte, poi fai X copie da Y caratteri, vedi quanto tempo
	// ci avresti messo a fare X * Y copie, sottrai ed ottieni il tempo per enter/leave.
	decltype(std::mega::num) max_copy = cm.get_len().back();
	auto vector_size = std::min(max_copy, 50 * std::mega::num);
	auto bmp_time = bmp_bench<fast_copy>(vector_size, copy_time);
	std::cerr << "Copy enter time = " << bmp_time.count() << " nsecs" << std::endl;

	// LITERAL PENALTY TIME
	auto lit_penalty_time = lit_pen_time(encoder, nanosecs_t(phrase_times.cm(0,0)), lit_fix);
	std::cerr << "Lit BMP time = " << lit_penalty_time.count() << std::endl;

	// TODO: output model
	std::cerr << "Output model into standard output" << std::endl;
//	const size_t line_size = 64;

	double max_factor = 1.5;
	if (argc >= 5) {
		max_factor = std::stof(*++argv);
	}

	auto time_cm = get_cm(
		latencies, copy_time, phrase_times, lit_fix, lit_var, bmp_time, lit_penalty_time, max_factor
	); //, line_size);
	std::cout << wm_serialize(time_cm);
}
