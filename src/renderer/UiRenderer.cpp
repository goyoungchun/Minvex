#include "UiRenderer.h"
#include "ui.glsl.h"
#include "platform/FrameProfiling.h"
#include <stdexcept>
#include <cstring>
namespace lv::detail {
UiRenderer::UiRenderer() {
    shader_=sg_make_shader(ui_shader_desc(sg_query_backend()));sg_pipeline_desc p{};p.shader=shader_;p.colors[0].pixel_format=SG_PIXELFORMAT_RGBA8;p.depth.pixel_format=SG_PIXELFORMAT_NONE;p.sample_count=1;p.label="minvex-ui-composite";pipeline_=sg_make_pipeline(p);
    sg_sampler_desc s{};s.min_filter=s.mag_filter=SG_FILTER_LINEAR;s.wrap_u=s.wrap_v=SG_WRAP_CLAMP_TO_EDGE;sampler_=sg_make_sampler(s);
    if (sg_query_shader_state(shader_)!=SG_RESOURCESTATE_VALID||sg_query_pipeline_state(pipeline_)!=SG_RESOURCESTATE_VALID||sg_query_sampler_state(sampler_)!=SG_RESOURCESTATE_VALID) {sg_destroy_sampler(sampler_);sg_destroy_pipeline(pipeline_);sg_destroy_shader(shader_);throw std::runtime_error("Cannot create UI compositor.");}
}
void UiRenderer::ClearImages() noexcept {sg_destroy_view(overlayView_);sg_destroy_view(outputView_);sg_destroy_view(attachment_);sg_destroy_image(overlay_);sg_destroy_image(output_);sg_destroy_buffer(upload_);overlayView_=outputView_=attachment_={};overlay_=output_={};upload_={};padded_={};}
UiRenderer::~UiRenderer() {ClearImages();sg_destroy_sampler(sampler_);sg_destroy_pipeline(pipeline_);sg_destroy_shader(shader_);}
void UiRenderer::Composite(const UiCanvas& canvas,sg_view scene,int width,int height,float scale) {
    const bool resize=width_!=width||height_!=height;
    if (resize||revision_!=canvas.Revision()||scale_!=scale) {
        auto raster=[&] {ProfileScope profileRaster(FramePhase::UiRaster);return canvas.Rasterize(width,height,scale);}();if (!raster) {throw UiRasterError(raster.Error());}
        ProfileScope profileUpload(FramePhase::UiUpload);
        if (resize) {
            sg_image output{},overlay{};sg_view attachment{},outputView{},overlayView{};sg_buffer upload{};
            const int rowPitch=sg_query_row_pitch(SG_PIXELFORMAT_RGBA8,width,256);
            try {
                sg_image_desc d{};d.width=width;d.height=height;d.pixel_format=SG_PIXELFORMAT_RGBA8;d.usage.color_attachment=true;d.label="minvex-ui-output";output=sg_make_image(d);
                sg_view_desc v{};v.color_attachment.image=output;attachment=sg_make_view(v);v={};v.texture.image=output;outputView=sg_make_view(v);
                d.usage={};d.usage.copy_dst=true;d.label="minvex-ui-bitmap";overlay=sg_make_image(d);v={};v.texture.image=overlay;overlayView=sg_make_view(v);
                sg_buffer_desc b{};b.size=static_cast<std::size_t>(rowPitch)*height;b.usage.staging_buffer=true;b.usage.write_transient=true;b.usage.copy_src=true;b.label="minvex-ui-upload";upload=sg_make_buffer(b);
                if (sg_query_image_state(output)!=SG_RESOURCESTATE_VALID||sg_query_image_state(overlay)!=SG_RESOURCESTATE_VALID||sg_query_view_state(attachment)!=SG_RESOURCESTATE_VALID||sg_query_view_state(outputView)!=SG_RESOURCESTATE_VALID||sg_query_view_state(overlayView)!=SG_RESOURCESTATE_VALID||sg_query_buffer_state(upload)!=SG_RESOURCESTATE_VALID) {throw std::runtime_error("Cannot create UI overlay resources.");}
            }catch (...) {sg_destroy_view(overlayView);sg_destroy_view(outputView);sg_destroy_view(attachment);sg_destroy_image(overlay);sg_destroy_image(output);sg_destroy_buffer(upload);throw;}
            ClearImages();output_=output;overlay_=overlay;attachment_=attachment;outputView_=outputView;overlayView_=overlayView;upload_=upload;width_=width;height_=height;rowPitch_=rowPitch;
        }
        const auto& pixels=raster.Value();const void* source=pixels.data();std::size_t bytes=pixels.size();
        if (rowPitch_!=width*4) {padded_.resize(static_cast<std::size_t>(rowPitch_)*height);for (int y=0;y<height;++y) {std::memcpy(padded_.data()+static_cast<std::size_t>(y)*rowPitch_,pixels.data()+static_cast<std::size_t>(y)*width*4,static_cast<std::size_t>(width)*4);}source=padded_.data();bytes=padded_.size();}
        sg_write_buffer_desc write{};write.src.data={source,bytes};write.dst.buffer=upload_;sg_write_buffer_transient(write);
        sg_copy_buffer_to_image_desc copy{};copy.src.buffer=upload_;copy.src.bytes_per_row=rowPitch_;copy.src.bytes_per_slice=rowPitch_*height;copy.dst.image=overlay_;copy.size={width,height,1};sg_copy_buffer_to_image(copy);
        if (sg_query_image_state(output_)!=SG_RESOURCESTATE_VALID||sg_query_image_state(overlay_)!=SG_RESOURCESTATE_VALID||sg_query_view_state(attachment_)!=SG_RESOURCESTATE_VALID||sg_query_view_state(outputView_)!=SG_RESOURCESTATE_VALID||sg_query_view_state(overlayView_)!=SG_RESOURCESTATE_VALID) {throw std::runtime_error("Cannot create UI overlay textures.");}
        revision_=canvas.Revision();scale_=scale;
    }
    ProfileScope profileDraw(FramePhase::UiDraw);
    sg_pass pass{};pass.attachments.colors[0]=attachment_;pass.action.colors[0].load_action=SG_LOADACTION_DONTCARE;sg_begin_pass(pass);sg_bindings b{};b.views[VIEW_ui_scene]=scene;b.views[VIEW_ui_overlay]=overlayView_;b.samplers[SMP_ui_sampler]=sampler_;sg_apply_pipeline(pipeline_);sg_apply_bindings(b);sg_draw(0,3,1);sg_end_pass();
}
}
