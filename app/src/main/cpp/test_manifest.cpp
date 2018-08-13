//
// Created by Eric Berdahl on 4/26/18.
//

#include "test_manifest.hpp"

#include "copybuffertoimage_kernel.hpp"
#include "copyimagetobuffer_kernel.hpp"
#include "fill_kernel.hpp"
#include "readconstantdata_kernel.hpp"
#include "readlocalsize_kernel.hpp"
#include "resample2dimage_kernel.hpp"
#include "resample3dimage_kernel.hpp"
#include "strangeshuffle_kernel.hpp"
#include "testgreaterthanorequalto_kernel.hpp"

#include "util.hpp" // for LOGxx macros

namespace {
    typedef test_utils::KernelTest::invocation_tests (series_gen_signature)();

    typedef std::function<series_gen_signature>  series_gen_fn;

    series_gen_fn createGenerator(std::function<test_utils::InvocationTest ()> delegate)
    {
        return series_gen_fn([delegate]() {
            return test_utils::KernelTest::invocation_tests({ delegate() });
        });
    }

    series_gen_fn createGenerator(series_gen_fn genFn)
    {
        return genFn;
    }

    test_utils::KernelTest::invocation_tests lookup_test_series(const std::string& testName) {
        static const auto test_map = {
                std::make_pair("copyBufferToImage",  createGenerator(copybuffertoimage_kernel::getAllTestVariants)),
                std::make_pair("copyImageToBuffer",  createGenerator(copyimagetobuffer_kernel::getAllTestVariants)),
                std::make_pair("fill",               createGenerator(fill_kernel::getAllTestVariants)),
                std::make_pair("fill<float4>",       createGenerator(fill_kernel::getTestVariant<gpu_types::float4>)),
                std::make_pair("fill<half4>",        createGenerator(fill_kernel::getTestVariant<gpu_types::half4>)),
                std::make_pair("resample2dimage",    createGenerator(resample2dimage_kernel::getAllTestVariants)),
                std::make_pair("resample3dimage",    createGenerator(resample3dimage_kernel::getAllTestVariants)),
                std::make_pair("readLocalSize",      createGenerator(readlocalsize_kernel::getAllTestVariants)),
                std::make_pair("readConstantData",   createGenerator(readconstantdata_kernel::getAllTestVariants)),
                std::make_pair("strangeShuffle",     createGenerator(strangeshuffle_kernel::getAllTestVariants)),
                std::make_pair("testGtEq",           createGenerator(testgreaterthanorequalto_kernel::getAllTestVariants)),
        };

        test_utils::KernelTest::invocation_tests result;

        auto found = std::find_if(std::begin(test_map), std::end(test_map),
                                  [&testName](decltype(test_map)::const_reference entry){
                                      return testName == entry.first;
                                  });
        if (found != std::end(test_map)) {
            result = found->second();
        }

        return result;
    }
}

namespace test_manifest {
    test_manifest::results run(const manifest_t &manifest,
             clspv_utils::device_t &device)
    {
        test_manifest::results results;

        for (auto& m : manifest.tests) {
            results.push_back(test_utils::test_module(device, m));
        }

        return results;
    }

    manifest_t read(const std::string &inManifest) {
        std::istringstream is(inManifest);
        return read(is);
    }

    manifest_t read(std::istream &in) {
        manifest_t result;
        unsigned int iterations = 1;
        bool verbose = false;

        test_utils::ModuleTest* currentModule = NULL;
        while (!in.eof()) {
            std::string line;
            std::getline(in, line);

            std::istringstream in_line(line);

            std::string op;
            in_line >> op;
            if (op.empty() || op[0] == '#') {
                // line is either blank or a comment, skip it
            } else if (op == "module") {
                // add module to list of modules to load
                test_utils::ModuleTest moduleEntry;
                in_line >> moduleEntry.mName;

                result.tests.push_back(moduleEntry);
                currentModule = &result.tests.back();
            } else if (op == "test" || op == "test2d" || op == "test3d") {
                // test kernel in module
                if (currentModule) {
                    test_utils::KernelTest testEntry;
                    testEntry.mIsVerbose = verbose;
                    testEntry.mIterations = iterations;

                    std::string testName;
                    in_line >> testEntry.mEntryName
                            >> testName
                            >> testEntry.mWorkgroupSize.width
                            >> testEntry.mWorkgroupSize.height;
                    if (op == "test3d") {
                        in_line >> testEntry.mWorkgroupSize.depth;
                    } else {
                        testEntry.mWorkgroupSize.depth = 1;
                    }

                    while (!in_line.eof()) {
                        std::string arg;
                        in_line >> arg;

                        // comment delimiter halts collection of test arguments
                        if (arg[0] == '#') break;

                        testEntry.mArguments.push_back(arg);
                    }

                    testEntry.mInvocationTests = lookup_test_series(testName);

                    bool lineIsGood = true;

                    if (testEntry.mInvocationTests.empty()) {
                        LOGE("%s: cannot find tests '%s' from command '%s'",
                             __func__,
                             testName.c_str(),
                             line.c_str());
                        lineIsGood = false;
                    }
                    if (1 > testEntry.mWorkgroupSize.width || 1 > testEntry.mWorkgroupSize.height || 1 > testEntry.mWorkgroupSize.depth) {
                        LOGE("%s: bad workgroup dimensions {%d,%d,%d} from command '%s'",
                             __func__,
                             testEntry.mWorkgroupSize.width,
                             testEntry.mWorkgroupSize.height,
                             testEntry.mWorkgroupSize.depth,
                             line.c_str());
                        lineIsGood = false;
                    }

                    if (lineIsGood) {
                        currentModule->mKernelTests.push_back(testEntry);
                    }
                } else {
                    LOGE("%s: no module for test '%s'", __func__, line.c_str());
                }
            } else if (op == "skip") {
                // skip kernel in module
                if (currentModule) {
                    test_utils::KernelTest skipEntry;
                    skipEntry.mWorkgroupSize = vk::Extent3D(0, 0, 0);

                    in_line >> skipEntry.mEntryName;

                    currentModule->mKernelTests.push_back(skipEntry);
                } else {
                    LOGE("%s: no module for skip '%s'", __func__, line.c_str());
                }
            } else if (op == "vkValidation") {
                // turn vulkan validation layers on/off
                std::string on_off;
                in_line >> on_off;

                if (on_off == "all") {
                    result.use_validation_layer = true;
                } else if (on_off == "none") {
                    result.use_validation_layer = false;
                } else {
                    LOGE("%s: unrecognized vkValidation token '%s'", __func__, on_off.c_str());
                }
            } else if (op == "verbosity") {
                // set verbosity of tests
                std::string verbose_level;
                in_line >> verbose_level;

                if (verbose_level == "full") {
                    verbose = true;
                } else if (verbose_level == "silent") {
                    verbose = false;
                } else {
                    LOGE("%s: unrecognized verbose level '%s'", __func__, verbose_level.c_str());
                }
            } else if (op == "iterations") {
                // set number of iterations for tests
                int iterations_requested;
                in_line >> iterations_requested;

                if (0 >= iterations_requested) {
                    LOGE("%s: illegal iteration count requested '%d'", __func__,
                         iterations_requested);
                } else {
                    iterations = iterations_requested;
                }
            } else if (op == "end") {
                // terminate reading the manifest
                break;
            } else {
                LOGE("%s: ignoring ill-formed line '%s'", __func__, line.c_str());
            }
        }

        return result;
    }

}