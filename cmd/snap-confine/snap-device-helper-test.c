/*
 * Copyright (C) 2018 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../libsnap-confine-private/test-utils.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>

// TODO: build at runtime
static const char *sdh_path_default = "snap-confine/snap-device-helper";

// A variant of unsetenv that is compatible with GDestroyNotify
static void my_unsetenv(const char *k)
{
	g_unsetenv(k);
}

// A variant of rm_rf_tmp that calls g_free() on its parameter
static void rm_rf_tmp_free(gchar * dirpath)
{
	rm_rf_tmp(dirpath);
	g_free(dirpath);
}

static gchar *find_sdh_path(void)
{
	const char *sdh_from_env = g_getenv("SNAP_DEVICE_HELPER");
	if (sdh_from_env != NULL) {
		return g_strdup(sdh_from_env);
	}
	return g_strdup(sdh_path_default);
}

static int run_sdh(gchar * action,
		   gchar * appname, gchar * devpath, gchar * majmin)
{
	gchar *mod_appname = g_strdup(appname);
	gchar *sdh_path = find_sdh_path();

	// appnames have the following format:
	// - snap.<snap>.<app>
	// - snap.<snap>_<instance>.<app>
	// snap-device-helper expects:
	// - snap_<snap>_<app>
	// - snap_<snap>_<instance>_<app>
	for (size_t i = 0; i < strlen(mod_appname); i++) {
		if (mod_appname[i] == '.') {
			mod_appname[i] = '_';
		}
	}
	g_debug("appname modified from %s to %s", appname, mod_appname);

	GError *err = NULL;

	GPtrArray *argv = g_ptr_array_new();
	g_ptr_array_add(argv, sdh_path);
	g_ptr_array_add(argv, action);
	g_ptr_array_add(argv, mod_appname);
	g_ptr_array_add(argv, devpath);
	g_ptr_array_add(argv, majmin);
	g_ptr_array_add(argv, NULL);

	int status = 0;

	gboolean ret = g_spawn_sync(NULL, (gchar **) argv->pdata, NULL, 0,
				    NULL, NULL, NULL, NULL, &status, &err);
	g_free(mod_appname);
	g_free(sdh_path);
	g_ptr_array_unref(argv);

	if (!ret) {
		g_debug("failed with: %s", err->message);
		g_error_free(err);
		return -2;
	}

	return WEXITSTATUS(status);
}

struct sdh_test_data {
	char *action;
	// snap.foo.bar
	char *app;
	// snap_foo_bar
	char *mangled_appname;
	char *file_with_data;
	char *file_with_no_data;
};

static void test_sdh_action(gconstpointer test_data)
{
	struct sdh_test_data *td = (struct sdh_test_data *)test_data;

	gchar *mock_dir = g_dir_make_tmp(NULL, NULL);
	g_assert_nonnull(mock_dir);
	g_test_queue_destroy((GDestroyNotify) rm_rf_tmp_free, mock_dir);

	gchar *app_dir = g_build_filename(mock_dir, td->app, NULL);
	gchar *with_data = g_build_filename(mock_dir,
					    td->app,
					    td->file_with_data,
					    NULL);
	gchar *without_data = g_build_filename(mock_dir,
					       td->app,
					       td->file_with_no_data,
					       NULL);
	gchar *data = NULL;

	g_assert(g_mkdir_with_parents(app_dir, 0755) == 0);
	g_free(app_dir);

	g_debug("mock cgroup dir: %s", mock_dir);

	g_setenv("DEVICES_CGROUP", mock_dir, TRUE);

	g_test_queue_destroy((GDestroyNotify) my_unsetenv, "DEVICES_CGROUP");

	int ret =
	    run_sdh(td->action, td->app, "/devices/foo/block/sda/sda4", "8:4");
	g_assert_cmpint(ret, ==, 0);
	g_assert_true(g_file_get_contents(with_data, &data, NULL, NULL));
	g_assert_cmpstr(data, ==, "b 8:4 rwm\n");
	g_clear_pointer(&data, g_free);
	g_assert(g_remove(with_data) == 0);

	g_assert_false(g_file_get_contents(without_data, &data, NULL, NULL));

	ret =
	    run_sdh(td->action, td->mangled_appname, "/devices/foo/tty/ttyS0",
		    "4:64");
	g_assert_cmpint(ret, ==, 0);
	g_assert_true(g_file_get_contents(with_data, &data, NULL, NULL));
	g_assert_cmpstr(data, ==, "c 4:64 rwm\n");
	g_clear_pointer(&data, g_free);
	g_assert(g_remove(with_data) == 0);

	g_assert_false(g_file_get_contents(without_data, &data, NULL, NULL));

	g_free(with_data);
	g_free(without_data);
}

static void test_sdh_err(void)
{
	// missing appname
	int ret = run_sdh("add", "", "/devices/foo/block/sda/sda4", "8:4");
	g_assert_cmpint(ret, ==, 1);
	// malformed appname
	ret = run_sdh("add", "foo_bar", "/devices/foo/block/sda/sda4", "8:4");
	g_assert_cmpint(ret, ==, 1);
	// missing devpath
	ret = run_sdh("add", "snap_foo_bar", "", "8:4");
	g_assert_cmpint(ret, ==, 1);
	// missing device major:minor numbers
	ret = run_sdh("add", "snap_foo_bar", "/devices/foo/block/sda/sda4", "");
	g_assert_cmpint(ret, ==, 0);

	// mock some stuff so that we can reach the 'action' checks
	gchar *mock_dir = g_dir_make_tmp(NULL, NULL);
	g_assert_nonnull(mock_dir);
	g_test_queue_destroy((GDestroyNotify) rm_rf_tmp_free, mock_dir);

	gchar *app_dir = g_build_filename(mock_dir, "snap.foo.bar", NULL);
	g_assert(g_mkdir_with_parents(app_dir, 0755) == 0);
	g_free(app_dir);
	g_setenv("DEVICES_CGROUP", mock_dir, TRUE);

	g_test_queue_destroy((GDestroyNotify) my_unsetenv, "DEVICES_CGROUP");

	ret =
	    run_sdh("badaction", "snap_foo_bar", "/devices/foo/block/sda/sda4",
		    "8:4");
	g_assert_cmpint(ret, ==, 1);
}

static struct sdh_test_data add_data =
    { "add", "snap.foo.bar", "snap_foo_bar", "devices.allow", "devices.deny" };
static struct sdh_test_data change_data =
    { "change", "snap.foo.bar", "snap_foo_bar", "devices.allow",
"devices.deny" };
static struct sdh_test_data remove_data =
    { "remove", "snap.foo.bar", "snap_foo_bar", "devices.deny",
"devices.allow" };
static struct sdh_test_data instance_add_data =
    { "add", "snap.foo_bar.baz", "snap_foo_bar_baz", "devices.allow",
"devices.deny" };
static struct sdh_test_data instance_change_data =
    { "change", "snap.foo_bar.baz", "snap_foo_bar_baz", "devices.allow",
"devices.deny" };
static struct sdh_test_data instance_remove_data =
    { "remove", "snap.foo_bar.baz", "snap_foo_bar_baz", "devices.deny",
"devices.allow" };
static struct sdh_test_data add_hook_data =
    { "add", "snap.foo.hook.configure", "snap_foo_hook_configure",
"devices.allow", "devices.deny" };
static struct sdh_test_data instance_add_hook_data =
    { "add", "snap.foo_bar.hook.configure", "snap_foo_bar_hook_configure",
"devices.allow", "devices.deny" };
static struct sdh_test_data instance_add_instance_name_is_hook_data =
    { "add", "snap.foo_hook.hook.configure", "snap_foo_hook_hook_configure",
"devices.allow", "devices.deny" };

static void __attribute__((constructor)) init(void)
{

	g_test_add_data_func("/snap-device-helper/add",
			     &add_data, test_sdh_action);
	g_test_add_data_func("/snap-device-helper/change", &change_data,
			     test_sdh_action);
	g_test_add_data_func("/snap-device-helper/remove", &remove_data,
			     test_sdh_action);
	g_test_add_func("/snap-device-helper/err", test_sdh_err);
	g_test_add_data_func("/snap-device-helper/parallel/add",
			     &instance_add_data, test_sdh_action);
	g_test_add_data_func("/snap-device-helper/parallel/change",
			     &instance_change_data, test_sdh_action);
	g_test_add_data_func("/snap-device-helper/parallel/remove",
			     &instance_remove_data, test_sdh_action);
	// hooks
	g_test_add_data_func("/snap-device-helper/hook/add",
			     &add_hook_data, test_sdh_action);
	g_test_add_data_func("/snap-device-helper/hook/parallel/add",
			     &instance_add_hook_data, test_sdh_action);
	g_test_add_data_func("/snap-device-helper/hook-name-hook/parallel/add",
			     &instance_add_instance_name_is_hook_data,
			     test_sdh_action);
}
