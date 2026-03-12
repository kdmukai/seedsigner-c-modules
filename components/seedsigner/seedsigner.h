#ifndef SEEDSIGNER_H
#define SEEDSIGNER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens
void demo_screen(void *ctx);
void button_list_screen(void *ctx_json);
void main_menu_screen(void *ctx);
void screensaver_screen(void *ctx_json);

// misc
void lv_seedsigner_screen_close(void);


#ifdef __cplusplus
}
#endif

#endif // SEEDSIGNER_H
