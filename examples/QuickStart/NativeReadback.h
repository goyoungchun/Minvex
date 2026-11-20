#pragma once
#include <minvex/EmbeddedRenderer.h>

#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#undef GetObject
#elif defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#endif

namespace capture {
template<class Function>
decltype(auto) GraphicsScope(Function&& function) {
#if defined(__APPLE__)
    @autoreleasepool { return function(); }
#else
    return function();
#endif
}
// The Host owns the render target and staging storage. Both backends return
// tightly packed linear RGBA8 rows from the top left.
class NativeReadback final {
    int width_, height_;
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> color_, staging_;
#elif defined(__APPLE__)
    id<MTLDevice> device_;
    id<MTLTexture> color_;
    id<MTLBuffer> staging_;
    NSUInteger row_;
#endif
public:
    NativeReadback(int width, int height, [[maybe_unused]] bool allowSoftwareDevice = false)
        : width_(width), height_(height) {
#if defined(_WIN32)
        auto createDevice = [&](D3D_DRIVER_TYPE type) {
            constexpr D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
            return D3D11CreateDevice(nullptr, type, nullptr, 0, &level, 1,
                D3D11_SDK_VERSION, &device_, nullptr, &context_);
        };
        auto status = createDevice(D3D_DRIVER_TYPE_HARDWARE);
        if (FAILED(status) && allowSoftwareDevice) { status = createDevice(D3D_DRIVER_TYPE_WARP); }
        if (FAILED(status)) { throw std::runtime_error("D3D11 device unavailable."); }
#elif defined(__APPLE__)
        [NSApplication sharedApplication];
        device_ = MTLCreateSystemDefaultDevice();
        if (!device_) { throw std::runtime_error("Metal device unavailable."); }
#endif
        Resize(width, height);
    }
    // Caller detaches and waits for GPU completion before replacing a target.
    void Resize(int width, int height) {
        if (width < 1 || height < 1 || width > 8192 || height > 8192) {
            throw std::invalid_argument("Invalid Host render target extent.");
        }
#if defined(_WIN32)
        Microsoft::WRL::ComPtr<ID3D11Texture2D> color, staging;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &color))) {
            throw std::runtime_error("Cannot create D3D11 color target.");
        }
        desc.BindFlags = 0;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &staging))) {
            throw std::runtime_error("Cannot create D3D11 staging texture.");
        }
        color_ = std::move(color);
        staging_ = std::move(staging);
#elif defined(__APPLE__)
        auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:width height:height mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModePrivate;
        auto color = [device_ newTextureWithDescriptor:desc];
        const auto row = (static_cast<NSUInteger>(width) * 4 + 255) & ~NSUInteger(255);
        auto staging = [device_ newBufferWithLength:row * height options:MTLResourceStorageModeShared];
        if (!color || !staging) { throw std::runtime_error("Cannot create Metal color target/readback buffer."); }
        color_ = color;
        staging_ = staging;
        row_ = row;
#endif
        width_ = width;
        height_ = height;
    }
    NativeReadback(const NativeReadback&)=delete;
    NativeReadback& operator=(const NativeReadback&)=delete;
    lv::HostGraphicsContext Context() const {
        lv::HostGraphicsContext host;
        host.ColorFormat = lv::EmbeddedColorFormat::Rgba8;
#if defined(_WIN32)
        host.Backend = lv::GraphicsBackend::D3D11;
        host.Device = device_.Get();
        host.ImmediateContext = context_.Get();
#elif defined(__APPLE__)
        host.Backend = lv::GraphicsBackend::Metal;
        host.Device = (__bridge const void*)device_;
#endif
        return host;
    }
    lv::HostRenderTarget Target() const {
#if defined(_WIN32)
        return {color_.Get(), nullptr, width_, height_};
#elif defined(__APPLE__)
        return {(__bridge const void*)color_, nullptr, width_, height_};
#endif
    }
    std::vector<std::uint8_t> Pixels(lv::EmbeddedRenderer& renderer) {
        const auto rowBytes = static_cast<std::size_t>(width_) * 4;
        std::vector<std::uint8_t> result(rowBytes * height_);
        const auto copyRows = [&](const void* data, std::size_t stride) {
            const auto* source = static_cast<const std::uint8_t*>(data);
            for (int y = 0; y < height_; ++y) {
                std::memcpy(result.data() + y * rowBytes, source + y * stride, rowBytes);
            }
        };
#if defined(_WIN32)
        // Map waits for the copy queued on the Host's immediate context.
        (void)renderer;
        context_->CopyResource(staging_.Get(), color_.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
            throw std::runtime_error("D3D11 staging readback failed.");
        }
        copyRows(mapped.pData, mapped.RowPitch);
        context_->Unmap(staging_.Get(), 0);
#elif defined(__APPLE__)
        @autoreleasepool {
            // Use the same queue so the copy follows rendering into the Host texture.
            const auto borrowed = renderer.GetMetalCommandQueue();
            if (!borrowed) { throw std::runtime_error(borrowed.Error().Message); }
            id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)borrowed.Value();
            auto command = [queue commandBuffer];
            auto copy = [command blitCommandEncoder];
            if (!copy) { throw std::runtime_error("Cannot encode Metal readback."); }
            [copy copyFromTexture:color_ sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                sourceSize:MTLSizeMake(width_, height_, 1) toBuffer:staging_ destinationOffset:0
                destinationBytesPerRow:row_ destinationBytesPerImage:row_ * height_];
            [copy endEncoding];
            [command commit];
            [command waitUntilCompleted];
            if (command.status != MTLCommandBufferStatusCompleted) { throw std::runtime_error("Metal readback failed."); }
            copyRows(staging_.contents, row_);
        }
#endif
        return result;
    }
};
} // namespace capture
