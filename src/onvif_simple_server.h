/*
 * Copyright (c) 2024 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ONVIF_SIMPLE_SERVER_H
#define ONVIF_SIMPLE_SERVER_H
#include <stddef.h>

#define DAEMON_NO_CHDIR 01          /* Don't chdir ("/") */
#define DAEMON_NO_CLOSE_FILES 02    /* Don't close all open files */
#define DAEMON_NO_REOPEN_STD_FDS 04 /* Don't reopen stdin, stdout, and stderr to /dev/null */
#define DAEMON_NO_UMASK0 010        /* Don't do a umask(0) */
#define DAEMON_MAX_CLOSE 8192       /* Max file descriptors to close if sysconf(_SC_OPEN_MAX) is indeterminate */

typedef struct {
    int enable;
    const char *username;
    const char *password;
    const char *nonce;
    const char *created;
    int type;
} username_token_t;

typedef enum { VIDEO_NONE, JPEG, MPEG4, H264, H265 } stream_type;

typedef enum { AUDIO_NONE, G711, G726, AAC } audio_type;

typedef enum { IDLE_STATE_CLOSE, IDLE_STATE_OPEN } idle_state;

typedef struct {
    char *name;
    int width;
    int height;
    char *url;
    char *snapurl;
    int type;
    int audio_encoder;
    int audio_decoder;
} stream_profile_t;

typedef struct {
    int idle_state;
    char *close;
    char *open;
} relay_output_t;

typedef struct {
    int enable;
    double min_step_x;
    double max_step_x;
    double min_step_y;
    double max_step_y;
    double min_step_z;
    double max_step_z;
    double pan_min;
    double pan_max;
    double tilt_min;
    double tilt_max;
    double fov_pan;
    double fov_tilt;
    int pan_inverted;
    int tilt_inverted;
    char *get_position;
    char *is_moving;
    char *move_left;
    char *move_right;
    char *move_up;
    char *move_down;
    char *move_in;
    char *move_out;
    char *move_stop;
    char *move_preset;
    char *goto_home_position;
    char *set_preset;
    char *set_home_position;
    char *remove_preset;
    char *jump_to_abs;
    char *jump_to_rel;
    char *get_presets;
    // Optional extensions
    int max_preset_tours;    // 0 means not supported
    char *start_tracking;    // Command to start tracking (for MoveAndStartTracking)
    char *preset_tour_start; // Backend start tour command (token)
    char *preset_tour_stop;  // Backend stop tour command (token)
    char *preset_tour_pause; // Backend pause tour command (token)
    char *jump_to_abs_speed; // Absolute move with speed: fmt(dx,dy,dz,pt_speed,zoom_speed)
    char *jump_to_rel_speed; // Relative move with speed: fmt(dx,dy,dz,pt_speed,zoom_speed)
    char *continuous_move;   // Continuous move with both axes: fmt(x_target, y_target) for diagonal movement
    int reverse_supported;
    int reverse_mode_on;
    int eflip_supported;
    int eflip_mode_on;
} ptz_node_t;

typedef struct {
    char *topic;
    char *source_name;
    char *source_type;
    char *source_value;
    char *input_file;
} event_t;

typedef struct {
    int output_level;
    int output_level_min;
    int output_level_max;
    char *name;
    char *token;
    char *configuration_token;
    char *receive_token;
    char *uri;
    char *transport;
} audio_output_config_t;

typedef struct {
    int output_enabled;
    audio_output_config_t backchannel;
} audio_settings_t;

typedef enum { IRCUT_MODE_UNSPECIFIED = 0, IRCUT_MODE_ON, IRCUT_MODE_OFF, IRCUT_MODE_AUTO } ircut_mode_t;

typedef struct {
    int present;
    float value;
    int has_value;
    float min;
    int has_min;
    float max;
    int has_max;
} imaging_float_value_t;

typedef struct {
    char **items;
    int count;
} imaging_string_list_t;

typedef struct {
    int present;
    char *mode;
    imaging_string_list_t modes;
    imaging_float_value_t level;
} imaging_mode_level_t;

typedef struct {
    int present;
    char *mode;
    imaging_string_list_t modes;
    char *priority;
    imaging_string_list_t priorities;
    imaging_float_value_t min_exposure_time;
    imaging_float_value_t max_exposure_time;
    imaging_float_value_t exposure_time;
    imaging_float_value_t min_gain;
    imaging_float_value_t max_gain;
    imaging_float_value_t gain;
    imaging_float_value_t min_iris;
    imaging_float_value_t max_iris;
    imaging_float_value_t iris;
} imaging_exposure_config_t;

typedef struct {
    int present;
    char *mode;
    imaging_string_list_t modes;
    imaging_float_value_t default_speed;
    imaging_float_value_t near_limit;
    imaging_float_value_t far_limit;
} imaging_focus_config_t;

typedef enum {
    IMAGING_FOCUS_STATE_UNKNOWN = 0,
    IMAGING_FOCUS_STATE_IDLE,
    IMAGING_FOCUS_STATE_MOVING,
} imaging_focus_state_t;

typedef struct {
    int supported;
    char *command;
    imaging_float_value_t position;
    imaging_float_value_t speed;
} imaging_focus_absolute_move_t;

typedef struct {
    int supported;
    char *command;
    imaging_float_value_t distance;
    imaging_float_value_t speed;
} imaging_focus_relative_move_t;

typedef struct {
    int supported;
    char *command;
    imaging_float_value_t speed;
} imaging_focus_continuous_move_t;

typedef struct {
    imaging_focus_absolute_move_t absolute;
    imaging_focus_relative_move_t relative;
    imaging_focus_continuous_move_t continuous;
    char *cmd_stop;
} imaging_focus_move_config_t;

typedef struct {
    char *token;
    char *name;
    char *type;
    char *command;
} imaging_preset_entry_t;

typedef struct {
    int present;
    char *mode;
    imaging_string_list_t modes;
    imaging_float_value_t cr_gain;
    imaging_float_value_t cb_gain;
} imaging_white_balance_config_t;

typedef struct {
    int present;
    char *boundary_type;
    imaging_string_list_t boundary_types;
    imaging_float_value_t boundary_offset;
    imaging_float_value_t response_time;
} imaging_ircut_auto_adjustment_t;

typedef struct {
    char *video_source_token;
    ircut_mode_t ircut_mode;
    int supports_ircut_on;
    int supports_ircut_off;
    int supports_ircut_auto;
    char *cmd_ircut_on;
    char *cmd_ircut_off;
    char *cmd_ircut_auto;

    imaging_mode_level_t backlight;
    imaging_float_value_t brightness;
    imaging_float_value_t color_saturation;
    imaging_float_value_t contrast;
    imaging_float_value_t sharpness;
    imaging_exposure_config_t exposure;
    imaging_focus_config_t focus;
    imaging_mode_level_t wide_dynamic_range;
    imaging_white_balance_config_t white_balance;
    imaging_ircut_auto_adjustment_t ircut_auto_adjustment;
    imaging_mode_level_t image_stabilization;
    imaging_mode_level_t tone_compensation;
    imaging_mode_level_t defogging;
    imaging_float_value_t noise_reduction;
    imaging_focus_move_config_t focus_move;
    imaging_focus_state_t focus_state;
    int focus_has_last_position;
    float focus_last_position;
    imaging_preset_entry_t *presets;
    int preset_count;
    char *cmd_apply_preset;
    char *default_preset_token;
    char *current_preset_token;
} imaging_entry_t;

typedef struct {
    int port;
    char *username;
    char *password;

    //Device Information
    char *manufacturer;
    char *model;
    char *firmware_ver;
    char *serial_num;
    char *hardware_id;

    char *ifs;

    int adv_enable_media2;
    int adv_fault_if_unknown;
    int adv_fault_if_set;
    int adv_synology_nvr;

    stream_profile_t *profiles;
    int profiles_num;

    audio_settings_t audio;

    char **scopes;
    int scopes_num;

    relay_output_t *relay_outputs;
    int relay_outputs_num;
    ptz_node_t ptz_node;
    event_t *events;
    int events_enable;
    int events_num;
    int events_min_interval_ms; // Global debounce for events; 0 disables
    int loglevel;               // Controlled by 'log_level' config; 0=FATAL..5=TRACE, default 0

    // Raw XML logging configuration
    char *raw_log_directory;   // Path to external storage for raw XML logs via 'log_directory'
    int raw_log_on_error_only; // 'log_on_error_only' toggles error-only captures when general logging is disabled

    imaging_entry_t *imaging;
    int imaging_num;
} service_context_t;

// Expose global context
extern service_context_t service_ctx;

// Access to original raw request (pre-parse) for error logging
const char *get_raw_request_data(size_t *out_size);

// returns 0 on success -1 on error
int daemonize(int flags);

#endif //ONVIF_SIMPLE_SERVER_H
