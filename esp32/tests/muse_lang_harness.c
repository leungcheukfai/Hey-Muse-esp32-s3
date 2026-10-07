#include <assert.h>
#include <string.h>

#include "muse_lang.h"

int main(void)
{
    assert(strcmp(muse_lang_get("READY"), "READY") == 0);
    assert(strcmp(muse_lang_get("Language"), "Language") == 0);
    assert(strcmp(muse_lang_chat_instruction(),
                  "Reply in clear, natural English. Keep names and technical terms clear.") == 0);
    assert(strcmp(muse_lang_tts_style(),
                  "Speak naturally in clear English with neutral pronunciation. Read only the provided reply text.") == 0);
    assert(strcmp(muse_lang_get("untranslated"), "untranslated") == 0);
    return 0;
}
