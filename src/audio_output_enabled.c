#include <json_config.h>
#include <stdio.h>
#include <stdlib.h>

// Returns 1 if audio.output_enabled is true, 0 if false or not found
int is_audio_output_enabled(const char *filename)
{
    JsonValue *root = load_config(filename);
    if (!root)
        return 0;
    JsonValue *audio = get_object_item(root, "audio");
    if (!audio || audio->type != JSON_OBJECT) {
        free_json_value(root);
        return 0;
    }
    JsonValue *enabled = get_object_item(audio, "output_enabled");
    int result = 0;
    if (enabled && enabled->type == JSON_BOOL) {
        result = enabled->value.boolean ? 1 : 0;
    }
    free_json_value(root);
    return result;
}
