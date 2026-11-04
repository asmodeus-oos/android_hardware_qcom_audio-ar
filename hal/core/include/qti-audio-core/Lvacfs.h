/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace qti::audio::core {

struct lvacfs_wrapper_ops {
    void* lib_handle;
    int (*create_instance)(void**, uint32_t, uint64_t, uint32_t);
    int (*destroy_instance)(void*);
    int (*process)(void*, void*, void*, int32_t, void*);
    void (*update_zoom_info)(void*, float);
    void (*update_angle_info)(void*, float);
    void (*set_params_file_path)(const char*);
    void (*set_profile)(void*, uint32_t);
    void (*set_audio_direction)(void*, uint32_t);
    void (*set_device_orientation)(void*, uint32_t);
    void (*get_versions)(char*, size_t);
};

class Lvacfs {
  public:
    static Lvacfs& getInstance() {
        static Lvacfs instance;
        return instance;
    }

    const struct lvacfs_wrapper_ops* getWrapperOps() const { return wrapper_ops_.get(); }

    void** startInputStream(int32_t source, uint32_t sampleRate, uint32_t channelCount);
    bool processInputStream(void** handle, void* buffer, uint32_t numFrames);
    void stopInputStream(void** handle);

  private:
    Lvacfs();

    Lvacfs(const Lvacfs&) = delete;
    Lvacfs& operator=(const Lvacfs&) = delete;

    static bool isLvacfsEnabled();
    void init();
    void deinit();

    std::unique_ptr<struct lvacfs_wrapper_ops> wrapper_ops_;
    const char* params_file_path_;
};

} // namespace qti::audio::core
