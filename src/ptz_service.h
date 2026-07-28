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

#ifndef PTZ_SERVICE_H
#define PTZ_SERVICE_H

typedef struct {
    int number;
    char *name;
    double x; // pan
    double y; // tilt
    double z; // zoom
} preset_t;

typedef struct {
    int count;
    preset_t *items;
} presets_t;

int ptz_get_service_capabilities();
int ptz_get_configurations();
int ptz_get_configuration();
int ptz_get_configuration_options();
int ptz_get_nodes();
int ptz_get_node();
int ptz_get_presets();
int ptz_goto_preset();
int ptz_goto_home_position();
int ptz_continuous_move();
int ptz_relative_move();
int ptz_absolute_move();
int ptz_stop();
int ptz_get_status();
int ptz_set_preset();
int ptz_set_home_position();
int ptz_remove_preset();
int ptz_send_auxiliary_command();
int ptz_set_configuration();
int ptz_get_compatible_configurations();

int ptz_move_and_start_tracking();
// Preset Tours
int ptz_get_preset_tours();
int ptz_get_preset_tour();
int ptz_get_preset_tour_options();
int ptz_create_preset_tour();
int ptz_modify_preset_tour();
int ptz_operate_preset_tour();
int ptz_remove_preset_tour();

int ptz_unsupported(const char *method);

int ptz_supports_zoom();

// Zoom template sections – passed as replacements for %ZOOM_*% placeholders.
// When ptz_supports_zoom() is false, these expand to empty strings.
#define ZOOM_DEFAULT_SPACES_XML \
    "                <tt:DefaultAbsoluteZoomPositionSpace>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:DefaultAbsoluteZoomPositionSpace>\n" \
    "                <tt:DefaultRelativeZoomTranslationSpace>http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace</tt:DefaultRelativeZoomTranslationSpace>\n" \
    "                <tt:DefaultContinuousZoomVelocitySpace>http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace</tt:DefaultContinuousZoomVelocitySpace>"

#define ZOOM_SPEED_XML \
    "                    <tt:Zoom x=\"1.0\"\n" \
    "                             space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/ZoomGenericSpeedSpace\"/>"

#define ZOOM_LIMITS_XML \
    "                <tt:ZoomLimits>\n" \
    "                    <tt:Range>\n" \
    "                        <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>\n" \
    "                        <tt:XRange>\n" \
    "                            <tt:Min>0.0</tt:Min>\n" \
    "                            <tt:Max>1.0</tt:Max>\n" \
    "                        </tt:XRange>\n" \
    "                    </tt:Range>\n" \
    "                </tt:ZoomLimits>"

#define ZOOM_ABS_SPACE_XML \
    "                    <tt:AbsoluteZoomPositionSpace>\n" \
    "                        <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>\n" \
    "                        <tt:XRange>\n" \
    "                            <tt:Min>0.0</tt:Min>\n" \
    "                            <tt:Max>1.0</tt:Max>\n" \
    "                        </tt:XRange>\n" \
    "                    </tt:AbsoluteZoomPositionSpace>"

#define ZOOM_REL_SPACE_XML \
    "                    <tt:RelativeZoomTranslationSpace>\n" \
    "                        <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace</tt:URI>\n" \
    "                        <tt:XRange>\n" \
    "                            <tt:Min>-1.0</tt:Min>\n" \
    "                            <tt:Max>1.0</tt:Max>\n" \
    "                        </tt:XRange>\n" \
    "                    </tt:RelativeZoomTranslationSpace>"

#define ZOOM_VEL_SPACE_XML \
    "                    <tt:ContinuousZoomVelocitySpace>\n" \
    "                        <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace</tt:URI>\n" \
    "                        <tt:XRange>\n" \
    "                            <tt:Min>-1.0</tt:Min>\n" \
    "                            <tt:Max>1.0</tt:Max>\n" \
    "                        </tt:XRange>\n" \
    "                    </tt:ContinuousZoomVelocitySpace>"

#define ZOOM_SPEED_SPACE_XML \
    "                    <tt:ZoomSpeedSpace>\n" \
    "                        <tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/ZoomGenericSpeedSpace</tt:URI>\n" \
    "                        <tt:XRange>\n" \
    "                            <tt:Min>0.0</tt:Min>\n" \
    "                            <tt:Max>1.0</tt:Max>\n" \
    "                        </tt:XRange>\n" \
    "                    </tt:ZoomSpeedSpace>"

#endif //PTZ_SERVICE_H
