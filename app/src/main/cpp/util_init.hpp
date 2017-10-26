/*
 * Vulkan Samples
 *
 * Copyright (C) 2015-2016 Valve Corporation
 * Copyright (C) 2015-2016 LunarG, Inc.
 * Copyright (C) 2015-2016 Google, Inc.
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
 */

#ifndef CLSPVTEST_UTIL_INIT_HPP
#define CLSPVTEST_UTIL_INIT_HPP

#include "util.hpp"

// Make sure functions start with init, execute, or destroy to assist codegen

void init_global_layer_properties(sample_info &info);

VkResult init_instance(struct sample_info &info,
                       char const *const app_short_name);
VkResult init_device(struct sample_info &info);
VkResult init_enumerate_device(struct sample_info &info,
                               uint32_t gpu_count = 1);
VkBool32 demo_check_layers(const std::vector<layer_properties> &layer_props,
                           const std::vector<const char *> &layer_names);
void init_command_pool(struct sample_info &info);
void init_device_queue(struct sample_info &info);

VkResult init_debug_report_callback(struct sample_info &info,
                                    PFN_vkDebugReportCallbackEXT dbgFunc);
void destroy_debug_report_callback(struct sample_info &info);
void destroy_descriptor_pool(struct sample_info &info);
void destroy_command_pool(struct sample_info &info);
void destroy_device(struct sample_info &info);
void destroy_instance(struct sample_info &info);

#endif // CLSPVTEST_UTIL_INIT
