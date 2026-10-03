#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <opencv2/opencv.hpp>

// enums

typedef enum {
    LSB_SUB,
    LSB_MATCH,
    PVD,
    DWT,
    WOW,
    FULL_ANALYSIS
} ttype;

typedef enum {
    EMBED,
    EXTRACT,
    EXIT
} op;

// shared structs 

typedef struct {
    uint32_t checksum;
    std::string data;
} secret;

struct DWTBands {
    cv::Mat ll, lh, hl, hh;
};

// technique function-pointer type

using tfptr = cv::Mat(*)(cv::Mat image_bytes, std::string bitstream);

// technique table (defined in Stegano.cpp) 

typedef struct {
    tfptr   ptr;
    ttype   type;
    std::string name;
} t_info;

extern std::vector<t_info> techniques;          // definition in Stegano.cpp

// technique entry points (each in its own .cpp)

cv::Mat t_lsb_sub  (cv::Mat image_bytes, std::string bitstream);
cv::Mat t_lsb_match(cv::Mat image_bytes, std::string bitstream);
cv::Mat t_pvd      (cv::Mat image_bytes, std::string bitstream);
cv::Mat t_dwt      (cv::Mat image_bytes, std::string bitstream);
cv::Mat t_wow      (cv::Mat image_bytes, std::string bitstream);

//  Helper.cpp — high-level operations 

void embedding (std::wstring image_path, std::wstring secret_path, ttype technique);
void extraction(std::wstring image_path, ttype technique);

// Helper.cpp — packet / bitstream utilities 

uint32_t    compute_checksum(const uint8_t* data, size_t size);
std::string to_bitstream    (const cv::Mat& data);
std::vector<uint8_t> from_bitstream(const std::string& bitstream);
std::string build_packet    (const std::string& bitstream);
cv::Mat     decode_packet   (const std::string& bits);

// LSB.cpp — shared extractor 

cv::Mat lsb_extract(const cv::Mat& image_bytes);

// DWT.cpp — utilities shared with WOW.cpp

cv::Mat float_to_u8(const cv::Mat& img);

// Stegano.cpp

void run_technique(const t_info& tech, cv::Mat image_bytes,
                   const std::string& bitstream, const std::wstring& image_path);
