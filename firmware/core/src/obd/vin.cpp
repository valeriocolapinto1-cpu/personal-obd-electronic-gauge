// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/obd/vin.h"

#include <cstring>

namespace core::obd {

namespace {

bool valid_char(char c)
{
    if (c >= '0' && c <= '9') {
        return true;
    }
    return c >= 'A' && c <= 'Z' && c != 'I' && c != 'O' && c != 'Q';
}

int transliterate(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    // ISO 3779 / FMVSS 565 table
    static const char *const kLetters = "ABCDEFGHJKLMNPRSTUVWXYZ";
    static const int kValues[] = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 7, 9, 2, 3, 4, 5, 6, 7, 8, 9};
    const char *p = std::strchr(kLetters, c);
    return p ? kValues[p - kLetters] : -1;
}

} // namespace

bool Vin::has_prefix(const char *prefix) const
{
    const size_t n = std::strlen(prefix);
    return n <= 17 && std::strncmp(text, prefix, n) == 0;
}

VinStatus parse_vin(const char *chars, size_t len, Vin &out)
{
    if (chars == nullptr || len != 17) {
        return VinStatus::WrongLength;
    }
    for (size_t i = 0; i < 17; ++i) {
        if (!valid_char(chars[i])) {
            return VinStatus::InvalidCharacter;
        }
        out.text[i] = chars[i];
    }
    out.text[17] = '\0';
    return VinStatus::Ok;
}

VinStatus parse_vin_response(const uint8_t *data, size_t len, Vin &out)
{
    if (data == nullptr) {
        return VinStatus::WrongLength;
    }
    // Skip the item count and any leading padding, take the last 17 bytes.
    if (len < 17) {
        return VinStatus::WrongLength;
    }
    const char *chars = reinterpret_cast<const char *>(data + (len - 17));
    return parse_vin(chars, 17, out);
}

bool vin_check_digit_valid(const Vin &vin)
{
    static const int kWeights[17] = {8, 7, 6, 5, 4, 3, 2, 10, 0, 9, 8, 7, 6, 5, 4, 3, 2};
    int sum = 0;
    for (int i = 0; i < 17; ++i) {
        const int v = transliterate(vin.text[i]);
        if (v < 0) {
            return false;
        }
        sum += v * kWeights[i];
    }
    const int r = sum % 11;
    const char expected = (r == 10) ? 'X' : static_cast<char>('0' + r);
    return vin.text[8] == expected;
}

size_t vin_model_years(char code, uint16_t out[2])
{
    // Codes cycle every 30 years: A=1980/2010 ... Y=2000/2030, 1=2001/2031 ... 9=2009/2039.
    static const char *const kCodes = "ABCDEFGHJKLMNPRSTVWXY123456789";
    const char *p = std::strchr(kCodes, code);
    if (p == nullptr || code == '\0') {
        return 0;
    }
    const uint16_t first = static_cast<uint16_t>(1980 + (p - kCodes));
    out[0] = first;
    out[1] = static_cast<uint16_t>(first + 30);
    return 2;
}

} // namespace core::obd
