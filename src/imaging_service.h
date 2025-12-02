/*
 * Copyright (c) 2025 Thingino
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

#ifndef IMAGING_SERVICE_H
#define IMAGING_SERVICE_H

int imaging_get_service_capabilities(void);
int imaging_get_imaging_settings(void);
int imaging_get_options(void);
int imaging_set_imaging_settings(void);
int imaging_move(void);
int imaging_get_move_options(void);
int imaging_stop(void);
int imaging_get_status(void);
int imaging_get_presets(void);
int imaging_get_current_preset(void);
int imaging_set_current_preset(void);
int imaging_unsupported(const char *method);

#endif // IMAGING_SERVICE_H
