#include "stdafx.h"
#include "common.h"
#include <opencv2/core/utils/logger.hpp>
#include <zlib.h>
#include <fstream>
#include "Helper.h"



std::string to_bitstream(const cv::Mat& data)
{
    std::string bitstream;

    size_t total_bytes = data.total() * data.elemSize();
    bitstream.reserve(total_bytes * 8);

    const unsigned char* bytes = data.data;

    for (size_t k = 0; k < total_bytes; ++k)
    {
        unsigned char byte = bytes[k];
        for (int i = 7; i >= 0; --i)
            bitstream.push_back(((byte >> i) & 1) ? '1' : '0');
    }

    return bitstream;
}

std::vector<uint8_t> from_bitstream(const std::string& bitstream)
{
    if (bitstream.size() % 8 != 0)
        throw std::runtime_error("Bitstream length must be multiple of 8");

    std::vector<uint8_t> bytes;
    bytes.reserve(bitstream.size() / 8);

    for (size_t i = 0; i < bitstream.size(); i += 8)
    {
        uint8_t byte = 0;
        for (int j = 0; j < 8; ++j)
            byte = (byte << 1) | (bitstream[i + j] == '1');
        bytes.push_back(byte);
    }

    return bytes;
}

uint32_t compute_checksum(const uint8_t* data, size_t size) {
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, data, size);
    return crc;
}

// 32-bit bitstream encode
std::string bits32(uint32_t value)
{
    std::string s;
    s.reserve(32);
    for (int i = 31; i >= 0; --i)
        s.push_back(((value >> i) & 1) ? '1' : '0');
    return s;
}

// [32-bit size | payload bits | 32-bit CRC]
std::string build_packet(const std::string& bitstream)
{
    std::vector<uint8_t> payload_bytes = from_bitstream(bitstream);
    uint32_t checksum = compute_checksum(payload_bytes.data(), payload_bytes.size());
    return bits32((uint32_t)bitstream.size()) + bitstream + bits32(checksum);
}

// recover secret as cv::Mat from the packet bitstream
cv::Mat decode_packet(const std::string& bits)
{
    if (bits.size() < 32)
        throw std::runtime_error("Invalid stego image: no payload header");

    uint32_t payload_bits = 0;
    for (int i = 0; i < 32; i++)
        payload_bits = (payload_bits << 1) | (bits[i] - '0');

    if (32ULL + payload_bits + 32ULL > bits.size())
        throw std::runtime_error("Corrupted payload length");

    std::string payload_bitstream = bits.substr(32, payload_bits);

    uint32_t extracted_checksum = 0;
    for (size_t i = 32 + payload_bits; i < 32 + payload_bits + 32; i++)
        extracted_checksum = (extracted_checksum << 1) | (bits[i] - '0');

    std::vector<uint8_t> secret_bytes = from_bitstream(payload_bitstream);

    uint32_t calculated_checksum = compute_checksum(secret_bytes.data(), secret_bytes.size());
    if (calculated_checksum != extracted_checksum)
        std::cout << "Warning: checksum mismatch! Secret may be corrupted.\n";

    return cv::Mat(1, (int)secret_bytes.size(), CV_8UC1, secret_bytes.data()).clone();
}

void embedding(std::wstring image_path, std::wstring secret_path, ttype technique)
{
    cv::Mat image_bytes, secret_bytes;
    std::string bitstream;

    image_bytes = cv::imread(std::string(image_path.begin(), image_path.end()), cv::IMREAD_GRAYSCALE);
    if (image_bytes.empty()) {
        std::cout << "Failed to load cover image!\n";
        return;
    }

    std::ifstream file(std::string(secret_path.begin(), secret_path.end()), std::ios::binary);
    if (!file.is_open()) {
        std::cout << "Failed to open secret file!\n";
        return;
    }

    std::vector<uchar> buffer;
    char byte;
    while (file.read(&byte, 1))
        buffer.push_back((uchar)byte);

    secret_bytes = cv::Mat(1, (int)buffer.size(), CV_8UC1, buffer.data()).clone();

    if (secret_bytes.empty()) {
        std::cout << "Secret data is empty!\n";
        return;
    }

    bitstream = to_bitstream(secret_bytes);

    if (technique == FULL_ANALYSIS) {
        for (const auto& tech : techniques)
            run_technique(tech, image_bytes, bitstream, image_path);
        return;
    }

    for (const auto& tech : techniques) {
        if (tech.type == technique) {
            run_technique(tech, image_bytes, bitstream, image_path);
            return;
        }
    }

    std::cout << "Invalid technique type\n";
    system("pause");
}

void extraction(std::wstring image_path, ttype technique)
{
    cv::Mat image_bytes = cv::imread(std::string(image_path.begin(), image_path.end()), cv::IMREAD_GRAYSCALE);
    if (image_bytes.empty()) {
        std::cout << "Failed to load image!\n";
        return;
    }

    for (const auto& tech : techniques) {
        if (tech.type == technique) {
            cv::Mat secret;
            try {
                secret = tech.ptr(image_bytes, "");   // empty bitstream -> extract
            }
            catch (const std::exception& e) {
                std::cerr << "Extraction failed: " << e.what() << std::endl;
                return;
            }


            if (secret.empty()) {
                std::cerr << "Extraction returned empty data.\n";
                return;
            }

            std::string output_path(image_path.begin(), image_path.end());
            output_path += ".secret";

            std::ofstream out(output_path, std::ios::binary);
            out.write(reinterpret_cast<char*>(secret.data), secret.total());
            out.close();

            std::cout << "Secret saved to: " << output_path << "\n";
            std::cout << "Secret successfully extracted!\n";
            return;
        }
    }

    std::cout << "Invalid technique type";
    system("pause");
}
