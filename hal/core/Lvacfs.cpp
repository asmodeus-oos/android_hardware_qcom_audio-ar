/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AHAL_Lvacfs_QTI"

#include <PalApi.h>
#include <android-base/logging.h>
#include <dlfcn.h>
#include <qti-audio-core/Lvacfs.h>
#include <unistd.h>

#include <cstdlib>

#ifdef __LP64__
#define VENDOR_LIBS "/vendor/lib64/"
#define ODM_LIBS "/odm/lib64/"
#else
#define VENDOR_LIBS "/vendor/lib/"
#define ODM_LIBS "/odm/lib/"
#endif
#define LVACFS_WRAPPER_LIB_NAME "liblvacfs_wrapper.so"
#define VENDOR_LIB_PATH VENDOR_LIBS LVACFS_WRAPPER_LIB_NAME
#define ODM_LIB_PATH ODM_LIBS LVACFS_WRAPPER_LIB_NAME

#define ODM_PARAMS_DIR_PATH "/odm/etc/lvacfs_params"
#define VENDOR_PARAMS_DIR_PATH "/vendor/etc/lvacfs_params"

#define LOAD_SYMBOL(handle, symbol_ptr, symbol_name)                                    \
    do {                                                                                \
        (symbol_ptr) = reinterpret_cast<decltype(symbol_ptr)>(dlsym((handle), (symbol_name))); \
        if (!(symbol_ptr)) {                                                            \
            LOG(ERROR) << "Failed to load symbol " << (symbol_name) << ": " << dlerror(); \
            deinit();                                                                   \
            return;                                                                     \
        }                                                                               \
    } while (0)

namespace qti::audio::core {

Lvacfs::Lvacfs() : wrapper_ops_(nullptr), params_file_path_(nullptr) {
    if (isLvacfsEnabled()) {
        init();
    }
}

bool Lvacfs::isLvacfsEnabled() {
    bool isEnabled = false;
    bool* payload_lvacfs = &isEnabled;
    size_t param_size = 0;

    int ret = pal_get_param(PAL_PARAM_ID_LVACFS, (void**)&payload_lvacfs, &param_size, nullptr);

    return (!ret && isEnabled);
}

void Lvacfs::init() {
    if (access(ODM_PARAMS_DIR_PATH, F_OK) == 0) {
        params_file_path_ = ODM_PARAMS_DIR_PATH;
    } else if (access(VENDOR_PARAMS_DIR_PATH, F_OK) == 0) {
        params_file_path_ = VENDOR_PARAMS_DIR_PATH;
    } else {
        LOG(ERROR) << "No params directory found";
        return;
    }

    wrapper_ops_ = std::make_unique<struct lvacfs_wrapper_ops>();

    wrapper_ops_->lib_handle = dlopen(ODM_LIB_PATH, RTLD_NOW);
    if (!wrapper_ops_->lib_handle) {
        wrapper_ops_->lib_handle = dlopen(VENDOR_LIB_PATH, RTLD_NOW);
        if (!wrapper_ops_->lib_handle) {
            LOG(ERROR) << "dlopen failed for " << ODM_LIB_PATH << " and " << VENDOR_LIB_PATH
                       << ": " << dlerror();
            deinit();
            return;
        }
    }

    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->create_instance,
                "lvacfs_wrapper_CreateLibraryInstance");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->destroy_instance,
                "lvacfs_wrapper_DestroyLibraryInstance");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->process, "lvacfs_wrapper_Process");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->update_zoom_info,
                "lvacfs_wrapper_UpdateZoomInfo");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->update_angle_info,
                "lvacfs_wrapper_UpdateAngleInfo");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_params_file_path,
                "lvacfs_SetParamsFilePath");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_profile, "lvacfs_wrapper_SetProfile");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_audio_direction,
                "lvacfs_wrapper_SetAudioDirection");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->set_device_orientation,
                "lvacfs_wrapper_SetDeviceOrientation");
    LOAD_SYMBOL(wrapper_ops_->lib_handle, wrapper_ops_->get_versions, "lvacfs_wrapper_GetVersions");

    LOG(INFO) << "LVACFS initialized successfully";
}

void Lvacfs::deinit() {
    if (wrapper_ops_ && wrapper_ops_->lib_handle) {
        dlclose(wrapper_ops_->lib_handle);
    }
    wrapper_ops_.reset();
    LOG(INFO) << "LVACFS deinitialized";
}

void** Lvacfs::startInputStream(int32_t source, uint32_t sampleRate, uint32_t channelCount) {
    void** handle = static_cast<void**>(calloc(1, sizeof(void*)));
    if (!handle) {
        LOG(ERROR) << "Failed to allocate lvacfs handle";
        return nullptr;
    }
    wrapper_ops_->set_params_file_path(params_file_path_);
    uint32_t channels = ((channelCount & 0xFFFF) << 16) | (channelCount & 0xFFFF);
    // Set format to false (0) since lvacfs does not support pal audio format
    uint64_t sample_rate_and_format = ((uint64_t)0 << 32) | sampleRate;
    int ret = wrapper_ops_->create_instance(handle, source, sample_rate_and_format, channels);
    if (ret < 0) {
        LOG(ERROR) << "create instance failed: " << ret;
        stopInputStream(handle);
        return nullptr;
    }
    return handle;
}

bool Lvacfs::processInputStream(void** handle, void* buffer, uint32_t numFrames) {
    uint8_t status_buffer[0x160] = {0};
    int ret = wrapper_ops_->process(handle, buffer, buffer, numFrames, status_buffer);
    if (ret < 0) {
        LOG(ERROR) << "process failed: " << ret;
        return false;
    }
    return true;
}

void Lvacfs::stopInputStream(void** handle) {
    int ret = wrapper_ops_->destroy_instance(handle);
    if (ret < 0) {
        LOG(ERROR) << "destroy instance failed: " << ret;
    }
    free(handle);
}

} // namespace qti::audio::core
