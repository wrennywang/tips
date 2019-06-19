# **webm2mp4 shell cmd**
> gst-launch-1.0 filesrc location=sintel_trailer-480p.webm ! decodebin name=demux ! queue ! x264enc ! mp4mux name=mux ! filesink location=1.mp4 demux. ! queue ! progressreport ! audioconvert ! audioresample ! avenc_aac ! mux.

# **c code**
## 1.gst_parse_launch
参考gst-docs-master中examples/tutorials/basic-tutorial-12.c中gst_parse_launch函数中第一个参数替换成上面的shell cmd

```
#include <gst/gst.h>
#include <string.h>

typedef struct _CustomData
{
  gboolean is_live;
  GstElement *pipeline;
  GMainLoop *loop;
} CustomData;

static void
cb_message (GstBus * bus, GstMessage * msg, CustomData * data)
{

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (msg, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_main_loop_quit (data->loop);
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      gst_element_set_state (data->pipeline, GST_STATE_READY);
      g_main_loop_quit (data->loop);
      break;
    case GST_MESSAGE_BUFFERING:{
      gint percent = 0;

      /* If the stream is live, we do not care about buffering. */
      if (data->is_live)
        break;

      gst_message_parse_buffering (msg, &percent);
      g_print ("Buffering (%3d%%)\r", percent);
      /* Wait until buffering is complete before start/resume playing */
      if (percent < 100)
        gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
      else
        gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
      break;
    }
    case GST_MESSAGE_CLOCK_LOST:
      /* Get a new clock */
      gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
      gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
      break;
    default:
      /* Unhandled message */
      break;
  }
}

int
main (int argc, char *argv[])
{
  GstElement *pipeline;
  GstBus *bus;
  GstStateChangeReturn ret;
  GMainLoop *main_loop;
  CustomData data;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Initialize our data structure */
  memset (&data, 0, sizeof (data));

  /* Build the pipeline */
  pipeline =
      gst_parse_launch
      ("playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm",
      NULL);
  bus = gst_element_get_bus (pipeline);

  /* Start playing */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    return -1;
  } else if (ret == GST_STATE_CHANGE_NO_PREROLL) {
    data.is_live = TRUE;
  }

  main_loop = g_main_loop_new (NULL, FALSE);
  data.loop = main_loop;
  data.pipeline = pipeline;

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (cb_message), &data);

  g_main_loop_run (main_loop);

  /* Free resources */
  g_main_loop_unref (main_loop);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}

```
## 2.no gst_parse_launch
- 官网查询并网上搜索decodebin,queue,mp4mux相关资料
- 参照[gst_vis](http://noraisin.net/gst/gst_vis.c)进行改写
  1. 编译命令修改，gstreamer-0.10改为gstreamer-1.0，另外在ubuntu下需要调整顺序
  > gcc -g -Wall `pkg-config --cflags --libs gtk+-2.0 gstreamer-0.10` -o gst_vis gst_vis.c
  2. 版本相关导致的调用函数修改
  gstreamer-0.10 | gstreamer-1.0
  ---|---:
    gst_element_get_pad|gst_element_get_static_pad
- 
