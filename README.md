# Steganography Project

An image steganography project that implements and compares multiple spatial-domain and frequency-domain embedding techniques using OpenCV.

The application can hide a secret file inside a grayscale cover image, extract the hidden payload from a stego image, and analyze the visual/statistical impact of the embedding process.

## Features

* Embed a secret file into a grayscale image
* Extract an embedded secret file
* Shared packet format with:

  * 32-bit payload length header
  * payload bits
  * 32-bit CRC-32 checksum for integrity verification
* Five steganographic techniques
* Visual comparison of cover and stego images
* Amplified difference map
* Histogram comparison
* Random-palette remapping for subtle pixel-change visualization
* Quality and steganalysis metrics such as PSNR, SSIM, MSE, MAE, Chi-Square, KL Divergence, entropy, embedding efficiency, and BPP

## Implemented Techniques

|Technique|Domain|Mode|Main Idea|
|-|-|-|-|
|**LSB Substitution**|Spatial|Static|Replaces the least significant bit of each pixel with secret data|
|**LSB Matching**|Spatial|Adaptive|Adjusts pixel values by `+1` or `-1` when the LSB does not match the target bit|
|**PVD**|Spatial|Adaptive|Uses the difference between neighboring pixels to determine how many bits can be embedded|
|**DWT**|Frequency|Static|Uses Haar wavelet decomposition and QIM in the detail sub-bands|
|**WOW**|Frequency|Adaptive|Uses wavelet-domain distortion costs and band weighting to choose lower-impact embedding locations|

### LSB Substitution

Each payload bit is written directly into the least significant bit of a pixel.

* Capacity: approximately **1 bit per pixel**, minus packet overhead
* Simple and high-capacity
* More vulnerable to LSB and chi-square analysis

### LSB Matching

Instead of forcing the LSB directly, the pixel is left unchanged when its LSB already matches the target bit. Otherwise, the pixel value is randomly adjusted by `+1` or `-1`, with boundary handling at `0` and `255`.

* Capacity: approximately **1 bit per pixel**
* Produces a more natural noise distribution than direct substitution
* Still statistically detectable at larger payloads

### Pixel Value Differencing (PVD)

Pixels are processed in adjacent pairs. The absolute difference between the two values determines the number of bits that can be embedded.

|Difference Range|Bits Embedded|
|-|-:|
|0–7|3|
|8–15|3|
|16–31|4|
|32–63|5|
|64–127|6|
|128–255|7|

* Variable capacity: **3–7 bits per pixel pair**
* Adapts embedding strength to local image contrast
* Requires careful overflow and boundary handling

### Discrete Wavelet Transform (DWT)

The image is decomposed using a Haar wavelet transform into:

* `LL` — approximation
* `LH` — horizontal detail
* `HL` — vertical detail
* `HH` — diagonal detail

Embedding is performed only in the detail bands (`LH`, `HL`, `HH`) using Quantization Index Modulation (QIM) with a fixed step size.

A round-trip verification reconstructs the image, converts it back to 8-bit, decomposes it again, and checks whether the embedded bits survived quantization.

### Wavelet Obtained Weights (WOW)

WOW extends the DWT approach with adaptive embedding.

A distortion cost map is computed from wavelet-band energy, and candidate coefficients are ranked so that lower-distortion locations are used first. Different bands are weighted according to perceptual risk, and QIM uses an adaptive step size based on coefficient magnitude.

This makes the embedding order content-aware rather than fixed.

## Command-Line Usage

```text
OpenCVApplication.exe embed   -c <cover> --secret <file> -t <technique>
OpenCVApplication.exe extract -s <stego>                -t <technique>
```

### Operations

```text
embed      Hide a secret file inside a cover image
extract    Recover the secret file from a stego image
```

### Options

```text
-t, --technique <name>   Steganography technique
-c, --cover     <path>   Cover image path (embed only)
-s, --stego     <path>   Stego image path (extract only)
    --secret    <path>   Secret file path (embed only)
-h, --help               Show help
```

The default technique is `lsb\_sub`.

### Technique Names

```text
lsb\_sub     LSB Substitution
lsb\_match   LSB Matching
pvd         Pixel Value Differencing
dwt         Discrete Wavelet Transform
wow         Wavelet Obtained Weights
full        Run all techniques (embed only)
```

### Examples

Embed a secret text file using LSB substitution:

```powershell
.\\OpenCVApplication.exe embed -c cover.png --secret msg.txt -t lsb\_sub
```

Extract the secret from a stego image:

```powershell
.\\OpenCVApplication.exe extract -s cover\_embedded.png -t lsb\_sub
```

## Analysis Dashboard

After embedding, the application can compare the cover and stego images using several image-quality and steganalysis measurements:

* **PSNR** — Peak Signal-to-Noise Ratio
* **SSIM** — Structural Similarity Index
* **MSE** — Mean Squared Error
* **MAE** — Mean Absolute Error
* **Maximum per-pixel deviation**
* **Chi-Square statistic**
* **KL Divergence** between cover and stego histograms
* **Shannon entropy**
* **LSB Pair Difference**
* **Embedding Efficiency**
* **BPP** — payload bits per pixel

The dashboard also includes:

* cover image
* stego image
* amplified difference image (`×20`)
* random-palette remaps
* cover/stego histograms
* technique-specific statistics

## Technique Comparison

|Technique|Capacity / Behavior|Strengths|Limitations|
|-|-|-|-|
|**LSB Substitution**|\~1 bpp|Easy to implement, high capacity|Detectable by chi-square and LSB analysis|
|**LSB Matching**|\~1 bpp|Natural `±1` noise distribution|Still statistically detectable|
|**PVD**|3–7 bits per pixel pair|Content-adaptive, variable capacity|Overflow edge cases; artifacts can appear at high payloads|
|**DWT**|Detail-band coefficients|Frequency-domain embedding, round-trip verification|Fixed quantization step limits flexibility|
|**WOW**|Adaptive wavelet embedding|Content-adaptive placement and distortion weighting|More complex and computationally expensive|

## Development Notes

Some of the main implementation challenges were:

* handling pixel overflow in PVD while preserving pair differences
* understanding Haar-wavelet sub-bands and QIM-based embedding
* preventing embedded DWT bits from being corrupted during 8-bit reconstruction
* designing and tuning the WOW distortion-cost and band-weighting strategy

## References

1. D. C. Wu and W. H. Tsai, **“A steganographic method for images by pixel-value differencing,”** *Pattern Recognition Letters*, vol. 24, no. 9–10, pp. 1613–1626, 2003.
2. J. Mielikainen, **“LSB matching revisited,”** *IEEE Signal Processing Letters*, vol. 13, no. 5, pp. 285–287, 2006.
3. V. Holub and J. Fridrich, **“Designing steganographic distortion using directional filters,”** *2012 IEEE International Workshop on Information Forensics and Security (WIFS)*, pp. 234–239, 2012.
4. B. Chen and G. W. Wornell, **“Quantization index modulation: A class of provably good methods for digital watermarking and information embedding,”** *IEEE Transactions on Information Theory*, vol. 47, no. 4, pp. 1423–1443, 2001.
5. V. Holub, J. Fridrich, and T. Denemark, **“Universal distortion function for steganography in an arbitrary domain,”** *EURASIP Journal on Information Security*, 2014.
6. W. Luo, F. Huang, and J. Huang, **“Edge adaptive image steganography based on LSB matching revisited,”** *IEEE Transactions on Information Forensics and Security*, vol. 5, no. 2, pp. 201–214, 2010.

