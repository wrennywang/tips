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
    gst_pad_get_caps|gst_pad_get_current_caps
    new-decoded-pad|pad-added
    
  3. 相关内容修改
  - 尝试在new_decoded_pad_cb中，当获取到视频时，调用gst_bin_add，将x264enc,mp4mux,filesink添加至vsinkbin;
  当获取到音频时，将audioconver,audioresample,avenc_aac,mp4mux添加至asinkbin;
  卡在音频的mp4mux连接上，做过大量的尝试，测试，未能找到解决办法；
  
  - 读源码，分析gst_parse_launch处理；
  中间有grammar.y，理解起来有点费劲，直接搜索函数名，看大概调用流程；
  在代码中添加gst_debug_set_default_threshold(4)打开调试输出，分析1中内部调用处理；
  重新编写代码，边分析边添加代码；
  解决！
  
- 代码
> gcc -Wall $(pkg-config --cflags --libs gstreamer-1.0 gtk+-2.0) -g myconv.c -o myconv

```
#include <gst/gst.h>

typedef struct _CustomData {
	GstElement *filesrc;
	GstElement *decodebin;
	GstElement *vqueue;
	GstElement *x264enc;
	GstElement *mp4mux;
	GstElement *mp4file;
	GstElement *aqueue;
	GstElement *prog;
	GstElement *aconv;
	GstElement *aresample;
	GstElement *avenc;
	GstElement *pipeline;
	GMainLoop *loop;
} CustomData;

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
	GMainLoop *loop = data;

	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_EOS:
		g_print("End-of-stream\n");
		g_main_loop_quit(loop);
		break;
	case GST_MESSAGE_ERROR: {
		gchar *debug;
		GError *err;

		gst_message_parse_error(msg, &err, &debug);
		g_free(debug);

		g_print("Error: %s\n", err->message);
		g_error_free(err);

		g_main_loop_quit(loop);
		break;
	}
	default:
		g_print("msg.......\n");
		break;
	}

	return TRUE;
}
static void cb_message(GstBus * bus, GstMessage * msg, CustomData * data)
{
	g_print("cb_message message type %d\n", GST_MESSAGE_TYPE(msg));
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_ERROR: {
		GError *err;
		gchar *debug;

		gst_message_parse_error(msg, &err, &debug);
		g_print("Error: %s\n", err->message);
		g_error_free(err);
		g_free(debug);

		gst_element_set_state(data->pipeline, GST_STATE_READY);
		g_main_loop_quit(data->loop);
		break;
	}
	case GST_MESSAGE_EOS:
		/* end-of-stream */
		gst_element_set_state(data->pipeline, GST_STATE_READY);
		g_main_loop_quit(data->loop);
		break;
	case GST_MESSAGE_BUFFERING: {
		gint percent = 0;

		/* If the stream is live, we do not care about buffering. */
		//if (data->is_live)
		//  break;

		gst_message_parse_buffering(msg, &percent);
		g_print("Buffering (%3d%%)\r", percent);
		/* Wait until buffering is complete before start/resume playing */
		if (percent < 100)
			gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
		else
			gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
		break;
	}
	case GST_MESSAGE_CLOCK_LOST:
		/* Get a new clock */
		gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
		gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
		break;
	default:
		/* Unhandled message */
		break;
	}
}

static void new_decoded_pad_cb(GstElement *decodebin, GstPad *new_pad, CustomData *user_data)
{
	GstCaps *caps;
	gchar *str;
	GstPadLinkReturn ret;
	caps = gst_pad_get_current_caps(new_pad);
	str = gst_caps_to_string(caps);
	if (g_str_has_prefix(str, "video/"))
	{
		GstPad *vpad = gst_element_get_static_pad(user_data->vqueue, "sink");
		ret = gst_pad_link(new_pad, vpad);
	}
	else
	{
		GstPad *apad = gst_element_get_static_pad(user_data->aqueue, "sink");
		ret = gst_pad_link(new_pad, apad);
	}

	g_print("new_decoded_pad_cb %s \nret %d \n", str, ret);

	// 下面一行必须注释掉，否则会卡住
	// gst_element_set_state(user_data->pipeline, GST_STATE_PAUSED);
}

int main(int argc, char *argv[])
{
	CustomData data;
	GstBus *bus;
	GstMessage *msg;

	// 打印输出控制
	// gst_debug_set_default_threshold(4);
	char str[200];
	memset(str, 0, 200);

	/* Initialize GStreamer */
	gst_init(&argc, &argv);

	/* Build the pipeline */
	/* pipeline = gst_parse_launch("gst-launch-1.0 filesrc location=sintel_trailer-480p.webm ! decodebin name=demux
		! queue ! x264enc ! mp4mux name=mux ! filesink location=1.mp4 demux.
		! queue ! progressreport ! audioconvert ! audioresample ! avenc_aac ! mux.", NULL);*/

	data.filesrc = gst_element_factory_make("filesrc", "filesrc");
	g_object_set(G_OBJECT(data.filesrc), "location", argv[1], NULL);
	data.decodebin = gst_element_factory_make("decodebin", "decode bin");
	data.vqueue = gst_element_factory_make("queue", "video queue");
	data.x264enc = gst_element_factory_make("x264enc", "x264 encode");
	data.mp4mux = gst_element_factory_make("mp4mux", "mp4 mux");
	data.mp4file = gst_element_factory_make("filesink", "mp4 file");
	g_object_set(data.mp4file, "location", "1.mp4", NULL);
	data.aqueue = gst_element_factory_make("queue", "audio queue");
	data.prog = gst_element_factory_make("progressreport", "progress report");
	data.aconv = gst_element_factory_make("audioconvert", "audio convert");
	data.aresample = gst_element_factory_make("audioresample", "audio resample");
	data.avenc = gst_element_factory_make("avenc_aac", "aac encode");
	GstElement *pipeline = gst_pipeline_new(NULL);
	data.pipeline = pipeline;

	gst_bin_add_many(GST_BIN(pipeline), data.filesrc, data.decodebin, data.vqueue, data.x264enc,
		data.mp4mux, data.mp4file, data.aqueue, data.prog, data.aconv, data.aresample, data.avenc, NULL);
	gst_element_link_pads(data.filesrc, "src", data.decodebin, "sink");

	/* video link */
	// 首先尝试vqueue/aqueue直接连接至decodebin；看打印输出连接失败，
	// 再对照gst_parse_launch输出，在此链接时有trying delayed linking xxx,
	// 故通过signal, pad-added，进行连接；
	// gst_element_link_pads(decodebin, "src_%u", vqueue, "sink");

	// vqueue中element连接，先尝试一个一个连接，OK；然后再尝试一次连接，OK！
	gst_element_link_many(data.vqueue, data.x264enc, data.mp4mux, data.mp4file, NULL);
	// gst_element_link_pads(data.vqueue, "src", data.x264enc, "sink");
	// gst_element_link_pads(data.x264enc, "src", data.mp4mux, "video_%u");
	// gst_element_link_pads(data.mp4mux, "src", data.mp4file, "sink");

	/* audio link */
	// gst_element_link_pads(decodebin, "src_%u", aqueue, "sink");
	gst_element_link_many(data.aqueue, data.prog, data.aconv, data.aresample, data.avenc, data.mp4mux, NULL);
	// gst_element_link_pads(data.aqueue, "src", data.prog, "sink");
	// gst_element_link_pads(data.prog, "src", data.aconv, "sink");
	// gst_element_link_pads(data.aconv, "src", data.aresample, "sink");
	// gst_element_link_pads(data.aresample, "src", data.avenc, "sink");
	// gst_element_link_pads(data.avenc, "src", data.mp4mux, "audio_%u");

	g_signal_connect(data.decodebin, "pad-added", G_CALLBACK(new_decoded_pad_cb), &data);

	GMainLoop *loop;
	loop = g_main_loop_new(NULL, FALSE);
	data.loop = loop;
	bus = gst_element_get_bus(pipeline);

	// 测试gst_bus_add_watch/gst_bus_add_signal_watch调用区别；都没有进入回调函数；
	// gst_bus_add_watch(bus, bus_call, loop);

	gst_bus_add_signal_watch(bus);
	g_signal_connect(bus, "message", G_CALLBACK(cb_message), &data);

	/* Start playing */
	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	/* Wait until error or EOS */
	// bus = gst_element_get_bus (pipeline);
	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
		GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

	/* Free resources */
	if (msg != NULL)
		gst_message_unref(msg);
	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
	return 0;
}
```
