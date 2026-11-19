// Solar.h - 日出日落计算（NOAA 简化算法）
#pragma once
#include "pch.h"

struct SunTimes {
    bool valid = false;
    bool polarNight = false;
    bool polarDay = false;
    long long sunrise = 0;   // Unix 时间戳
    long long sunset = 0;
};

SunTimes ComputeSunTimes(int year, int month, int day, double lat, double lon);