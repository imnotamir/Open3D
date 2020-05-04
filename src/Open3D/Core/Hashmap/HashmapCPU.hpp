// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#pragma once

// Implementation for the CPU hashmap. Separated from HashmapCPU.h for brevity.

#include "Open3D/Core/Hashmap/HashmapCPU.h"

namespace open3d {

template <typename Hash, typename KeyEq>
CPUHashmap<Hash, KeyEq>::CPUHashmap(uint32_t max_keys,
                                    uint32_t dsize_key,
                                    uint32_t dsize_value,
                                    Device device)
    : Hashmap<Hash, KeyEq>(max_keys, dsize_key, dsize_value, device) {
    cpu_hashmap_impl_ = std::make_shared<
            std::unordered_map<uint8_t*, uint8_t*, Hash, KeyEq>>(
            max_keys, Hash(dsize_key), KeyEq(dsize_key));
};

template <typename Hash, typename KeyEq>
CPUHashmap<Hash, KeyEq>::~CPUHashmap() {
    for (auto kv_pair : kv_pairs_) {
        MemoryManager::Free(kv_pair.first, this->device_);
        MemoryManager::Free(kv_pair.second, this->device_);
    }
};

template <typename Hash, typename KeyEq>
std::pair<iterator_t*, uint8_t*> CPUHashmap<Hash, KeyEq>::Insert(
        uint8_t* input_keys, uint8_t* input_values, uint32_t input_key_size) {
    // TODO handle memory release
    auto iterators =
            (iterator_t*)std::malloc(input_key_size * sizeof(iterator_t));
    auto masks = (uint8_t*)std::malloc(input_key_size * sizeof(uint8_t));

    for (int i = 0; i < input_key_size; ++i) {
        uint8_t* src_key = (uint8_t*)input_keys + this->dsize_key_ * i;
        uint8_t* src_value = (uint8_t*)input_values + this->dsize_value_ * i;

        // Manually copy before insert
        void* dst_key = MemoryManager::Malloc(this->dsize_key_, this->device_);
        void* dst_value =
                MemoryManager::Malloc(this->dsize_value_, this->device_);

        MemoryManager::Memcpy(dst_key, this->device_, src_key, this->device_,
                              this->dsize_key_);
        MemoryManager::Memcpy(dst_value, this->device_, src_value,
                              this->device_, this->dsize_value_);

        // Try insertion
        auto res = cpu_hashmap_impl_->insert(
                {(uint8_t*)dst_key, (uint8_t*)dst_value});

        // Handle memory
        if (res.second) {
            iterators[i] = iterator_t((uint8_t*)dst_key, (uint8_t*)dst_value);
            masks[i] = 1;
        } else {
            MemoryManager::Free(dst_key, this->device_);
            MemoryManager::Free(dst_value, this->device_);
            iterators[i] = iterator_t();
            masks[i] = 0;
        }
    }

    return std::make_pair(iterators, masks);
}

template <typename Hash, typename KeyEq>
std::pair<iterator_t*, uint8_t*> CPUHashmap<Hash, KeyEq>::Search(
        uint8_t* input_keys, uint32_t input_key_size) {
    // TODO: handle memory release
    auto iterators =
            (iterator_t*)std::malloc(input_key_size * sizeof(iterator_t));
    auto masks = (uint8_t*)std::malloc(input_key_size * sizeof(uint8_t));

    for (int i = 0; i < input_key_size; ++i) {
        uint8_t* key = (uint8_t*)input_keys + this->dsize_key_ * i;

        auto iter = cpu_hashmap_impl_->find(key);
        if (iter == cpu_hashmap_impl_->end()) {
            iterators[i] = iterator_t();
            masks[i] = 0;
        } else {
            void* key = iter->first;
            iterators[i] = iterator_t(iter->first, iter->second);
            masks[i] = 1;
        }
    }

    return std::make_pair(iterators, masks);
}

template <typename Hash, typename KeyEq>
uint8_t* CPUHashmap<Hash, KeyEq>::Remove(uint8_t* input_keys,
                                         uint32_t input_key_size) {
    utility::LogError("Unimplemented method");
    uint8_t* masks = nullptr;
    return masks;
}

template <typename Hash, typename KeyEq>
std::shared_ptr<CPUHashmap<Hash, KeyEq>> CreateCPUHashmap(
        uint32_t max_keys,
        uint32_t dsize_key,
        uint32_t dsize_value,
        open3d::Device device) {
    return std::make_shared<CPUHashmap<Hash, KeyEq>>(max_keys, dsize_key,
                                                     dsize_value, device);
}
}  // namespace open3d