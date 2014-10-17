/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * OS version library
 * Copyright (C) 2014 Collabora Ltd.
 *
 * OS version library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * OS version library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with OS version library.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Philip Withnall <philip.withnall@collabora.co.uk>
 */

#include "config.h"

#include <string.h>

#include <glib.h>

#ifdef HAVE_SYS_UTSNAME_H
/* Standard on Unices. */
#include <sys/utsname.h>
#endif
#ifdef HAVE_WINDOWS_H
/* Standard on Windows. */
#include <windows.h>
#endif
#ifdef HAVE_ANDROID_API_LEVEL_H
/* Need to compile with the Android NDK for this. */
#include <android/api-level.h>
#include <sys/system_properties.h>
#endif
#ifdef HAVE_TARGET_CONDITIONALS_H
/* Need to compile with the iOS SDK for this. */
#include <TargetConditionals.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "osversion.h"


#if defined(__APPLE__) && defined(__MACH__)
static gchar *
get_apple_hw_property (const gchar *property_name)
{
	gchar *out;
	size_t length;

	if (sysctlbyname (property_name, NULL, &length, NULL, 0) < 0) {
		return g_strdup ("Unknown");
	}

	out = g_malloc0 (length);

	if (sysctlbyname (property_name, out, &length, NULL, 0) < 0) {
		g_free (out);
		return g_strdup ("Unknown");
	}

	return out;
}
#endif  /* Apple */

#ifdef HAVE_SYS_UTSNAME_H
static void
get_uname_fields (GPtrArray/*<owned string>*/ *fields)
{
	struct utsname name;

	memset (&name, 0, sizeof (name));

	if (uname (&name) != -1) {
		g_ptr_array_add (fields, g_strdup (name.sysname));
		g_ptr_array_add (fields, g_strdup (name.release));
		g_ptr_array_add (fields, g_strdup (name.version));
		g_ptr_array_add (fields, g_strdup (name.machine));
	}
}
#endif /* HAVE_SYS_UTSNAME_H */

/**
 * get_os_version:
 *
 * Gets detailed information about the OS the client is currently running on.
 * This is returned in the following format:
 * |[
 * OS name[, other version data[, …]], OS_VERSION
 * ]|
 *
 * ``OS_VERSION`` is as specified at configure time. The OS name is a string
 * like ‘iOS’ or ‘Linux’, and may contain any character except a comma. The
 * other version data will be zero or more fields, separated by commas, quoted
 * with double quotation marks and escaped using g_strescape(). Each field may
 * contain any character, but will typically be an ASCII string, integer, or
 * version number (integers separated by dots).
 *
 * This should not return any machine-specific identifiable information, such
 * as the hostname.
 *
 * Returns: (transfer full): the UTF-8 OS version string
 *
 * Since: 0.1.0
 */
gchar *
get_os_version (void)
{
	GPtrArray/*<owned string>*/ *fields;
	GString *out;
	guint j;

	fields = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

#if defined(__APPLE__) && defined(__MACH__)
{
	/* Darwin and iOS. */
	const gchar *os_name;

	/* Get the device specified at compile time. */
#if TARGET_IPHONE_SIMULATOR == 1
	/* iOS in Xcode simulator */
	os_name = "iOS Xcode";
#elif TARGET_OS_IPHONE == 1
	/* iOS on iPhone, iPad, etc. */
	os_name = "iOS";
#elif TARGET_OS_EMBEDDED == 1
	/* iOS embedded elsewhere. */
	os_name = "iOS embedded";
#elif TARGET_OS_MAC == 1
	/* OS X */
	os_name = "Darwin";
#else
	/* Unknown. */
	os_name = "Apple";
#endif

	g_ptr_array_add (fields, g_strdup (os_name));

	/* Grab some general purpose kernel information. */
	get_uname_fields (fields);

	/* Get the runtime device.
	 *
	 * Reference: https://gist.github.com/Jaybles/1323251
	 * Reference: https://developer.apple.com/library/mac/documentation/
	 *            Darwin/Reference/ManPages/man3/sysctlbyname.3.html*/
	g_ptr_array_add (fields, get_apple_hw_property ("hw.machine"));
	g_ptr_array_add (fields, get_apple_hw_property ("hw.model"));
}
#elif defined(_WIN64) || defined(_WIN32)
{
	/* Windows. */
	OSVERSIONINFOEX info;
	SYSTEM_INFO sys_info;
	gboolean success = FALSE;
	gboolean have_extended_fields = FALSE;

	g_ptr_array_add (fields, g_strdup ("Windows"));

	/* Get the version information. */
	memset (&info, 0, sizeof (info));
	info.dwOSVersionInfoSize = sizeof (info);

	if (GetVersionEx ((OSVERSIONINFO *) &info) != 0) {
		success = TRUE;
		have_extended_fields = TRUE;
	} else {
		info.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

		if (GetVersionEx ((OSVERSIONINFO *) &info) != 0) {
			success = TRUE;
			have_extended_fields = FALSE;
		}
	}

	/* Success? */
	if (success) {
		g_ptr_array_add (fields,
		                 g_strdup_printf ("%u",
		                                  info.dwOSVersionInfoSize));
		g_ptr_array_add (fields,
		                 g_strdup_printf ("%u.%u.%u",
		                                  info.dwMajorVersion,
		                                  info.dwMinorVersion,
		                                  info.dwBuildNumber));
		g_ptr_array_add (fields,
		                 g_strdup_printf ("%u",
		                                  info.dwPlatformId));
		g_ptr_array_add (fields, g_strdup (info.szCSDVersion));

		if (have_extended_fields) {
			g_ptr_array_add (fields,
			                 g_strdup_printf ("%u.%u",
			                                  info.wServicePackMajor,
			                                  info.wServicePackMinor));
			g_ptr_array_add (fields,
			                 g_strdup_printf ("%u",
			                                  info.wSuiteMask));
			g_ptr_array_add (fields,
			                 g_strdup_printf ("%u",
			                                  info.wProductType));
		}
	}

	/* Get the system info. */
	memset (&sys_info, 0, sizeof (sys_info));
	GetSystemInfo (&sys_info);

	g_ptr_array_add (fields,
	                 g_strdup_printf ("%u",
	                                  sys_info.wProcessorArchitecture));
	g_ptr_array_add (fields,
	                 g_strdup_printf ("%u", sys_info.wProcessorLevel));
	g_ptr_array_add (fields,
	                 g_strdup_printf ("%u", sys_info.wProcessorRevision));
}
#elif defined(__ANDROID__)
{
	/* Android. */
	guint i;
	const gchar *property_names[] = {
		"ro.product.model",
		"ro.product.brand",
		"ro.product.name",
		"ro.product.device",
		"ro.product.board",
		"ro.product.manufacturer",
		"ro.build.id",
		"ro.build.display.id",
		"ro.build.version.incremental",
		"ro.build.version.sdk",
		"ro.build.version.codename",
		"ro.build.version.release",
		/* Add new entries here; the order affects how the server parses
		 * the OS details, so can’t be changed. */
	};

	g_ptr_array_add (fields, g_strdup ("Android"));
	g_ptr_array_add (fields, g_strdup_printf ("%u", __ANDROID_API__));

	/* Grab stuff from the kernel. Probably not very useful. */
	get_uname_fields (fields);

	/* Grab stuff via JNI.
	 *
	 * Reference: https://gist.github.com/deltheil/2291028 */
	for (i = 0; i < G_N_ELEMENTS (property_names); i++) {
		gchar prop[PROP_VALUE_MAX + 1];
		gint length;

		/* length will be zero if the property doesn’t exist. */
		length = __system_property_get (property_names[i], prop);

		if (length > 0) {
			g_ptr_array_add (fields, g_strndup (prop, length));
		} else {
			g_ptr_array_add (fields, g_strdup ("Unknown"));
		}
	}
}
#else
{
	/* Linux. */
	g_ptr_array_add (fields, g_strdup ("Linux"));
	get_uname_fields (fields);
}
#endif

	/* Escape and implode the fields. */
	out = g_string_new ("");

	for (j = 0; j < fields->len; j++) {
		gchar *value, *escaped;

		value = g_ptr_array_index (fields, j);
		escaped = g_strescape (value, "");

		g_string_append_printf (out, "%s\"%s\"",
		                        (j > 0) ? ", " : "", escaped);

		g_free (escaped);
	}

	g_ptr_array_unref (fields);

	return g_string_free (out, FALSE);
}

int
main (void)
{
	return 0;
}
