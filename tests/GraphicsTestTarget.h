#pragma once
#include <minvex/EmbeddedRenderer.h>
#include <cstring>
#include <stdexcept>
#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#undef GetObject
#elif defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#endif

namespace test {
template<class Function> decltype(auto) GraphicsScope(Function&& function) {
#if defined(__APPLE__)
    @autoreleasepool {return function();}
#else
    return function();
#endif
}
// Native readback belongs to the tests. Both backends expose the same linear
// RGBA8, top-left image so the assertions do not depend on Host channel order.
class GraphicsTestTarget final {
    int width_,height_;
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> color_,staging_;
#elif defined(__APPLE__)
    id<MTLDevice> device_;
    id<MTLTexture> color_;
    id<MTLBuffer> staging_;
    NSUInteger row_;
#endif
public:
    GraphicsTestTarget(int width,int height):width_(width),height_(height) {
        if(width<1||height<1||width>8192||height>8192){throw std::invalid_argument("Invalid test target extent.");}
#if defined(_WIN32)
        D3D_FEATURE_LEVEL level;
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_))){throw std::runtime_error("D3D11 device unavailable.");}
#elif defined(__APPLE__)
        [NSApplication sharedApplication];
        device_=MTLCreateSystemDefaultDevice();if(!device_){throw std::runtime_error("Metal device unavailable.");}
#endif
        Resize(width,height);
    }
    // Caller detaches and waits for GPU completion before replacing a target.
    void Resize(int width,int height) {
        if(width<1||height<1||width>8192||height>8192){throw std::invalid_argument("Invalid test target extent.");}
#if defined(_WIN32)
        Microsoft::WRL::ComPtr<ID3D11Texture2D> color,staging;
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=1;d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&color))){throw std::runtime_error("Cannot create test color target.");}
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&staging))){throw std::runtime_error("Cannot create test staging target.");}
        color_=std::move(color);staging_=std::move(staging);
#elif defined(__APPLE__)
        auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];
        d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;d.storageMode=MTLStorageModePrivate;auto color=[device_ newTextureWithDescriptor:d];
        const auto row=(static_cast<NSUInteger>(width)*4+255)&~NSUInteger(255);
        auto staging=[device_ newBufferWithLength:row*height options:MTLResourceStorageModeShared];
        if(!color||!staging){throw std::runtime_error("Cannot create Metal test target/readback.");}
        color_=color;staging_=staging;row_=row;
#endif
        width_=width;height_=height;
    }
    GraphicsTestTarget(const GraphicsTestTarget&)=delete;
    GraphicsTestTarget& operator=(const GraphicsTestTarget&)=delete;
    lv::HostGraphicsContext Context() const {
        lv::HostGraphicsContext host;host.ColorFormat=lv::EmbeddedColorFormat::Rgba8;
#if defined(_WIN32)
        host.Backend=lv::GraphicsBackend::D3D11;host.Device=device_.Get();host.ImmediateContext=context_.Get();
#elif defined(__APPLE__)
        host.Backend=lv::GraphicsBackend::Metal;host.Device=(__bridge const void*)device_;
#endif
        return host;
    }
    lv::HostRenderTarget Target() const {
#if defined(_WIN32)
        return {color_.Get(),nullptr,width_,height_};
#elif defined(__APPLE__)
        return {(__bridge const void*)color_,nullptr,width_,height_};
#endif
    }
    std::vector<std::uint8_t> Pixels(lv::EmbeddedRenderer& renderer) {
        std::vector<std::uint8_t> result(static_cast<std::size_t>(width_)*height_*4);
#if defined(_WIN32)
        (void)renderer;context_->CopyResource(staging_.Get(),color_.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context_->Map(staging_.Get(),0,D3D11_MAP_READ,0,&mapped))){throw std::runtime_error("D3D11 test readback failed.");}
        for(int y=0;y<height_;++y){std::memcpy(result.data()+static_cast<std::size_t>(y)*width_*4,static_cast<const std::uint8_t*>(mapped.pData)+static_cast<std::size_t>(y)*mapped.RowPitch,width_*4);}
        context_->Unmap(staging_.Get(),0);
#elif defined(__APPLE__)
        @autoreleasepool {
            const auto borrowed=renderer.GetMetalCommandQueue();if(!borrowed){throw std::runtime_error(borrowed.Error().Message);}
            id<MTLCommandQueue> queue=(__bridge id<MTLCommandQueue>)borrowed.Value();auto command=[queue commandBuffer];auto copy=[command blitCommandEncoder];
            if(!command||!copy){throw std::runtime_error("Cannot encode Metal test readback.");}
            [copy copyFromTexture:color_ sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width_,height_,1) toBuffer:staging_ destinationOffset:0 destinationBytesPerRow:row_ destinationBytesPerImage:row_*height_];
            [copy endEncoding];[command commit];[command waitUntilCompleted];
            if(command.status!=MTLCommandBufferStatusCompleted){throw std::runtime_error("Metal test readback command failed.");}
            for(int y=0;y<height_;++y){std::memcpy(result.data()+static_cast<std::size_t>(y)*width_*4,static_cast<const std::uint8_t*>(staging_.contents)+static_cast<std::size_t>(y)*row_,width_*4);}
        }
#endif
        return result;
    }
};
}
