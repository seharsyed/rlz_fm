#include "rlz_algo_char.h"
#include "fm_wrapper.h"
#include <fstream>
#include <cstdlib>
#include <sdsl/bit_vectors.hpp>
#include <sdsl/suffix_arrays.hpp>
#include <sdsl/int_vector.hpp>
#include <vector>
#include <tuple>
#include <cstdio>
#include <stack>
#include <map>
#include <omp.h>
#include "spdlog/spdlog.h"
#include "spdlog/stopwatch.h"
#include <chrono>

std::chrono::duration<double> backward_match_time_char{0.0};
std::chrono::duration<double> sa_time_char{0.0};
std::chrono::duration<double> serialize_time_char{0.0};

/**
* @brief Constructor of RLZ_CHAR class
* @param[in] ref_file [string] Path to reference file
*/

RLZ_CHAR::RLZ_CHAR(const std::string ref_file): ref_file(ref_file){}

/**
* @brief Constuctor of RLZ_CHAR class.
*
* Assigns the ref_file and seq_file variables with the file paths
*
* @param[in] ref_file [string] Path to reference file 
* @param[in] seq_file [string] Path to sequence file
*/

RLZ_CHAR::RLZ_CHAR(const std::string ref_file, const std::string seq_file): ref_file(ref_file), seq_file(seq_file){}

/**
* @brief Destructor of RLZ_CHAR class.
*
* Currently does nothing.
*
*/

RLZ_CHAR::~RLZ_CHAR(){}

/**
* @brief Loads the file content into a string.
*
* Loads the file content directly into a string. Opens the input file in binary mode
* and moves pointer at end of file to get file size quickly. We then resize the string
* to be large enough to hold the file content in bytes.   
*
* @param[in] input_file [string] Path to either the reference or sequence file 
* @param[in] content [string] Where the string representation of the file is located.
* @return void
*/

void RLZ_CHAR::load_file_to_string(const std::string& input_file, std::string& content)
{
    spdlog::stopwatch sw_convert;
    spdlog::debug("Storing file as string");

    // std::ios::ate moves cursor to end of file
    std::ifstream file(input_file, std::ios::binary | std::ios::ate);
    if (!file) {
        spdlog::error("Error opening {}", input_file);
        std::exit(EXIT_FAILURE);
    }

    // Get the file size in bytes
    std::streamsize file_size = file.tellg();
    // std::ios::beg moves cursor to beginning
    file.seekg(0, std::ios::beg);

    // Resize the content field
    content.resize(file_size);

    // Load the file as string
    if (file.read(&content[0], file_size)) {
        spdlog::debug("File read successfully.");
    } else {
        spdlog::error("Error reading file: {}", input_file);
        std::exit(EXIT_FAILURE);
    }

    file.close();
    auto sw_convert_elapsed = sw_convert.elapsed();
    spdlog::debug("Finished storing file in {:.3} seconds", sw_convert_elapsed.count());

    // spdlog::stopwatch sw_save;
    // spdlog::debug("Saving bit array sdsl object to file");
    //// Save the bit vector to a file
    // sdsl::store_to_file(bit_array, input_file + ".sdsl");
    // auto sw_save_elapsed = sw_save.elapsed();
    // spdlog::debug("Finished saving in {:.3} seconds", sw_save_elapsed.count());
}


/**
* @brief Loads the reversed file content into a string.
*
* Loads the reversed file content directly into a string. Opens the input file in binary mode
* and moves pointer at end of file to get file size quickly. We then resize the string
* to be large enough to hold the file content in bytes.  
*
* @param[in] input_file [string] Path to either the reference or sequence file 
* @param[in] content [string] Where the string representation of the file is located.
* @return void
*/

void RLZ_CHAR::load_reverse_file_to_string(const std::string& input_file, std::string& content)
{
    spdlog::stopwatch sw_convert;
    spdlog::debug("Storing file as string");

    // std::ios::ate moves cursor to end of file
    std::ifstream file(input_file, std::ios::binary | std::ios::ate);
    if (!file) {
        spdlog::error("Error opening {}", input_file);
        std::exit(EXIT_FAILURE);
    }

    // Get the file size in bytes
    std::streamsize file_size = file.tellg();
    // std::ios::beg moves cursor to beginning
    file.seekg(0, std::ios::beg);

    // Resize the content field
    content.resize(file_size);

    // Efficiently read and build the reversed string
    for (std::streamsize i = 0; i < file_size; ++i) {
        char byte;
        file.get(byte); // Read a byte from the file
        content[file_size - 1 - i] = byte; // Place it at the reverse position
    }

    spdlog::debug("File read successfully.");

    file.close();
    auto sw_convert_elapsed = sw_convert.elapsed();
    spdlog::debug("Finished storing file in {:.3} seconds", sw_convert_elapsed.count());

    // spdlog::stopwatch sw_save;
    // spdlog::debug("Saving bit array sdsl object to file");
    //// Save the bit vector to a file
    // sdsl::store_to_file(bit_array, input_file + ".sdsl");
    // auto sw_save_elapsed = sw_save.elapsed();
    // spdlog::debug("Finished saving in {:.3} seconds", sw_save_elapsed.count());
}

/**
* @brief Parses the sequence file in relation to the reference file
*
* This function does the RLZ_CHAR parsing of the sequence file. It parses relative to the original file content of the reference.
* Fails if there is a character in the sequence that is not present in the reference.
*
* RLZ algorithm tries to greedily find the longest sequence substring match within the reference.
* The RLZ parse in the end contains (pos, len) pairs
* in relation to the reference such that the sequence can be reconstructed from only the RLZ parse and the reference
* file. It is a O(n) algorithm. The size is the reference file + the RLZ parse.
*
* The algorithm implemented here is as follows.
* 1. Starting from the last char of the reversed sequence file or sequence file chunk, check if char matches the reversed reference
* (via backwards match with FM-index) 
* 2a. If match, check if next char also matches (ex. aab. I know that b matches then check if ab matches etc...)
* 2b. If match and end of sequence file or sequence file chunk, push current (pos,len) pair to parse vector
* 2c. If mismatch, push (prev pos, len - 1) to parse stack. Reset search from char that caused mismatch.
*
*
* @param [in] fm_index [sdsl::csa_wt<sdsl::wt_huff<sdsl::rrr_vector<15>>, 16, 32>] the fm-index of the reference
* @param [in] fm_support [FM_Wrapper] Utility object that allows us to do search and locate queries with fm-index.
* @param [in] seq_parse_vec_vec [std::vector<std::vector<std::tuple<uint64_t, uint64_t>>>] empty RLZ_CHAR parse vectors equal to number of threads
* @param [in] num_char_to_process [size_t] the number of chars that should be processed. Useful for the OpenMP parallelization.
* @param [in] loop_iter [size_t] the loop iteration. Useful for OpenMP and making sure we are thread-safe.
* @param [in] num_threads [size_t] the total number of threads allocated.
*
* @return void
*/

void RLZ_CHAR::parse(const sdsl::csa_wt<sdsl::wt_huff<sdsl::rrr_vector<15>>, 16, 32>& fm_index,
        FM_Wrapper& fm_support,
        const std::map<char, uint64_t>& occs, 
        const std::string& seq_content,
        std::vector<std::vector<std::tuple<uint64_t, uint64_t>>>& seq_parse_vec_vec,
        size_t num_char_to_process,
        size_t loop_iter,
        size_t num_threads)
{
    std::string pattern = "";
    size_t prev_left = 0;
    size_t prev_right = fm_index.bwt.size();
    size_t next_left = 0;
    size_t next_right = fm_index.bwt.size();
    long long int seq_size = static_cast<long long int>(seq_content.size());

    long long int start_loc = (seq_size - 1) - (loop_iter * num_char_to_process);
    long long int end_loc;

    // Last chunk so process remaining chars
    if (loop_iter == num_threads - 1)
        end_loc = -1;
    // Process chars up to next chunk
    else
        end_loc = (seq_size - 1) - ((loop_iter + 1) * num_char_to_process);

    // Process the file in reverse for backwards matching with FM-index.
    for (long long int i = start_loc; i > end_loc; i--) 
    {
        char next_char = seq_content[i];

        pattern = next_char + pattern;

        std::tuple<size_t,size_t> previous_ranges = std::make_tuple(prev_left, prev_right);
        auto back_start = std::chrono::high_resolution_clock::now();
        std::tuple<size_t,size_t> next_ranges = fm_support.backward_match(fm_index, occs, previous_ranges, next_char);
        auto back_end = std::chrono::high_resolution_clock::now();
        backward_match_time_char += back_end - back_start;
        next_left = std::get<0>(next_ranges);
        next_right = std::get<1>(next_ranges);

        // If same then that means no perfect match so we reset.
        if (next_left == next_right){
            uint64_t pattern_len = pattern.size() - 1; // -1 due to not matching the last character successfully
            auto sa_start = std::chrono::high_resolution_clock::now();
            uint64_t sa_pos = fm_support.get_suffix_array_value(fm_index, prev_left);
            uint64_t mirrored_sa_pos = fm_index.bwt.size() - 1 - sa_pos; // 0 based involution formula of sa position to correct for the reverse string matching (will give pos in ref where pattern ends)
            uint64_t adjusted_sa_pos = mirrored_sa_pos - pattern_len; // adjust the position to where pattern starts
            auto sa_end = std::chrono::high_resolution_clock::now();
            sa_time_char += sa_end - sa_start;
            seq_parse_vec_vec[loop_iter].emplace_back(std::make_tuple(adjusted_sa_pos, pattern_len));
            prev_left = 0;
            prev_right = fm_index.bwt.size();
            next_left = 0;
            next_right = fm_index.bwt.size();
            pattern = "";
            ++i;
        }
        // If at the end we are still in a perfect match, we save what we have. 
        else if (i == end_loc + 1)
        {
            uint64_t pattern_len = pattern.size();
            auto sa_start = std::chrono::high_resolution_clock::now();
            uint64_t sa_pos = fm_support.get_suffix_array_value(fm_index, next_left);
            uint64_t mirrored_sa_pos = fm_index.bwt.size() - 1 - sa_pos;
            uint64_t adjusted_sa_pos = mirrored_sa_pos - pattern_len;
            auto sa_end = std::chrono::high_resolution_clock::now();
            sa_time_char += sa_end - sa_start;
            seq_parse_vec_vec[loop_iter].emplace_back(std::make_tuple(adjusted_sa_pos, pattern_len));
        }
        // Currently in a perfect match
        else{
            prev_left = next_left;
            prev_right = next_right;
        }
    }
}


/**
* @brief Parses the sequence file in relation to the reference file
*
* This function does the RLZ_CHAR parsing of the sequence file. It parses relative to the original file content of the reference.
* Fails if there is a character in the sequence that is not present in the reference.
*
* RLZ algorithm tries to greedily find the longest sequence substring match within the reference.
* The RLZ parse in the end contains (pos, len) pairs
* in relation to the reference such that the sequence can be reconstructed from only the RLZ parse and the reference
* file. It is a O(n) algorithm. The size is the reference file + the RLZ parse.
*
* The algorithm implemented here is as follows.
* 1. Starting from the last char of the reversed sequence file or sequence file chunk, check if char matches the reversed reference
* (via backwards match with FM-index) 
* 2a. If match, check if next char also matches (ex. aab. I know that b matches then check if ab matches etc...)
* 2b. If match and end of sequence file or sequence file chunk, push current (pos,len) pair to parse vector
* 2c. If mismatch, push (prev pos, len - 1) to parse stack. Reset search from char that caused mismatch.
*
* We stream the sequence file in this function
*
* @param [in] fm_index [sdsl::csa_wt<sdsl::wt_huff<sdsl::rrr_vector<15>>, 16, 32>] the fm-index of the reference
* @param [in] fm_support [FM_Wrapper] Utility object that allows us to do search and locate queries with fm-index.
* @param [in] occs [std::map<char, uint64_t>] the number of occurences of each char in the ref file
* @param [in] seq_file [std::string] the sequence file.
* @param [in] seq_parse_vec [std::vector<std::tuple<uint64_t, uint64_t>>] empty RLZ_CHAR parse vector
*
* @return void
*/

void RLZ_CHAR::stream_parse(const sdsl::csa_wt<sdsl::wt_huff<sdsl::rrr_vector<15>>, 16, 32>& fm_index,
        FM_Wrapper& fm_support,
        const std::map<char, uint64_t>& occs, 
        const std::string& seq_file,
        std::vector<std::tuple<uint64_t, uint64_t>>& seq_parse_vec)
{
    std::string pattern = "";
    size_t prev_left = 0;
    size_t prev_right = fm_index.bwt.size();
    size_t next_left = 0;
    size_t next_right = fm_index.bwt.size();
    
    std::ifstream sfile(seq_file);
    if (!sfile) {
        spdlog::error("Error opening {}", seq_file);
        std::exit(EXIT_FAILURE);
    }

    bool retry = false;
    char next_char;

    // Process the file in reverse for backwards matching with FM-index.
    while (sfile)
    {
        if (!retry) {  // Read a new character only if we're not retrying a char
            sfile.get(next_char);
            if (sfile.eof()) break; // Exit if end of file
        }

        pattern = next_char + pattern;

        std::tuple<size_t,size_t> previous_ranges = std::make_tuple(prev_left, prev_right);
        auto back_start = std::chrono::high_resolution_clock::now();
        std::tuple<size_t,size_t> next_ranges = fm_support.backward_match(fm_index, occs, previous_ranges, next_char);
        auto back_end = std::chrono::high_resolution_clock::now();
        backward_match_time_char += back_end - back_start;
        next_left = std::get<0>(next_ranges);
        next_right = std::get<1>(next_ranges);

        // If same then that means no perfect match so we reset.
        if (next_left == next_right){
            uint64_t pattern_len = pattern.size() - 1; // -1 due to not matching the last character successfully
            auto sa_start = std::chrono::high_resolution_clock::now();
            uint64_t sa_pos = fm_support.get_suffix_array_value(fm_index, prev_left);
            uint64_t mirrored_sa_pos = fm_index.bwt.size() - 1 - sa_pos; // 0 based involution formula of sa position to correct for the reverse string matching (will give pos in ref where pattern ends)
            uint64_t adjusted_sa_pos = mirrored_sa_pos - pattern_len; // adjust the position to where pattern starts
            auto sa_end = std::chrono::high_resolution_clock::now();
            sa_time_char += sa_end - sa_start;
            seq_parse_vec.emplace_back(std::make_tuple(adjusted_sa_pos, pattern_len));
            prev_left = 0;
            prev_right = fm_index.bwt.size();
            next_left = 0;
            next_right = fm_index.bwt.size();
            pattern = "";
            retry = true;
        }
        // If at the end we are still in a perfect match, we save what we have. 
        else if (sfile.peek() == EOF)
        {
            uint64_t pattern_len = pattern.size();
            auto sa_start = std::chrono::high_resolution_clock::now();
            uint64_t sa_pos = fm_support.get_suffix_array_value(fm_index, next_left);
            uint64_t mirrored_sa_pos = fm_index.bwt.size() - 1 - sa_pos;
            uint64_t adjusted_sa_pos = mirrored_sa_pos - pattern_len;
            auto sa_end = std::chrono::high_resolution_clock::now();
            sa_time_char += sa_end - sa_start;
            seq_parse_vec.emplace_back(std::make_tuple(adjusted_sa_pos, pattern_len));
            retry = false;
        }
        // Currently in a perfect match
        else{
            prev_left = next_left;
            prev_right = next_right;
            retry = false;
        }
    }
    sfile.close();
}


/** 
* @brief Calculates the occurances of each char in the provided text in lexicographical order
*
* @param [in] content [string] The string which we are deriving the occurances from
*
* @return void
*/

void RLZ_CHAR::calculate_occs(std::string content, std::map<char, uint64_t>& occs)
{
    // Sort the string lexicographically
    std::sort(content.begin(), content.end());
    uint64_t count = 0;
    char prev_char = '\0';

    for (size_t i = 0; i < content.size(); i++)
    {
        if (prev_char != content[i])
        {
            occs[content[i]] = count;
        }

        prev_char = content[i];
        count++;
    }
}


/**
* @brief Compresses the sequence file in relation to the reference file.
*
* Creates a FM-index from the reversed reference string which we query using the reversed sequence string in order to simulate forward matching.
* We first create the FM-index from the reveresed string representation of the reference.
* We query the index one char at time from the reversed sequence string. When the sequence char does not have a match, 
* we add the last matching ref position of the sequence and the length of the match to the parse. Then we 
* restart the match at the last mismatch position. The parse is ultimately
* stored in a vector in the correct order. The parse at the end is serialized to a file.
*
* @param [in] threads [int] The number of threads provided by the user.
*
* @return void
*
* @warning Providing multiple threads changes the output of the RLZ_CHAR parse slightly. 
* Might create two phrases at chunk boundaries if phrase spans chunk boundary. For proper RLZ_CHAR parse should run with 1 thread.
* 
* @warning Will fail if the sequence file contains a char not present in the reference file
*
*/

void RLZ_CHAR::compress(int threads)
{
    sdsl::csa_wt<sdsl::wt_huff<sdsl::rrr_vector<15>>, 16, 32> fm_index;
    
    // Creates the FM-index
    construct_im(fm_index, ref_content, 1);

    // Get the number of occurances of each char in lexicographical order
    std::map<char, uint64_t> occs;
    calculate_occs(ref_content, occs);

    FM_Wrapper fm_support;

    std::vector<std::vector<std::tuple<uint64_t, uint64_t>>> seq_parse_vec_vec(threads);
    size_t num_char_to_process = seq_content.size() / threads;  // Integer division

    // Comment (Testing only)
    // bits_to_str(seq_bit_array, ".orig.bits");

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < threads; i++)
    {
        parse(fm_index, fm_support, occs, seq_content, seq_parse_vec_vec, num_char_to_process, i, threads);
    }

    spdlog::debug("Total FM-index time (s): {:.6f}", std::chrono::duration<double>(backward_match_time_char).count());
    spdlog::debug("Total SA time (s): {:.6f}", std::chrono::duration<double>(sa_time_char).count());

    // Store tuples of (pos,len) in correct order in vector
    size_t chars_stored = 0;
    std::vector<std::tuple<uint64_t, uint64_t>> seq_parse;
    // Can process the parse vectors sequentially since the first vector contains the parse of the start of the non-reversed sequence.
    for (int i = 0; i < threads; i++)
    {
        for (int j = 0; j < seq_parse_vec_vec[i].size(); j++)
        {
            chars_stored += std::get<1>(seq_parse_vec_vec[i][j]);
            seq_parse.emplace_back(seq_parse_vec_vec[i][j]);
            // spdlog::debug("Ref Pos: {}, Len: {}", std::get<0>(seq_parse.back()), std::get<1>(seq_parse.back()));
        }
    }

    spdlog::debug("The sequence was encoded in {} chars", seq_content.size());
    spdlog::debug("The rlz parse encodes for {} chars", chars_stored);

    auto serialize_start = std::chrono::high_resolution_clock::now();
    serialize(seq_parse, seq_file);
    auto serialize_end = std::chrono::high_resolution_clock::now();
    serialize_time_char += serialize_end - serialize_start;
    spdlog::debug("Total serialize time (s): {:.6f}", std::chrono::duration<double>(serialize_time_char).count());

    // Comment (Testing only)
    // print_serialize(seq_parse);
}


/**
* @brief Compresses the sequence file in relation to the reference file.
*
* Creates a FM-index from the reversed reference string which we query using the reversed sequence string in order to simulate forward matching.
* We first create the FM-index from the reveresed string representation of the reference.
* We query the index one char at time from the reversed sequence string. When the sequence char does not have a match, 
* we add the last matching ref position of the sequence and the length of the match to the parse. Then we 
* restart the match at the last mismatch position. The parse is ultimately
* stored in a vector in the correct order. The parse at the end is serialized to a file.
*
* We stream the sequence file in this function
*
* @param [in] seq_file [std::string] The sequence file 
*
* @return void
* 
* @warning Will fail if the sequence file contains a char not present in the reference file
*
*/

void RLZ_CHAR::stream_compress(const std::string& seq_file)
{
    sdsl::csa_wt<sdsl::wt_huff<sdsl::rrr_vector<15>>, 16, 32> fm_index;
    
    // Creates the FM-index
    construct_im(fm_index, ref_content, 1);

    // Get the number of occurances of each char in lexicographical order
    std::map<char, uint64_t> occs;
    calculate_occs(ref_content, occs);

    FM_Wrapper fm_support;

    std::vector<std::tuple<uint64_t, uint64_t>> seq_parse_vec;

    stream_parse(fm_index, fm_support, occs, seq_file, seq_parse_vec);
    
    spdlog::debug("Total FM-index time (s): {:.6f}", std::chrono::duration<double>(backward_match_time_char).count());
    spdlog::debug("Total SA time (s): {:.6f}", std::chrono::duration<double>(sa_time_char).count());

    // Store tuples of (pos,len) in correct order in vector
    size_t chars_stored = 0;
    std::vector<std::tuple<uint64_t, uint64_t>> seq_parse;
    // Can process the parse vectors sequentially since the first vector contains the parse of the start of the non-reversed sequence.
    for (int i = 0; i < seq_parse_vec.size(); i++)
    {
        chars_stored += std::get<1>(seq_parse_vec[i]);
        seq_parse.emplace_back(seq_parse_vec[i]);
        // spdlog::debug("Ref Pos: {}, Len: {}", std::get<0>(seq_parse.back()), std::get<1>(seq_parse.back()));
    }
    
    // Get file size of sequence file
    std::ifstream sfile(seq_file, std::ios::ate);
    if (!sfile) {
        spdlog::error("Error opening {}", seq_file);
        std::exit(EXIT_FAILURE);
    }

    // Get the file size in bytes
    std::streamsize sfile_size = sfile.tellg();
    sfile.close();    

    spdlog::debug("The sequence was encoded in {} chars", sfile_size);
    spdlog::debug("The rlz parse encodes for {} chars", chars_stored);

    auto serialize_start = std::chrono::high_resolution_clock::now();
    serialize(seq_parse, seq_file);
    auto serialize_end = std::chrono::high_resolution_clock::now();
    serialize_time_char += serialize_end - serialize_start;
    spdlog::debug("Total serialize time (s): {:.6f}", std::chrono::duration<double>(serialize_time_char).count());

    // Comment (Testing only)
    // print_serialize(seq_parse);
}


/**
* @brief Serializes the parse of the sequence file
*
* The sequence parse contains tuples (ref pos, size) that can reconstruct the sequence file given the reference.
* We serialize the parse vector into binary file called seq_file_name.rlz
*
* File content of the .rlz file
* (uint64_t byte: size num of pair) (uint64_t byte: size pos) (uint64_t byte: size len) (uint64_t byte: size pos) (uint64_t byte: size len) ...
*  
* @param[in] seq_parse [std::vector<std::tuple<uint64_t, uint64_t>>] The parse of the seq <(ref pos,len),(ref pos,len),(ref pos,len)... >
* @param[in] seq_file [std::string] the sequence file name
*
* @return void
*/

void RLZ_CHAR::serialize(const std::vector<std::tuple<uint64_t, uint64_t>>& seq_parse, const std::string& seq_file)
{
    std::ofstream ofs(seq_file + ".rlz", std::ios::binary);
    if (!ofs) {
        spdlog::error("Error opening {}", seq_file + ".rlz");
        std::exit(EXIT_FAILURE);
    }
    size_t size = seq_parse.size();
    ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
    for (size_t i = 0; i < size; i++)
    {
        ofs.write(reinterpret_cast<const char*>(&std::get<0>(seq_parse[i])), sizeof(uint64_t));
        ofs.write(reinterpret_cast<const char*>(&std::get<1>(seq_parse[i])), sizeof(uint64_t));
    }
    ofs.close();
}


/**
* @brief Deserializes the parse of the sequence file
*
* Decompress seq_file_name.rlz into tuple vector <(ref pos,len),(ref pos,len),(ref pos,len)... > . 
* Return the vector.
*
* @return Return the vector.
*/

std::vector<std::tuple<uint64_t, uint64_t>> RLZ_CHAR::deserialize()
{
    std::ifstream ifs(seq_file + ".rlz", std::ios::binary);
    if (!ifs) {
        spdlog::error("Error opening {}", seq_file + ".rlz");
        std::exit(EXIT_FAILURE);
    }
    uint64_t size;
    std::vector<std::tuple<uint64_t, uint64_t>> seq_parse;

    ifs.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
    seq_parse.reserve(size);
    std::tuple<uint64_t, uint64_t> elem;
    uint64_t val;

    for (size_t i = 0; i < 2 * size; i++)
    {
        ifs.read(reinterpret_cast<char*>(&val), sizeof(uint64_t));
        if (i % 2 == 0){
            std::get<0>(elem) = val;
        }
        else{
            std::get<1>(elem) = val;
            seq_parse.emplace_back(elem);
        }
    }
    ifs.close();

    return seq_parse;
}

/**
* @brief Decompresses the sequence parse back to the original sequence file
*
* The sequence parse contains tuples (ref pos, size) that can reconstruct the sequence file given the reference.
* We read the ref position and length from each tuple in the sequence parse and get the corresponding chars from the reference to reconstruct the sequence
*
* @warning might have to change int to long long int depending on size
*/

void RLZ_CHAR::decompress()
{
    std::vector<std::tuple<uint64_t, uint64_t>> seq_parse = deserialize();
    
    size_t char_size = 0;
    for (const auto& [pos, len] : seq_parse){
        char_size += len;
    }

    spdlog::debug("The compessed sequence file had {} chars", char_size);

    // Resize the array to be equal to the number of bits to be stored
    seq_content.resize(char_size);

    // Get the sequence chars from the parse + reference chars
    size_t prev_pos = 0;
    for (const auto& [pos, len] : seq_parse){
        size_t curr_pos = prev_pos + len;
        size_t len_count = 0;
        for (size_t i = prev_pos; i < curr_pos; ++i){
            seq_content[i] = ref_content[pos + len_count];
            len_count++;
        }
        prev_pos = curr_pos;
    }

    // Comment (Testing only)
    // bits_to_str(seq_bit_array, ".decompress.bits");

    std::ofstream output_file(seq_file + ".out");
    if (!output_file) {
        spdlog::error("Error opening {}", seq_file + ".out");
        std::exit(EXIT_FAILURE);
    }

    output_file << seq_content;
    output_file.close();
}


/**
* @brief Write the non-binary serialization of the sequence parse to a file.
*
* The sequence parse contains tuples (binary ref pos, size) that can reconstruct the sequence file given the reference.
* We write the non-binary serialization to a file. Testing purposes only.
* 
* @param[in] seq_parse [std::vector<std::tuple<uint64_t, uint64_t>>] The parse of the seq <(binary ref pos,len),(binary ref pos,len),(binary ref pos,len)... >
*
* @return void
*/

void RLZ_CHAR::print_serialize(const std::vector<std::tuple<uint64_t, uint64_t>>& seq_parse)
{
    std::ofstream ofs(seq_file + ".readable.rlz");
    if (!ofs) {
        spdlog::error("Error opening {}", seq_file + ".readable.rlz");
        std::exit(EXIT_FAILURE);
    }

    for (const auto& [pos, len] : seq_parse){
        ofs << "Position: " << pos << "  Length: " << len << std::endl;
    }

    ofs.close();
}


/**
* @brief Cleans some intermediate file
*
* Removes intermediate files for faster testing
*
* @return void
*/

void RLZ_CHAR::clean()
{
    std::remove((ref_file + ".sdsl").c_str());
    spdlog::info("Removed {} if it existed", ref_file + ".sdsl");
    std::remove((seq_file + ".sdsl").c_str());
    spdlog::info("Removed {} if it existed", seq_file + ".sdsl");
    std::remove((seq_file + ".rlz").c_str());
    spdlog::info("Removed {} if it existed", seq_file + ".rlz");
    std::remove((seq_file + ".readable.rlz").c_str());
    spdlog::info("Removed {} if it existed", seq_file + ".readable.rlz");
    std::remove((seq_file + ".orig.bits").c_str());
    spdlog::info("Removed {} if it existed", seq_file + ".orig.bits");
    std::remove((seq_file + ".decompress.bits").c_str());
    spdlog::info("Removed {} if it existed", seq_file + ".decompress.bits");
    std::remove((seq_file + ".out").c_str());
    spdlog::info("Removed {} if it existed", seq_file + ".out");
}


