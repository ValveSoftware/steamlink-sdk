// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_UDEV_LINUX_UDEV_H_
#define DEVICE_UDEV_LINUX_UDEV_H_

#include <libudev.h>

#include "base/memory/scoped_ptr.h"

#if !defined(USE_UDEV)
#error "USE_UDEV not defined"
#endif

#include <dlfcn.h>

namespace device {

struct UdevDeleter {
  void operator()(udev* dev) const;
};
struct UdevEnumerateDeleter {
  void operator()(udev_enumerate* enumerate) const;
};
struct UdevDeviceDeleter {
  void operator()(udev_device* device) const;
};
struct UdevMonitorDeleter {
  void operator()(udev_monitor* monitor) const;
};

typedef scoped_ptr<udev, UdevDeleter> ScopedUdevPtr;
typedef scoped_ptr<udev_enumerate, UdevEnumerateDeleter> ScopedUdevEnumeratePtr;
typedef scoped_ptr<udev_device, UdevDeviceDeleter> ScopedUdevDevicePtr;
typedef scoped_ptr<udev_monitor, UdevMonitorDeleter> ScopedUdevMonitorPtr;


class LibUdevWrapper {
public:
    LibUdevWrapper() : m_libUdev(0), m_loaded(false)
    {
        load();
    }

    virtual ~LibUdevWrapper()
    {
        if (m_libUdev)
            dlclose(m_libUdev);
    }

private:
    void* m_libUdev;
    bool m_loaded;
    void load()
    {
        m_libUdev = dlopen("libudev.so.1", RTLD_NOW);
        m_loaded = m_libUdev;
        if (resolveMethods())
            return;
        if (!m_libUdev)
            m_libUdev = dlopen("libudev.so.0", RTLD_NOW);
        m_loaded = m_libUdev;
        resolveMethods();
    }

    void* resolve(const char* name)
    {
        void* ptr = dlsym(m_libUdev, name);
        if (!ptr) {
            m_loaded = false;
        }
        return ptr;
    }

    bool resolveMethods()
    {
        if (!m_loaded)
            return false;
        udev_new = (udev* (*)())resolve("udev_new");
        udev_unref = (void (*)(udev*))resolve("udev_unref");
        udev_monitor_new_from_netlink = (udev_monitor* (*)(udev*, const char*))resolve("udev_monitor_new_from_netlink");
        udev_monitor_unref = (void (*)(udev_monitor*))resolve("udev_monitor_unref");
        udev_monitor_enable_receiving = (int (*)(udev_monitor*))resolve("udev_monitor_enable_receiving");
        udev_monitor_get_fd = (int (*)(udev_monitor*))resolve("udev_monitor_get_fd");
        udev_monitor_filter_add_match_subsystem_devtype = (int (*)(udev_monitor*, const char*, const char*))resolve("udev_monitor_filter_add_match_subsystem_devtype");
        udev_monitor_receive_device = (udev_device* (*)(udev_monitor*))resolve("udev_monitor_receive_device");
        udev_enumerate_new = (udev_enumerate* (*)(udev*))resolve("udev_enumerate_new");
        udev_enumerate_unref = (void (*)(udev_enumerate*))resolve("udev_enumerate_unref");
        udev_enumerate_add_match_subsystem = (int (*)(udev_enumerate*, const char*))resolve("udev_enumerate_add_match_subsystem");
        udev_enumerate_add_match_property = (int (*)(udev_enumerate*, const char*, const char*))resolve("udev_enumerate_add_match_property");
        udev_enumerate_scan_devices = (int (*)(udev_enumerate*))resolve("udev_enumerate_scan_devices");
        udev_enumerate_get_list_entry = (udev_list_entry* (*)(udev_enumerate*))resolve("udev_enumerate_get_list_entry");
        udev_list_entry_get_next = (udev_list_entry* (*)(udev_list_entry*))resolve("udev_list_entry_get_next");
        udev_list_entry_get_name = (const char* (*)(udev_list_entry*))resolve("udev_list_entry_get_name");
        udev_device_new_from_syspath = (udev_device* (*)(udev*, const char*))resolve("udev_device_new_from_syspath");
        udev_device_unref = (void (*)(udev_device*))resolve("udev_device_unref");
        udev_device_get_syspath = (const char* (*)(udev_device*))resolve("udev_device_get_syspath");
        udev_device_get_devnode = (const char* (*)(udev_device*))resolve("udev_device_get_devnode");
        udev_device_get_property_value = (const char* (*)(udev_device*, const char*))resolve("udev_device_get_property_value");
        udev_device_get_action = (const char* (*)(udev_device*))resolve("udev_device_get_action");
        udev_device_get_subsystem = (const char* (*)(udev_device*))resolve("udev_device_get_subsystem");
        udev_device_get_sysattr_value = (const char* (*)(udev_device*, const char*))resolve("udev_device_get_sysattr_value");
        udev_device_get_parent_with_subsystem_devtype =
            (struct udev_device* (*)(struct udev_device*, const char*, const char*))resolve("udev_device_get_parent_with_subsystem_devtype");

        return m_loaded;
    }

public:
    struct udev* (*udev_new)();
    void (*udev_unref)(struct udev*);

    struct udev_monitor* (*udev_monitor_new_from_netlink)(struct udev*, const char *name);
    void (*udev_monitor_unref)(struct udev_monitor*);
    int (*udev_monitor_enable_receiving)(struct udev_monitor*);
    int (*udev_monitor_get_fd)(struct udev_monitor*);
    int (*udev_monitor_filter_add_match_subsystem_devtype)(struct udev_monitor*, const char *subsystem, const char *devtype);
    struct udev_device* (*udev_monitor_receive_device)(struct udev_monitor*);

    struct udev_enumerate* (*udev_enumerate_new)(struct udev*);
    void (*udev_enumerate_unref)(struct udev_enumerate*);
    int (*udev_enumerate_add_match_subsystem)(struct udev_enumerate*, const char *subsystem);
    int (*udev_enumerate_add_match_property)(struct udev_enumerate*, const char *property, const char *value);
    int (*udev_enumerate_scan_devices)(struct udev_enumerate*);
    struct udev_list_entry* (*udev_enumerate_get_list_entry)(struct udev_enumerate*);

    struct udev_list_entry* (*udev_list_entry_get_next)(struct udev_list_entry*);
    const char* (*udev_list_entry_get_name)(struct udev_list_entry*);

    struct udev_device* (*udev_device_new_from_syspath)(struct udev *udev, const char *syspath);
    void (*udev_device_unref)(struct udev_device *udev_device);
    const char* (*udev_device_get_syspath)(struct udev_device *udev_device);
    const char* (*udev_device_get_devnode)(struct udev_device *udev_device);
    const char* (*udev_device_get_property_value)(struct udev_device *udev_device, const char *key);
    const char* (*udev_device_get_action)(struct udev_device *udev_device);
    const char* (*udev_device_get_subsystem)(struct udev_device *udev_device);
    const char* (*udev_device_get_sysattr_value)(struct udev_device *udev_device, const char *sysattr);
    struct udev_device* (*udev_device_get_parent_with_subsystem_devtype)(struct udev_device *udev_device,
                                                                  const char *subsystem, const char *devtype);
};

}  // namespace device

extern device::LibUdevWrapper libudev;

#endif  // DEVICE_UDEV_LINUX_UDEV_H_
