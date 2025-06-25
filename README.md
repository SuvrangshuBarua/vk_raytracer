
# vk_raytracer is an extension of this tutorial series -> [Ray Tracing Tutorial](https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/vkrt_tutorial.md.html)


# Environment Setup


## Repositories

Besides the current repository, you will also need to clone or download the following repositories:

* [nvpro_core](https://github.com/nvpro-samples/nvpro_core): The primary framework that all samples depend on.

Cloning all repositories 

~~~~~
git clone --recursive --shallow-submodules https://github.com/nvpro-samples/nvpro_core.git
git clone https://github.com/SuvrangshuBarua/vk_raytracer.git
~~~~~

The directory structure should be looking like this:

~~~~
   C:\Vulkan\nvpro-samples
   | 
   +---nvpro_core
   +---vk_raytracer
   |   +---ray_tracing_final
   |   \---...   
~~~~

See also [build_all](https://github.com/nvpro-samples/build_all) from nvpro-samples.

## Latest Vulkan SDK

This repository tries to always be up to date with the latest Vulkan SDK, therefore we suggest to download and install it.
Version 1.2.162.0 and up has ray tracing extensions support.

**Vulkan SDK**: https://vulkan.lunarg.com/sdk/home


## Driver

NVIDIA driver 450.0 and up support Vulkan ray tracing. 

* Standard driver: https://www.nvidia.com/Download/index.aspx
* Vulkan beta driver: https://developer.nvidia.com/vulkan-driver


## CMake

The CMakefile will use other makefiles from `nvpro_core` and look for Vulkan environment variables for the installation of the SDK. Therefore, it is important to have all the above installed before running Cmake in the 
`vk_raytracer` directory.

**Note:** Ray tracing only works with 64 bit environment. Therefore, make sure to choose the right build environment.

**Note:** If you are using your own Vulkan header files, it is possible to overide the default search path.
  Modify `VULKAN > VULKAN_HEADERS_OVERRIDE_INCLUDE_DIR` to the path to beta vulkan headers.