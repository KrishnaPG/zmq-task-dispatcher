#include "headers.hpp"

template<>
void handleMethod<MethodID::GStreamer_Pipeline_Start>(const MethodParams<MethodID::GStreamer_Pipeline_Start>& params)
{
    std::cout << "GStreamer_Pipeline_Start" << std::endl;
}

template<>
void handleMethod<MethodID::GStreamer_Pipeline_Stop>(const MethodParams<MethodID::GStreamer_Pipeline_Stop>& params)
{
    std::cout << "GStreamer_Pipeline_Stop" << std::endl;
}