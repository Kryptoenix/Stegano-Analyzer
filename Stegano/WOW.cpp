#include "stdafx.h"
#include "common.h"
#include <opencv2/core/utils/logger.hpp>
#include "Helper.h"

struct WaveletFilter {
    std::vector<float> low_pass;
    std::vector<float> high_pass;
    std::vector<float> low_pass_inv;
    std::vector<float> high_pass_inv;
};

WaveletFilter get_wavelet_filter(const std::string& type) {
    WaveletFilter wf;
    if (type == "haar") {
        wf.low_pass = { 0.7071f,  0.7071f };
        wf.high_pass = { 0.7071f, -0.7071f };
        wf.low_pass_inv = { 0.7071f,  0.7071f };
        wf.high_pass_inv = { 0.7071f, -0.7071f };
    }
    return wf;
}

DWTBands waveletDWT(const cv::Mat& img, const WaveletFilter& wf)
{
    int rows = img.rows / 2;
    int cols = img.cols / 2;

    cv::Mat LL(rows, cols, CV_32F);
    cv::Mat LH(rows, cols, CV_32F);
    cv::Mat HL(rows, cols, CV_32F);
    cv::Mat HH(rows, cols, CV_32F);

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            float a = img.at<float>(2 * i, 2 * j);
            float b = img.at<float>(2 * i, 2 * j + 1);
            float c = img.at<float>(2 * i + 1, 2 * j);
            float d = img.at<float>(2 * i + 1, 2 * j + 1);

            LL.at<float>(i, j) = (a + b + c + d) / 2;
            LH.at<float>(i, j) = (a - b + c - d) / 2;
            HL.at<float>(i, j) = (a + b - c - d) / 2;
            HH.at<float>(i, j) = (a - b - c + d) / 2;
        }
    }

    return { LL, LH, HL, HH };
}

cv::Mat inverseWaveletDWT(const DWTBands& b, const WaveletFilter& wf)
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

cv::Mat wow_embed(cv::Mat image_bytes,
    const std::string& bitstream,
    DWTBands& band,
    cv::Mat cost,
    WaveletFilter& wf)
{
    std::string packet = build_packet(bitstream);

    size_t capacity =
        band.lh.total() +
        band.hl.total() +
        band.hh.total();

    if (packet.size() > capacity) {
        std::cout << "Secret size exceeds maximum capacity.\n";
        return image_bytes;
    }

    struct Candidate {
        float cost;
        float band_weight;
        int band_id;   // 0 = LH, 1 = HL, 2 = HH
        int i, j;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(capacity);

    auto add_band = [&](cv::Mat& b, int band_id, float weight)
        {
            for (int i = 0; i < b.rows; i++) {
                for (int j = 0; j < b.cols; j++) {


                    float c = 1.0f;
                    c = cost.at<float>(i, j);

                    candidates.push_back({
                        c,
                        weight,
                        band_id,
                        i, j
                        });
                }
            }
        };


    // build candidates from all bands
    // HH = safest -> lowest weight
    // LH = riskiest -> highest weight
    add_band(band.lh, 0, 1.5f);
    add_band(band.hl, 1, 1.0f);
    add_band(band.hh, 2, 0.5f);


    // sort by effective cost
    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            return (a.cost * a.band_weight) < (b.cost * b.band_weight);
        });

    // adaptive embedding 
    for (size_t k = 0; k < packet.size(); k++) {

        const Candidate& cand = candidates[k];

        cv::Mat* target_band;

        if (cand.band_id == 0) target_band = &band.lh;
        else if (cand.band_id == 1) target_band = &band.hl;
        else target_band = &band.hh;

        float& coeff = target_band->at<float>(cand.i, cand.j);

        float delta = 0.15f * std::abs(coeff) + 0.01f;

        int target_bit = packet[k] - '0';

        int current_bit =
            ((int)std::round(coeff / delta)) % 2;

        if (current_bit != target_bit) {

            float q = std::round(coeff / delta);

            if (((int)q % 2) != target_bit) {
                q += 1.0f;
            }

            coeff = q * delta;
        }
    }

    cv::Mat recon = inverseWaveletDWT(band, wf);
    cv::Mat recon8u = float_to_u8(recon);

    cv::Mat out = image_bytes.clone();
    recon8u.copyTo(out(cv::Rect(0, 0, recon8u.cols, recon8u.rows)));
    return out;
}

cv::Mat compute_distortion_map(cv::Mat image_bytes) {

    cv::Mat cost(image_bytes.size(), CV_32F, cv::Scalar(0));

    for (int i = 1; i < image_bytes.rows - 1; i++) {
        for (int j = 1; j < image_bytes.cols - 1; j++) {

            float h = std::abs(image_bytes.at<float>(i, j + 1) - image_bytes.at<float>(i, j - 1));
            float v = std::abs(image_bytes.at<float>(i + 1, j) - image_bytes.at<float>(i - 1, j));
            float d = std::abs(image_bytes.at<float>(i + 1, j + 1) - image_bytes.at<float>(i - 1, j - 1));

            cost.at<float>(i, j) = 1.0f + (h * v * d); // smooth surface = expensive
        }
    }

    return cost;
}

cv::Mat compute_wavelet_cost_map(const DWTBands& band)
{
    cv::Mat cost(band.hh.size(), CV_32F);

    const float eps = 1e-5f;

    for (int i = 0; i < band.hh.rows; i++)
    {
        for (int j = 0; j < band.hh.cols; j++)
        {
            float ll = std::abs(band.ll.at<float>(i, j));
            float lh = std::abs(band.lh.at<float>(i, j));
            float hl = std::abs(band.hl.at<float>(i, j));
            float hh = std::abs(band.hh.at<float>(i, j));

            float energy =
                0.2f * ll +
                0.5f * lh +
                0.7f * hl +
                1.0f * hh;

            cost.at<float>(i, j) = 1.0f / (energy + eps);
        }
    }

    cv::normalize(cost, cost, 0.0f, 1.0f, cv::NORM_MINMAX);
    return cost;
}

cv::Mat wow_extract(cv::Mat image_bytes, DWTBands& band)
{
    size_t capacity =
        band.lh.total() +
        band.hl.total() +
        band.hh.total();

    std::string bits;
    bits.reserve(capacity);

    struct Candidate {
        float cost;
        float band_weight;
        int band_id;
        int i, j;
    };

    cv::Mat imgf;
    image_bytes.convertTo(imgf, CV_32F, 1.0 / 255.0);
    //cv::Mat cost = compute_distortion_map(imgf);
    cv::Mat cost = compute_wavelet_cost_map(band);

    std::vector<Candidate> candidates;
    candidates.reserve(capacity);

    auto add_band = [&](cv::Mat& b, int band_id, float weight)
        {
            for (int i = 0; i < b.rows; i++) {
                for (int j = 0; j < b.cols; j++) {

                    float c = 1.0f;
                    c = cost.at<float>(i, j);

                    candidates.push_back({
                        c, weight, band_id, i, j
                        });
                }
            }
        };

    add_band(band.lh, 0, 1.5f);
    add_band(band.hl, 1, 1.0f);
    add_band(band.hh, 2, 0.5f);

    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            return (a.cost * a.band_weight) < (b.cost * b.band_weight);
        });

    for (size_t k = 0; k < capacity; k++) {

        const Candidate& cand = candidates[k];

        cv::Mat* target_band;

        if (cand.band_id == 0) target_band = &band.lh;
        else if (cand.band_id == 1) target_band = &band.hl;
        else target_band = &band.hh;

        float coeff = target_band->at<float>(cand.i, cand.j);

        float delta = 0.15f * std::abs(coeff) + 0.01f;

        int q = (int)std::round(coeff / delta);

        int bit = q % 2;

        bits.push_back(bit ? '1' : '0');
    }

    return decode_packet(bits);
}

cv::Mat t_wow(cv::Mat image_bytes, std::string bitstream)
{
    cv::Mat imgf;
    image_bytes.convertTo(imgf, CV_32F, 1.0 / 255.0);

    WaveletFilter wf = get_wavelet_filter("haar");
    DWTBands bands = waveletDWT(imgf, wf);

    //cv::Mat cost = compute_distortion_map(imgf);
    cv::Mat cost = compute_wavelet_cost_map(bands);

    if (!bitstream.empty())
        return wow_embed(image_bytes, bitstream, bands, cost, wf);
    else
        return wow_extract(image_bytes, bands);
}
