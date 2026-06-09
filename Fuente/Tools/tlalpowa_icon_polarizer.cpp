#include "tlalpowa_icon_polarizer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <objbase.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr UINT kCanonicalSide = 2048u;
constexpr UINT kCanonicalPadding = 24u;
constexpr UINT kMaxDecodedSide = 8192u;
constexpr unsigned long long kMaxDecodedPixels = 16777216ull;
constexpr unsigned char kBoundsAlphaThreshold = 3u;

template <class T>
void safe_release(T*& p) noexcept {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

class ComApartment {
public:
    ComApartment() noexcept {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        initialized_ = (hr == S_OK || hr == S_FALSE);
    }
    ~ComApartment() noexcept {
        if (initialized_) CoUninitialize();
    }
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
    bool ok() const noexcept { return initialized_; }
private:
    bool initialized_ = false;
};

struct Stats {
    unsigned long long candidates = 0;
    unsigned long long decoded = 0;
    unsigned long long processed = 0;
    unsigned long long unchanged = 0;
    unsigned long long skipped = 0;
    unsigned long long failed = 0;
};

struct Bounds {
    UINT x0 = 0;
    UINT y0 = 0;
    UINT x1 = 0;
    UINT y1 = 0;
    bool any = false;
};

struct ComponentInfo {
    size_t count = 0;
    unsigned long long alpha_sum = 0;
};

int luminance_bgra(const unsigned char* p) noexcept {
    const int b = p[0];
    const int g = p[1];
    const int r = p[2];
    return (77 * r + 150 * g + 29 * b + 128) >> 8;
}

unsigned char clamp_to_u8_from_int(int v) noexcept {
    if (v <= 0) return 0u;
    if (v >= 255) return 255u;
    return static_cast<unsigned char>(v);
}

double smoothstep(double edge0, double edge1, double x) noexcept {
    if (!(edge1 > edge0)) return x >= edge1 ? 1.0 : 0.0;
    double t = (x - edge0) / (edge1 - edge0);
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return t * t * (3.0 - 2.0 * t);
}

bool is_png_path(const fs::path& p) {
    std::wstring ext = p.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    
    return ext == L".png";
}

bool is_square_with_icon_tolerance(UINT w, UINT h) noexcept {
    
    if (w == 0u || h == 0u) return false;
    const UINT lo = (w < h) ? w : h;
    const UINT hi = (w > h) ? w : h;
    const UINT abs_tol = std::max<UINT>(5u, hi / 128u);
    return (hi - lo) <= abs_tol;
}

bool image_uses_meaningful_alpha(const std::vector<unsigned char>& px) noexcept {
    if (px.empty() || (px.size() % 4u) != 0u) return false;
    const size_t n = px.size() / 4u;
    size_t clear_or_soft = 0;
    size_t opaque = 0;
    for (size_t i = 0; i < px.size(); i += 4u) {
        const unsigned char a = px[i + 3u];
        if (a < 250u) ++clear_or_soft;
        if (a > 250u) ++opaque;
    }
    return clear_or_soft > std::max<size_t>(32u, n / 200u) && opaque > 0u;
}

bool is_white_alpha_only(const std::vector<unsigned char>& px, bool& has_partial_alpha) noexcept {
    has_partial_alpha = false;
    if (px.empty() || (px.size() % 4u) != 0u) return false;
    for (size_t i = 0; i < px.size(); i += 4u) {
        const unsigned char b = px[i + 0u];
        const unsigned char g = px[i + 1u];
        const unsigned char r = px[i + 2u];
        const unsigned char a = px[i + 3u];
        if (a == 0u) continue;
        if (a < 255u) has_partial_alpha = true;
        if (r < 250u || g < 250u || b < 250u) return false;
    }
    return true;
}

Bounds find_alpha_bounds(const std::vector<unsigned char>& alpha, UINT w, UINT h, unsigned char threshold = kBoundsAlphaThreshold) noexcept {
    Bounds b{};
    if (w == 0u || h == 0u || alpha.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) return b;
    b.x0 = w;
    b.y0 = h;
    for (UINT y = 0; y < h; ++y) {
        for (UINT x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            if (alpha[idx] < threshold) continue;
            b.any = true;
            if (x < b.x0) b.x0 = x;
            if (y < b.y0) b.y0 = y;
            if (x + 1u > b.x1) b.x1 = x + 1u;
            if (y + 1u > b.y1) b.y1 = y + 1u;
        }
    }
    if (!b.any) b.x0 = b.y0 = b.x1 = b.y1 = 0u;
    return b;
}

Bounds find_visible_bounds_bgra_alpha(const std::vector<unsigned char>& px, UINT w, UINT h) noexcept {
    std::vector<unsigned char> alpha;
    if (w == 0u || h == 0u || px.size() != static_cast<size_t>(w) * static_cast<size_t>(h) * 4u) return {};
    alpha.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (size_t i = 0, j = 0; i + 3u < px.size(); i += 4u, ++j) alpha[j] = px[i + 3u];
    return find_alpha_bounds(alpha, w, h);
}

bool looks_canonically_processed_2048(const std::vector<unsigned char>& px, UINT w, UINT h) noexcept {
    
    if (w != kCanonicalSide || h != kCanonicalSide) return false;
    bool has_partial = false;
    if (!is_white_alpha_only(px, has_partial) || !has_partial) return false;
    const Bounds b = find_visible_bounds_bgra_alpha(px, w, h);
    if (!b.any || b.x1 <= b.x0 || b.y1 <= b.y0) return false;
    const UINT left = b.x0;
    const UINT top = b.y0;
    const UINT right = kCanonicalSide - b.x1;
    const UINT bottom = kCanonicalSide - b.y1;
    const UINT min_margin = std::min(std::min(left, top), std::min(right, bottom));
    const UINT low = (kCanonicalPadding > 8u) ? (kCanonicalPadding - 8u) : 0u;
    const UINT high = kCanonicalPadding + 10u;
    return min_margin >= low && min_margin <= high;
}

int otsu_threshold_for_visible_pixels_bgra(const std::vector<unsigned char>& px) noexcept {
    std::array<unsigned long long, 256> hist{};
    unsigned long long total = 0;
    unsigned long long sum = 0;
    for (size_t i = 0; i + 3u < px.size(); i += 4u) {
        const unsigned char a = px[i + 3u];
        if (a < 8u) continue;
        const int lum = luminance_bgra(&px[i]);
        ++hist[static_cast<size_t>(lum)];
        ++total;
        sum += static_cast<unsigned long long>(lum);
    }
    if (total == 0ull) return 180;

    unsigned long long weight_b = 0;
    unsigned long long sum_b = 0;
    double best_score = -1.0;
    int best_t = 180;
    for (int t = 0; t < 256; ++t) {
        weight_b += hist[static_cast<size_t>(t)];
        if (weight_b == 0ull) continue;
        const unsigned long long weight_f = total - weight_b;
        if (weight_f == 0ull) break;
        sum_b += static_cast<unsigned long long>(t) * hist[static_cast<size_t>(t)];
        const double mean_b = static_cast<double>(sum_b) / static_cast<double>(weight_b);
        const double mean_f = static_cast<double>(sum - sum_b) / static_cast<double>(weight_f);
        const double d = mean_b - mean_f;
        const double score = static_cast<double>(weight_b) * static_cast<double>(weight_f) * d * d;
        if (score > best_score) {
            best_score = score;
            best_t = t;
        }
    }
    return std::clamp(best_t, 72, 220);
}

bool derive_icon_alpha_mask_bgra(const std::vector<unsigned char>& px, UINT w, UINT h, std::vector<unsigned char>& alpha) noexcept {
    
    if (w == 0u || h == 0u || px.size() != static_cast<size_t>(w) * static_cast<size_t>(h) * 4u) return false;
    alpha.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0u);
    const bool use_source_alpha = image_uses_meaningful_alpha(px);
    const int threshold = otsu_threshold_for_visible_pixels_bgra(px);
    const double lo = static_cast<double>(std::clamp(threshold - 64, 8, 224));
    const double hi = static_cast<double>(std::clamp(threshold + 36, 48, 252));
    bool any = false;

    for (size_t i = 0, j = 0; i + 3u < px.size(); i += 4u, ++j) {
        const unsigned char src_a = px[i + 3u];
        if (src_a == 0u) {
            alpha[j] = 0u;
            continue;
        }
        double coverage = 0.0;
        if (use_source_alpha) {
            coverage = static_cast<double>(src_a) / 255.0;
        } else {
            const double lum = static_cast<double>(luminance_bgra(&px[i]));
            coverage = (static_cast<double>(src_a) / 255.0) * smoothstep(lo, hi, lum);
        }
        int a = static_cast<int>(std::lround(coverage * 255.0));
        if (a < 3) a = 0;
        if (a > 252) a = 255;
        alpha[j] = clamp_to_u8_from_int(a);
        any = any || alpha[j] != 0u;
    }
    return any;
}

void remove_tiny_components_and_noise(std::vector<unsigned char>& alpha, UINT w, UINT h) noexcept {
    if (w == 0u || h == 0u || alpha.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) return;
    const size_t n = alpha.size();
    if (n > static_cast<size_t>(std::numeric_limits<int>::max())) return;

    std::vector<int> labels(n, -1);
    std::vector<ComponentInfo> comps;
    std::vector<unsigned int> stack;
    stack.reserve(4096u);

    const auto push_neighbor = [&](std::vector<unsigned int>& st, int label, int xx, int yy) noexcept {
        if (xx < 0 || yy < 0 || xx >= static_cast<int>(w) || yy >= static_cast<int>(h)) return;
        const size_t ni = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
        if (alpha[ni] < kBoundsAlphaThreshold || labels[ni] >= 0) return;
        labels[ni] = label;
        st.push_back(static_cast<unsigned int>(ni));
    };

    size_t largest = 0;
    for (size_t i = 0; i < n; ++i) {
        if (alpha[i] < kBoundsAlphaThreshold || labels[i] >= 0) continue;
        const int label = static_cast<int>(comps.size());
        comps.push_back({});
        labels[i] = label;
        stack.clear();
        stack.push_back(static_cast<unsigned int>(i));
        while (!stack.empty()) {
            const unsigned int cur = stack.back();
            stack.pop_back();
            const size_t ci = static_cast<size_t>(cur);
            ++comps[static_cast<size_t>(label)].count;
            comps[static_cast<size_t>(label)].alpha_sum += alpha[ci];
            const int x = static_cast<int>(ci % static_cast<size_t>(w));
            const int y = static_cast<int>(ci / static_cast<size_t>(w));
            push_neighbor(stack, label, x - 1, y - 1);
            push_neighbor(stack, label, x,     y - 1);
            push_neighbor(stack, label, x + 1, y - 1);
            push_neighbor(stack, label, x - 1, y);
            push_neighbor(stack, label, x + 1, y);
            push_neighbor(stack, label, x - 1, y + 1);
            push_neighbor(stack, label, x,     y + 1);
            push_neighbor(stack, label, x + 1, y + 1);
        }
        largest = std::max(largest, comps[static_cast<size_t>(label)].count);
    }
    if (largest == 0u) return;

    const size_t min_component = std::max<size_t>(4u, largest / 20000u);
    for (size_t i = 0; i < n; ++i) {
        const int label = labels[i];
        if (label < 0) {
            alpha[i] = 0u;
            continue;
        }
        const ComponentInfo& c = comps[static_cast<size_t>(label)];
        if (c.count < min_component || c.alpha_sum < 256ull) alpha[i] = 0u;
    }
}

std::vector<unsigned char> lightly_blurred_alpha(const std::vector<unsigned char>& input, UINT w, UINT h, unsigned iterations) {
    if (w == 0u || h == 0u || input.size() != static_cast<size_t>(w) * static_cast<size_t>(h) || iterations == 0u) return input;
    std::vector<unsigned char> a = input;
    std::vector<unsigned char> tmp(a.size(), 0u);
    for (unsigned it = 0; it < iterations; ++it) {
        for (UINT y = 0; y < h; ++y) {
            for (UINT x = 0; x < w; ++x) {
                const UINT xm = (x == 0u) ? 0u : x - 1u;
                const UINT xp = (x + 1u >= w) ? (w - 1u) : x + 1u;
                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                const size_t im = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(xm);
                const size_t ip = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(xp);
                tmp[idx] = static_cast<unsigned char>((static_cast<int>(a[im]) + 2 * static_cast<int>(a[idx]) + static_cast<int>(a[ip]) + 2) / 4);
            }
        }
        for (UINT y = 0; y < h; ++y) {
            const UINT ym = (y == 0u) ? 0u : y - 1u;
            const UINT yp = (y + 1u >= h) ? (h - 1u) : y + 1u;
            for (UINT x = 0; x < w; ++x) {
                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                const size_t im = static_cast<size_t>(ym) * static_cast<size_t>(w) + static_cast<size_t>(x);
                const size_t ip = static_cast<size_t>(yp) * static_cast<size_t>(w) + static_cast<size_t>(x);
                a[idx] = static_cast<unsigned char>((static_cast<int>(tmp[im]) + 2 * static_cast<int>(tmp[idx]) + static_cast<int>(tmp[ip]) + 2) / 4);
            }
        }
    }
    return a;
}

unsigned char sample_alpha_bilinear(const std::vector<unsigned char>& a, UINT w, UINT h, double x, double y) noexcept {
    if (w == 0u || h == 0u || a.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) return 0u;
    if (x < 0.0 || y < 0.0 || x > static_cast<double>(w - 1u) || y > static_cast<double>(h - 1u)) return 0u;
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min<int>(x0 + 1, static_cast<int>(w - 1u));
    const int y1 = std::min<int>(y0 + 1, static_cast<int>(h - 1u));
    const double fx = x - static_cast<double>(x0);
    const double fy = y - static_cast<double>(y0);
    const auto at = [&](int xx, int yy) noexcept -> double {
        return static_cast<double>(a[static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx)]);
    };
    const double v00 = at(x0, y0);
    const double v10 = at(x1, y0);
    const double v01 = at(x0, y1);
    const double v11 = at(x1, y1);
    const double v0 = v00 + (v10 - v00) * fx;
    const double v1 = v01 + (v11 - v01) * fx;
    const int out = static_cast<int>(std::lround(v0 + (v1 - v0) * fy));
    return clamp_to_u8_from_int(out);
}

Bounds expand_bounds(Bounds b, UINT w, UINT h, UINT guard) noexcept {
    if (!b.any) return b;
    b.x0 = (b.x0 > guard) ? (b.x0 - guard) : 0u;
    b.y0 = (b.y0 > guard) ? (b.y0 - guard) : 0u;
    b.x1 = std::min<UINT>(w, b.x1 + guard);
    b.y1 = std::min<UINT>(h, b.y1 + guard);
    return b;
}

bool render_canonical_icon_2048(const std::vector<unsigned char>& alpha, UINT w, UINT h, std::vector<unsigned char>& out) {
    if (w == 0u || h == 0u || alpha.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) return false;
    Bounds b = find_alpha_bounds(alpha, w, h);
    if (!b.any || b.x1 <= b.x0 || b.y1 <= b.y0) return false;
    b = expand_bounds(b, w, h, 2u);

    const double crop_w = static_cast<double>(b.x1 - b.x0);
    const double crop_h = static_cast<double>(b.y1 - b.y0);
    if (!(crop_w > 0.0) || !(crop_h > 0.0)) return false;
    const double target = static_cast<double>(kCanonicalSide - 2u * kCanonicalPadding);
    const double scale = std::min(target / crop_w, target / crop_h);
    if (!(scale > 0.0) || !std::isfinite(scale)) return false;

    const unsigned blur_iterations = scale >= 10.0 ? 2u : (scale >= 2.0 ? 1u : 0u);
    const std::vector<unsigned char> prepared = lightly_blurred_alpha(alpha, w, h, blur_iterations);
    const double render_w = crop_w * scale;
    const double render_h = crop_h * scale;
    const double dst_x0 = (static_cast<double>(kCanonicalSide) - render_w) * 0.5;
    const double dst_y0 = (static_cast<double>(kCanonicalSide) - render_h) * 0.5;
    const double dst_x1 = dst_x0 + render_w;
    const double dst_y1 = dst_y0 + render_h;

    out.assign(static_cast<size_t>(kCanonicalSide) * static_cast<size_t>(kCanonicalSide) * 4u, 0u);
    for (UINT y = 0; y < kCanonicalSide; ++y) {
        const double dy = static_cast<double>(y) + 0.5;
        if (dy < dst_y0 || dy >= dst_y1) continue;
        const double sy = (dy - dst_y0) / scale + static_cast<double>(b.y0) - 0.5;
        for (UINT x = 0; x < kCanonicalSide; ++x) {
            const double dx = static_cast<double>(x) + 0.5;
            if (dx < dst_x0 || dx >= dst_x1) continue;
            const double sx = (dx - dst_x0) / scale + static_cast<double>(b.x0) - 0.5;
            int a = static_cast<int>(sample_alpha_bilinear(prepared, w, h, sx, sy));
            if (a < 3) a = 0;
            if (a > 252) a = 255;
            if (a == 0) continue;
            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(kCanonicalSide) + static_cast<size_t>(x)) * 4u;
            out[di + 0u] = 255u;
            out[di + 1u] = 255u;
            out[di + 2u] = 255u;
            out[di + 3u] = clamp_to_u8_from_int(a);
        }
    }
    return find_visible_bounds_bgra_alpha(out, kCanonicalSide, kCanonicalSide).any;
}

bool decode_png_bgra(IWICImagingFactory* factory, const fs::path& path, UINT& w, UINT& h, std::vector<unsigned char>& px) noexcept {
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool ok = false;

    HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) goto done;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) goto done;
    hr = frame->GetSize(&w, &h);
    if (FAILED(hr) || w == 0u || h == 0u || w > kMaxDecodedSide || h > kMaxDecodedSide) goto done;
    if (static_cast<unsigned long long>(w) * static_cast<unsigned long long>(h) > kMaxDecodedPixels) goto done;

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) goto done;
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                               nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto done;

    px.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0u);
    hr = converter->CopyPixels(nullptr, w * 4u, static_cast<UINT>(px.size()), px.data());
    if (FAILED(hr)) goto done;
    ok = true;

done:
    safe_release(converter);
    safe_release(frame);
    safe_release(decoder);
    return ok;
}

bool encode_png_bgra_atomic(IWICImagingFactory* factory, const fs::path& path, UINT w, UINT h, const std::vector<unsigned char>& px) noexcept {
    if (w == 0u || h == 0u || px.size() != static_cast<size_t>(w) * static_cast<size_t>(h) * 4u) return false;
    const fs::path tmp = fs::path(path.wstring() + L".tlalpowa.tmp.png");
    std::error_code ec;
    fs::remove(tmp, ec);

    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* bag = nullptr;
    bool ok = false;

    HRESULT hr = factory->CreateStream(&stream);
    if (FAILED(hr)) goto done;
    hr = stream->InitializeFromFilename(tmp.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) goto done;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) goto done;
    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) goto done;
    hr = encoder->CreateNewFrame(&frame, &bag);
    if (FAILED(hr)) goto done;
    hr = frame->Initialize(bag);
    if (FAILED(hr)) goto done;
    hr = frame->SetSize(w, h);
    if (FAILED(hr)) goto done;
    {
        WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
        hr = frame->SetPixelFormat(&fmt);
        if (FAILED(hr) || !IsEqualGUID(fmt, GUID_WICPixelFormat32bppBGRA)) goto done;
    }
    hr = frame->WritePixels(h, w * 4u, static_cast<UINT>(px.size()), const_cast<unsigned char*>(px.data()));
    if (FAILED(hr)) goto done;
    hr = frame->Commit();
    if (FAILED(hr)) goto done;
    hr = encoder->Commit();
    if (FAILED(hr)) goto done;

    safe_release(bag);
    safe_release(frame);
    safe_release(encoder);
    safe_release(stream);

    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    ok = MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) fs::remove(tmp, ec);
    return ok;

done:
    safe_release(bag);
    safe_release(frame);
    safe_release(encoder);
    safe_release(stream);
    fs::remove(tmp, ec);
    return false;
}

bool normalize_icon_to_2048_bgra(UINT& w, UINT& h, std::vector<unsigned char>& px) {
    
    if (!is_square_with_icon_tolerance(w, h)) return false;
    if (px.size() != static_cast<size_t>(w) * static_cast<size_t>(h) * 4u) return false;
    if (looks_canonically_processed_2048(px, w, h)) return false;

    const UINT original_w = w;
    const UINT original_h = h;
    const std::vector<unsigned char> original = px;

    std::vector<unsigned char> alpha;
    if (!derive_icon_alpha_mask_bgra(px, w, h, alpha)) return false;
    remove_tiny_components_and_noise(alpha, w, h);
    if (!find_alpha_bounds(alpha, w, h).any) return false;

    std::vector<unsigned char> normalized;
    if (!render_canonical_icon_2048(alpha, w, h, normalized)) return false;
    if (original_w == kCanonicalSide && original_h == kCanonicalSide && original == normalized) return false;

    w = kCanonicalSide;
    h = kCanonicalSide;
    px.swap(normalized);
    return true;
}

enum class ProcessResult {
    Processed,
    Unchanged,
    Failed,
    Skipped
};

ProcessResult process_png_file(IWICImagingFactory* factory, const fs::path& path) noexcept {
    UINT w = 0, h = 0;
    std::vector<unsigned char> px;
    if (!decode_png_bgra(factory, path, w, h, px)) return ProcessResult::Failed;
    if (!is_square_with_icon_tolerance(w, h)) return ProcessResult::Skipped;
    try {
        if (!normalize_icon_to_2048_bgra(w, h, px)) return ProcessResult::Unchanged;
    } catch (...) {
        return ProcessResult::Failed;
    }
    return encode_png_bgra_atomic(factory, path, w, h, px) ? ProcessResult::Processed : ProcessResult::Failed;
}

void account_result(Stats& stats, ProcessResult r) noexcept {
    switch (r) {
    case ProcessResult::Processed: ++stats.processed; break;
    case ProcessResult::Unchanged: ++stats.unchanged; break;
    case ProcessResult::Failed: ++stats.failed; break;
    case ProcessResult::Skipped: ++stats.skipped; break;
    }
}

bool process_one_path(IWICImagingFactory* factory, const fs::path& p, Stats& stats) noexcept {
    std::error_code ec;
    if (fs::is_regular_file(p, ec)) {
        if (!is_png_path(p)) {
            ++stats.skipped;
            return true;
        }
        ++stats.candidates;
        ++stats.decoded;
        const ProcessResult r = process_png_file(factory, p);
        account_result(stats, r);
        if (r == ProcessResult::Processed) std::wprintf(L"[ICONOS] normalizado: %ls\n", p.c_str());
        if (r == ProcessResult::Failed) std::wprintf(L"[ICONOS] fallo: %ls\n", p.c_str());
        return r != ProcessResult::Failed;
    }
    if (!fs::is_directory(p, ec)) return true;

    const fs::directory_options opts = fs::directory_options::skip_permission_denied;
    for (fs::recursive_directory_iterator it(p, opts, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        const fs::path child = it->path();
        const std::wstring name = child.filename().wstring();
        if (name.find(L".tlalpowa.tmp") != std::wstring::npos) continue;
        if (!fs::is_regular_file(child, ec)) continue;
        if (!is_png_path(child)) continue;
        ++stats.candidates;
        ++stats.decoded;
        const ProcessResult r = process_png_file(factory, child);
        account_result(stats, r);
        if (r == ProcessResult::Processed) std::wprintf(L"[ICONOS] normalizado: %ls\n", child.c_str());
        if (r == ProcessResult::Failed) std::wprintf(L"[ICONOS] fallo: %ls\n", child.c_str());
    }
    return true;
}

void append_if_directory(std::vector<fs::path>& roots, const fs::path& p) {
    std::error_code ec;
    if (fs::exists(p, ec) && fs::is_directory(p, ec)) roots.push_back(p);
}

void append_standard_icon_roots(std::vector<fs::path>& roots, const fs::path& base) {
    
    append_if_directory(roots, base / L"Datos" / L"icon");
    append_if_directory(roots, base / L"Datos" / L"Icon");
    append_if_directory(roots, base / L"Datos" / L"icons");
    append_if_directory(roots, base / L"Datos" / L"Iconos");
    append_if_directory(roots, base / L"Fuente" / L"Config" / L"movilidad_iconos");
    append_if_directory(roots, base / L"Fuente" / L"Config" / L"iconos");
    append_if_directory(roots, base / L"Fuente" / L"Config" / L"geo" / L"iconos");
}

std::wstring widen_argument_lossless(const char* arg) {
    if (!arg || !arg[0]) return std::wstring();
#ifdef _WIN32
    const int bytes = static_cast<int>(std::strlen(arg));
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, arg, bytes, nullptr, 0);
    UINT codepage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (len <= 0) {
        codepage = CP_ACP;
        flags = 0;
        len = MultiByteToWideChar(codepage, flags, arg, bytes, nullptr, 0);
    }
    if (len <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(len), L'\0');
    const int written = MultiByteToWideChar(codepage, flags, arg, bytes, out.data(), len);
    if (written <= 0) return std::wstring();
    out.resize(static_cast<size_t>(written));
    return out;
#else
    std::wstring out;
    while (*arg) out.push_back(static_cast<unsigned char>(*arg++));
    return out;
#endif
}

std::vector<std::wstring> widen_arguments(int argc, char** argv) {
    std::vector<std::wstring> out;
    if (argc <= 0) return out;
    out.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) out.push_back(widen_argument_lossless(argv[i]));
    return out;
}

std::vector<fs::path> roots_from_command_line(const std::vector<std::wstring>& argv) {
    std::vector<fs::path> roots;
    for (size_t i = 1; i < argv.size(); ++i) {
        const std::wstring& arg = argv[i];
        if (arg == L"--nopause" || arg == L"/nopause") {
            continue;
        } else if ((arg == L"--root" || arg == L"/root") && i + 1u < argv.size()) {
            append_standard_icon_roots(roots, fs::path(argv[++i]));
        } else if (arg == L"--help" || arg == L"/help" || arg == L"-h") {
            std::wprintf(L"Uso: tlalpowa_icon_polarizer.exe [--root CARPETA_BASE] [archivo_o_carpeta_png...]\n");
        } else if (!arg.empty()) {
            roots.emplace_back(arg);
        }
    }
    if (roots.empty()) append_standard_icon_roots(roots, fs::current_path());
    return roots;
}

} 

int main(int argc, char** argv) {
    ComApartment com;
    if (!com.ok()) {
        std::wprintf(L"[ICONOS] No pude inicializar COM.\n");
        return 2;
    }

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        std::wprintf(L"[ICONOS] No pude crear IWICImagingFactory.\n");
        return 2;
    }

    Stats stats{};
    int rc = 0;
    try {
        const std::vector<std::wstring> wide_argv = widen_arguments(argc, argv);
        const std::vector<fs::path> roots = roots_from_command_line(wide_argv);
        if (roots.empty()) {
            std::wprintf(L"[ICONOS] No encontre carpetas de iconos PNG.\n");
        }
        for (const fs::path& root : roots) {
            std::wprintf(L"[ICONOS] raiz: %ls\n", root.c_str());
            (void)process_one_path(factory, root, stats);
        }
    } catch (...) {
        ++stats.failed;
        rc = 3;
    }

    std::wprintf(L"[ICONOS] PNG candidatos: %llu | procesados: %llu | intactos: %llu | omitidos: %llu | fallidos: %llu\n",
                 stats.candidates, stats.processed, stats.unchanged, stats.skipped, stats.failed);
    if (stats.failed != 0ull && rc == 0) rc = 1;
    safe_release(factory);
    return rc;
}
