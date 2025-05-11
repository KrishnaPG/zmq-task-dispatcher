#include "gstreamer_executor.hpp"
#include <iostream>
#include <glib.h>

// Define sample pipelines
const std::string GStreamerPipelineExecutor::AUDIO_TEST_PIPELINE = 
    "audiotestsrc wave=white-noise ! audioconvert ! autoaudiosink";

const std::string GStreamerPipelineExecutor::VIDEO_TEST_PIPELINE = 
    "videotestsrc pattern=smpte ! videoconvert ! autovideosink";

const std::string GStreamerPipelineExecutor::AUDIO_VIDEO_TEST_PIPELINE = 
    "videotestsrc pattern=smpte ! videoconvert ! autovideosink "
    "audiotestsrc wave=sine ! audioconvert ! autoaudiosink";

GStreamerPipelineExecutor::GStreamerPipelineExecutor(size_t thread_count) 
    : m_thread_pool(thread_count) {
    // Initialize GStreamer
    gst_init(nullptr, nullptr);
}

GStreamerPipelineExecutor::~GStreamerPipelineExecutor() {
    stop_all_pipelines();
}

void GStreamerPipelineExecutor::execute_pipeline(const std::string& pipeline_config, 
                                               PipelineCallback callback,
                                               const std::string& pipeline_id) {
    std::string id = pipeline_id.empty() ? std::to_string(reinterpret_cast<uintptr_t>(this)) : pipeline_id;

    {
        std::lock_guard<std::mutex> lock(m_pipeline_mutex);
        if (m_pipelines.find(id) != m_pipelines.end()) {
            std::cerr << "Pipeline with ID " << id << " already exists\n";
            return;
        }
    }

    m_thread_pool.push_task([this, pipeline_config, callback, id] {
        GError* error = nullptr;
        GstElement* pipeline = gst_parse_launch(pipeline_config.c_str(), &error);

        if (error) {
            std::cerr << "Failed to create pipeline: " << error->message << "\n";
            g_error_free(error);
            return;
        }

        auto pipeline_data = std::make_unique<PipelineData>();
        pipeline_data->pipeline = pipeline;
        pipeline_data->running = true;
        pipeline_data->callback = callback;

        // Set up bus callback
        GstBus* bus = gst_element_get_bus(pipeline);
        pipeline_data->bus_watch_id = gst_bus_add_watch(bus, bus_callback, pipeline_data.get());
        gst_object_unref(bus);

        {
            std::lock_guard<std::mutex> lock(m_pipeline_mutex);
            m_pipelines[id] = std::move(pipeline_data);
        }

        // Start pipeline
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Failed to start pipeline\n";
            cleanup_pipeline(id);
            return;
        }

        // Run main loop for this pipeline
        GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
        while (m_pipelines[id]->running) {
            g_main_context_iteration(g_main_loop_get_context(loop), FALSE);
            std::this_thread::yield(); // Avoid busy waiting
        }
        g_main_loop_unref(loop);
    });
}

void GStreamerPipelineExecutor::stop_pipeline(const std::string& pipeline_id) {
    std::lock_guard<std::mutex> lock(m_pipeline_mutex);
    auto it = m_pipelines.find(pipeline_id);
    if (it != m_pipelines.end()) {
        it->second->running = false;
    }
}

void GStreamerPipelineExecutor::stop_all_pipelines() {
    std::lock_guard<std::mutex> lock(m_pipeline_mutex);
    for (auto& [id, pipeline_data] : m_pipelines) {
        pipeline_data->running = false;
    }
}

gboolean GStreamerPipelineExecutor::bus_callback(GstBus* bus, GstMessage* message, gpointer data) {
    auto* pipeline_data = static_cast<PipelineData*>(data);
    
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_EOS:
            pipeline_data->running = false;
            break;
        case GST_MESSAGE_ERROR: {
            gchar* debug;
            GError* error;
            gst_message_parse_error(message, &error, &debug);
            std::cerr << "Error: " << error->message << "\n";
            g_free(debug);
            g_error_free(error);
            pipeline_data->running = false;
            break;
        }
        default:
            break;
    }

    if (pipeline_data->callback) {
        pipeline_data->callback(message);
    }

    return TRUE;
}

void GStreamerPipelineExecutor::cleanup_pipeline(const std::string& pipeline_id) {
    std::lock_guard<std::mutex> lock(m_pipeline_mutex);
    auto it = m_pipelines.find(pipeline_id);
    if (it != m_pipelines.end()) {
        auto* pipeline_data = it->second.get();
        
        if (pipeline_data->bus_watch_id) {
            GstBus* bus = gst_element_get_bus(pipeline_data->pipeline);
            gst_bus_remove_watch(bus);
            gst_object_unref(bus);
        }
        
        if (pipeline_data->pipeline) {
            gst_element_set_state(pipeline_data->pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline_data->pipeline);
        }
        
        m_pipelines.erase(it);
    }
}