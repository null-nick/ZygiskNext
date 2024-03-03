#include <mntent.h>
#include <sys/mount.h>
#include <algorithm>

#include "files.hpp"
#include "logging.h"
#include "misc.hpp"
#include "zygisk.hpp"

using namespace std::string_view_literals;

namespace {
    constexpr auto MODULE_DIR = "/data/adb/modules";
    constexpr auto KSU_MOUNT_SOURCE = "KSU";
    constexpr auto APATCH_MOUNT_SOURCE = "APatch";
    constexpr auto ZYGISK_MOUNT_SOURCE = "zygisk";

    void lazy_unmount(const char* mountpoint) {
        if (umount2(mountpoint, MNT_DETACH) != -1) {
            LOGD("Unmounted (%s)", mountpoint);
        } else {
#ifndef NDEBUG
            PLOGE("Unmount (%s)", mountpoint);
#endif
        }
    }
}

void revert_unmount_ksu_apatch() {
    std::vector<mount_info> mount_info_vector = parse_mount_info("self");
    auto mod_dir_it = std::find_if(mount_info_vector.begin(), mount_info_vector.end(), [](const mount_info& info) {
        return info.target == MODULE_DIR;
    });
    std::string mod_dir_source = mod_dir_it != mount_info_vector.end() ? mod_dir_it->source.c_str() : "";

    for (const auto& info : mount_info_vector) {
        if (info.target == MODULE_DIR) {
            continue;
        }
        // Unmount everything mounted to /data/adb
        if (info.target.starts_with("/data/adb")) {
            lazy_unmount(info.target.c_str());
        }
        // Unmount KSU/APatch overlayfs and tmpfs
        if ((info.type == "overlay" || info.type == "tmpfs")
            && (info.source == KSU_MOUNT_SOURCE || info.source == APATCH_MOUNT_SOURCE)) {
            lazy_unmount(info.target.c_str());
        }
        // Unmount fuse
        if (info.type == "fuse" && info.source == ZYGISK_MOUNT_SOURCE) {
            lazy_unmount(info.target.c_str());
        }
        // Unmount everything where the source is the module dir except the module dir itself
        if (info.source == mod_dir_source && info.target != MODULE_DIR) {
            lazy_unmount(info.target.c_str());
        }
    }

    // Unmount KSU/APatch module dir last
    lazy_unmount(MODULE_DIR);
}

void revert_unmount_magisk() {
    std::vector<std::string> targets;

    // Unmount dummy skeletons and MAGISKTMP
    // since mirror nodes are always mounted under skeleton, we don't have to specifically unmount
    for (auto& info: parse_mount_info("self")) {
        if (info.source == "magisk" || info.source == "worker" || // magisktmp tmpfs
            info.root.starts_with("/adb/modules")) { // bind mount from data partition
            targets.push_back(info.target);
        }
        // Unmount everything mounted to /data/adb
        if (info.target.starts_with("/data/adb")) {
            targets.emplace_back(info.target);
        }
    }

    for (auto& s: reversed(targets)) {
        lazy_unmount(s.data());
    }
}