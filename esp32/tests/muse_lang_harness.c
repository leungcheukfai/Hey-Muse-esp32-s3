#include <assert.h>
#include <string.h>

#include "muse_lang.h"

int main(void)
{
    assert(strcmp(muse_lang_get("READY"), "就緒") == 0);
    assert(strcmp(muse_lang_get("Language"), "語言") == 0);
    assert(strcmp(muse_lang_get("Cantonese"), "廣東話") == 0);
    assert(strcmp(muse_lang_get("Mandarin"), "普通話") == 0);
    assert(strstr(muse_lang_chat_instruction(1), "Hong Kong") != NULL);
    assert(strstr(muse_lang_chat_instruction(0), "Mandarin") != NULL);
    assert(strstr(muse_lang_tts_style(1), "廣東話") != NULL);
    assert(strstr(muse_lang_tts_style(0), "普通話") != NULL);
    assert(strcmp(muse_lang_get("untranslated"), "untranslated") == 0);
    return 0;
}
