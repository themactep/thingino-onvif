/*
 * Copyright (c) 2025 Thingino
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MXML_WRAPPER_H
#define MXML_WRAPPER_H

#include <mxml.h>

void init_xml(char* buffer, int buffer_size);
void close_xml();
const char* get_method(int skip_prefix);
const char* get_element(char* name, char* first_node);
mxml_node_t* get_element_ptr(mxml_node_t* start_from, char* name, char* first_node);
const char* get_element_in_element(const char* name, mxml_node_t* father);
mxml_node_t* get_element_in_element_ptr(const char* name, mxml_node_t* father);
const char* get_attribute(mxml_node_t* node, char* name);

#endif //MXML_WRAPPER_H
