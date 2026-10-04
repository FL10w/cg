#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include "graphics_internal.hpp"
#include "application.hpp"

int main(int argc,char** argv) {
    int frames=0;
    bool resizeTest=false;
    std::string view="default",capture;
    try {
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--help") {
                std::cout<<"Usage: cg-lab-01 [--frames N] [--view default|side] [--capture file.ppm] [--resize-test]\n";
                return EXIT_SUCCESS;
            } else if(arg=="--resize-test") resizeTest=true;
            else if((arg=="--frames" || arg=="--view" || arg=="--capture") && i+1<argc) {
                const std::string value=argv[++i];
                if(arg=="--frames") { frames=std::stoi(value); if(frames<1) throw std::runtime_error("frames must be positive"); }
                else if(arg=="--view") view=value;
                else capture=value;
            } else throw std::runtime_error("Unknown or incomplete option: "+arg);
        }
        if(view!="default" && view!="side") throw std::runtime_error("Unknown camera view");
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return EXIT_FAILURE; }
    if(!capture.empty() && frames==0) frames=30;
    if(resizeTest && frames==0) frames=60;
    glfwSetErrorCallback([](int code,const char* message){ std::cerr<<"GLFW "<<code<<": "<<message<<'\n'; });
    if(!glfwInit()) return EXIT_FAILURE;
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);
    GLFWwindow* window=glfwCreateWindow(1280,900,"CG Lab 1 - Variant 7 - Torus / Vulkan",nullptr,nullptr);
    if(!window) { glfwTerminate(); return EXIT_FAILURE; }
    glfwSetFramebufferSizeCallback(window,[](GLFWwindow*,int w,int h){
        if(w>0 && h>0) graphics::internal::resize(static_cast<uint32_t>(w),static_cast<uint32_t>(h));
    });
    glfwSetKeyCallback(window,[](GLFWwindow* w,int key,int,int action,int){
        if(key==GLFW_KEY_ESCAPE && action==GLFW_PRESS) glfwSetWindowShouldClose(w,GLFW_TRUE);
    });
    ImGui::CreateContext();
    bool glfwBackend=ImGui_ImplGlfw_InitForVulkan(window,true),graphicsReady=false,applicationReady=false;
    int status=EXIT_FAILURE;
    try {
        if(!glfwBackend) throw std::runtime_error("Cannot initialize ImGui GLFW backend");
        graphicsReady=graphics::internal::initialize(window);
        if(!graphicsReady) throw std::runtime_error("Cannot initialize Vulkan");
        applicationReady=application::initialize();
        if(!applicationReady) throw std::runtime_error("Cannot initialize application");
        application::configureView(view);
        int rendered=0;
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            int w=0,h=0; glfwGetFramebufferSize(window,&w,&h);
            if(w==0 || h==0) { glfwWaitEvents(); continue; }
            if(resizeTest && rendered==10) glfwSetWindowSize(window,1000,700);
            if(resizeTest && rendered==25) glfwSetWindowSize(window,1280,900);
            ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
            application::update();
            ImGui::Render();
            auto fd=graphics::internal::prepare();
            application::render(fd);
            if(!capture.empty() && rendered+1==frames) graphics::internal::requestCapture(capture);
            graphics::internal::submitAndPresent();
            ++rendered;
            if(frames>0 && rendered>=frames) glfwSetWindowShouldClose(window,GLFW_TRUE);
        }
        std::cout<<"Completed successfully\n"; status=EXIT_SUCCESS;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; }
    if(applicationReady) application::shutdown();
    if(graphicsReady) graphics::internal::shutdown();
    if(graphics::internal::context.validation_errors) status=EXIT_FAILURE;
    if(glfwBackend) ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate();
    return status;
}
