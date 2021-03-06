//
// Created by Eric Berdahl on 10/22/17.
//

#ifndef CLSPVUTILS_DEVICE_HPP
#define CLSPVUTILS_DEVICE_HPP

#include "clspv_utils_fwd.hpp"

#include "clspv_utils_interop.hpp"
#include "interface.hpp"

#include <vulkan/vulkan.hpp>

#include <memory>

namespace clspv_utils {

    class device {
    public:
        struct descriptor_group
        {
            vk::DescriptorSet       mDescriptor;
            vk::DescriptorSetLayout mLayout;
        };

        typedef vk::ArrayProxy<const sampler_spec_t> sampler_list_proxy;

        device() {}

        device(vk::PhysicalDevice   physicalDevice,
               vk::Device           device,
               vk::DescriptorPool   descriptorPool,
               vk::CommandPool      commandPool,
               vk::Queue            computeQueue);

        vk::PhysicalDevice  getPhysicalDevice() const { return mPhysicalDevice; }
        vk::Device          getDevice() const { return mDevice; }
        vk::DescriptorPool  getDescriptorPool() const { return mDescriptorPool; }
        vk::CommandPool     getCommandPool() const { return mCommandPool; }
        vk::Queue           getComputeQueue() const { return mComputeQueue; }

        const vk::PhysicalDeviceMemoryProperties&   getMemoryProperties() const { return mMemoryProperties; }

        vk::Sampler                     getCachedSampler(int opencl_flags);

        vk::UniqueDescriptorSetLayout   createSamplerDescriptorLayout(const sampler_list_proxy& samplers) const;

        vk::UniqueDescriptorSet         createSamplerDescriptor(const sampler_list_proxy& samplers,
                                                                vk::DescriptorSetLayout layout);

        descriptor_group                getCachedSamplerDescriptorGroup(const sampler_list_proxy& samplers);

    private:
        struct unique_descriptor_group
        {
            vk::UniqueDescriptorSet       mDescriptor;
            vk::UniqueDescriptorSetLayout mLayout;
        };

        typedef map<std::size_t, unique_descriptor_group> descriptor_cache;
        typedef map<int, vk::UniqueSampler> sampler_cache;

    private:
        vk::PhysicalDevice                  mPhysicalDevice;
        vk::Device                          mDevice;
        vk::PhysicalDeviceMemoryProperties  mMemoryProperties;
        vk::DescriptorPool                  mDescriptorPool;
        vk::CommandPool                     mCommandPool;
        vk::Queue                           mComputeQueue;

        shared_ptr<descriptor_cache>        mSamplerDescriptorCache;
        shared_ptr<sampler_cache>           mSamplerCache;
    };

    vk::UniqueDescriptorSet allocateDescriptorSet(const device&           inDevice,
                                                  vk::DescriptorSetLayout layout);

}

#endif //CLSPVUTILS_DEVICE_HPP
