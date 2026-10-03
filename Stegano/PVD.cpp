#include "stdafx.h"
#include "common.h"
#include <opencv2/core/utils/logger.hpp>
#include "Helper.h"

int get_range_width(int value, int* lower, int* upper)
{
    if (value < 8) { *lower = 0;   *upper = 7;   return 3; }
    if (value < 16) { *lower = 8;   *upper = 15;  return 3; }
    if (value < 32) { *lower = 16;  *upper = 31;  return 4; }
    if (value < 64) { *lower = 32;  *upper = 63;  return 5; }
    if (value < 128) { *lower = 64;  *upper = 127; return 6; }
    else { *lower = 128; *upper = 255; return 7; }
}

cv::Mat t_pvd(cv::Mat image_bytes, std::string bitstream)
{

    cv::Mat result = image_bytes.clone();
    uchar* data = result.data;
    size_t total = result.total();

    if (!bitstream.empty()) // embed
    {
        std::string packet = build_packet(bitstream);
        size_t j = 0;

        for (size_t i = 0; i + 1 < total && j < packet.size(); i += 2)
        {
            int x = data[i];
            int y = data[i + 1];
            int diff = abs(x - y);

            int lower, upper;
            int n_bits = get_range_width(diff, &lower, &upper);

            int secret_part = 0;
            for (int k = 0; k < n_bits; k++)
            {
                int bit = (j < packet.size()) ? (packet[j++] - '0') : 0;
                secret_part = (secret_part << 1) | bit;
            }

            int new_diff = lower + secret_part;
            int m = new_diff - diff;

            int new_x, new_y;
            if (x >= y) {
                new_x = x + (int)ceil(m / 2.0);
                new_y = y - (int)floor(m / 2.0);
            }
            else {
                new_x = x - (int)ceil(m / 2.0);
                new_y = y + (int)floor(m / 2.0);
            }

            if (new_x < 0) { 
                new_y -= new_x;         
                new_x = 0; 
            }
            if (new_x > 255) { 
                new_y -= (new_x - 255); 
                new_x = 255; 
            }
            if (new_y < 0) {
                new_x -= new_y;         
                new_y = 0;
            }
            if (new_y > 255) { 
                new_x -= (new_y - 255); 
                new_y = 255; 
            }

            new_x = (new_x < 0) ? 0 : (new_x > 255) ? 255 : new_x;
            new_y = (new_y < 0) ? 0 : (new_y > 255) ? 255 : new_y;

            data[i] = (uchar)new_x;
            data[i + 1] = (uchar)new_y;
        }

        return result;
    }
    else // extract
    {
        std::string extracted_bits;
        extracted_bits.reserve(total * 4);

        for (size_t i = 0; i + 1 < total; i += 2)
        {
            int x = data[i];
            int y = data[i + 1];
            int diff = abs(x - y);

            int lower, upper;
            int n_bits = get_range_width(diff, &lower, &upper);

            int secret_part = diff - lower;
            for (int k = n_bits - 1; k >= 0; k--)
                extracted_bits.push_back(((secret_part >> k) & 1) ? '1' : '0');
        }

        return decode_packet(extracted_bits);
    }
}