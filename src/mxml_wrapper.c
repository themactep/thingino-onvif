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

#include "mxml_wrapper.h"

#include "log.h"
#include "string.h"
#include "utils.h"

// In mxml v4, mxmlLoadString returns a document node, not the root element
// We need to keep track of both for proper memory management
static mxml_node_t *doc_xml = NULL; // Document node (for cleanup)
mxml_node_t *root_xml = NULL;       // Root element (Envelope)

/**
 * Init xml parser
 * @param buffer The buffer containing the xml file
 * @param buffer_size The size of the buffer
 */
void init_xml(char *buffer, int buffer_size)
{
    log_debug("init_xml: buffer=%p, size=%d", buffer, buffer_size);
    if (!buffer) {
        log_error("init_xml: NULL buffer provided");
        return;
    }

    // Log first 200 characters of XML for debugging
    int log_len = (buffer_size > 200) ? 200 : buffer_size;
    log_debug("init_xml: XML content (first %d chars): %.200s", log_len, buffer);

    doc_xml = mxmlLoadString(NULL, NULL, buffer);
    if (!doc_xml) {
        log_error("Failed to parse XML string - mxmlLoadString returned NULL");
        log_error("Buffer size: %d, Buffer content: %.100s", buffer_size, buffer);
        root_xml = NULL;
        return;
    }

    mxml_type_t doc_type = mxmlGetType(doc_xml);
    log_debug("init_xml: doc_xml=%p, type=%d", doc_xml, doc_type);

    // Check if mxmlLoadString returned an element directly (older behavior)
    // or a document node (newer behavior)
    if (doc_type == MXML_TYPE_ELEMENT) {
        // mxmlLoadString returned the root element directly
        root_xml = doc_xml;
        doc_xml = NULL; // Don't try to delete it separately
        log_debug("init_xml: mxmlLoadString returned element directly: %s", mxmlGetElement(root_xml));
    } else {
        // mxmlLoadString returned a document node, find the first element child (the Envelope)
        root_xml = mxmlGetFirstChild(doc_xml);
        log_debug("init_xml: First child=%p, type=%d", root_xml, root_xml ? mxmlGetType(root_xml) : -1);

        while (root_xml && mxmlGetType(root_xml) != MXML_TYPE_ELEMENT) {
            root_xml = mxmlGetNextSibling(root_xml);
            log_debug("init_xml: Next sibling=%p, type=%d", root_xml, root_xml ? mxmlGetType(root_xml) : -1);
        }
    }

    if (!root_xml) {
        log_error("Failed to find root element in XML document");
        if (doc_xml) {
            mxmlDelete(doc_xml);
            doc_xml = NULL;
        }
    } else {
        const char *element_name = mxmlGetElement(root_xml);
        log_debug("XML parsed successfully, root_xml=%p (element: %s)", root_xml, element_name ? element_name : "NULL");

        // Debug: print parent information
        mxml_node_t *parent = mxmlGetParent(root_xml);
        if (parent) {
            log_debug("init_xml: root_xml has parent=%p, parent type=%d", parent, mxmlGetType(parent));
            if (mxmlGetType(parent) == MXML_TYPE_ELEMENT) {
                log_debug("init_xml: parent element name=%s", mxmlGetElement(parent));
            }
        } else {
            log_debug("init_xml: root_xml has no parent (is truly root)");
        }

        // Verify this is actually the Envelope element
        if (element_name) {
            int len = strlen(element_name);
            // Check if element name ends with "Envelope" (handles namespaced names like "v:Envelope", "SOAP-ENV:Envelope", etc.)
            if (len < 8 || strcmp(&element_name[len - 8], "Envelope") != 0) {
                log_warn("Root element is '%s', not Envelope - this may indicate a parsing issue", element_name);

                // If we have a parent, check if the parent is the Envelope
                if (parent && mxmlGetType(parent) == MXML_TYPE_ELEMENT) {
                    const char *parent_name = mxmlGetElement(parent);
                    if (parent_name) {
                        int parent_len = strlen(parent_name);
                        if (parent_len >= 8 && strcmp(&parent_name[parent_len - 8], "Envelope") == 0) {
                            log_debug("Parent is Envelope, correcting root_xml from %s to %s", element_name, parent_name);
                            root_xml = parent;
                            element_name = parent_name;
                            len = parent_len;
                        }
                    }
                }

                // If still not Envelope, try to find it
                if (len < 8 || strcmp(&element_name[len - 8], "Envelope") != 0) {
                    // Try to find Envelope element explicitly
                    mxml_node_t *search_root = doc_xml ? doc_xml : root_xml;
                    mxml_node_t *envelope = mxmlFindElement(search_root, search_root, "Envelope", NULL, NULL, MXML_DESCEND_ALL);
                    if (!envelope) {
                        // Try with namespace suffix matching
                        mxml_node_t *node = mxmlFindElement(search_root, search_root, NULL, NULL, NULL, MXML_DESCEND_ALL);
                        while (node) {
                            if (mxmlGetType(node) == MXML_TYPE_ELEMENT) {
                                const char *node_name = mxmlGetElement(node);
                                if (node_name) {
                                    int node_len = strlen(node_name);
                                    if (node_len >= 8 && strcmp(&node_name[node_len - 8], "Envelope") == 0) {
                                        envelope = node;
                                        log_debug("Found Envelope element with namespace: %s", node_name);
                                        break;
                                    }
                                }
                            }
                            node = mxmlWalkNext(node, search_root, MXML_DESCEND_ALL);
                        }
                    }

                    if (envelope) {
                        log_debug("Corrected root_xml to Envelope element: %s", mxmlGetElement(envelope));
                        root_xml = envelope;
                    } else {
                        log_error("Could not find Envelope element in XML document");
                    }
                }
            }
        }
    }
}

/**
 * Init xml parser from file
 * @param file The name of the xml file
 */
void init_xml_from_file(char *file)
{
    root_xml = mxmlLoadFilename(NULL, NULL, file);
    if (!root_xml) {
        log_error("Failed to parse XML file: %s", file);
    }
}

/**
 * Close xml parser
 */
void close_xml()
{
    // Delete the document node, which will also delete all children including root_xml
    if (doc_xml) {
        mxmlDelete(doc_xml);
        doc_xml = NULL;
        root_xml = NULL;
    } else if (root_xml) {
        // If doc_xml is NULL but root_xml is not, it means mxmlLoadString returned
        // the element directly, so we need to delete root_xml
        mxmlDelete(root_xml);
        root_xml = NULL;
    }
}

/**
 * Get method name from SOAP request
 * @param skip_prefix Skip namespace prefix if non-zero
 * @return Method name or NULL if not found
 */
const char *get_method(int skip_prefix)
{
    log_debug("get_method: skip_prefix=%d, root_xml=%p", skip_prefix, root_xml);

    if (!root_xml) {
        log_error("get_method: root_xml is NULL - XML not initialized or parsing failed");
        return NULL;
    }

    mxml_node_t *body = NULL;

    // First: Check if "Body" element exists
    body = mxmlFindElement(root_xml, root_xml, "Body", NULL, NULL, MXML_DESCEND_ALL);

    if (body && mxmlGetFirstChild(body)) {
        mxml_node_t *method_node = mxmlGetFirstChild(body);
        while (method_node && mxmlGetType(method_node) != MXML_TYPE_ELEMENT) {
            method_node = mxmlGetNextSibling(method_node);
        }

        if (method_node) {
            const char *method_name = mxmlGetElement(method_node);
            if (method_name) {
                log_debug("get_method: Found method in Body: %s", method_name);
                if (skip_prefix) {
                    const char *colon = strchr(method_name, ':');
                    if (colon) {
                        return colon + 1;
                    } else {
                        return method_name;
                    }
                } else {
                    return method_name;
                }
            }
        }
    }

    // Second: Check if "something:Body" element exists (namespace suffix matching)
    // Search all elements in the tree for ones ending with ":Body"
    mxml_node_t *node = mxmlFindElement(root_xml, root_xml, NULL, NULL, NULL, MXML_DESCEND_ALL);
    while (node) {
        if (mxmlGetType(node) == MXML_TYPE_ELEMENT) {
            const char *element_name = mxmlGetElement(node);
            if (element_name) {
                int len = strlen(element_name);
                // Check if element name ends with ":Body"
                if (len >= 5 && strcmp(&element_name[len - 5], ":Body") == 0) {
                    body = node;
                    log_debug("get_method: Found Body element with namespace: %s", element_name);
                    break;
                }
            }
        }
        node = mxmlWalkNext(node, root_xml, MXML_DESCEND_ALL);
    }

    if (body && mxmlGetFirstChild(body)) {
        mxml_node_t *method_node = mxmlGetFirstChild(body);
        while (method_node && mxmlGetType(method_node) != MXML_TYPE_ELEMENT) {
            method_node = mxmlGetNextSibling(method_node);
        }

        if (method_node) {
            const char *method_name = mxmlGetElement(method_node);
            if (method_name) {
                log_debug("get_method: Found method in namespaced Body: %s", method_name);
                if (skip_prefix) {
                    const char *colon = strchr(method_name, ':');
                    if (colon) {
                        return colon + 1;
                    } else {
                        return method_name;
                    }
                } else {
                    return method_name;
                }
            }
        }
    }

    log_error("get_method: Could not find Body element or method");
    return NULL;
}

/**
 * Get element text content
 * @param name Element name to find
 * @param first_node Starting node name (e.g., "Body")
 * @return Element text content or NULL if not found
 */
// Recursive helper function
static const char *get_element_rec_mxml(mxml_node_t *xml, char *name, char *first_node, int *go_to_parent)
{
    if (*go_to_parent)
        return NULL;

    // If it's the root of the document choose the first node ("Header" or "Body")
    if (!mxmlGetParent(xml)) {
        mxml_node_t *px = mxmlGetFirstChild(xml);
        while (px) {
            if (mxmlGetType(px) == MXML_TYPE_ELEMENT) {
                const char *px_name = mxmlGetElement(px);
                if (px_name) {
                    int px_len = strlen(px_name);
                    int first_len = strlen(first_node);
                    // Check suffix match: "Header" matches "v:Header", "soap:Header", etc.
                    if (px_len >= first_len && strcmp(first_node, &px_name[px_len - first_len]) == 0) {
                        const char *ret = get_element_rec_mxml(px, name, first_node, go_to_parent);
                        return ret;
                    }
                }
            }
            px = mxmlGetNextSibling(px);
        }
        // 1st node not found, exit
    } else {
        // Check if this node is "<name>"
        const char *xml_name = mxmlGetElement(xml);
        if (xml_name && strcmp(name, xml_name) == 0) {
            // Get text content
            mxml_node_t *text_node = mxmlGetFirstChild(xml);
            while (text_node && mxmlGetType(text_node) != MXML_TYPE_TEXT) {
                text_node = mxmlGetNextSibling(text_node);
            }
            if (text_node) {
                return mxmlGetText(text_node, NULL);
            }
            return "";
        }

        // Check if this node is "<something:name>" (namespace suffix matching)
        if (xml_name) {
            int xml_len = strlen(xml_name);
            int name_len = strlen(name);
            if (xml_len >= name_len + 1 && strcmp(name, &xml_name[xml_len - name_len]) == 0 && xml_name[xml_len - name_len - 1] == ':') {
                // Get text content
                mxml_node_t *text_node = mxmlGetFirstChild(xml);
                while (text_node && mxmlGetType(text_node) != MXML_TYPE_TEXT) {
                    text_node = mxmlGetNextSibling(text_node);
                }
                if (text_node) {
                    return mxmlGetText(text_node, NULL);
                }
                return "";
            }
        }

        // Check children
        mxml_node_t *child = mxmlGetFirstChild(xml);
        if (child) {
            const char *ret = get_element_rec_mxml(child, name, first_node, go_to_parent);
            *go_to_parent = 0;
            if (ret != NULL)
                return ret;
        }

        // Check brothers (siblings)
        mxml_node_t *pk = mxmlGetNextSibling(xml);
        if (pk) {
            const char *ret = get_element_rec_mxml(pk, name, first_node, go_to_parent);
            if (*go_to_parent)
                return NULL;
            if (ret != NULL)
                return ret;
        }

        *go_to_parent = 1;
    }

    return NULL;
}

const char *get_element(char *name, char *first_node)
{
    int go_to_parent = 0;
    const char *ret = get_element_rec_mxml(root_xml, name, first_node, &go_to_parent);
    return ret;
}

/**
 * Get element pointer
 * @param start_from Starting node (NULL to start from root)
 * @param name Element name to find
 * @param first_node Starting node name (e.g., "Body")
 * @return Element pointer or NULL if not found
 */
mxml_node_t *get_element_ptr(mxml_node_t *start_from, char *name, char *first_node)
{
    if (!root_xml) {
        return NULL;
    }

    mxml_node_t *search_root = start_from ? start_from : root_xml;

    // If first_node is specified, find it first using suffix matching
    if (first_node) {
        mxml_node_t *first = NULL;

        // Try exact match first
        first = mxmlFindElement(search_root, search_root, first_node, NULL, NULL, MXML_DESCEND_ALL);

        // If not found, try namespace suffix matching
        if (!first) {
            mxml_node_t *node = mxmlFindElement(search_root, search_root, NULL, NULL, NULL, MXML_DESCEND_ALL);
            while (node) {
                if (mxmlGetType(node) == MXML_TYPE_ELEMENT) {
                    const char *element_name = mxmlGetElement(node);
                    if (element_name) {
                        int len = strlen(element_name);
                        int first_len = strlen(first_node);
                        // Check if element name ends with first_node (suffix matching)
                        if (len >= first_len && strcmp(&element_name[len - first_len], first_node) == 0) {
                            first = node;
                            break;
                        }
                    }
                }
                node = mxmlWalkNext(node, search_root, MXML_DESCEND_ALL);
            }
        }

        if (!first) {
            return NULL;
        }
        search_root = first;
    }

    // Find the target element with exact match first
    mxml_node_t *target = mxmlFindElement(search_root, search_root, name, NULL, NULL, MXML_DESCEND_ALL);

    // If not found with exact match, try namespace suffix matching
    if (!target) {
        mxml_node_t *node = mxmlFindElement(search_root, search_root, NULL, NULL, NULL, MXML_DESCEND_ALL);
        while (node) {
            if (mxmlGetType(node) == MXML_TYPE_ELEMENT) {
                const char *element_name = mxmlGetElement(node);
                if (element_name) {
                    // Check if this node is "<name>"
                    if (strcmp(name, element_name) == 0) {
                        target = node;
                        break;
                    }

                    // Check if this node is "<something:name>" (namespace suffix matching)
                    int len = strlen(element_name);
                    int name_len = strlen(name);
                    if (len >= name_len + 1 && strcmp(&element_name[len - name_len], name) == 0 && element_name[len - name_len - 1] == ':') {
                        target = node;
                        break;
                    }
                }
            }
            node = mxmlWalkNext(node, search_root, MXML_DESCEND_ALL);
        }
    }

    return target;
}

/**
 * Get element text content within another element
 * @param name Element name to find
 * @param father Parent element
 * @return Element text content or NULL if not found
 */
const char *get_element_in_element(const char *name, mxml_node_t *father)
{
    mxml_node_t *node = get_element_in_element_ptr(name, father);
    if (!node) {
        return NULL;
    }

    // Get the text content of the element
    mxml_node_t *text_node = mxmlGetFirstChild(node);
    while (text_node && mxmlGetType(text_node) != MXML_TYPE_TEXT) {
        text_node = mxmlGetNextSibling(text_node);
    }

    if (text_node) {
        return mxmlGetText(text_node, NULL);
    }

    return NULL;
}

/**
 * Get element pointer within another element
 * @param name Element name to find
 * @param father Parent element
 * @return Element pointer or NULL if not found
 */
mxml_node_t *get_element_in_element_ptr(const char *name, mxml_node_t *father)
{
    if (!father) {
        return NULL;
    }

    mxml_node_t *child = mxmlGetFirstChild(father);

    while (child) {
        if (mxmlGetType(child) == MXML_TYPE_ELEMENT) {
            const char *element_name = mxmlGetElement(child);
            if (element_name) {
                // Check if this node is "<name>"
                if (strcmp(name, element_name) == 0) {
                    return child;
                }

                // Check if this node is "<something:name>" (namespace suffix matching)
                int len = strlen(element_name);
                int name_len = strlen(name);
                if (len >= name_len + 1 && strcmp(&element_name[len - name_len], name) == 0 && element_name[len - name_len - 1] == ':') {
                    return child;
                }
            }
        }
        child = mxmlGetNextSibling(child);
    }

    return NULL;
}

/**
 * Get attribute value from element
 * @param node Element node
 * @param name Attribute name
 * @return Attribute value or NULL if not found
 */
const char *get_attribute(mxml_node_t *node, char *name)
{
    if (!node || !name) {
        return NULL;
    }

    return mxmlElementGetAttr(node, name);
}
