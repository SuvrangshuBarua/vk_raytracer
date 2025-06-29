/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


// ImGui - standalone example application for Glfw + Vulkan, using programmable
// pipeline If you are new to ImGui, see examples/README.txt and documentation
// at the top of imgui.cpp.

#include <array>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui/imgui_helper.h"

#include "hello_vulkan.h"
#include "imgui/imgui_camera_widget.h"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvpsystem.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/context_vk.hpp"
#include <random>



//////////////////////////////////////////////////////////////////////////
#define UNUSED(x) (void)(x)
//////////////////////////////////////////////////////////////////////////

// Default search path for shaders
std::vector<std::string> defaultSearchPaths;


// GLFW Callback functions
static void onErrorCallback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Extra UI
void renderUI(HelloVulkan& helloVk)
{
  bool changed = false;

  changed |= ImGuiH::CameraWidget();
  if(ImGui::CollapsingHeader("Light"))
  {
    auto& pc = helloVk.m_pcRaster;
    changed |= ImGui::RadioButton("Point", &pc.lightType, 0);
    ImGui::SameLine();
    changed |= ImGui::RadioButton("Infinite", &pc.lightType, 1);

    changed |= ImGui::SliderFloat3("Position", &pc.lightPosition.x, -20.f, 20.f);
    changed |= ImGui::SliderFloat("Intensity", &pc.lightIntensity, 0.f, 150.f);
  }
  changed |= ImGui::SliderInt("Max Frames", &helloVk.m_maxFrames, 1, 100);
  if(changed)
    helloVk.resetFrame();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static int const SAMPLE_WIDTH  = 1280;
static int const SAMPLE_HEIGHT = 720;


//--------------------------------------------------------------------------------------------------
// Application Entry
//
int main(int argc, char** argv)
{
  UNUSED(argc);

  // Setup GLFW window
  glfwSetErrorCallback(onErrorCallback);
  if(!glfwInit())
  {
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(SAMPLE_WIDTH, SAMPLE_HEIGHT, PROJECT_NAME, nullptr, nullptr);


  // Setup camera
  CameraManip.setWindowSize(SAMPLE_WIDTH, SAMPLE_HEIGHT);
  CameraManip.setLookat(glm::vec3(4, 4, 4), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));

  // Setup Vulkan
  if(!glfwVulkanSupported())
  {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }

  // setup some basic things for the sample, logging file for example
  NVPSystem system(PROJECT_NAME);

  // Search path for shaders and other media
  defaultSearchPaths = {
      NVPSystem::exePath() + PROJECT_RELDIRECTORY,
      NVPSystem::exePath() + PROJECT_RELDIRECTORY "..",
      std::string(PROJECT_NAME),
  };

  // Vulkan required extensions
  assert(glfwVulkanSupported() == 1);
  uint32_t count{0};
  auto     reqExtensions = glfwGetRequiredInstanceExtensions(&count);

  // Requesting Vulkan extensions and layers
  nvvk::ContextCreateInfo contextInfo;
  contextInfo.setVersion(1, 2);                       // Using Vulkan 1.2
  for(uint32_t ext_id = 0; ext_id < count; ext_id++)  // Adding required extensions (surface, win32, linux, ..)
    contextInfo.addInstanceExtension(reqExtensions[ext_id]);
  contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);              // FPS in titlebar
  contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);  // Allow debug names
  contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);            // Enabling ability to present rendering

    // #VKRay: Activate the ray tracing extension
  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);  // To build acceleration structures
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);  // To use vkCmdTraceRaysKHR
  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);  // Required by ray tracing pipeline

  // Creating Vulkan base application
  nvvk::Context vkctx{};
  vkctx.initInstance(contextInfo);
  // Find all compatible devices
  auto compatibleDevices = vkctx.getCompatibleDevices(contextInfo);
  assert(!compatibleDevices.empty());
  // Use a compatible device
  vkctx.initDevice(compatibleDevices[0], contextInfo);

  // Create example
  HelloVulkan helloVk;

  // Window need to be opened to get the surface on which to draw
  const VkSurfaceKHR surface = helloVk.getVkSurface(vkctx.m_instance, window);
  vkctx.setGCTQueueWithPresent(surface);

  helloVk.setup(vkctx.m_instance, vkctx.m_device, vkctx.m_physicalDevice, vkctx.m_queueGCT.familyIndex);
  helloVk.createSwapchain(surface, SAMPLE_WIDTH, SAMPLE_HEIGHT);
  helloVk.createDepthBuffer();
  helloVk.createRenderPass();
  helloVk.createFrameBuffers();

  // Setup Imgui
  helloVk.initGUI(0);  // Using sub-pass 0

  // Model set for Any Hit test
  //helloVk.loadModel(nvh::findFile("media/scenes/cube_multi.obj", defaultSearchPaths, true));
  // Alternatively, load some models
  helloVk.loadModel(nvh::findFile("media/scenes/wuson.obj", defaultSearchPaths, true),
                    glm::translate(glm::mat4(1.f),glm::vec3(0.0, .5f, 1.5f)));
  helloVk.loadModel(nvh::findFile("media/scenes/sphere.obj", defaultSearchPaths, true),
                    glm::scale(glm::mat4(1.f), glm::vec3(1.5f)) * glm::translate(glm::mat4(1), glm::vec3(0.0f, 1.0f, 1.f)));
  helloVk.loadModel(nvh::findFile("media/scenes/plane.obj", defaultSearchPaths, true));

  // Model set for Reflection test
  /*helloVk.loadModel(nvh::findFile("media/scenes/cube.obj", defaultSearchPaths, true),
                    glm::translate(glm::mat4(1),glm::vec3(3, 1, 0))
                        * glm::scale(glm::mat4(1.f),glm::vec3(.1f, 5.f, 5.f)));
  helloVk.loadModel(nvh::findFile("media/scenes/cube.obj", defaultSearchPaths, true),
                    glm::translate(glm::mat4(1),glm::vec3(7, 1, 0))
                        * glm::scale(glm::mat4(1.f),glm::vec3(.1f, 5.f, 5.f)));*/
  helloVk.loadModel(nvh::findFile("media/scenes/Medieval_building.obj", defaultSearchPaths, true),
                              glm::translate(glm::mat4(1.f),glm::vec3(5, 0.f, 0))
                             * glm::scale(glm::mat4(1.f),glm::vec3(.5f, .5f, .5f)));
  helloVk.loadModel(nvh::findFile("media/scenes/cube_multi.obj", defaultSearchPaths, true),
                    glm::scale(glm::mat4(1.f), glm::vec3(0.5f)) * glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.0f, 6.0f)));

  std::random_device              rd;         // Will be used to obtain a seed for the random number engine
  std::mt19937                    gen(rd());  // Standard mersenne_twister_engine seeded with rd()
  std::normal_distribution<float> dis(2.0f, 2.0f);
  std::normal_distribution<float> disn(0.5f, 0.2f);
  auto                            wusonIndex = static_cast<int>(helloVk.m_objModel.size() - 1);

  for(int n = 0; n < 2000; ++n)
  {
    HelloVulkan::ObjInstance inst;
    inst.objIndex   = wusonIndex;
    float     scale = fabsf(disn(gen));
    glm::mat4 mat   = glm::translate(glm::mat4(1), glm::vec3{dis(gen), 0.5f, dis(gen) + 6});
    //    mat              = mat * glm::rotation_mat4_x(dis(gen));
    mat            = glm::scale(mat, glm::vec3(scale));
    inst.transform = mat;

    helloVk.m_instances.push_back(inst);
  }

  helloVk.createOffscreenRender();
  helloVk.createDescriptorSetLayout();
  helloVk.createGraphicsPipeline();
  helloVk.createUniformBuffer();
  helloVk.createObjDescriptionBuffer();
  helloVk.updateDescriptorSet();

  // #VKRay
  helloVk.initRayTracing();
  helloVk.createBottomLevelAS();
  helloVk.createTopLevelAS();
  helloVk.createRtDescriptorSet();
  helloVk.createRtPipeline();
  helloVk.createRtShaderBindingTable();

  helloVk.createPostDescriptor();
  helloVk.createPostPipeline();
  helloVk.updatePostDescriptorSet();

  glm::vec4 clearColor                          = glm::vec4(1, 1, 1, 1.00f);
  bool      useRaytracer                        = true;
  auto      start         = std::chrono::system_clock::now();

  helloVk.setupGlfwCallbacks(window);
  ImGui_ImplGlfw_InitForVulkan(window, true);

  // Main loop
  while(!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if(helloVk.isMinimized())
      continue;

    // Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Show UI window.
    if(helloVk.showGui())
    {
      ImGuiH::Panel::Begin();
      bool changed = false;
      // Edit 3 floats representing a color
      changed |= ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&clearColor));
      // Switch between raster and ray tracing
      changed |= ImGui::Checkbox("Ray Tracer mode", &useRaytracer);
      if(changed)
        helloVk.resetFrame();
      renderUI(helloVk);
      ImGui::SliderInt("Max Depth", &helloVk.m_pcRay.maxDepth, 1, 50);
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGuiH::Control::Info("", "", "(F10) Toggle Pane", ImGuiH::Control::Flags::Disabled);
      ImGuiH::Panel::End();
    }

    // #VK_animation
    std::chrono::duration<float> diff = std::chrono::system_clock::now() - start;
    //helloVk.animationInstances(diff.count() );
    //helloVk.animationObject(diff.count());

    // Start rendering the scene
    helloVk.prepareFrame();

    // Start command buffer of this frame
    auto                   curFrame = helloVk.getCurFrame();
    const VkCommandBuffer& cmdBuf   = helloVk.getCommandBuffers()[curFrame];

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // Updating camera buffer
    helloVk.updateUniformBuffer(cmdBuf);

    // Clearing screen
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
    clearValues[1].depthStencil = {1.0f, 0};

    // Offscreen render pass
    {
      VkRenderPassBeginInfo offscreenRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      offscreenRenderPassBeginInfo.clearValueCount = 2;
      offscreenRenderPassBeginInfo.pClearValues    = clearValues.data();
      offscreenRenderPassBeginInfo.renderPass      = helloVk.m_offscreenRenderPass;
      offscreenRenderPassBeginInfo.framebuffer     = helloVk.m_offscreenFramebuffer;
      offscreenRenderPassBeginInfo.renderArea      = {{0, 0}, helloVk.getSize()};

      // Rendering Scene
      if(useRaytracer)
      {
        helloVk.raytrace(cmdBuf, clearColor);
      }
      else
      {
        vkCmdBeginRenderPass(cmdBuf, &offscreenRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        helloVk.rasterize(cmdBuf);
        vkCmdEndRenderPass(cmdBuf);
      }
    }


    // 2nd rendering pass: tone mapper, UI
    {
      VkRenderPassBeginInfo postRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      postRenderPassBeginInfo.clearValueCount = 2;
      postRenderPassBeginInfo.pClearValues    = clearValues.data();
      postRenderPassBeginInfo.renderPass      = helloVk.getRenderPass();
      postRenderPassBeginInfo.framebuffer     = helloVk.getFramebuffers()[curFrame];
      postRenderPassBeginInfo.renderArea      = {{0, 0}, helloVk.getSize()};

      // Rendering tonemapper
      vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
      helloVk.drawPost(cmdBuf);
      // Rendering UI
      ImGui::Render();
      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
      vkCmdEndRenderPass(cmdBuf);
    }

    // Submit for display
    vkEndCommandBuffer(cmdBuf);
    helloVk.submitFrame();
  }

  // Cleanup
  vkDeviceWaitIdle(helloVk.getDevice());

  helloVk.destroyResources();
  helloVk.destroy();
  vkctx.deinit();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
