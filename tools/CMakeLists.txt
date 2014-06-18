add_executable(calibrator calibrator.cpp)
m_link(calibrator decompress bcobjs)

add_executable(get_latencies get_latencies.cpp)
