#include "application.hpp"
#include "scene.hpp"
#include "texture_image.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

namespace application {
namespace {
using namespace scene;
constexpr float panelWidth=400;
struct Buffer { VkBuffer handle{}; VmaAllocation allocation{}; void* mapped{}; };
Buffer vertexBuffer,indexBuffer,uniformBuffer;
constexpr uint32_t modelCount=1;
VkDeviceSize modelStride{};
struct Texture {
    VkImage image{};
    VmaAllocation allocation{};
    VkImageView view{};
    VkSampler sampler{};
} texture;
uint32_t indexCount{};
VkDescriptorSetLayout descriptorLayout{};
VkDescriptorPool descriptorPool{};
VkDescriptorSet descriptor{};
VkPipelineLayout pipelineLayout{};
VkPipeline pipeline{};
Camera camera;
Lighting light;
Material material;

void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" failed: "+std::to_string(result));
}
Buffer createHostBuffer(VkDeviceSize size,VkBufferUsageFlags usage,const void* data=nullptr) {
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
        const auto result=vmaFlushAllocation(ctx.allocator,b.allocation,0,size);
        if(result!=VK_SUCCESS) {
            vmaDestroyBuffer(ctx.allocator,b.handle,b.allocation);
            check(result,"vmaFlushAllocation");
        }
    }
    return b;
}
// Uploads run only at startup, on the same queue as rendering. The fence keeps
// staging memory and the temporary command pool alive until the copy completes.
void upload(const std::function<void(VkCommandBuffer)>& record) {
    auto& ctx=graphics::internal::context;
    VkCommandPool pool{};
    VkFence fence{};
    bool submitted=false;
    try {
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags=VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex=ctx.graphics_queue_index;
        check(vkCreateCommandPool(ctx.device,&poolInfo,nullptr,&pool),"Create upload command pool");
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.commandPool=pool; alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc.commandBufferCount=1;
        VkCommandBuffer cmd{};
        check(vkAllocateCommandBuffers(ctx.device,&alloc,&cmd),"Allocate upload command buffer");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(cmd,&begin),"Begin upload");
        record(cmd);
        check(vkEndCommandBuffer(cmd),"End upload");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        check(vkCreateFence(ctx.device,&fenceInfo,nullptr,&fence),"Create upload fence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
        check(vkQueueSubmit(ctx.graphics_queue,1,&submit,fence),"Submit upload");
        submitted=true;
        check(vkWaitForFences(ctx.device,1,&fence,VK_TRUE,std::numeric_limits<uint64_t>::max()),"Wait for upload");
    } catch(...) {
        if(submitted) vkQueueWaitIdle(ctx.graphics_queue);
        if(fence) vkDestroyFence(ctx.device,fence,nullptr);
        if(pool) vkDestroyCommandPool(ctx.device,pool,nullptr);
        throw;
    }
    vkDestroyFence(ctx.device,fence,nullptr);
    vkDestroyCommandPool(ctx.device,pool,nullptr);
}
Buffer createDeviceBuffer(VkDeviceSize size,VkBufferUsageFlags usage,const void* data) {
    auto& ctx=graphics::internal::context;
    auto staging=createHostBuffer(size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,data);
    Buffer result;
    try {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=size; info.usage=usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocation{};
        allocation.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocation.requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        check(vmaCreateBuffer(ctx.allocator,&info,&allocation,&result.handle,&result.allocation,nullptr),"Create device buffer");
        upload([&](VkCommandBuffer cmd) {
            VkBufferCopy copy{0,0,size};
            vkCmdCopyBuffer(cmd,staging.handle,result.handle,1,&copy);
            VkBufferMemoryBarrier ready{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            ready.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            ready.dstAccessMask=usage==VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT : VK_ACCESS_INDEX_READ_BIT;
            ready.srcQueueFamilyIndex=ready.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            ready.buffer=result.handle; ready.offset=0; ready.size=size;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,nullptr,1,&ready,0,nullptr);
        });
    } catch(...) {
        if(result.handle) vmaDestroyBuffer(ctx.allocator,result.handle,result.allocation);
        vmaDestroyBuffer(ctx.allocator,staging.handle,staging.allocation);
        throw;
    }
    vmaDestroyBuffer(ctx.allocator,staging.handle,staging.allocation);
    return result;
}
void createTexture() {
    auto& ctx=graphics::internal::context;
    const auto image=loadTextureImage(LAB_ASSET_DIR "/torus.bmp");
    auto staging=createHostBuffer(image.pixels.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,image.pixels.data());
    try {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType=VK_IMAGE_TYPE_2D; info.format=VK_FORMAT_R8G8B8A8_SRGB;
        info.extent={image.width,image.height,1}; info.mipLevels=info.arrayLayers=1;
        info.samples=VK_SAMPLE_COUNT_1_BIT; info.tiling=VK_IMAGE_TILING_OPTIMAL;
        info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
        info.sharingMode=VK_SHARING_MODE_EXCLUSIVE; info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo allocation{};
        allocation.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocation.requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        check(vmaCreateImage(ctx.allocator,&info,&allocation,&texture.image,&texture.allocation,nullptr),"Create texture image");
        upload([&](VkCommandBuffer cmd) {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            barrier.image=texture.image; barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            VkBufferImageCopy copy{};
            copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            copy.imageExtent={image.width,image.height,1};
            vkCmdCopyBufferToImage(cmd,staging.handle,texture.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        });
    } catch(...) {
        vmaDestroyBuffer(ctx.allocator,staging.handle,staging.allocation);
        throw; // initialize() releases the texture through shutdown().
    }
    vmaDestroyBuffer(ctx.allocator,staging.handle,staging.allocation);
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image=texture.image; view.viewType=VK_IMAGE_VIEW_TYPE_2D; view.format=VK_FORMAT_R8G8B8A8_SRGB;
    view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    check(vkCreateImageView(ctx.device,&view,nullptr,&texture.view),"Create texture view");
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;
    sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.maxLod=0; // A single mip level; no optional anisotropy feature needed.
    check(vkCreateSampler(ctx.device,&sampler,nullptr,&texture.sampler),"Create texture sampler");
    std::cout<<"Texture: torus.bmp, "<<image.width<<'x'<<image.height<<", RGBA8 sRGB, device-local image\n";
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
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding=0; bindings[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    bindings[0].descriptorCount=1; bindings[0].stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding=1; bindings[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount=1; bindings[1].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount=2; layout.pBindings=bindings;
    check(vkCreateDescriptorSetLayout(device,&layout,nullptr,&descriptorLayout),"vkCreateDescriptorSetLayout");
    VkDescriptorPoolSize poolSizes[]{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.maxSets=1; pool.poolSizeCount=2; pool.pPoolSizes=poolSizes;
    check(vkCreateDescriptorPool(device,&pool,nullptr,&descriptorPool),"vkCreateDescriptorPool");
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(graphics::internal::context.physical_device,&properties);
    modelStride=uniformStride(static_cast<size_t>(properties.limits.minUniformBufferOffsetAlignment));
    uniformBuffer=createHostBuffer(modelStride*modelCount,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool=descriptorPool; alloc.descriptorSetCount=1; alloc.pSetLayouts=&descriptorLayout;
    check(vkAllocateDescriptorSets(device,&alloc,&descriptor),"vkAllocateDescriptorSets");
    VkDescriptorBufferInfo buffer{uniformBuffer.handle,0,sizeof(Uniforms)};
    VkDescriptorImageInfo image{texture.sampler,texture.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet writes[2]{};
    for(auto& write:writes) {
        write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet=descriptor; write.descriptorCount=1;
    }
    writes[0].dstBinding=0; writes[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[0].pBufferInfo=&buffer;
    writes[1].dstBinding=1; writes[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo=&image;
    vkUpdateDescriptorSets(device,2,writes,0,nullptr);
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
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,normal)},
            {2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,uv)}};
        VkPipelineVertexInputStateCreateInfo vertices{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertices.vertexBindingDescriptionCount=1; vertices.pVertexBindingDescriptions=&binding;
        vertices.vertexAttributeDescriptionCount=3; vertices.pVertexAttributeDescriptions=attributes;
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
    ImGui::Begin("ЛР №3 — Текстурирование",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
    ImGui::TextColored({.35f,.8f,1,1},"ВАРИАНТ 7 / ТОР");
    ImGui::Text("Шитов Н. В. · М80-314БВ-24");
    ImGui::TextDisabled("Vulkan · GLFW · ImGui / Блинн–Фонг");
    ImGui::Text("Текстура: torus.bmp");
    ImGui::Separator();
    if(ImGui::CollapsingHeader("Камера",ImGuiTreeNodeFlags_DefaultOpen)) {
        float position[]{camera.position.x,camera.position.y,camera.position.z};
        if(ImGui::DragFloat3("Позиция XYZ",position,.05f))
            camera.position={position[0],position[1],position[2]};
        ImGui::SliderFloat("Поворот Y",&camera.yaw,-180,180,"%.1f°",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Наклон",&camera.pitch,-89,89,"%.1f°",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Угол обзора",&camera.fov,15,100,"%.1f°",ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Ближняя пл.",&camera.nearPlane,.01f,.01f,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Дальняя пл.",&camera.farPlane,1,11,1000,"%.1f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Скорость",&camera.speed,.1f,15,"%.1f",ImGuiSliderFlags_AlwaysClamp);
        if(ImGui::Button("Сбросить камеру")) configureView("default");
    }
    if(ImGui::CollapsingHeader("Направленный свет",ImGuiTreeNodeFlags_DefaultOpen)) {
        float direction[]{light.direction.x,light.direction.y,light.direction.z};
        if(ImGui::DragFloat3("Направление",direction,.01f,-1,1,"%.2f",ImGuiSliderFlags_AlwaysClamp))
            light.direction={direction[0],direction[1],direction[2]};
        ImGui::TextDisabled("Направление хода лучей (XYZ)");
        if(dot(light.direction,light.direction)<=1e-8f)
            ImGui::TextWrapped("Нулевой вектор: используется (0, -1, 0).");
        ImGui::ColorEdit3("Цвет света",&light.color.x);
        ImGui::SliderFloat("Интенсивность",&light.intensity,0,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::ColorEdit3("Фоновый цвет",&light.ambientColor.x);
        ImGui::SliderFloat("Фоновый свет",&light.ambientIntensity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    }
    if(ImGui::CollapsingHeader("Материал",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Альбедо загружается из assets/torus.bmp.");
        ImGui::ColorEdit3("Цвет блика",&material.specularColor.x);
        ImGui::SliderFloat("Коэф. блика",&material.shininess,1,256,"%.1f",ImGuiSliderFlags_Logarithmic|ImGuiSliderFlags_AlwaysClamp);
    }
    if(ImGui::Button("Сбросить свет и материал")) configurePreset("default");
    ImGui::Separator();
    ImGui::TextWrapped("В области сцены: ПКМ — поворот; WASD — движение; Q/E — вниз/вверх; Shift — быстрее; колесо — угол обзора.");
    ImGui::TextDisabled("2145 вершин · 4096 треугольников");
    ImGui::TextDisabled("Esc — закрыть окно");
    ImGui::End();
    if(!io.WantCaptureMouse && io.MousePos.x>width) {
        if(ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            camera.rotate(-io.MouseDelta.x*.2f,-io.MouseDelta.y*.2f);
        camera.fov=std::clamp(camera.fov-io.MouseWheel*2,15.0f,100.0f);
        if(!io.WantCaptureKeyboard) {
            const auto held=[](ImGuiKey key) { return ImGui::IsKeyDown(key) ? 1.0f : 0.0f; };
            const Vec3 local{held(ImGuiKey_D)-held(ImGuiKey_A),held(ImGuiKey_E)-held(ImGuiKey_Q),
                             held(ImGuiKey_W)-held(ImGuiKey_S)};
            camera.move(local,std::min(io.DeltaTime,.1f)*(io.KeyShift ? 3.0f : 1.0f));
        }
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
        vertexBuffer=createDeviceBuffer(mesh.vertices.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,mesh.vertices.data());
        indexBuffer=createDeviceBuffer(mesh.indices.size()*sizeof(uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,mesh.indices.data());
        createTexture();
        createDescriptors(); createPipeline();
        std::cout<<"Torus: "<<mesh.vertices.size()<<" vertices, "<<indexCount/3<<" triangles; one descriptor set; dynamic UBO stride "
                 <<modelStride<<"; vertex/index buffers in device-local memory\n";
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
    if(texture.sampler) vkDestroySampler(ctx.device,texture.sampler,nullptr);
    if(texture.view) vkDestroyImageView(ctx.device,texture.view,nullptr);
    if(texture.image) vmaDestroyImage(ctx.allocator,texture.image,texture.allocation);
    if(uniformBuffer.handle) vmaDestroyBuffer(ctx.allocator,uniformBuffer.handle,uniformBuffer.allocation);
    if(indexBuffer.handle) vmaDestroyBuffer(ctx.allocator,indexBuffer.handle,indexBuffer.allocation);
    if(vertexBuffer.handle) vmaDestroyBuffer(ctx.allocator,vertexBuffer.handle,vertexBuffer.allocation);
}
void configureView(std::string_view name) {
    camera=Camera{};
    if(name=="side") {
        const float distance=7.5f,yaw=70,pitch=20;
        camera.position={distance*std::cos(radians(pitch))*std::sin(radians(yaw)),
                         distance*std::sin(radians(pitch)),distance*std::cos(radians(pitch))*std::cos(radians(yaw))};
        camera.yaw=yaw-180; camera.pitch=-pitch;
    } else if(name=="wide") camera.fov=75;
}
void configurePreset(std::string_view name) {
    light=Lighting{}; material=Material{};
    if(name=="ambient") light.intensity=0;
    else if(name=="diffuse") { light.ambientIntensity=0; material.specularColor={}; }
    else if(name=="red-light") { light.ambientIntensity=0; light.color={1,0,0}; }
    else if(name=="matte") material.specularColor={};
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
    // Array of per-model records in one dynamic UBO. This variant has one torus.
    for(uint32_t model=0;model<modelCount;++model) {
        const auto data=uniforms(camera,light,material,width/height);
        auto* destination=static_cast<std::byte*>(uniformBuffer.mapped)+model*modelStride;
        std::memcpy(destination,&data,sizeof(data));
    }
    check(vmaFlushAllocation(ctx.allocator,uniformBuffer.allocation,0,modelStride*modelCount),"Flush uniform");
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
    for(uint32_t model=0;model<modelCount;++model) {
        const uint32_t dynamicOffset=static_cast<uint32_t>(model*modelStride);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&descriptor,1,&dynamicOffset);
        vkCmdDrawIndexed(cmd,indexCount,1,0,0,0);
    }
    vkCmdEndRenderPass(cmd); check(vkEndCommandBuffer(cmd),"vkEndCommandBuffer");
}
} // namespace application
