#pragma once
#include "SceneRenderer.h"
#include "LightMath.h"
#include "DebugResources.h"
#include "mesh.glsl.h"
#include <bit>
namespace lv::detail {
inline std::uint64_t ShadowKey(std::uint64_t hash,const Mat4& matrix,ShaderVec4 light={}) {const auto add=[&](float v){hash^=std::bit_cast<std::uint32_t>(v);hash*=1099511628211ull;};for(float v:matrix.Elements){add(v);}for(float v:{light.X,light.Y,light.Z,light.W}){add(v);}return hash;}
class LocalShadowAtlas final {
    struct Job {ShaderVec4 Radial{};std::uint64_t Key=0;unsigned First=0,Count=0;};
    LocalShadowLayout layout_;
    LocalShadowSampling sampling_;
    sg_image color_{},depth_{};sg_view colorView_{},depthView_{};
    sg_shader clearShader_{};sg_pipeline clearPipeline_{};
    std::array<std::uint64_t,4> keys_{};bool initialized_=false;
    void Destroy() noexcept {sg_destroy_pipeline(clearPipeline_);sg_destroy_shader(clearShader_);sg_destroy_view(sampling_.Texture);sg_destroy_view(colorView_);sg_destroy_view(depthView_);sg_destroy_image(color_);sg_destroy_image(depth_);sg_destroy_sampler(sampling_.Sampler);}
public:
    explicit LocalShadowAtlas(LocalShadowLayout layout):layout_(layout) {
        try {
            if(layout.Width()>sg_query_limits().max_image_size_2d||layout.Height()>sg_query_limits().max_image_size_2d){throw std::length_error("Local shadow atlas exceeds the GPU extent; reduce Scene.LocalShadowResolution.");}
            sampling_.Columns=layout.Columns;sampling_.Rows=layout.Rows;sampling_.Resolution=layout.Resolution;
            sg_image_desc d{};d.width=layout.Width();d.height=layout.Height();d.pixel_format=SG_PIXELFORMAT_R32F;d.usage.color_attachment=true;d.label="local-shadow-atlas";color_=sg_make_image(d);d.pixel_format=SG_PIXELFORMAT_DEPTH;d.usage={};d.usage.depth_stencil_attachment=true;d.label="local-shadow-depth";depth_=sg_make_image(d);
            sg_view_desc v{};v.color_attachment.image=color_;colorView_=sg_make_view(v);v={};v.depth_stencil_attachment.image=depth_;depthView_=sg_make_view(v);v={};v.texture.image=color_;sampling_.Texture=sg_make_view(v);
            sg_sampler_desc sampler{};sampler.min_filter=sampler.mag_filter=SG_FILTER_NEAREST;sampler.wrap_u=sampler.wrap_v=SG_WRAP_CLAMP_TO_EDGE;sampling_.Sampler=sg_make_sampler(sampler);
            clearShader_=sg_make_shader(shadow_clear_shader_desc(sg_query_backend()));sg_pipeline_desc p{};p.shader=clearShader_;p.colors[0].pixel_format=SG_PIXELFORMAT_R32F;p.depth.pixel_format=SG_PIXELFORMAT_DEPTH;p.depth.compare=SG_COMPAREFUNC_ALWAYS;p.depth.write_enabled=true;p.sample_count=1;p.label="local-shadow-tile-clear";clearPipeline_=sg_make_pipeline(p);
            if(sg_query_image_state(color_)!=SG_RESOURCESTATE_VALID||sg_query_image_state(depth_)!=SG_RESOURCESTATE_VALID||sg_query_view_state(colorView_)!=SG_RESOURCESTATE_VALID||sg_query_view_state(depthView_)!=SG_RESOURCESTATE_VALID||sg_query_view_state(sampling_.Texture)!=SG_RESOURCESTATE_VALID||sg_query_sampler_state(sampling_.Sampler)!=SG_RESOURCESTATE_VALID||sg_query_shader_state(clearShader_)!=SG_RESOURCESTATE_VALID||sg_query_pipeline_state(clearPipeline_)!=SG_RESOURCESTATE_VALID){throw std::runtime_error("Cannot create local shadow atlas.");}
        }catch(...){Destroy();throw;}
    }
    ~LocalShadowAtlas(){Destroy();}
    void Invalidate() noexcept {initialized_=false;}
    bool Matches(LocalShadowLayout l) const {return l.Faces==layout_.Faces&&l.Resolution==layout_.Resolution;}
    std::uint64_t Bytes() const {return layout_.Bytes();}
    void Inspect(std::vector<DebugResource>& r) const {InspectImage(r,color_,"Local shadow atlas",true);InspectImage(r,depth_,"Local shadow depth",true);InspectShader(r,clearShader_,"Local shadow tile clear");InspectPipeline(r,clearPipeline_,"Local shadow tile clear");}
    const LocalShadowSampling& Update(const Scene& scene,SceneRenderer& renderer,std::uint64_t geometry) {
        sampling_.FirstFace.fill(0);std::array<Job,4> jobs;unsigned count=0,tile=0,index=0;bool dirty=false;
        const auto add=[&](const auto& l,unsigned faces,const auto& matrix) {
            if(l.Shadow.Enabled&&l.Intensity>0){auto& job=jobs[count];job.First=tile;job.Count=faces;job.Radial={l.Position.X,l.Position.Y,l.Position.Z,l.Range};job.Key=geometry;sampling_.FirstFace[index]=tile+1;
                for(unsigned face=0;face<faces;++face){auto& m=sampling_.Matrices[tile+face];m=matrix(face);job.Key=ShadowKey(job.Key,m,job.Radial);}dirty|=!initialized_||keys_[count]!=job.Key;++count;tile+=faces;
            }++index;
        };
        for(const auto& l:scene.PointLights){add(l,6,[&](unsigned f){return PointShadowMatrix(l,f);});}for(const auto& l:scene.SpotLights){add(l,1,[&](unsigned){return SpotShadowMatrix(l);});}
        if(!dirty){return sampling_;}
        sg_pass pass{};pass.attachments.colors[0]=colorView_;pass.attachments.depth_stencil=depthView_;pass.action.colors[0].load_action=initialized_ ? SG_LOADACTION_LOAD:SG_LOADACTION_CLEAR;pass.action.colors[0].clear_value={1,1,1,1};pass.action.colors[0].store_action=SG_STOREACTION_STORE;pass.action.depth.load_action=initialized_ ? SG_LOADACTION_LOAD:SG_LOADACTION_CLEAR;pass.action.depth.clear_value=1;pass.action.depth.store_action=SG_STOREACTION_STORE;
        sg_begin_pass(pass);
        try {
            for(unsigned j=0;j<count;++j){const auto& job=jobs[j];if(initialized_&&keys_[j]==job.Key){continue;}for(unsigned f=0;f<job.Count;++f){const auto face=job.First+f;const int x=(face%layout_.Columns)*layout_.Resolution,y=(face/layout_.Columns)*layout_.Resolution;
                sg_apply_viewport(x,y,layout_.Resolution,layout_.Resolution,true);sg_apply_scissor_rect(x,y,layout_.Resolution,layout_.Resolution,true);
                if(initialized_){sg_apply_pipeline(clearPipeline_);sg_draw(0,3,1);renderer.RecordShadowClear();}
                renderer.DrawShadow(scene,sampling_.Matrices[face],job.Radial,true);
            }}
        }catch(...){sg_end_pass();initialized_=false;throw;}
        sg_end_pass();for(unsigned j=0;j<count;++j){keys_[j]=jobs[j].Key;}initialized_=true;return sampling_;
    }
};
}
