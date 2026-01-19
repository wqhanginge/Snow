#pragma once

#include "stdafx.hpp"


struct Snow {
    static constexpr float DEFTDPI = USER_DEFAULT_SCREEN_DPI;
    static constexpr float DEFTFPS = 60;
    static constexpr float BASEPPS = 12;
    static constexpr float FASTPPS = 240;
    static constexpr float SLOWPPS = 60;
    static constexpr float FADEPPS = 30;
    static constexpr float FADELEN = 20;

    float size;
    float pixel_per_second;
    float x;
    float y;
    float ground;
    UINT sleep_frames;

    constexpr float alpha() const { //fade out if touch the ground
        return 1 - clamp<float>(y - ground, 0, FADELEN) / FADELEN;
    }
    constexpr float speed(float dpi = DEFTDPI) const {  //pixel per second by snow size
        return clamp<float>(BASEPPS * size * DEFTDPI / dpi, SLOWPPS, FASTPPS);
    }
    constexpr float step(float fps = DEFTFPS) const {   //pixel per frame by snow speed and position
        return (y - ground >= 0 && y - ground < FADELEN) ? FADEPPS / fps : pixel_per_second / fps;
    }
};

struct SnowList {
    static constexpr std::array<float, 5> SNOWSIZES = { 3, 5, 8, 12, 20 };
    static constexpr UINT FPS = 60;
    static constexpr UINT MAXSLEEPFRAMES = (FPS * 30);
    static constexpr UINT COUNTPERSIZE = 20;
    static constexpr UINT DEFXRESOLU = 1920;
    static constexpr UINT DEFYRESOLU = 1080;

    UINT xres;
    UINT yres;
    UINT ground;
    UINT dpi;
    std::minstd_rand urand;
    std::vector<Snow> list;

    SnowList(UINT seed = std::minstd_rand::default_seed) :urand(seed) {}
    void initialize(UINT xres, UINT yres, UINT ground, UINT dpi);
    void refreshSnowState(Snow& snow);
    void refreshList();
    void applyGround();
    void nextFrame();

    inline auto begin() { return list.begin(); }
    inline auto begin() const { return list.begin(); }
    inline auto end() { return list.end(); }
    inline auto end() const { return list.end(); }
};
