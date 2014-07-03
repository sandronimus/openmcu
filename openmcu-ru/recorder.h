
#ifndef _MCU_RECORDER_H
#define _MCU_RECORDER_H

extern "C"
{
  #include "libavcodec/avcodec.h"
  #include "libavformat/avformat.h"
  #include "libswresample/swresample.h"
  #include "libavutil/opt.h"
  #include "libavutil/mem.h"
  #include "libavutil/avutil.h"
}

#include "config.h"
#include "conference.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

static struct recorder_resolution {
  unsigned width;
  unsigned height;
  unsigned macroblocks;
} const recorder_resolutions[] = {
  { 176,  144,  99   },
  { 320,  240,  300  },
  { 352,  288,  396  },
  { 640,  360,  900  },
  { 640,  480,  1200 },
  { 704,  576,  1584 },
  { 854,  480,  1601 },
  { 800,  600,  1900 },
  { 1024, 768,  3072 },
  { 1280, 720,  3600 },
  { 1366, 768,  4098 },
  { 1280, 1024, 5120 },
  { 1408, 1152, 6336 },
  { 1920, 1080, 8100 },
  { 0 }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class ConferenceRecorder : public ConferenceMember
{
  PCLASSINFO(ConferenceRecorder, ConferenceMember);

  public:
    ConferenceRecorder(Conference *_conference);
    ~ConferenceRecorder();

    virtual ConferenceConnection * CreateConnection()
    { return new ConferenceConnection(this); }

    virtual PString GetName() const
    { return "conference recorder"; }

    virtual MemberTypes GetType()
    { return MEMBER_TYPE_RECORDER; }

    virtual PTime GetStartTime() const
    { return (running ? startTime : PTime()); }

    virtual BOOL IsVisible() const
    { return FALSE; }

    BOOL IsRunning()
    { return running; }

    BOOL Start();
    void Stop();

  protected:

    BOOL running;

    PString filename;
    PString format_name;
    PString trace_section;

    unsigned audio_bitrate;
    unsigned audio_samplerate;
    unsigned audio_channels;

    unsigned video_bitrate;
    unsigned video_width;
    unsigned video_height;
    unsigned video_framerate;

    struct SwrContext *swr_ctx;
    int sws_flags;

    uint8_t **src_samples_data;
    int src_samples;
    int src_samples_size;

    uint8_t **dst_samples_data;
    int dst_samples;
    int dst_samples_size;

    uint8_t *video_framebuf;
    uint8_t *video_outbuf;
    int video_framebuf_size;
    int video_outbuf_size;

    AVFrame *audio_frame;
    AVFrame *video_frame;

    AVStream *audio_st;
    AVStream *video_st;

    AVFormatContext *fmt_context;

    void Reset();
    BOOL InitRecorder();

    AVStream *AddStream(AVMediaType codec_type);

    BOOL OpenAudio();
    BOOL GetAudioFrame();
    BOOL WriteAudioFrame();

    BOOL OpenVideo();
    BOOL GetVideoFrame();
    BOOL WriteVideoFrame();

    BOOL OpenResampler();
    BOOL Resampler();
    int WriteFrame(AVStream *st, AVPacket *pkt);

    PThread *thread;
    PDECLARE_NOTIFIER(PThread, ConferenceRecorder, Recorder);

    PMutex mutex;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // _MCU_RECORDER_H
