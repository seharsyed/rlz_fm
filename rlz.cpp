#include <CLI11.hpp>
#include "rlz_algo.h"
#include "rlz_algo_char.h"
#include "spdlog/spdlog.h"
#include "spdlog/stopwatch.h"
#include <filesystem> // Note that this requires at least gcc 9

int main(int argc, char **argv) 
{
    CLI::App app("rlz - An implementation of RLZ that compresses a sequence file using a reference file.\n\nImplemented by Rahul Varki");

    std::string ref_file;
    std::string seq_file;
    std::string parse_file;
    bool decompress = false;
    int verbosity = 0;
    bool clean = false;
    bool bit = false;
    int threads = 1;
    std::string version = "Version: 1.0.0";

    app.add_option("-r,--ref", ref_file, "The reference file to be used for compression")->configurable()->required();
    app.add_option("-s,--seq", seq_file, "The sequence file to compress")->configurable();
    app.add_option("-p,--parse", parse_file, "The RLZ parse file to be decompressed.")->configurable();
    app.add_option("-t,--threads", threads, "Number of threads")->configurable();
    app.add_flag("-d,--decompress", decompress, "Decompress the RLZ parse into the original sequence file")->configurable();
    app.add_flag("--bit", bit, "Set if the reference does not contain all the unique sequence characters")->configurable();
    app.add_option("-v,--verbosity", verbosity, "Set verbosity level (0 = none, 1 = basic, 2 = detailed)")->default_val(0);
    app.set_version_flag("--version", version);
    app.footer("Example usage:\n"
           "  Default:\n"
           "    Compress: ./rlz -r reference.fasta -s sequence.fasta\n"
           "    Compress: ./rlz --ref reference.fasta --seq sequence.fasta\n"
           "    Decompress: ./rlz -r reference.fasta -p sequence.fasta.rlz -d\n"
           "    Decompress: ./rlz --ref reference.fasta --parse sequence.fasta.rlz --decompress\n"
           "  Bit-Level:\n"
           "    Compress: ./rlz -r reference.fasta -s sequence.fasta --bit\n"
           "    Compress: ./rlz --ref reference.fasta --seq sequence.fasta --bit\n"
           "    Decompress: ./rlz -r reference.fasta -p sequence.fasta.rlz -d --bit\n"
           "    Decompress: ./rlz --ref reference.fasta --parse sequence.fasta.rlz --bit --decompress\n");
    app.description("RLZ compression tool");
    CLI11_PARSE(app, argc, argv);

    if (verbosity == 2) {
        spdlog::set_level(spdlog::level::trace);
    }
    else if (verbosity == 1){
        spdlog::set_level(spdlog::level::debug);
    }
    else if (verbosity == 0){ 
        spdlog::set_level(spdlog::level::info);
    }
    else{
        spdlog::error("Set verbosity level to be 0,1,2");
    }

    if (bit)
    {
        if (decompress)
        {
            spdlog::info("Bit alphabet decompression enabled");
            if (app.count("-r") == 0){
                spdlog::error("Please provide the reference file used to compress the RLZ parse with -r or --ref");
                std::cout << app.help() << std::endl;
                return -1;
            }
            if (app.count("-p") == 0){
                spdlog::error("Please provide the RLZ parse to decompress with -p or --parse");
                std::cout << app.help() << std::endl;
                return -1;
            }
            spdlog::debug("Starting to decompress the compressed sequence file");
            spdlog::stopwatch sw;
            spdlog::stopwatch sw_parser;
            RLZ main_parser(ref_file);
            auto sw_parser_elapsed = sw_parser.elapsed();
            spdlog::debug("Built main parser in {:.3} seconds", sw_parser_elapsed.count());
            spdlog::debug("Starting to store the reference file as a bit vector");
            spdlog::stopwatch sw_ref;
            main_parser.load_file_to_bit_vector(ref_file, main_parser.ref_bit_array);
            auto sw_ref_elapsed = sw_ref.elapsed();
            spdlog::debug("Loaded file in {:.3} seconds", sw_ref_elapsed.count());
            spdlog::stopwatch sw_decompress;
            main_parser.decompress(parse_file);
            auto sw_decompress_elapsed = sw_decompress.elapsed();
            auto elapsed = sw.elapsed();
            spdlog::debug("Decompression function finished in {:.3} seconds", sw_decompress_elapsed.count());
            spdlog::debug("Finished decompressing the compressed sequence file");
            spdlog::info("Decompressed in {:.3} seconds", elapsed.count());

        }
        else
        {
            spdlog::info("Bit alphabet compression enabled");
            if (app.count("-r") == 0){
                spdlog::error("Please provide a reference file to compress against with -r or --ref");
                std::cout << app.help() << std::endl;
                return -1;
            }
            if (app.count("-s") == 0){
                spdlog::error("Please provide a sequence file to compress with -s or --seq");
                std::cout << app.help() << std::endl;
                return -1;
            }
            spdlog::debug("Starting to compress the sequence file");
            spdlog::stopwatch sw;
            spdlog::debug("The reference file provided: {}", ref_file);
            spdlog::debug("The sequence file provided: {}", seq_file);
            spdlog::stopwatch sw_parser;
            // Stream the sequence file
            RLZ main_parser(ref_file);
            auto sw_parser_elapsed = sw_parser.elapsed();
            spdlog::debug("Built main parser in {:.3} seconds", sw_parser_elapsed.count());
            spdlog::stopwatch sw_ref;
            spdlog::debug("Starting to store the reference file as a bit vector");
            main_parser.load_reverse_file_to_bit_vector(ref_file, main_parser.ref_bit_array);
            auto sw_ref_elapsed = sw_ref.elapsed();
            spdlog::debug("Loaded file in {:.3} seconds", sw_ref_elapsed.count());
            spdlog::stopwatch sw_compress;
            main_parser.compress(threads, seq_file);
            auto sw_compress_elapsed = sw_compress.elapsed();
            spdlog::debug("Compression function finished in {:.3} seconds", sw_compress_elapsed.count());
            auto elapsed = sw.elapsed();
            spdlog::debug("Finished compressing the sequence file");
            spdlog::info("Compressed in {:.3} seconds", elapsed.count());
            spdlog::info("#############################################################");
            spdlog::info("File Size Statistics:");
            uintmax_t ref_size = std::filesystem::file_size(ref_file); //bytes
            uintmax_t seq_size = std::filesystem::file_size(seq_file); //bytes
            uintmax_t parse_size = std::filesystem::file_size(seq_file + ".rlz"); //bytes
            double comp_ratio = static_cast<double>(ref_size + parse_size) / 
                        static_cast<double>(ref_size + seq_size) * 100;
            spdlog::info("The reference (ref) file provided: {} is {} bytes", ref_file, ref_size);
            spdlog::info("The sequence (seq) file provided: {} is {} bytes", seq_file, seq_size);
            spdlog::info("The parse (parse) file created: {} is {} bytes", seq_file + ".rlz", parse_size);
            spdlog::info("Compression ratio [((ref + parse)/(ref + seq)) * 100]: {:.3}%", comp_ratio);
        }
    }
    else
    {
        if (decompress)
        {
            spdlog::info("Original alphabet decompression enabled");
            if (app.count("-r") == 0){
                spdlog::error("Please provide the reference file used to compress the RLZ parse with -r or --ref");
                std::cout << app.help() << std::endl;
                return -1;
            }
            if (app.count("-p") == 0){
                spdlog::error("Please provide the RLZ parse to decompress with -p or --parse");
                std::cout << app.help() << std::endl;
                return -1;
            }
            spdlog::debug("Starting to decompress the compressed sequence file");
            spdlog::stopwatch sw;
            spdlog::stopwatch sw_parser;
            RLZ_CHAR main_parser(ref_file);
            auto sw_parser_elapsed = sw_parser.elapsed();
            spdlog::debug("Built main parser in {:.3} seconds", sw_parser_elapsed.count());
            spdlog::debug("Starting to read the reference file");
            spdlog::stopwatch sw_ref;
            main_parser.load_file_to_string(ref_file, main_parser.ref_content);
            auto sw_ref_elapsed = sw_ref.elapsed();
            spdlog::debug("Loaded file in {:.3} seconds", sw_ref_elapsed.count());
            spdlog::stopwatch sw_decompress;
            main_parser.decompress(parse_file);
            auto sw_decompress_elapsed = sw_decompress.elapsed();
            auto elapsed = sw.elapsed();
            spdlog::debug("Decompression function finished in {:.3} seconds", sw_decompress_elapsed.count());
            spdlog::debug("Finished decompressing the compressed sequence file");
            spdlog::info("Decompressed in {:.3} seconds", elapsed.count());

        }
        else
        {
            spdlog::info("Original alphabet compression enabled");
            if (app.count("-r") == 0){
                spdlog::error("Please provide a reference file to compress against with -r or --ref");
                std::cout << app.help() << std::endl;
                return -1;
            }
            if (app.count("-s") == 0){
                spdlog::error("Please provide a sequence file to compress  with -s or --seq");
                std::cout << app.help() << std::endl;
                return -1;
            }
            spdlog::debug("Starting to compress the sequence file");
            spdlog::stopwatch sw;
            spdlog::debug("The reference file provided: {}", ref_file);
            spdlog::debug("The sequence file provided: {}", seq_file);
            spdlog::stopwatch sw_parser;
            // Stream the sequence file
            RLZ_CHAR main_parser(ref_file);
            auto sw_parser_elapsed = sw_parser.elapsed();
            spdlog::debug("Built main parser in {:.3} seconds", sw_parser_elapsed.count());
            spdlog::stopwatch sw_ref;
            spdlog::debug("Starting to read the reference file");
            main_parser.load_reverse_file_to_string(ref_file, main_parser.ref_content);
            auto sw_ref_elapsed = sw_ref.elapsed();
            spdlog::debug("Finished reading file in {:.3} seconds", sw_ref_elapsed.count());
            spdlog::stopwatch sw_compress;
            main_parser.compress(threads, seq_file);
            auto sw_compress_elapsed = sw_compress.elapsed();
            spdlog::debug("Compression function finished in {:.3} seconds", sw_compress_elapsed.count());
            auto elapsed = sw.elapsed();
            spdlog::debug("Finished compressing the sequence file");
            spdlog::info("Compressed in {:.3} seconds", elapsed.count());
            spdlog::info("#############################################################");
            spdlog::info("File Size Statistics:");
            uintmax_t ref_size = std::filesystem::file_size(ref_file); //bytes
            uintmax_t seq_size = std::filesystem::file_size(seq_file); //bytes
            uintmax_t parse_size = std::filesystem::file_size(seq_file + ".rlz"); //bytes
            double comp_ratio = static_cast<double>(ref_size + parse_size) / 
                        static_cast<double>(ref_size + seq_size) * 100;
            spdlog::info("The reference (ref) file provided: {} is {} bytes", ref_file, ref_size);
            spdlog::info("The sequence (seq) file provided: {} is {} bytes", seq_file, seq_size);
            spdlog::info("The parse (parse) file created: {} is {} bytes", seq_file + ".rlz", parse_size);
            spdlog::info("Compression ratio [((ref + parse)/(ref + seq)) * 100]: {:.3}%", comp_ratio);
        }
    }
    

    return 0;
}