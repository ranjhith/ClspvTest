//
// Created by Eric Berdahl on 10/22/17.
//

#include "kernel.hpp"

#include "vulkan_utils/vulkan_utils.hpp"

namespace {

    vk::UniquePipelineLayout create_pipeline_layout(vk::Device                                      device,
                                                    vk::ArrayProxy<const vk::DescriptorSetLayout>   layouts)
    {
        vk::PipelineLayoutCreateInfo createInfo;
        createInfo.setSetLayoutCount(layouts.size())
                .setPSetLayouts(layouts.data());

        return device.createPipelineLayoutUnique(createInfo);
    }

}

namespace clspv_utils {

    kernel::kernel()
    {
    }

    kernel::kernel(kernel_req_t         layout,
                   const vk::Extent3D&  workgroup_sizes) :
            mReq(std::move(layout)),
            mSpecConstants({ workgroup_sizes.width, workgroup_sizes.height, workgroup_sizes.depth })
    {
        if (-1 != getKernelArgumentDescriptorSet(mReq.mKernelSpec.mArguments)) {
            mArgumentsLayout = createKernelArgumentDescriptorLayout(mReq.mKernelSpec.mArguments, mReq.mDevice.getDevice());

            mArgumentsDescriptor = allocateDescriptorSet(mReq.mDevice, *mArgumentsLayout);
        }

        vector<vk::DescriptorSetLayout> layouts;
        if (mReq.mLiteralSamplerLayout) layouts.push_back(mReq.mLiteralSamplerLayout);
        if (mArgumentsLayout) layouts.push_back(*mArgumentsLayout);
        mPipelineLayout = create_pipeline_layout(mReq.mDevice.getDevice(), layouts);
    }

    kernel::~kernel() {
    }

    kernel::kernel(kernel &&other)
            : kernel()
    {
        swap(other);
    }

    kernel& kernel::operator=(kernel&& other)
    {
        swap(other);
        return *this;
    }

    void kernel::swap(kernel& other)
    {
        using std::swap;

        swap(mReq, other.mReq);
        swap(mArgumentsLayout, other.mArgumentsLayout);
        swap(mArgumentsDescriptor, other.mArgumentsDescriptor);
        swap(mPipelineLayout, other.mPipelineLayout);
        swap(mPipeline, other.mPipeline);
        swap(mSpecConstants, other.mSpecConstants);
    }

    invocation_req_t kernel::createInvocationReq() {
        invocation_req_t result;

        result.mDevice = mReq.mDevice;
        result.mKernelSpec = mReq.mKernelSpec;
        result.mPipelineLayout = *mPipelineLayout;
        result.mGetPipelineFn = std::bind(&kernel::updatePipeline, this, std::placeholders::_1);
        result.mLiteralSamplerDescriptor = mReq.mLiteralSamplerDescriptor;
        result.mArgumentsDescriptor = *mArgumentsDescriptor;

        return result;
    }

    vk::Pipeline kernel::updatePipeline(vk::ArrayProxy<uint32_t> otherSpecConstants) {
        // TODO Consider case where there are multiple invocations active at one.
        // In that case, we should not delete the previous pipeline!
        // Thus, the pipeline really needs to be in a shared state.

        // If the spec constants being passed are equal to the spec constants we already have,
        // the existing pipeline is up to date.
        if (mPipeline
            && mSpecConstants.size() == otherSpecConstants.size() + 3
            && std::equal(std::next(mSpecConstants.begin(), 3), mSpecConstants.end(),
                                    otherSpecConstants.begin()) ) {
            return *mPipeline;
        }

        mSpecConstants.resize(3 + otherSpecConstants.size());
        std::copy(otherSpecConstants.begin(), otherSpecConstants.end(), std::next(mSpecConstants.begin(), 3));

        mPipeline = vulkan_utils::create_compute_pipeline(mReq.mDevice.getDevice(),
                                                          mReq.mShaderModule,
                                                          mReq.mKernelSpec.mName.c_str(),
                                                          *mPipelineLayout,
                                                          mReq.mPipelineCache,
                                                          mSpecConstants);

        return *mPipeline;
    }

} // namespace clspv_utils
