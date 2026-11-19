// Solar.cpp - 日出日落计算（NOAA 简化算法）
#include "pch.h"
#include "Solar.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180.0;

// 公历日期 -> 1970-01-01 起的天数
long long DaysFromCivil(int y, int m, int d) {
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (long long)era * 146097 + (long long)doe - 719468;
}

} // namespace

SunTimes ComputeSunTimes(int year, int month, int day, double lat, double lon) {
    SunTimes out;
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return out;

    const long long days = DaysFromCivil(year, month, day);
    const long long jan1 = DaysFromCivil(year, 1, 1);
    const int n = (int)(days - jan1) + 1;

    const double gamma = 2.0 * kPi / 365.0 * (n - 1 + (12.0 - 12.0) / 24.0);
    const double eqTime = 229.18 * (0.000075
        + 0.001868 * cos(gamma)
        - 0.032077 * sin(gamma)
        - 0.014615 * cos(2 * gamma)
        - 0.040849 * sin(2 * gamma));
    const double decl = 0.006918
        - 0.399912 * cos(gamma)
        + 0.070257 * sin(gamma)
        - 0.006758 * cos(2 * gamma)
        + 0.000907 * sin(2 * gamma)
        - 0.002697 * cos(3 * gamma)
        + 0.00148 * sin(3 * gamma);

    const double latRad = lat * kRad;
    const double cosHa = cos(90.833 * kRad) / (cos(latRad) * cos(decl))
                       - tan(latRad) * tan(decl);

    if (cosHa > 1.0) {
        out.valid = true;
        out.polarNight = true;
        return out;
    }
    if (cosHa < -1.0) {
        out.valid = true;
        out.polarDay = true;
        return out;
    }

    const double ha = acos(cosHa) / kRad;
    const double sunriseMin = 720.0 - 4.0 * (lon + ha) - eqTime;
    const double sunsetMin = 720.0 - 4.0 * (lon - ha) - eqTime;

    out.valid = true;
    out.sunrise = days * 86400 + (long long)llround(sunriseMin * 60.0);
    out.sunset = days * 86400 + (long long)llround(sunsetMin * 60.0);
    return out;
}