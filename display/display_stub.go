package main

import "github.com/BurntSushi/xgb/xproto"

import "dlib/dbus"

func (dpy *Display) GetDBusInfo() dbus.DBusInfo {
	return dbus.DBusInfo{
		"com.deepin.daemon.Display",
		"/com/deepin/daemon/Display",
		"com.deepin.daemon.Display",
	}
}

func (dpy *Display) setPropScreenWidth(v uint16) {
	if dpy.ScreenWidth != v {
		dpy.ScreenWidth = v
		dbus.NotifyChange(dpy, "ScreenWidth")
	}
}

func (dpy *Display) setPropScreenHeight(v uint16) {
	if dpy.ScreenHeight != v {
		dpy.ScreenHeight = v
		dbus.NotifyChange(dpy, "ScreenHeight")
	}
}

func (dpy *Display) setPropPrimaryRect(v xproto.Rectangle) {
	if dpy.PrimaryRect != v {
		dpy.PrimaryRect = v
		dbus.NotifyChange(dpy, "PrimaryRect")

		if dpy.PrimaryChanged != nil {
			dpy.PrimaryChanged(dpy.PrimaryRect)
		}
	}
}

func (dpy *Display) setPropDisplayMode(v int16) {
	if dpy.DisplayMode != v {
		dpy.DisplayMode = v
		dbus.NotifyChange(dpy, "DisplayMode")
	}
}

func (dpy *Display) setPropMonitors(v []*Monitor) {
	for _, m := range dpy.Monitors {
		dbus.UnInstallObject(m)
	}

	dpy.Monitors = v
	for _, m := range dpy.Monitors {
		dbus.InstallOnSession(m)
	}
	dbus.NotifyChange(dpy, "Monitors")
}
