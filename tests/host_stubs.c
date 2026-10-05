/*
 * Host stubs for the media layer (no LGNC, no audio device): lets the UI and
 * protocol stack run on a development machine.
 */
#include "media.h"

bool media_init(void) { return true; }
bool media_video_open(int width, int height) { (void) width; (void) height; return true; }
bool media_video_feed(const uint8_t *au, size_t len) { (void) au; (void) len; return true; }
void media_video_close(void) {}
bool media_audio_open_pcm(int sample_rate, int channels) { (void) sample_rate; (void) channels; return true; }
bool media_audio_decode(const uint8_t *data, size_t len) { (void) data; (void) len; return true; }
void media_audio_close(void) {}
void media_quit(void) {}
