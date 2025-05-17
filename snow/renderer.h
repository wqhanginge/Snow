#pragma once

#include "stdafx.h"
#include "snow.h"


struct ComException {
    HRESULT hr;
    ComException(HRESULT val) :hr(val) {}
};

class SnowRenderer {
    template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    static inline void HR(const HRESULT hr) {
        if (FAILED(hr)) throw ComException(hr);
    }

    UINT _scx;
    UINT _scy;
    UINT _width;
    UINT _height;
    HWND _hwnd;
    ComPtr<IWICBitmapSource> _snow_bmp;
    ComPtr<ID3D11Device> _d3d_device;
    ComPtr<ID2D1DeviceContext6> _d2d_dc;
    ComPtr<ID2D1Effect> _d2d_dc_bmp_affine;
    ComPtr<ID2D1Effect> _d2d_dc_bmp_opacity;
    ComPtr<ID2D1Image> _d2d_dc_bmp;
    ComPtr<IDXGISwapChain1> _dxgi_swap_chain;
    ComPtr<IDCompositionTarget> _dcomp_target;

    void _loadSnowBitmap(HICON hisnow);
    void _createDevice();
    void _createDeviceContext();
    void _createSwapChain(UINT width, UINT height);
    void _createRenderTarget();
    void _createDCompTarget(HWND hwnd);
    void _createRenderer(bool post_swap_chain = false);
    void _releaseRenderer(bool post_swap_chain = false) noexcept;

public:
    SnowRenderer() :_scx(0), _scy(0), _width(0), _height(0), _hwnd(nullptr) {}
    ~SnowRenderer() noexcept { _releaseRenderer(); _snow_bmp.Reset(); }
    HRESULT initialize(HICON hisnow, UINT width, UINT height, HWND hwnd);
    HRESULT refreash();
    HRESULT presentTest();
    HRESULT render(const SnowList& snows, float alpha = 1.0f);
    HRESULT resize(UINT width, UINT height);
    HRESULT setWindow(HWND hwnd);
};
