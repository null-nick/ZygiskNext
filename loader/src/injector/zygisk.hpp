#pragma once

#include <stdint.h>
#include <jni.h>
#include <vector>

extern void *self_handle;

void hook_functions();

void revert_unmount_ksu_apatch();

void revert_unmount_magisk();
