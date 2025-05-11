#include "headers.hpp"

template<MethodID MID>
void handleMethod(const MethodParams<MID>& params) { std::cerr << "Unknown Method" << std::endl; }

template<>
void handleMethod<MethodID::GStreamer_Pipeline_Start>(const MethodParams<MethodID::GStreamer_Pipeline_Start>& params)
{
    std::cout << "GStreamer_Pipeline_Start" << std::endl;
}

template<>
void handleMethod<MethodID::GStreamer_Pipeline_Pause>(const MethodParams<MethodID::GStreamer_Pipeline_Pause>& params)
{
    std::cout << "GStreamer_Pipeline_Pause" << std::endl;
}

template<>
void handleMethod<MethodID::GStreamer_Pipeline_Resume>(const MethodParams<MethodID::GStreamer_Pipeline_Resume>& params)
{
    std::cout << "GStreamer_Pipeline_Resume" << std::endl;
}

template<>
void handleMethod<MethodID::GStreamer_Pipeline_Stop>(const MethodParams<MethodID::GStreamer_Pipeline_Stop>& params)
{
    std::cout << "GStreamer_Pipeline_Stop" << std::endl;
}