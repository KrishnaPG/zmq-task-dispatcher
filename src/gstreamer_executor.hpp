#pragma once

#include <gst/gst.h>
#include <memory>
#include <string>
#include <functional>
#include <bs_thread_pool.hpp>
#include <atomic>
#include <unordered_map>

class GStreamerPipelineExecutor {
public:
    using PipelineCallback = std::function<void(GstMessage*)>;
    
    // Pipeline configurations
    static const std::string AUDIO_TEST_PIPELINE;
    static const std::string VIDEO_TEST_PIPELINE;
    static const std::string AUDIO_VIDEO_TEST_PIPELINE;

    GStreamerPipelineExecutor(size_t thread_count = std::thread::hardware_concurrency());
    ~GStreamerPipelineExecutor();

    // Delete copy/move constructors and assignment operators
    GStreamerPipelineExecutor(const GStreamerPipelineExecutor&) = delete;
    GStreamerPipelineExecutor& operator=(const GStreamerPipelineExecutor&) = delete;
    GStreamerPipelineExecutor(GStreamerPipelineExecutor&&) = delete;
    GStreamerPipelineExecutor& operator=(GStreamerPipelineExecutor&&) = delete;

    // Execute a pipeline asynchronously
    void execute_pipeline(const std::string& pipeline_config, 
                         PipelineCallback callback = nullptr,
                         const std::string& pipeline_id = "");

    // Stop a specific pipeline
    void stop_pipeline(const std::string& pipeline_id);

    // Stop all pipelines
    void stop_all_pipelines();

private:
    struct PipelineData {
        GstElement* pipeline = nullptr;
        std::atomic<bool> running{false};
        guint bus_watch_id = 0;
        PipelineCallback callback;
    };

    bs::thread_pool m_thread_pool;
    std::unordered_map<std::string, std::unique_ptr<PipelineData>> m_pipelines;
    std::mutex m_pipeline_mutex;

    static gboolean bus_callback(GstBus* bus, GstMessage* message, gpointer data);
    void cleanup_pipeline(const std::string& pipeline_id);
};