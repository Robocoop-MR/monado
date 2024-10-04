// Copyright 2020-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  RoboCoop's driver's configuration handling
 *
 * @author kalmenn <kale@kalmenn.fr>
 * @ingroup drv_robocoop_internals
 */

#include "robocoop_config.h"
#include "util/u_file.h"
#include "util/u_json.h"
#include "util/u_debug.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

DEBUG_GET_ONCE_OPTION(robocoop_config_dir, "ROBOCOOP_CONFIG_DIR", NULL)

/*!
 * @brief Finds the configuration directory for the driver.
 *
 * Most likely ~/.config/monado/robocoop on linux but can be overridden with
 * the `ROBOCOOP_CONFIG_DIR` environment variable.
 *
 * @returns The path to the driver's configuration directory.
 *     - Always in a new allocation so must be freed.
 *     - It might not exist on the filesystem.
 *
 * @retval null We could neither get the system's default app local config path,
 *              nor get the location from an environment variable.
 * @retval "char *" The path to the driver's configuration directory
 */
char *
get_config_dir(void)
{
	const char *env_config_dir = debug_get_option_robocoop_config_dir();

	if (env_config_dir != NULL) {
		char *out_path = (char *)calloc(strlen(env_config_dir), sizeof(char));
		strcpy(out_path, env_config_dir);
		return out_path;
	}

	ssize_t buf_size = 128;
	char *monado_config_dir;

	while (buf_size < 1024) {
		monado_config_dir = calloc(buf_size, sizeof(char));

		ssize_t ret = u_file_get_config_dir(monado_config_dir, buf_size);

		if (ret < 0) {
			monado_config_dir = NULL;
			U_LOG_E("Couldn't locate monado's config directory");
			break;
		} else if (ret < buf_size) {
			// < and not <= because the null byte is not counted
			break;
		} else {
			free(monado_config_dir);
			monado_config_dir = NULL;
			// And retry with a larger buffer size
			buf_size *= 2;
		}
	}

	if (monado_config_dir == NULL) {
		free(monado_config_dir);
		return NULL;
	}

	unsigned long filepath_len = strlen(monado_config_dir) + 1 + strlen(ROBOCOOP_CONFIG_DIR_FRAGMENT) + 1;
	char *config_dir = (char *)calloc(filepath_len, sizeof(char));

	unsigned long written = snprintf(config_dir, filepath_len, "%s/%s", monado_config_dir, ROBOCOOP_CONFIG_DIR_FRAGMENT);
	assert((written + 1) == filepath_len);

	free(monado_config_dir);

	return config_dir;
}

/*!
 * @brief Finds the path to the config file of a type of RoboCoop device
 *
 * This is a single file under the directory returned by @ref "get_config_dir".
 * Its name is defined in @ref "robocoop_config.h".
 *
 * @param[in] device The model of the device whose config path should be retrieved
 *
 * @returns The path to the configuration file for this device
 *          (might not exist on the filesystem)
 * @retval null The call to @ref "get_config_dir" failed.
 * @retval "char *" The path to the configuration file for this device
 */
char *
get_device_config_path(enum robocoop_device device)
{
	char *config_dir = get_config_dir();

	if (config_dir == NULL) {
		U_LOG_W("Could not locate the driver's config directory");
		return NULL;
	}

	const char *device_path_fragment;
	switch (device) {
	case ROBOCOOP_HMD: device_path_fragment = ROBOCOOP_HMD_CONFIG_FRAGMENT; break;
	}

	unsigned long filepath_len = strlen(config_dir) + 1 + strlen(device_path_fragment) + 1;

	char *device_config_path = calloc(filepath_len, sizeof(char));
	unsigned long written = snprintf(device_config_path, filepath_len, "%s/%s", config_dir, device_path_fragment);
	assert(written + 1 == filepath_len);

	free(config_dir);

	return device_config_path;
}

// --- Exports ---

cJSON *
robocoop_get_device_config(enum robocoop_device device)
{
	char *filepath = get_device_config_path(device);
	if (filepath == NULL)
		return NULL;

	char *read = u_file_read_content_from_path(filepath);
	if (read == NULL) {
		U_LOG_E(
		    "Could not read %s "
		    "Either the file is absent or the permissions are not properly set.",
		    filepath);
		return NULL;
	}

	U_LOG_I("Parsing the configuration file");
	cJSON *parsed = cJSON_Parse(read);

	free(read);

	if (parsed == NULL) {
		U_LOG_E("Failed to parse the config in %s", filepath);

		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) {
			U_LOG_E("because of an error before %s", error_ptr);
		}
	}

	free(filepath);

	return parsed;
}

int
robocoop_save_device_config(enum robocoop_device device, cJSON *config)
{
	char *filepath = get_device_config_path(device);
	if (filepath == NULL)
		return 1;

	FILE *file = fopen(filepath, "wc");

	free(filepath);

	if (file == NULL) {
		return errno;
	}

	char *encoded_config = cJSON_Print(config);
	ssize_t len = strlen(encoded_config);

	fwrite(encoded_config, sizeof(char), len, file);

	free(encoded_config);
	fclose(file);

	return 0;
}
