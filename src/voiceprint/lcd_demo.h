#ifndef _DEMO_CAPTURE_
#define _DEMO_CAPTURE_

void lcd_show_init();
void lcd_draw_voiceprint();
bool find_current_capture(int *person, int *num);
void save_current_voiceprint(int person, int num, float *voiceprint);
void lcd_set_warning_message(char *warnmsg);
#endif
