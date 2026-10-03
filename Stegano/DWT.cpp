#include "stdafx.h"
#include "common.h"
#include <opencv2/core/utils/logger.hpp>
#include "Helper.h"

DWTBands haarDWT(const cv::Mat& img)
{
    int even_rows = img.rows - (img.rows % 2);
    int even_cols = img.cols - (img.cols % 2);

    cv::Mat src = img(cv::Rect(0, 0, even_cols, even_rows));

    int rows = even_rows / 2;
    int cols = even_cols / 2;

    cv::Mat LL(rows, cols, CV_32F);
    cv::Mat LH(rows, cols, CV_32F);
    cv::Mat HL(rows, cols, CV_32F);
    cv::Mat HH(rows, cols, CV_32F);

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            float a = src.at<float>(2 * i, 2 * j);
            float b = src.at<float>(2 * i, 2 * j + 1);
            float c = src.at<float>(2 * i + 1, 2 * j);
            float d = src.at<float>(2 * i + 1, 2 * j + 1);

            LL.at<float>(i, j) = (a + b + c + d) / 2;
            LH.at<float>(i, j) = (a - b + c - d) / 2;
            HL.at<float>(i, j) = (a + b - c - d) / 2;
            HH.at<float>(i, j) = (a - b - c + d) / 2;
        }
    }

    return { LL, LH, HL, HH };
}

cv::Mat inverseHaarDWT(const DWTBands& b)
{
    int rows = b.ll.rows * 2;
    int cols = b.ll.cols * 2;

    cv::Mat out(rows, cols, CV_32F);

    for (int i = 0; i < b.ll.rows; i++)
    {
        for (int j = 0; j < b.ll.cols; j++)
        {
            float ll = b.ll.at<float>(i, j);
            float lh = b.lh.at<float>(i, j);
            float hl = b.hl.at<float>(i, j);
            float hh = b.hh.at<float>(i, j);

            out.at<float>(2 * i, 2 * j) = (ll + lh + hl + hh) / 2;
            out.at<float>(2 * i, 2 * j + 1) = (ll - lh + hl - hh) / 2;
            out.at<float>(2 * i + 1, 2 * j) = (ll + lh - hl - hh) / 2;
            out.at<float>(2 * i + 1, 2 * j + 1) = (ll - lh - hl + hh) / 2;
        }
    }

    return out;
}

void embed_band_qim(cv::Mat& band, const std::string& packet, size_t& idx, float delta)
{
    for (int i = 0; i < band.rows && idx < packet.size(); i++)
    {
        for (int j = 0; j < band.cols && idx < packet.size(); j++)
        {
            float& c = band.at<float>(i, j);

            int q = (int)std::round(c / delta);
            int bit = packet[idx] - '0';
            int parity = ((q % 2) + 2) % 2;

            if (parity != bit)
            {
                float dist_plus = std::abs((q + 1) * delta - c);
                float dist_minus = std::abs((q - 1) * delta - c);
                q = (dist_plus <= dist_minus) ? q + 1 : q - 1;
            }

            c = q * delta;
            idx++;
        }
    }
}

void extract_band_qim(cv::Mat& band, std::string& bits, size_t target_bits, float delta)
{
    for (int i = 0; i < band.rows && bits.size() < target_bits; i++)
        for (int j = 0; j < band.cols && bits.size() < target_bits; j++)
        {
            int q = (int)std::round(band.at<float>(i, j) / delta);
            int bit = (q % 2 + 2) % 2;
            bits.push_back(bit ? '1' : '0');
        }
}

size_t verify_band_parity(const cv::Mat& rt_band, const std::string& packet, size_t start_idx, float delta)
{
    size_t errors = 0;
    size_t idx = start_idx;

    for (int i = 0; i < rt_band.rows && idx < packet.size(); i++)
        for (int j = 0; j < rt_band.cols && idx < packet.size(); j++)
        {
            int quantized = (int)std::round(rt_band.at<float>(i, j) / delta);
            int parity = ((quantized % 2) + 2) % 2;
            if (parity != (packet[idx] - '0'))
                ++errors;
            ++idx;
        }

    return errors;
}


cv::Mat float_to_u8(const cv::Mat& img)
{
    cv::Mat clamped;
    cv::threshold(img, clamped, 1.0, 1.0, cv::THRESH_TRUNC);
    cv::threshold(clamped, clamped, 0.0, 0.0, cv::THRESH_TOZERO);

    cv::Mat out;
    clamped.convertTo(out, CV_8U, 255.0);
    return out;
}

// roun-trip verification 
size_t dwt_verify_roundtrip(const DWTBands& bands, const std::string& packet, float delta)
{
    cv::Mat recon = inverseHaarDWT(bands);
    cv::Mat recon8u = float_to_u8(recon);

    cv::Mat verify_f;
    recon8u.convertTo(verify_f, CV_32F, 1.0 / 255.0);
    DWTBands verify_bands = haarDWT(verify_f);

    size_t errors = 0, idx_v = 0;
    errors += verify_band_parity(verify_bands.lh, packet, idx_v, delta); idx_v += bands.lh.total();
    errors += verify_band_parity(verify_bands.hl, packet, idx_v, delta); idx_v += bands.hl.total();
    errors += verify_band_parity(verify_bands.hh, packet, idx_v, delta);

    return errors;
}


cv::Mat dwt_embed(cv::Mat image_bytes, const std::string& bitstream, DWTBands& bands)
{
    std::string packet = build_packet(bitstream);

    size_t capacity = bands.lh.total() + bands.hl.total() + bands.hh.total();
    if (packet.size() > capacity) {
        std::cout << "Secret size exceeds maximum capacity.\n";
        return image_bytes;
    }

    const float delta = 0.1f;
    size_t idx = 0;
    embed_band_qim(bands.lh, packet, idx, delta);
    embed_band_qim(bands.hl, packet, idx, delta);
    embed_band_qim(bands.hh, packet, idx, delta);

    size_t errors = dwt_verify_roundtrip(bands, packet, delta);
    if (errors > 0) {
        std::cout << "Round-trip verification failed: " << errors
            << " bit(s) corrupted. "
            << "Try increasing delta or reducing payload size.\n";
        return image_bytes;
    }

    cv::Mat recon = inverseHaarDWT(bands);
    cv::Mat recon8u = float_to_u8(recon);

    cv::Mat out = image_bytes.clone();
    recon8u.copyTo(out(cv::Rect(0, 0, recon8u.cols, recon8u.rows)));
    return out;
}



cv::Mat dwt_extract(cv::Mat image_bytes, DWTBands& bands)
{
    size_t capacity = bands.lh.total() + bands.hl.total() + bands.hh.total();

    std::string bits;
    bits.reserve(capacity);

    const float delta = 0.1f;
    extract_band_qim(bands.lh, bits, capacity, delta);
    extract_band_qim(bands.hl, bits, capacity, delta);
    extract_band_qim(bands.hh, bits, capacity, delta);

    return decode_packet(bits);
}

cv::Mat t_dwt(cv::Mat image_bytes, std::string bitstream)
{
    cv::Mat imgf;
    image_bytes.convertTo(imgf, CV_32F, 1.0 / 255.0);
    DWTBands bands = haarDWT(imgf);

    if (!bitstream.empty())
        return dwt_embed(image_bytes, bitstream, bands);
    else
        return dwt_extract(image_bytes, bands);
}
