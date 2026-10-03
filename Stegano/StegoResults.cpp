#include "stdafx.h"
#include "common.h"
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <vector>
#include <array>
#include <random>

void setupOpenCV() {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);
}


cv::Scalar getMSSIM(const cv::Mat& i1, const cv::Mat& i2) {
    const double C1 = 6.5025, C2 = 58.5225;
    CV_Assert(i1.size() == i2.size() && i1.type() == i2.type());
    cv::Mat I1, I2;
    i1.convertTo(I1, CV_32F);
    i2.convertTo(I2, CV_32F);
    cv::Mat I1_2 = I1.mul(I1), I2_2 = I2.mul(I2), I1_I2 = I1.mul(I2);
    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);
    cv::Mat mu1_2 = mu1.mul(mu1), mu2_2 = mu2.mul(mu2), mu1_mu2 = mu1.mul(mu2);
    cv::Mat sigma1_2, sigma2_2, sigma12;
    cv::GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5); sigma1_2 -= mu1_2;
    cv::GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5); sigma2_2 -= mu2_2;
    cv::GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5); sigma12 -= mu1_mu2;
    cv::Mat t1 = 2 * mu1_mu2 + C1, t2 = 2 * sigma12 + C2, t3 = t1.mul(t2);
    t1 = mu1_2 + mu2_2 + C1;
    t2 = sigma1_2 + sigma2_2 + C2;
    t1 = t1.mul(t2);
    cv::Mat ssim_map; cv::divide(t3, t1, ssim_map);
    return cv::mean(ssim_map);
}

double calculateChiSquare(const cv::Mat& img1, const cv::Mat& img2) {
    CV_Assert(img1.size() == img2.size() && img1.type() == img2.type());
    int histSize = 256; float range[] = { 0,256 }; const float* histRange = { range };
    int channels[] = { 0 };  
    cv::Mat hist1, hist2;
    cv::calcHist(&img1, 1, channels, cv::Mat(), hist1, 1, &histSize, &histRange);
    cv::calcHist(&img2, 1, channels, cv::Mat(), hist2, 1, &histSize, &histRange);
    hist1 /= (double)img1.total(); hist2 /= (double)img2.total();
    double chiSquare = 0;
    for (int i = 0; i < histSize; i++) {
        double diff = hist1.at<float>(i) - hist2.at<float>(i);
        double sum = hist1.at<float>(i) + hist2.at<float>(i);
        if (sum > 0) chiSquare += (diff * diff) / sum;
    }
    return chiSquare;
}

double calculateEntropy(cv::Mat img) {
    int hist[256] = {};
    for (int r = 0; r < img.rows; r++)
        for (int c = 0; c < img.cols; c++)
            hist[img.at<uchar>(r, c)]++;
    double e = 0, total = (double)img.total();
    for (int i = 0; i < 256; i++) {
        if (hist[i] == 0) continue;
        double p = hist[i] / total;
        e -= p * std::log2(p);
    }
    return e;
}

double calculateLsbPairDiff(cv::Mat stego) {

    double lsbPairDiff = 0.0;
    int hist[256] = {};
    for (int r = 0; r < stego.rows; r++)
        for (int c = 0; c < stego.cols; c++)
            hist[stego.at<uchar>(r, c)]++;
    for (int i = 0; i < 255; i += 2)
        lsbPairDiff += std::abs(hist[i] - hist[i + 1]);
    lsbPairDiff /= (double)stego.total(); // normalize

    return lsbPairDiff;
}

double calculateKlDiv(cv::Mat cover, cv::Mat stego) {
    double klDiv = 0.0;

    int hC[256] = {}, hS[256] = {};
    for (int r = 0; r < cover.rows; r++)
        for (int c = 0; c < cover.cols; c++) {
            hC[cover.at<uchar>(r, c)]++;
            hS[stego.at<uchar>(r, c)]++;
        }
    double total = (double)cover.total();
    for (int i = 0; i < 256; i++) {
        double p = hC[i] / total, q = hS[i] / total;
        if (p > 0 && q > 0) klDiv += p * std::log2(p / q);
    }

    return klDiv;
}

double calculateMae(cv::Mat cover, cv::Mat stego, double* _maxDev) {
    double mae = 0; int maxDev = 0;
    for (int r = 0; r < cover.rows; r++)
        for (int c = 0; c < cover.cols; c++) {
            int cv_ = (int)cover.at<uchar>(r, c);
            int sv = (int)stego.at<uchar>(r, c);
            int d = cv_ - sv;
            if (d < 0) d = -d;
            mae += d;
            if (d > maxDev) maxDev = d;
        }
    mae /= (double)cover.total();
    *_maxDev = maxDev;

    return mae;
}

double calculateMse(cv::Mat cover, cv::Mat stego) {
    double mse = 0.0;
    for (int r = 0; r < cover.rows; r++)
        for (int c = 0; c < cover.cols; c++) {
            double d = (double)cover.at<uchar>(r, c) - stego.at<uchar>(r, c);
            mse += d * d;
        }
    mse /= (double)cover.total();

    return mse;
}


cv::Mat extractBitPlane(const cv::Mat& gray, int bit) {
    cv::Mat plane(gray.size(), CV_8UC1);
    for (int r = 0; r < gray.rows; r++)
        for (int c = 0; c < gray.cols; c++)
            plane.at<uchar>(r, c) = ((gray.at<uchar>(r, c) >> bit) & 1) ? 255 : 0;
    return plane;
}

cv::Mat randomPaletteRemap(const cv::Mat& gray, uint64_t seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);

    // build a lookup table: value -> random BGR color
    std::array<cv::Vec3b, 256> lut;
    for (int i = 0; i < 256; i++)
        lut[i] = cv::Vec3b(dist(rng), dist(rng), dist(rng));

    cv::Mat result(gray.size(), CV_8UC3);
    for (int r = 0; r < gray.rows; r++)
        for (int c = 0; c < gray.cols; c++)
            result.at<cv::Vec3b>(r, c) = lut[gray.at<uchar>(r, c)];
    return result;
}

cv::Mat amplifiedDiff(const cv::Mat& cover, const cv::Mat& stego, double factor = 20.0) {
    cv::Mat diff;
    cv::absdiff(cover, stego, diff);
    diff.convertTo(diff, CV_32F);
    diff *= factor;
    cv::threshold(diff, diff, 255.0, 255.0, cv::THRESH_TRUNC);
    diff.convertTo(diff, CV_8UC1);
    return diff;
}

cv::Mat buildHistPanel(const cv::Mat& gray, int panelW, int panelH) {
    int hist[256] = {};
    for (int r = 0; r < gray.rows; r++)
        for (int c = 0; c < gray.cols; c++)
            hist[gray.at<uchar>(r, c)]++;

    int maxVal = *std::max_element(hist, hist + 256);
    cv::Mat panel(panelH, panelW, CV_8UC3, cv::Scalar(18, 18, 18));
    double scale = maxVal > 0 ? (double)(panelH - 4) / maxVal : 0;

    for (int x = 0; x < 256; x++) {
        int barH = cvRound(hist[x] * scale);
        int px = (int)((double)x / 255.0 * (panelW - 1));
        // dark blue -> bright yellow as value increases
        cv::Scalar color(50 + x / 3, 80 + x / 5, 200 - x / 3);
        cv::line(panel, cv::Point(px, panelH - 1), cv::Point(px, panelH - 1 - barH), color, 1);
    }
    return panel;
}

static void paste(cv::Mat& dst, const cv::Mat& src, int x, int y,
    const std::string& label, int labelH = 22) {
    // convert to BGR if needed
    cv::Mat bgr;
    if (src.channels() == 1)
        cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
    else
        bgr = src;

    cv::putText(dst, label, cv::Point(x + 4, y + 14),
        cv::FONT_HERSHEY_SIMPLEX, 0.42, cv::Scalar(160, 160, 160), 1, cv::LINE_AA);

    cv::Rect roi(x, y + labelH, bgr.cols, bgr.rows);
    if (roi.x >= 0 && roi.y >= 0 &&
        roi.x + roi.width <= dst.cols &&
        roi.y + roi.height <= dst.rows)
        bgr.copyTo(dst(roi));
}

void display_analysis(const cv::Mat& cover, const cv::Mat& stego,
    size_t payload_capacity, const std::string& technique) {
    setupOpenCV();

    // metrics 
    double psnr = cv::PSNR(cover, stego);
    cv::Scalar mssim = getMSSIM(cover, stego);
    double chiSquare = calculateChiSquare(cover, stego);
    
    size_t payload_bytes = payload_capacity / 8;
    double bpp = (double)payload_capacity / (double)cover.total();
    double mse = calculateMse(cover, stego);
    double maxDev = 0.0;
    double mae = calculateMae(cover, stego, &maxDev);

    double entrCover = calculateEntropy(cover);
    double entrStego = calculateEntropy(stego);

    double klDiv = calculateKlDiv(cover, stego);

    double lsbPairDiff = calculateLsbPairDiff(stego);

    double embEfficiency = (mse > 0) ? ((double)payload_capacity / (double)cover.total()) / mse : 0;

    const double scale = 0.8;  

    const int MIN_PANEL = 180;
    int W = max(MIN_PANEL, cvRound(cover.cols * scale));
    int H = max(MIN_PANEL, cvRound(cover.rows * scale));

    auto scaleImg = [&](const cv::Mat& src, int interp = cv::INTER_AREA) {
        cv::Mat dst;
        cv::resize(src, dst, cv::Size(W, H), 0, 0, interp);
        return dst;
        };

    // difference panels 
    cv::Mat diffAmp = amplifiedDiff(cover, stego, 20.0);
    cv::Mat diffColor;
    cv::applyColorMap(diffAmp, diffColor, cv::COLORMAP_HOT);

    // random RGB map images panels
    cv::Mat coverRemap = randomPaletteRemap(cover, 42);
    cv::Mat stegoRemap = randomPaletteRemap(stego, 42);

    // histograms 
    cv::Mat histCover = buildHistPanel(cover, W, H);
    cv::Mat histStego = buildHistPanel(stego, W, H);

    // scale all image panels
    cv::Mat coverS = scaleImg(cover);
    cv::Mat stegoS = scaleImg(stego);
    cv::Mat diffS = scaleImg(diffColor);
    cv::Mat remapCS = scaleImg(coverRemap, cv::INTER_NEAREST);
    cv::Mat remapSS = scaleImg(stegoRemap, cv::INTER_NEAREST);

    // ---------------------------------------------------------------------------
    // Layout — every cell is exactly W×H:
    //   Row 0:  cover | stego | diff×20 | stats
    //   Row 1:  cover_remap | stego_remap | hist_cover | hist_stego
    // ---------------------------------------------------------------------------
    const int PAD = 8;
    const int LBL = 20;

    int canvasW = W * 4 + PAD * 5;
    int canvasH = PAD + (LBL + H) + PAD + (LBL + H) + PAD;

    cv::Mat canvas(canvasH, canvasW, CV_8UC3, cv::Scalar(18, 18, 18));

    int y0 = PAD;
    int x0 = PAD, x1 = PAD + W + PAD, x2 = PAD + W * 2 + PAD * 2, x3 = PAD + W * 3 + PAD * 3;

    paste(canvas, coverS, x0, y0, "Cover");
    paste(canvas, stegoS, x1, y0, "Stego");
    paste(canvas, diffS, x2, y0, "Diff x20");

    // stats panel
    {
        cv::Mat stats(H, W, CV_8UC3, cv::Scalar(28, 28, 28));

        // build stat entries
        struct StatEntry { std::string label; std::string value; };
        std::vector<StatEntry> entries = {
            {"Technique",   technique},
            {"PSNR",        cv::format("%.2f dB", psnr)},
            {"SSIM",        cv::format("%.6f", mssim.val[0])},
            {"MSE",         cv::format("%.4f", mse)},
            {"MAE",         cv::format("%.4f  max=%.0f", mae, maxDev)},
            {"Chi-Square",  cv::format("%.6f", chiSquare)},
            {"KL Div",      cv::format("%.6f", klDiv)},
            {"Entropy cov", cv::format("%.4f", entrCover)},
            {"Entropy stg", cv::format("%.4f (d=%.4f)", entrStego, entrStego - entrCover)},
            {"LSB pairs",   cv::format("%.6f", lsbPairDiff)},
            {"Emb.effic.",  cv::format("%.4f b/MSE", embEfficiency)},
            {"Payload",     cv::format("%zu B (%zu b)", payload_bytes, payload_capacity)},
            {"BPP",         cv::format("%.4f", bpp)},
        };
        int N = (int)entries.size(); // 13

        
        int margin = max(4, H / 30);
        double usable = (double)(H - 2 * margin);

        double rowH_px = usable / N;
        //char height = fontScale * 18px
        double ratio = (rowH_px * 0.45) / 18.0;
        double fontValue;
        if (ratio < 0.25) {
            fontValue = 0.25;
        }
        else if (ratio > 0.50) {
            fontValue = 0.50;
        }
        else {
            fontValue = ratio;
        }
        double fontLabel = fontValue * 0.80;

        // gap between the label baseline and value baseline within one row
        int intraGap = max(8, cvRound(rowH_px * 0.55));

        int xPad = max(6, W / 40);

        for (int i = 0; i < N; i++) {
            int yLabel = margin + cvRound(i * rowH_px);
            int yValue = yLabel + intraGap;
            // don't draw below panel
            if (yValue + 4 > H) break;
            cv::putText(stats, entries[i].label, cv::Point(xPad, yLabel),
                cv::FONT_HERSHEY_SIMPLEX, fontLabel, cv::Scalar(120, 120, 120), 1, cv::LINE_AA);
            cv::putText(stats, entries[i].value, cv::Point(xPad, yValue),
                cv::FONT_HERSHEY_SIMPLEX, fontValue, cv::Scalar(220, 220, 220), 1, cv::LINE_AA);
        }

        cv::rectangle(stats, cv::Rect(0, 0, W, H), cv::Scalar(50, 50, 50), 1);
        paste(canvas, stats, x3, y0, "Stats");
    }

    // --- Row 1 ---
    int y1 = PAD + LBL + H + PAD;

    paste(canvas, remapCS, x0, y1, "Cover — random palette remap");
    paste(canvas, remapSS, x1, y1, "Stego — random palette remap");
    paste(canvas, histCover, x2, y1, "Histogram — cover");
    paste(canvas, histStego, x3, y1, "Histogram — stego");

    cv::namedWindow("Steganography Analysis", cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::resizeWindow("Steganography Analysis", canvasW, canvasH); // initial size
    cv::imshow("Steganography Analysis", canvas);
    cv::waitKey(0);
    cv::destroyAllWindows();
}