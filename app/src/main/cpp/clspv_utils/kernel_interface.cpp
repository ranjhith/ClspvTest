//
// Created by Eric Berdahl on 10/22/17.
//

#include "interface.hpp"

#include "clspv_utils_interop.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>

namespace {
    using namespace clspv_utils;

    const auto kArgKind_DescriptorType_Map = {
            std::make_pair(arg_spec_t::kind_pod_ubo,  vk::DescriptorType::eUniformBuffer),
            std::make_pair(arg_spec_t::kind_pod,      vk::DescriptorType::eStorageBuffer),
            std::make_pair(arg_spec_t::kind_buffer,   vk::DescriptorType::eStorageBuffer),
            std::make_pair(arg_spec_t::kind_ro_image, vk::DescriptorType::eSampledImage),
            std::make_pair(arg_spec_t::kind_wo_image, vk::DescriptorType::eStorageImage),
            std::make_pair(arg_spec_t::kind_sampler,  vk::DescriptorType::eSampler)
    };

} // anonymous namespace

namespace clspv_utils {

    vk::DescriptorType getDescriptorType(arg_spec_t::kind argKind) {
        auto found = std::find_if(std::begin(kArgKind_DescriptorType_Map),
                                  std::end(kArgKind_DescriptorType_Map),
                                  [argKind](decltype(kArgKind_DescriptorType_Map)::const_reference p) {
                                      return argKind == p.first;
                                  });
        if (found == std::end(kArgKind_DescriptorType_Map)) {
            fail_runtime_error("unknown argKind encountered");
        }
        return found->second;
    }

    void validateKernelArg(const arg_spec_t &arg) {
        if (arg.mKind == arg_spec_t::kind_unknown) {
            fail_runtime_error("kernel argument kind unknown");
        }
        if (arg.mOrdinal < 0) {
            fail_runtime_error("kernel argument missing ordinal");
        }

        if (arg.mKind == arg_spec_t::kind_local) {
            if (arg.mSpecConstant < 0) {
                fail_runtime_error("local kernel argument missing spec constant");
            }
        }
        else {
            if (arg.mDescriptorSet < 0) {
                fail_runtime_error("kernel argument missing descriptorSet");
            }
            if (arg.mBinding < 0) {
                fail_runtime_error("kernel argument missing binding");
            }
            if (arg.mOffset < 0) {
                fail_runtime_error("kernel argument missing offset");
            }
        }
    }

    void standardizeKernelArgumentOrder(kernel_spec_t::arg_list& arguments)
    {
        std::sort(arguments.begin(), arguments.end(), [](const arg_spec_t& lhs, const arg_spec_t& rhs) {
            auto isPod = [](arg_spec_t::kind kind) {
                return (kind == arg_spec_t::kind_pod || kind == arg_spec_t::kind_pod_ubo);
            };

            const auto lhs_is_pod = isPod(lhs.mKind);
            const auto rhs_is_pod = isPod(rhs.mKind);

            return (lhs_is_pod == rhs_is_pod ? lhs.mOrdinal < rhs.mOrdinal : !lhs_is_pod);
        });
    }

    void validateKernel(const kernel_spec_t& spec, int requiredDescriptorSet)
    {
        if (spec.mName.empty()) {
            fail_runtime_error("kernel has no name");
        }

        const int arg_ds = getKernelArgumentDescriptorSet(spec.mArguments);
        if (requiredDescriptorSet > 0 && arg_ds != requiredDescriptorSet) {
            fail_runtime_error("kernel's arguments are in incorrect descriptor set");
        }

        for (auto& ka : spec.mArguments) {
            // All arguments for a given kernel that are passed in a descriptor set need to be in
            // the same descriptor set
            if (ka.mKind != arg_spec_t::kind_local && ka.mDescriptorSet != arg_ds) {
                fail_runtime_error("kernel arg descriptor_sets don't match");
            }

            validateKernelArg(ka);
        }

        // TODO: mArguments entries are in increasing binding, and pod/pod_ubo's come after non-pod/non-pod_ubo's
        // TODO: there cannot be both pod and pod_ubo arguments for a given kernel
        // TODO: if there is a pod or pod_ubo argument, its binding must be larger than other descriptor sets
    }

    int getKernelArgumentDescriptorSet(const kernel_spec_t::arg_list& arguments) {
        auto found = std::find_if(arguments.begin(), arguments.end(), [](const arg_spec_t& as) {
            return (-1 != as.mDescriptorSet);
        });
        return (found == arguments.end() ? -1 : found->mDescriptorSet);
    }

    vk::UniqueDescriptorSetLayout createKernelArgumentDescriptorLayout(const kernel_spec_t::arg_list& arguments, vk::Device inDevice)
    {
        vector<vk::DescriptorSetLayoutBinding> bindingSet;

        vk::DescriptorSetLayoutBinding binding;
        binding.setStageFlags(vk::ShaderStageFlagBits::eCompute)
                .setDescriptorCount(1);

        for (auto &ka : arguments) {
            // ignore any argument not in offset 0
            if (0 != ka.mOffset) continue;

            binding.descriptorType = getDescriptorType(ka.mKind);
            binding.binding = ka.mBinding;

            bindingSet.push_back(binding);
        }

        vk::DescriptorSetLayoutCreateInfo createInfo;
        createInfo.setBindingCount(bindingSet.size())
                .setPBindings(bindingSet.size() ? bindingSet.data() : nullptr);

        return inDevice.createDescriptorSetLayoutUnique(createInfo);
    }

} // namespace clspv_utils