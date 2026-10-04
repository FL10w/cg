#include "application.hpp"
#include "scene.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace application {
namespace {
using namespace scene;
constexpr float panelWidth=300;
struct Buffer { VkBuffer handle{}; VmaAllocation allocation{}; void* mapped{}; };
Buffer vertexBuffer,indexBuffer,uniformBuffer;
uint32_t indexCount{};
VkDescriptorSetLayout descriptorLayout{};
VkDescriptorPool descriptorPool{};
VkDescriptorSet descriptor{};
VkPipelineLayout pipelineLayout{};
VkPipeline pipeline{};
float cameraDistance=8.5f,cameraYaw=18,cameraPitch=46;

void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" failed: "+std::to_string(result));
}
Buffer createBuffer(VkDeviceSize size,VkBufferUsageFlags usage,const void* data=nullptr) {
    auto& ctx=graphics::internal::context;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size=size; info.usage=usage; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo allocation{};
    allocation.usage=VMA_MEMORY_USAGE_AUTO;
    allocation.flags=VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
    Buffer b; VmaAllocationInfo result{};
    check(vmaCreateBuffer(ctx.allocator,&info,&allocation,&b.handle,&b.allocation,&result),"vmaCreateBuffer");
    b.mapped=result.pMappedData;
    if(data) {
        std::memcpy(b.mapped,data,static_cast<size_t>(size));
        check(vmaFlushAllocation(ctx.allocator,b.allocation,0,size),"vmaFlushAllocation");
    }
    return b;
}
VkShaderModule loadShader(const char* filename) {
    std::ifstream file(std::string(LAB_SHADER_DIR)+"/"+filename,std::ios::binary|std::ios::ate);
    if(!file) throw std::runtime_error(std::string("Cannot open shader: ")+filename);
    auto size=file.tellg();
    if(size<=0 || size%4!=0) throw std::runtime_error("Invalid SPIR-V size");
    std::vector<uint32_t> data(static_cast<size_t>(size)/4);
    file.seekg(0); file.read(reinterpret_cast<char*>(data.data()),size);
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize=static_cast<size_t>(size); info.pCode=data.data();
    VkShaderModule module{};
    check(vkCreateShaderModule(graphics::internal::context.device,&info,nullptr,&module),"vkCreateShaderModule");
    return module;
}
void createDescriptors() {
    auto device=graphics::internal::context.device;
    VkDescriptorSetLayoutBinding binding{};
    binding.binding=0; binding.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount=1; binding.stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount=1; layout.pBindings=&binding;
    check(vkCreateDescriptorSetLayout(device,&layout,nullptr,&descriptorLayout),"vkCreateDescriptorSetLayout");
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.maxSets=1; pool.poolSizeCount=1; pool.pPoolSizes=&poolSize;
    check(vkCreateDescriptorPool(device,&pool,nullptr,&descriptorPool),"vkCreateDescriptorPool");
    uniformBuffer=createBuffer(sizeof(Mat4),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool=descriptorPool; alloc.descriptorSetCount=1; alloc.pSetLayouts=&descriptorLayout;
    check(vkAllocateDescriptorSets(device,&alloc,&descriptor),"vkAllocateDescriptorSets");
    VkDescriptorBufferInfo buffer{uniformBuffer.handle,0,sizeof(Mat4)};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet=descriptor; write.dstBinding=0; write.descriptorCount=1;
    write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; write.pBufferInfo=&buffer;
    vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    info.setLayoutCount=1; info.pSetLayouts=&descriptorLayout;
    check(vkCreatePipelineLayout(device,&info,nullptr,&pipelineLayout),"vkCreatePipelineLayout");
}
void createPipeline() {
    auto device=graphics::internal::context.device;
    VkShaderModule vert=loadShader("torus.vert.spv"),frag{};
    try {
        frag=loadShader("torus.frag.spv");
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType=stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=vert; stages[0].pName="main";
        stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=frag; stages[1].pName="main";
        VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[]{
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,position)},
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,normal)}};
        VkPipelineVertexInputStateCreateInfo vertices{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertices.vertexBindingDescriptionCount=1; vertices.pVertexBindingDescriptions=&binding;
        vertices.vertexAttributeDescriptionCount=2; vertices.pVertexAttributeDescriptions=attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount=viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_BACK_BIT;
        raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable=depth.depthWriteEnable=VK_TRUE; depth.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState color{};
        color.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount=1; blend.pAttachments=&color;
        VkDynamicState states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount=2; dynamic.pDynamicStates=states;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount=2; info.pStages=stages; info.pVertexInputState=&vertices; info.pInputAssemblyState=&assembly;
        info.pViewportState=&viewport; info.pRasterizationState=&raster; info.pMultisampleState=&ms;
        info.pDepthStencilState=&depth; info.pColorBlendState=&blend; info.pDynamicState=&dynamic;
        info.layout=pipelineLayout; info.renderPass=graphics::internal::context.render_pass;
        check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&pipeline),"vkCreateGraphicsPipelines");
    } catch(...) {
        vkDestroyShaderModule(device,vert,nullptr); if(frag) vkDestroyShaderModule(device,frag,nullptr); throw;
    }
    vkDestroyShaderModule(device,vert,nullptr); vkDestroyShaderModule(device,frag,nullptr);
}
void controls() {
    auto& io=ImGui::GetIO();
    const float width=std::min(panelWidth,std::max(1.0f,io.DisplaySize.x*.46f));
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({width,io.DisplaySize.y});
    ImGui::Begin("Лабораторная работа №1",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
    ImGui::TextColored({.35f,.8f,1,1},"ВАРИАНТ 7 / ТОР");
    ImGui::Text("Шитов Н. В. · М80-314БВ-24");
    ImGui::TextDisabled("Vulkan · C++20 · GLFW · ImGui");
    ImGui::Separator();
    ImGui::Text("Управление камерой");
    ImGui::TextWrapped("Зажмите ЛКМ в области сцены, чтобы осмотреть тор. Колесо мыши приближает и отдаляет камеру.");
    if(ImGui::Button("Сбросить камеру")) configureView("default");
    ImGui::Separator();
    ImGui::Text("2048 вершин");
    ImGui::Text("4096 треугольников");
    ImGui::Text("Перспективная проекция");
    ImGui::TextDisabled("Esc — закрыть окно");
    ImGui::End();
    if(!io.WantCaptureMouse && io.MousePos.x>width) {
        if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            cameraYaw-=io.MouseDelta.x*.35f;
            cameraPitch=std::clamp(cameraPitch+io.MouseDelta.y*.35f,-80.0f,80.0f);
        }
        cameraDistance=std::clamp(cameraDistance-io.MouseWheel*.5f,3.0f,18.0f);
    }
}
} // namespace
bool initialize() {
    try {
        ImGui::StyleColorsDark();
        auto& style=ImGui::GetStyle(); style.FrameRounding=4; style.WindowRounding=0;
        style.ItemSpacing={8,6}; style.WindowPadding={14,14};
        style.Colors[ImGuiCol_WindowBg]={.055f,.065f,.09f,1};
        ImGui::GetIO().IniFilename=nullptr;
        if(!ImGui::GetIO().Fonts->AddFontFromFileTTF(LAB_ASSET_DIR "/DejaVuSans.ttf",15))
            throw std::runtime_error("Cannot load Cyrillic font");
        auto mesh=torus(); indexCount=static_cast<uint32_t>(mesh.indices.size());
        vertexBuffer=createBuffer(mesh.vertices.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,mesh.vertices.data());
        indexBuffer=createBuffer(mesh.indices.size()*sizeof(uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,mesh.indices.data());
        createDescriptors(); createPipeline();
        std::cout<<"Torus: "<<mesh.vertices.size()<<" vertices, "<<indexCount/3<<" triangles; one descriptor set\n";
        return true;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; shutdown(); return false; }
}
void shutdown() {
    auto& ctx=graphics::internal::context;
    if(!ctx.device) return;
    vkDeviceWaitIdle(ctx.device);
    if(pipeline) vkDestroyPipeline(ctx.device,pipeline,nullptr);
    if(pipelineLayout) vkDestroyPipelineLayout(ctx.device,pipelineLayout,nullptr);
    if(descriptorPool) vkDestroyDescriptorPool(ctx.device,descriptorPool,nullptr);
    if(descriptorLayout) vkDestroyDescriptorSetLayout(ctx.device,descriptorLayout,nullptr);
    if(uniformBuffer.handle) vmaDestroyBuffer(ctx.allocator,uniformBuffer.handle,uniformBuffer.allocation);
    if(indexBuffer.handle) vmaDestroyBuffer(ctx.allocator,indexBuffer.handle,indexBuffer.allocation);
    if(vertexBuffer.handle) vmaDestroyBuffer(ctx.allocator,vertexBuffer.handle,vertexBuffer.allocation);
}
void configureView(std::string_view name) {
    cameraDistance=8.5f; cameraYaw=18; cameraPitch=46;
    if(name=="side") { cameraYaw=70; cameraPitch=20; }
}
void update() { controls(); }
void render(const graphics::internal::FrameData& fd) {
    auto& ctx=graphics::internal::context; auto cmd=fd.command_buffer;
    if(!cmd) throw std::runtime_error("Frame acquisition failed");
    // prepare() waits for the previous frame fence: CPU writes are safe now.
    const auto& io=ImGui::GetIO();
    const float panel=std::min(panelWidth,std::max(1.0f,io.DisplaySize.x*.46f));
    const float pixelPanel=panel*(ctx.swapchain_extent.width/std::max(1.0f,io.DisplaySize.x));
    const uint32_t left=std::min(static_cast<uint32_t>(pixelPanel),ctx.swapchain_extent.width-1);
    const float width=static_cast<float>(ctx.swapchain_extent.width-left),height=static_cast<float>(ctx.swapchain_extent.height);
    const Vec3 eye{cameraDistance*std::cos(radians(cameraPitch))*std::sin(radians(cameraYaw)),
                   cameraDistance*std::sin(radians(cameraPitch)),cameraDistance*std::cos(radians(cameraPitch))*std::cos(radians(cameraYaw))};
    const Mat4 vp=perspective(radians(45),width/height,.1f,100)*lookAt(eye,{});
    std::memcpy(uniformBuffer.mapped,&vp,sizeof(vp));
    check(vmaFlushAllocation(ctx.allocator,uniformBuffer.allocation,0,sizeof(vp)),"Flush uniform");
    check(vkResetCommandBuffer(cmd,0),"vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(cmd,&begin),"vkBeginCommandBuffer");
    VkClearValue clears[2]{}; clears[0].color={{.025f,.033f,.05f,1}}; clears[1].depthStencil={1,0};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass=ctx.render_pass; pass.framebuffer=fd.framebuffer; pass.renderArea.extent=ctx.swapchain_extent;
    pass.clearValueCount=2; pass.pClearValues=clears;
    vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{static_cast<float>(left),0,width,height,0,1};
    VkRect2D scissor{{static_cast<int32_t>(left),0},{ctx.swapchain_extent.width-left,ctx.swapchain_extent.height}};
    vkCmdSetViewport(cmd,0,1,&viewport); vkCmdSetScissor(cmd,0,1,&scissor);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    const VkDeviceSize offset=0; vkCmdBindVertexBuffers(cmd,0,1,&vertexBuffer.handle,&offset);
    vkCmdBindIndexBuffer(cmd,indexBuffer.handle,0,VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&descriptor,0,nullptr);
    vkCmdDrawIndexed(cmd,indexCount,1,0,0,0);
    vkCmdEndRenderPass(cmd); check(vkEndCommandBuffer(cmd),"vkEndCommandBuffer");
}
} // namespace application
