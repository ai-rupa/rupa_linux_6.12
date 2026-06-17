#ifndef __LED_TEST_H_
#define __LED_TEST_H_

#define AUDIO_PLAY_FILE "audio_test_start.wav"
#define AUDIO_RECORD_FILE "record_test.pcm"

void *audio_test(void *argv);        //Recording and Playback Test Procedure
void *audio_play_test(void *argv);   //Audio Playback Test
void *audio_record_test(void *argv); //Recording Test

#define AUDIO_LINEIN_FILE "linein_test.pcm"
void *audio_line_in_test(void *argv); //Line in Test

#endif
