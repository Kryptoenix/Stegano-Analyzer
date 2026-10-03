#include "stdafx.h"
#include "common.h"
#include <opencv2/core/utils/logger.hpp>
#include "Helper.h"


cv::Mat lsb_extract(const cv::Mat& image_bytes)
{

    if ((size_t)image_bytes.total() * image_bytes.elemSize() < 32) {
        std::cerr << "Image too small to contain LSB header.\n";
        return cv::Mat();
    }

    uint32_t payload_bits = 0;
    for (int i = 0; i < 32; ++i)
        payload_bits = (payload_bits << 1) | (image_bytes.data[i] & 1);


    std::string bits;
    size_t total = 32 + payload_bits + 32;


    size_t available = image_bytes.total() * image_bytes.elemSize();
    if (total > available) {
        std::cerr << "Extracted payload size (" << total
            << " bits) exceeds image capacity (" << available
            << " pixels). Image may not contain embedded data.\n";
        return cv::Mat();
    }

    for (size_t i = 0; i < total; ++i)
        bits.push_back((image_bytes.data[i] & 1) ? '1' : '0');

    return decode_packet(bits);
}


cv::Mat t_lsb_sub(cv::Mat image_bytes, std::string bitstream)
{
    if (!bitstream.empty()) { // embed

        cv::Mat result = image_bytes.clone();
        std::string packet = build_packet(bitstream);

        std::cout << packet.size() << "\n";
        std::cout << result.total() * result.elemSize();

        if (packet.size() > result.total() * result.elemSize()) {
            std::cout << "Secret size exceeds maximum capacity allowed.\n";
            system("pause");
            return image_bytes;
        }

        for (size_t i = 0; i < packet.size(); ++i)
            result.data[i] = (result.data[i] & 0xFE) | (packet[i] - '0');

        return result;
    }
    else { // extract
        return lsb_extract(image_bytes);
    }
}


cv::Mat t_lsb_match(cv::Mat image_bytes, std::string bitstream)
{
    if (!bitstream.empty()) { // embed
        
        cv::Mat result = image_bytes.clone();
        std::string packet = build_packet(bitstream);

        std::cout << packet.size() << "\n";
        std::cout << result.total() * result.elemSize();

        if (packet.size() > result.total() * result.elemSize()) {
            std::cout << "Secret size exceeds maximum capacity allowed.\n";
            system("pause");
            return image_bytes;
        }

        for (size_t i = 0; i < packet.size(); ++i)
        {
            uint8_t& pixel = result.data[i];
            uint8_t bit = packet[i] - '0';

            if ((pixel & 1) != bit)
            {
                if (pixel == 0)        pixel += 1;
                else if (pixel == 255) pixel -= 1;
                else                   pixel += (rand() % 2) ? 1 : -1;
            }
        }

        return result;
    }
    else { // extract
        return lsb_extract(image_bytes);
    }
}