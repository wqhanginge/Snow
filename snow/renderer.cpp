#include "stdafx.hpp"
#include "renderer.hpp"


void SnowRenderer::_loadSnowBitmap(HICON hisnow) {
    ComPtr<IWICImagingFactory2> wic_factory;
    HR(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, __uuidof(wic_factory), (void**)wic_factory.GetAddressOf()));

    ComPtr<IWICBitmap> wic_bmp;
    HR(wic_factory->CreateBitmapFromHICON(hisnow, wic_bmp.GetAddressOf()));
    HR(WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, wic_bmp.Get(), _snow_bmp.GetAddressOf()));
    HR(_snow_bmp->GetSize(&_scx, &_scy));
}

void SnowRenderer::_createDevice() {
    HR(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
        D3D11_SDK_VERSION, _d3d_device.GetAddressOf(), nullptr, nullptr
    ));
}

void SnowRenderer::_createDeviceContext() {
    ComPtr<IDXGIDevice4> dxgi_device;
    HR(_d3d_device.As(&dxgi_device));
    ComPtr<ID2D1Factory7> d2d_factory;
    HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory.GetAddressOf()));

    ComPtr<ID2D1Device6> d2d_device;
    HR(d2d_factory->CreateDevice(dxgi_device.Get(), d2d_device.GetAddressOf()));
    HR(d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, _d2d_dc.GetAddressOf()));

    ComPtr<ID2D1Bitmap1> d2d_bmp;
    HR(_d2d_dc->CreateBitmapFromWicBitmap(_snow_bmp.Get(), d2d_bmp.GetAddressOf()));
    HR(_d2d_dc->CreateEffect(CLSID_D2D12DAffineTransform, _d2d_dc_bmp_affine.GetAddressOf()));
    HR(_d2d_dc->CreateEffect(CLSID_D2D1Opacity, _d2d_dc_bmp_opacity.GetAddressOf()));

    _d2d_dc_bmp_affine->SetValue(D2D1_2DAFFINETRANSFORM_PROP_INTERPOLATION_MODE, D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    _d2d_dc_bmp_affine->SetInput(0, d2d_bmp.Get());
    _d2d_dc_bmp_opacity->SetInputEffect(0, _d2d_dc_bmp_affine.Get());
    _d2d_dc_bmp_opacity->GetOutput(_d2d_dc_bmp.GetAddressOf());
}

void SnowRenderer::_createSwapChain(UINT width, UINT height) {
    ComPtr<IDXGIDevice4> dxgi_device;
    HR(_d3d_device.As(&dxgi_device));
    ComPtr<IDXGIAdapter4> dxgi_adapter;
    HR(dxgi_device->GetParent(__uuidof(dxgi_adapter), (void**)dxgi_adapter.GetAddressOf()));
    ComPtr<IDXGIFactory7> dxgi_factory;
    HR(dxgi_adapter->GetParent(__uuidof(dxgi_factory), (void**)dxgi_factory.GetAddressOf()));

    DXGI_SWAP_CHAIN_DESC1 scdesc = {};
    scdesc.Width = width;
    scdesc.Height = height;
    scdesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    scdesc.BufferCount = 2;
    scdesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scdesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scdesc.SampleDesc.Count = 1;
    HR(dxgi_factory->CreateSwapChainForComposition(dxgi_device.Get(), &scdesc, nullptr, _dxgi_swap_chain.GetAddressOf()));
}

void SnowRenderer::_createRenderTarget() {
    ComPtr<IDXGISurface2> dxgi_surface;
    HR(_dxgi_swap_chain->GetBuffer(0, __uuidof(dxgi_surface), (void**)dxgi_surface.GetAddressOf()));

    ComPtr<ID2D1Bitmap1> d2d_bitmap;
    D2D1_BITMAP_PROPERTIES1 d2d_bmp_prop = {};
    d2d_bmp_prop.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d2d_bmp_prop.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    d2d_bmp_prop.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    HR(_d2d_dc->CreateBitmapFromDxgiSurface(dxgi_surface.Get(), &d2d_bmp_prop, d2d_bitmap.GetAddressOf()));

    _d2d_dc->SetTarget(d2d_bitmap.Get());
    _d2d_dc->SetDpi((FLOAT)USER_DEFAULT_SCREEN_DPI, (FLOAT)USER_DEFAULT_SCREEN_DPI);
}

void SnowRenderer::_createDCompTarget(HWND hwnd) {
    ComPtr<IDXGIDevice4> dxgi_device;
    HR(_d3d_device.As(&dxgi_device));
    ComPtr<IDCompositionDevice> dcomp_device;
    HR(DCompositionCreateDevice3(dxgi_device.Get(), __uuidof(dcomp_device), (void**)dcomp_device.GetAddressOf()));

    ComPtr<IDCompositionVisual> dcomp_visual;
    HR(dcomp_device->CreateVisual(dcomp_visual.GetAddressOf()));
    HR(dcomp_visual->SetContent(_dxgi_swap_chain.Get()));

    HR(dcomp_device->CreateTargetForHwnd(hwnd, true, _dcomp_target.GetAddressOf()));
    HR(_dcomp_target->SetRoot(dcomp_visual.Get()));
    HR(dcomp_device->Commit());
}

void SnowRenderer::_createRenderer(bool post_swap_chain /*false*/) {
    if (!post_swap_chain) {
        _createDevice();
        _createDeviceContext();
        _createSwapChain(_width, _height);
    }
    _createRenderTarget();
    _createDCompTarget(_hwnd);
}

void SnowRenderer::_releaseRenderer(bool post_swap_chain /*flase*/) noexcept {
    _dcomp_target.Reset();
    if (_d2d_dc) _d2d_dc->SetTarget(nullptr);
    if (!post_swap_chain) {
        _dxgi_swap_chain.Reset();
        _d2d_dc_bmp.Reset();
        if (_d2d_dc_bmp_opacity) _d2d_dc_bmp_opacity->SetInput(0, nullptr);
        if (_d2d_dc_bmp_affine) _d2d_dc_bmp_affine->SetInput(0, nullptr);
        _d2d_dc_bmp_opacity.Reset();
        _d2d_dc_bmp_affine.Reset();
        _d2d_dc.Reset();
        _d3d_device.Reset();
    }
}

HRESULT SnowRenderer::initialize(HICON hisnow, UINT width, UINT height, HWND hwnd) {
    if (!hisnow && !width && !height && !hwnd) return E_INVALIDARG;
    _width = width;
    _height = height;
    _hwnd = hwnd;
    try {
        _loadSnowBitmap(hisnow);
        _createRenderer();
    } catch (ComException& ce) {
        return ce.hr;
    }
    return S_OK;
}

HRESULT SnowRenderer::refreash() {
    try {
        _releaseRenderer();
        _createRenderer();
    } catch (ComException& ce) {
        return ce.hr;
    }
    return S_OK;
}

HRESULT SnowRenderer::presentTest() {
    return _dxgi_swap_chain->Present(1, DXGI_PRESENT_TEST);
}

HRESULT SnowRenderer::render(const SnowList& snows, float alpha /*1.0f*/) {
    _d2d_dc->BeginDraw();
    _d2d_dc->Clear();

    for (auto& item : snows) {
        D2D_MATRIX_3X2_F mtx = D2D1::Matrix3x2F(item.size / _scx, 0, 0, item.size / _scy, 0, 0);
        _d2d_dc_bmp_affine->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX, mtx);
        _d2d_dc_bmp_opacity->SetValue(D2D1_OPACITY_PROP_OPACITY, item.alpha() * alpha);

        D2D_POINT_2F dstpos = D2D1::Point2F(item.x, item.y);
        _d2d_dc->DrawImage(_d2d_dc_bmp.Get(), dstpos, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    }

    _d2d_dc->EndDraw();
    return _dxgi_swap_chain->Present(1, 0);
}

HRESULT SnowRenderer::resize(UINT width, UINT height) {
    if (!width && !height) return E_INVALIDARG;
    if (width != _width || height != _height) {
        _width = width;
        _height = height;
        _releaseRenderer(true);
        try {
            HR(_dxgi_swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));
            _createRenderer(true);
        } catch (ComException& ce) {
            return ce.hr;
        }
    }
    return S_OK;
}

HRESULT SnowRenderer::setWindow(HWND hwnd) {
    if (!hwnd) return E_INVALIDARG;
    if (hwnd != _hwnd) {
        _hwnd = hwnd;
        _dcomp_target.Reset();
        try {
            _createDCompTarget(hwnd);
        } catch (ComException& ce) {
            return ce.hr;
        }
    }
    return S_OK;
}
