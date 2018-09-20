//
// Created by Eric Berdahl on 10/22/17.
//

#include "module_interface.hpp"

#include "arg_spec.hpp"
#include "device.hpp"
#include "file_utils.hpp"
#include "getline_crlf_savvy.hpp"
#include "kernel_interface.hpp"
#include "kernel_module.hpp"
#include "opencl_types.hpp"
#include "sampler_spec.hpp"
#include "util.hpp" // AndroidFopen

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <map>


namespace {
    using namespace clspv_utils;

    typedef std::pair<std::string,std::string> key_value_t;

    const auto kSpvMapArgType_ArgKind_Map = {
            std::make_pair("pod", arg_spec_t::kind_pod),
            std::make_pair("pod_ubo", arg_spec_t::kind_pod_ubo),
            std::make_pair("buffer", arg_spec_t::kind_buffer),
            std::make_pair("ro_image", arg_spec_t::kind_ro_image),
            std::make_pair("wo_image", arg_spec_t::kind_wo_image),
            std::make_pair("sampler", arg_spec_t::kind_sampler),
            std::make_pair("local", arg_spec_t::kind_local)
    };

    arg_spec_t::kind_t find_arg_kind(const std::string &argType) {
        auto found = std::find_if(std::begin(kSpvMapArgType_ArgKind_Map),
                                  std::end(kSpvMapArgType_ArgKind_Map),
                                  [&argType](decltype(kSpvMapArgType_ArgKind_Map)::const_reference p) {
                                      return argType == p.first;
                                  });
        if (found == std::end(kSpvMapArgType_ArgKind_Map)) {
            throw std::runtime_error("unknown argType encountered");
        }
        return found->second;
    }

    std::string read_csv_field(std::istream& in) {
        std::string result;

        if (in.good()) {
            const bool is_quoted = (in.peek() == '"');

            if (is_quoted) {
                in.ignore(std::numeric_limits<std::streamsize>::max(), '"');
            }

            std::getline(in, result, is_quoted ? '"' : ',');

            if (is_quoted) {
                in.ignore(std::numeric_limits<std::streamsize>::max(), ',');
            }
        }

        return result;
    }

    key_value_t read_key_value_pair(std::istream& in) {
        return std::make_pair(read_csv_field(in), read_csv_field(in));
    };

    sampler_spec_t parse_spvmap_sampler(key_value_t tag, std::istream& in) {
        sampler_spec_t result;

        result.opencl_flags = std::atoi(tag.second.c_str());

        while (!in.eof()) {
            tag = read_key_value_pair(in);

            if ("descriptorSet" == tag.first) {
                result.descriptor_set = std::atoi(tag.second.c_str());
            } else if ("binding" == tag.first) {
                result.binding = std::atoi(tag.second.c_str());
            }
        }

        return result;
    }

    arg_spec_t parse_spvmap_kernel_arg(key_value_t tag, std::istream& in) {
        arg_spec_t result;

        while (!in.eof()) {
            tag = read_key_value_pair(in);

            if ("argOrdinal" == tag.first) {
                result.ordinal = std::atoi(tag.second.c_str());
            } else if ("descriptorSet" == tag.first) {
                result.descriptor_set = std::atoi(tag.second.c_str());
            } else if ("binding" == tag.first) {
                result.binding = std::atoi(tag.second.c_str());
            } else if ("offset" == tag.first) {
                result.offset = std::atoi(tag.second.c_str());
            } else if ("argKind" == tag.first) {
                result.kind = find_arg_kind(tag.second);
            } else if ("arrayElemSize" == tag.first) {
                // arrayElemSize is ignored by clspvtest
            } else if ("arrayNumElemSpecId" == tag.first) {
                result.spec_constant = std::atoi(tag.second.c_str());
            }

        }

        return result;
    }

} // anonymous namespace

namespace clspv_utils {

    module_interface::module_interface()
    {
    }

    module_interface::module_interface(const std::string& moduleName)
            : module_interface()
    {
        mName = moduleName;

        std::string buffer;
        file_utils::read_file_contents(moduleName + ".spvmap", buffer);

        std::map<std::string, kernel_interface::arg_list_t> kernel_args;

        std::string line;
        std::istringstream in(buffer);
        while (!in.eof()) {
            // spvmap files may have been generated on a system which uses different line ending
            // conventions than the system on which the consumer runs. Safer to fetch lines
            // using a function which recognizes multiple line endings.
            crlf_savvy::getline(in, line);

            std::istringstream in_line(line);
            auto tag = read_key_value_pair(in_line);
            if ("sampler" == tag.first) {
                addLiteralSampler(parse_spvmap_sampler(tag, in_line));
            } else if ("kernel" == tag.first) {
                kernel_args[tag.second].push_back(parse_spvmap_kernel_arg(tag, in_line));
            }
        }

        // Ensure that the literal samplers are sorted by increasing binding number. This will be
        // important if the sequence is later used to determine whether a cached sampler descriptor
        // set can be re-used for this module.
        std::sort(mSamplers.begin(), mSamplers.end(), [](const sampler_spec_t& lhs, const sampler_spec_t& rhs) {
            return lhs.binding < rhs.binding;
        });

        for (auto& k : kernel_args) {
            mKernels.push_back(kernel_interface(k.first, mSamplers, k.second));
        }
    }

    void module_interface::addLiteralSampler(clspv_utils::sampler_spec_t sampler) {
        sampler.validate();
        mSamplers.push_back(sampler);
    }

    const kernel_interface* module_interface::findKernelInterface(const std::string& name) const {
        auto kernel = std::find_if(mKernels.begin(), mKernels.end(),
                                   [&name](const kernel_interface &iter) {
                                       return iter.getEntryPoint() == name;
                                   });

        return (kernel == mKernels.end() ? nullptr : &(*kernel));
    }

    int module_interface::getLiteralSamplersDescriptorSet() const {
        auto found = std::find_if(mSamplers.begin(), mSamplers.end(), [](const sampler_spec_t& ss) {
            return (-1 != ss.descriptor_set);
        });
        return (found == mSamplers.end() ? -1 : found->descriptor_set);
    }

    std::vector<std::string> module_interface::getEntryPoints() const
    {
        std::vector<std::string> result;

        std::transform(mKernels.begin(), mKernels.end(),
                       std::back_inserter(result),
                       [](const kernel_interface& k) { return k.getEntryPoint(); });

        return result;
    }

    kernel_module module_interface::load(device inDevice) const
    {
        const int ds = getLiteralSamplersDescriptorSet();
        assert( (mSamplers.empty() && -1 == ds) || (!mSamplers.empty() && 0 == ds) );

        //
        // Create literal sampler descriptor set
        //

        const auto literalSamplerDescriptorGroup = inDevice.getCachedSamplerDescriptorGroup(mSamplers);

        return kernel_module(mName,
                             inDevice,
                             literalSamplerDescriptorGroup.descriptor,
                             literalSamplerDescriptorGroup.layout,
                             mKernels);
    }

} // namespace clspv_utils
