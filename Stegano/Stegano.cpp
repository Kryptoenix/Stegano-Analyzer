#include "stdafx.h"
#include "common.h"
#include <opencv2/core/utils/logger.hpp>
#include <bitset>
#include <vector>
#include <fstream>
#include "Helper.h"
#include "StegoResults.h"


#pragma comment(lib,"zlib.lib")

std::vector<t_info> techniques = {
    {t_lsb_sub,   LSB_SUB,   "LSB Substitution"},
    {t_lsb_match, LSB_MATCH, "LSB Matching"},
    {t_pvd,       PVD,       "PVD"},
    {t_dwt,       DWT,       "DWT"},
    {t_wow,       WOW,       "WOW"}
};

void run_technique(const t_info& tech, cv::Mat image_bytes, const std::string& bitstream, const std::wstring& image_path)
{
    cv::Mat cover = image_bytes.clone();
    cv::Mat stego;

    try {
        stego = tech.ptr(image_bytes, bitstream);
    }
    catch (const std::exception& e) {
        std::cerr << "Embedding failed: " << e.what() << std::endl;
        return;
    }

    if (stego.empty()) {
        std::cerr << "Embedding produced empty result for " << tech.name << "\n";
        return;
    }

    std::string out_path = std::string(image_path.begin(), image_path.end());
    out_path.insert(out_path.find_last_of('.'), "_embedded");

    if (!cv::imwrite(out_path, stego))
        std::cout << "Failed to write embedded image!\n";
    else
        std::cout << "Embedded image saved to: " << out_path << "\n";

    display_analysis(cover, stego, bitstream.size(), tech.name);
}


void print_help(const char* prog)
{
    std::cout
        << "\nUsage:\n"
        << "  " << prog << " embed   -c <cover> --secret <file> -t <technique>\n"
        << "  " << prog << " extract -s <stego>                 -t <technique>\n"
        << "\nOperations:\n"
        << "  embed     Hide a secret file inside a cover image\n"
        << "  extract   Recover the secret file from a stego image\n"
        << "\nOptions:\n"
        << "  -t, --technique <name>   Steganography technique (default: lsb_sub)\n"
        << "  -c, --cover     <path>   Cover image path         (embed only)\n"
        << "  -s, --stego     <path>   Stego image path         (extract only)\n"
        << "      --secret    <path>   Secret file path         (embed only)\n"
        << "  -h, --help               Show this help message\n"
        << "\nTechniques:\n"
        << "  lsb_sub    LSB Substitution\n"
        << "  lsb_match  LSB Matching\n"
        << "  pvd        Pixel Value Differencing\n"
        << "  dwt        Discrete Wavelet Transform\n"
        << "  wow        Wavelet Obtained Weights\n"
        << "  full       Run all techniques (embed only)\n"
        << "\nExamples:\n"
        << "  " << prog << " embed   -c cover.png --secret msg.txt -t lsb_sub\n"
        << "  " << prog << " extract -s cover_embedded.png         -t lsb_sub\n"
        << "\n";
}

int parse_technique(const std::string& name)
{
    if (name == "lsb_sub")   return LSB_SUB;
    if (name == "lsb_match") return LSB_MATCH;
    if (name == "pvd")       return PVD;
    if (name == "dwt")       return DWT;
    if (name == "wow")       return WOW;
    if (name == "full")      return FULL_ANALYSIS;
    return -1;
}

int main(int argc, char* argv[])
{
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_FATAL);

    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            print_help(argv[0]);
            return 0;
        }
    }

    // first arg: embed or extract
    std::string operation = argv[1];
    if (operation != "embed" && operation != "extract") {
        std::cerr << "Error: unknown operation '" << operation << "'\n";
        print_help(argv[0]);
        return 1;
    }

    // remaining args
    std::string cover_path, stego_path, secret_path, technique_name;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];

        // check value for each option
        bool has_next = (i + 1 < argc);

        if ((arg == "-t" || arg == "--technique") && has_next)
            technique_name = argv[++i];

        else if ((arg == "-c" || arg == "--cover") && has_next)
            cover_path = argv[++i];

        else if ((arg == "-s" || arg == "--stego") && has_next)
            stego_path = argv[++i];

        else if (arg == "--secret" && has_next)
            secret_path = argv[++i];

        else {
            std::cerr << "Error: unknown or incomplete option '" << arg << "'\n";
            print_help(argv[0]);
            return 1;
        }
    }

    // default technique
    if (technique_name.empty())
        technique_name = "lsb_sub";

    int tech_int = parse_technique(technique_name);
    if (tech_int == -1) {
        std::cerr << "Error: unknown technique '" << technique_name << "'\n";
        print_help(argv[0]);
        return 1;
    }
    ttype technique = static_cast<ttype>(tech_int);

    if (operation == "embed")
    {
        if (cover_path.empty()) {
            std::cerr << "Error: embed requires --cover / -c\n";
            print_help(argv[0]);
            return 1;
        }
        if (secret_path.empty()) {
            std::cerr << "Error: embed requires --secret\n";
            print_help(argv[0]);
            return 1;
        }

        std::wstring wcover(cover_path.begin(), cover_path.end());
        std::wstring wsecret(secret_path.begin(), secret_path.end());
        embedding(wcover, wsecret, technique);
    }
    else // extract
    {
        if (stego_path.empty()) {
            std::cerr << "Error: extract requires --stego / -s\n";
            print_help(argv[0]);
            return 1;
        }
        if (technique == FULL_ANALYSIS) {
            std::cerr << "Error: 'full' is not valid for extraction\n";
            print_help(argv[0]);
            return 1;
        }

        std::wstring wstego(stego_path.begin(), stego_path.end());
        extraction(wstego, technique);
    }

    return 0;
}