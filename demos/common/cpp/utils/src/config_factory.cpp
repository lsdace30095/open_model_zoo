/*
// Copyright (C) 2020-2022 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/


#include <set>
#include <string>
#include "utils/args_helper.hpp"
#include "utils/common.hpp"
#include "utils/config_factory.h"

std::set<std::string> ModelConfig::getDevices() {
    if (devices.empty()) {
        for (const std::string& device : ::parseDevices(deviceName)) {
            devices.insert(device);
        }
    }

    return devices;
}

ModelConfig ConfigFactory::getUserConfig(const std::string& flags_d,
    uint32_t flags_nireq, const std::string& flags_nstreams, uint32_t flags_nthreads) {
    auto config = getCommonConfig(flags_d, flags_nireq);

    std::map<std::string, unsigned> deviceNstreams = parseValuePerDevice(config.getDevices(), flags_nstreams);
    for (const auto& device : config.getDevices()) {
        if (device == "CPU") {  // CPU supports a few special performance-oriented keys
            // limit threading for CPU portion of inference
            if (flags_nthreads != 0)
                config.compiledModelConfig.emplace(CONFIG_KEY(CPU_THREADS_NUM), std::to_string(flags_nthreads));

            config.compiledModelConfig.emplace(CONFIG_KEY(CPU_BIND_THREAD), CONFIG_VALUE(NO));
        }
        else if (device == "GPU") {
            config.compiledModelConfig.emplace(CONFIG_KEY(GPU_THROUGHPUT_STREAMS),
                (deviceNstreams.count(device) > 0 ? std::to_string(deviceNstreams.at(device))
                    : CONFIG_VALUE(GPU_THROUGHPUT_AUTO)));

            if (flags_d.find("MULTI") != std::string::npos
                && config.getDevices().find("CPU") != config.getDevices().end()) {
                // multi-device execution with the CPU + GPU performs best with GPU throttling hint,
                // which releases another CPU thread (that is otherwise used by the GPU driver for active polling)
                config.compiledModelConfig.emplace(GPU_CONFIG_KEY(PLUGIN_THROTTLE), "1");
            }
        }
    }
    return config;
}

ModelConfig ConfigFactory::getMinLatencyConfig(const std::string& flags_d, uint32_t flags_nireq) {
    auto config = getCommonConfig(flags_d, flags_nireq);
    for (const auto& device : config.getDevices()) {
        if (device == "CPU") {  // CPU supports a few special performance-oriented keys
            config.compiledModelConfig.emplace(CONFIG_KEY(CPU_THROUGHPUT_STREAMS), "1");
        }
        else if (device == "GPU") {
            config.compiledModelConfig.emplace(CONFIG_KEY(GPU_THROUGHPUT_STREAMS), "1");
        }
    }
    return config;
}

ModelConfig ConfigFactory::getCommonConfig(const std::string& flags_d, uint32_t flags_nireq) {
    ModelConfig config;

    if (!flags_d.empty()) {
        config.deviceName = flags_d;
    }

    config.maxAsyncRequests = flags_nireq;

    return config;
}

std::map<std::string, std::string> ModelConfig::getLegacyConfig() {
    std::map<std::string, std::string> config;
    for (const auto& item : compiledModelConfig) {
        config[item.first] = item.second.as<std::string>();
    }
    return config;
}
