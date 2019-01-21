/*
 * Copyright (C) 2018  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"

#include <pylonc/PylonC.h>

#include <sys/time.h>
#include <libyuv.h>
#include <X11/Xlib.h>

#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <memory>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("camera")) ||
         (0 == commandlineArguments.count("width")) ||
         (0 == commandlineArguments.count("height")) ) {
        std::cerr << argv[0] << " interfaces with a Pylon camera (given by the numerical identifier, e.g., 0) and provides the captured image in two shared memory areas: one in I420 format and one in ARGB format." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --camera=<identifier> --width=<width> --height=<height> [--name.i420=<unique name for the shared memory in I420 format>] [--name.argb=<unique name for the shared memory in ARGB format>] --width=W --height=H [--offsetX=X] [--offsetY=Y] [--packetsize=1500] [--fps=17] [--verbose]" << std::endl;
        std::cerr << "         --camera:     numerical identifier for pylon-compatible camera to be used" << std::endl;
        std::cerr << "         --name.i420:  name of the shared memory for the I420 formatted image; when omitted, 'video0.i420' is chosen" << std::endl;
        std::cerr << "         --name.argb:  name of the shared memory for the I420 formatted image; when omitted, 'video0.argb' is chosen" << std::endl;
        std::cerr << "         --width:      desired width of a frame" << std::endl;
        std::cerr << "         --height:     desired height of a frame" << std::endl;
        std::cerr << "         --offsetX:    X for desired ROI (default: 0)" << std::endl;
        std::cerr << "         --offsetY:    Y for desired ROI (default: 0)" << std::endl;
        std::cerr << "         --packetsize: if supported by the adapter (eg., jumbo frames), use this packetsize (default: 1500)" << std::endl;
        std::cerr << "         --fps:        desired acquisition frame rate (depends on bandwidth)" << std::endl;
        std::cerr << "         --verbose:    display captured image" << std::endl;
        std::cerr << "Example: " << argv[0] << " --camera=0 --width=640 --height=480 --verbose" << std::endl;
        retCode = 1;
    }
    else {
        const uint32_t CAMERA{static_cast<uint32_t>((commandlineArguments.count("camera") != 0) ?std::stoi(commandlineArguments["camera"]) : 0)};
        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const uint32_t OFFSET_X{static_cast<uint32_t>((commandlineArguments.count("offsetX") != 0) ?std::stoi(commandlineArguments["offsetX"]) : 0)};
        const uint32_t OFFSET_Y{static_cast<uint32_t>((commandlineArguments.count("offsetY") != 0) ?std::stoi(commandlineArguments["offsetY"]) : 0)};
        const uint32_t PACKET_SIZE{static_cast<uint32_t>((commandlineArguments.count("packetsize") != 0) ?std::stoi(commandlineArguments["packetsize"]) : 1500)};
        const float FPS{static_cast<float>((commandlineArguments.count("fps") != 0) ?std::stof(commandlineArguments["fps"]) : 17)};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};

        // Set up the names for the shared memory areas.
        std::string NAME_I420{"video0.i420"};
        if ((commandlineArguments["name.i420"].size() != 0)) {
            NAME_I420 = commandlineArguments["name.i420"];
        }
        std::string NAME_ARGB{"video0.argb"};
        if ((commandlineArguments["name.argb"].size() != 0)) {
            NAME_ARGB = commandlineArguments["name.argb"];
        }

        std::unique_ptr<cluon::SharedMemory> sharedMemoryI420(new cluon::SharedMemory{NAME_I420, WIDTH * HEIGHT * 3/2});
        if (!sharedMemoryI420 || !sharedMemoryI420->valid()) {
            std::cerr << "[opendlv-device-camera-pylon]: Failed to create shared memory '" << NAME_I420 << "'." << std::endl;
            return retCode = 1;
        }

        std::unique_ptr<cluon::SharedMemory> sharedMemoryARGB(new cluon::SharedMemory{NAME_ARGB, WIDTH * HEIGHT * 4});
        if (!sharedMemoryARGB || !sharedMemoryARGB->valid()) {
            std::cerr << "[opendlv-device-camera-pylon]: Failed to create shared memory '" << NAME_ARGB << "'." << std::endl;
            return retCode = 1;
        }

        if ( (sharedMemoryI420 && sharedMemoryI420->valid()) &&
             (sharedMemoryARGB && sharedMemoryARGB->valid()) ) {
            std::clog << "[opendlv-device-camera-pylon]: Data from camera '" << commandlineArguments["camera"]<< "' available in I420 format in shared memory '" << sharedMemoryI420->name() << "' (" << sharedMemoryI420->size() << ") and in ARGB format in shared memory '" << sharedMemoryARGB->name() << "' (" << sharedMemoryARGB->size() << ")." << std::endl;

            // Delegate to handle errors.
            auto checkForErrorAndExitWhenError = [](GENAPIC_RESULT errorCode, uint32_t line){
                if (errorCode != GENAPI_E_OK) {
                    size_t lengthOfErrorMessage{0};
                    char *errorMessage{nullptr};
                    {
                        // Get error message for detailed information.
                        GenApiGetLastErrorMessage(nullptr, &lengthOfErrorMessage);
                        errorMessage = reinterpret_cast<char*>(std::malloc(lengthOfErrorMessage));
                        GenApiGetLastErrorMessage(errorMessage, &lengthOfErrorMessage);
                        std::cerr << "[opendlv-device-camera-pylon]: Error 0x" << std::hex << errorCode << std::dec << ": '" << errorMessage << "' (line " << line << ")" << std::endl;
                        std::free(errorMessage);
                    }
                    {
                        // Check for detailed information.
                        GenApiGetLastErrorDetail(nullptr, &lengthOfErrorMessage);
                        errorMessage = reinterpret_cast<char*>(std::malloc(lengthOfErrorMessage));
                        GenApiGetLastErrorDetail(errorMessage, &lengthOfErrorMessage);
                        std::cerr << "[opendlv-device-camera-pylon]: Details: '" << errorMessage << "'." << std::endl;
                        std::free(errorMessage);
                    }
                    PylonTerminate();
                    std::exit(1);
                }
            };

            // Initialize the pylon library.
            PylonInitialize();

            // What devices are available?
            size_t numberOfAvailableDevices{0};
            checkForErrorAndExitWhenError(PylonEnumerateDevices(&numberOfAvailableDevices), __LINE__);
            if (0 == numberOfAvailableDevices) {
                std::cerr << "[opendlv-device-camera-pylon]: No pylon-compatible devices available." << std::endl;
                PylonTerminate();
                std::exit(1);
            }

            // Create a handle for the desired camera.
            PYLON_DEVICE_HANDLE handleForDevice{0};
            checkForErrorAndExitWhenError(PylonCreateDeviceByIndex(CAMERA, &handleForDevice), __LINE__);

            // Open device.
            checkForErrorAndExitWhenError(PylonDeviceOpen(handleForDevice, PYLONC_ACCESS_MODE_CONTROL | PYLONC_ACCESS_MODE_STREAM), __LINE__);

            // Print device information.
            {
                size_t MAX_BUFFER{256};
                char buffer[MAX_BUFFER];

                if (PylonDeviceFeatureIsReadable(handleForDevice, "DeviceModelName")) {
                    checkForErrorAndExitWhenError(PylonDeviceFeatureToString(handleForDevice, "DeviceModelName", buffer, &MAX_BUFFER), __LINE__);
                    std::cerr << "[opendlv-device-camera-pylon]: Found camera '" << buffer << "'." << std::endl;
                }
            }

            // Load factory-settings named "AutoFunctions".
            {
                checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "UserSetSelector", "AutoFunctions"), __LINE__);
                checkForErrorAndExitWhenError(PylonDeviceExecuteCommandFeature(handleForDevice, "UserSetLoad"), __LINE__);
            }

            // Enable PTP for the current camera.
            checkForErrorAndExitWhenError(PylonDeviceSetBooleanFeature(handleForDevice, "GevIEEE1588", 1), __LINE__);

            // Setup pixel format.
            if (PylonDeviceFeatureIsAvailable(handleForDevice, "EnumEntry_PixelFormat_YUV422_YUYV_Packed")) {
                checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "PixelFormat", "YUV422_YUYV_Packed"), __LINE__);
            }
            else {
                std::cerr << "[opendlv-device-camera-pylon]: Could not set YUV422_YUYV_Packed pixel format." << std::endl;
                PylonTerminate();
                return 1;
            }

            // Setup Auto functions.
            {
                if (PylonDeviceFeatureIsWritable(handleForDevice, "GrayValueAdjustmentDampingAbs")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetFloatFeature(handleForDevice, "GrayValueAdjustmentDampingAbs", 0.683594), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "BalanceWhiteAdjustmentDampingAbs")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetFloatFeature(handleForDevice, "BalanceWhiteAdjustmentDampingAbs", 0.976562), __LINE__);
                }
                if (PylonDeviceFeatureIsAvailable(handleForDevice, "AutoFunctionProfile")) {
                    checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "AutoFunctionProfile", "GainMinimum"), __LINE__);
                }
            }

            // Setup AutoGain.
            {
//                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoGainRawLowerLimit")) {
//                    checkForErrorAndExitWhenError(PylonDeviceSetFloatFeature(handleForDevice, "AutoGainRawLowerLimit", 0), __LINE__);
//                }
//                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoGainRawUpperLimit")) {
//                    checkForErrorAndExitWhenError(PylonDeviceSetFloatFeature(handleForDevice, "AutoGainRawUpperLimit", 100), __LINE__);
//                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoTargetValue")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "AutoTargetValue", 50), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoFunctionAOISelector")) {
                    checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "AutoFunctionAOISelector", "AOI1"), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoFunctionAOIUsageIntensity")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetBooleanFeature(handleForDevice, "AutoFunctionAOIUsageIntensity", 1), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoFunctionAOIUsageWhiteBalance")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetBooleanFeature(handleForDevice, "AutoFunctionAOIUsageWhiteBalance", 1), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoFunctionAOIWidth")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "AutoFunctionAOIWidth", WIDTH), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoFunctionAOIHeight")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "AutoFunctionAOIHeight", HEIGHT), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoFunctionAOIOffsetX")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "AutoFunctionAOIOffsetX", OFFSET_X), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoFunctionAOIOffsetY")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "AutoFunctionAOIOffsetY", OFFSET_Y), __LINE__);
                }
                if (PylonDeviceFeatureIsAvailable(handleForDevice, "GainAuto")) {
                    checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "GainAuto", "Continuous"), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "GainRaw")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "GainRaw", 0), __LINE__);
                }
            }

            // Setup AutoExposure.
            {
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoExposureTimeAbsLowerLimit")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetFloatFeature(handleForDevice, "AutoExposureTimeAbsLowerLimit", 26), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AutoExposureTimeAbsUpperLimit")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetFloatFeature(handleForDevice, "AutoExposureTimeAbsUpperLimit", 50000), __LINE__);
                }
                if (PylonDeviceFeatureIsAvailable(handleForDevice, "ExposureAuto")) {
                    checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "ExposureAuto", "Continuous"), __LINE__);
                }
            }

            // Setup Acquisition parameters.
            {
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AcquisitionFrameRateEnable")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetBooleanFeature(handleForDevice, "AcquisitionFrameRateEnable", 1), __LINE__);
                }
                if (PylonDeviceFeatureIsWritable(handleForDevice, "AcquisitionFrameRateAbs")) {
                    checkForErrorAndExitWhenError(PylonDeviceSetFloatFeature(handleForDevice, "AcquisitionFrameRateAbs", FPS), __LINE__);
                }
            }

            // If available for the given device, disable acquisition start trigger.
            if (PylonDeviceFeatureIsAvailable(handleForDevice, "EnumEntry_TriggerSelector_AcquisitionStart")) {
                checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "TriggerSelector", "AcquisitionStart"), __LINE__);
                checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "TriggerMode", "Off"), __LINE__);
            }

            // If available for the given device, disable frame burst start trigger.
            if (PylonDeviceFeatureIsAvailable(handleForDevice, "EnumEntry_TriggerSelector_FrameBurstStart")) {
                checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "TriggerSelector", "FrameBurstStart"), __LINE__);
                checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "TriggerMode", "Off"), __LINE__);
            }

            // If available for the given device, disable frame start trigger.
            if (PylonDeviceFeatureIsAvailable(handleForDevice, "EnumEntry_TriggerSelector_FrameStart")) {
                checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "TriggerSelector", "FrameStart"), __LINE__);
                checkForErrorAndExitWhenError(PylonDeviceFeatureFromString(handleForDevice, "TriggerMode", "Off"), __LINE__);
            }

            // Define desired ROI.
            {
                checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "Width", WIDTH), __LINE__);
                checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "Height", HEIGHT), __LINE__);
                checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "OffsetX", OFFSET_X), __LINE__);
                checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "OffsetY", OFFSET_Y), __LINE__);
            }

            // Adjust packetsize.
            if (PylonDeviceFeatureIsWritable(handleForDevice, "GevSCPSPacketSize")) {
                checkForErrorAndExitWhenError(PylonDeviceSetIntegerFeature(handleForDevice, "GevSCPSPacketSize", PACKET_SIZE), __LINE__);
            }

            // What payload size do we need?
            int32_t sizeOfPayload{0};
            if (PylonDeviceFeatureIsReadable(handleForDevice, "PayloadSize")) {
                checkForErrorAndExitWhenError(PylonDeviceGetIntegerFeatureInt32(handleForDevice, "PayloadSize", &sizeOfPayload), __LINE__);
            }
            else
            {
                // From Basler's SimpleGrab.c example:
                //Note: Some camera devices, e.g Camera Link or BCON, don't have a payload size node.
                //      In this case we'll look in the stream grabber node map for the PayloadSize node
                //      The stream grabber, this can be a frame grabber, needs to be open to compute the
                //      required payload size.
                PYLON_STREAMGRABBER_HANDLE hGrabber;
                checkForErrorAndExitWhenError(PylonDeviceGetStreamGrabber(handleForDevice, 0, &hGrabber), __LINE__);
                checkForErrorAndExitWhenError(PylonStreamGrabberOpen(hGrabber), __LINE__);

                NODEMAP_HANDLE hStreamNodeMap;
                checkForErrorAndExitWhenError(PylonStreamGrabberGetNodeMap(hGrabber, &hStreamNodeMap), __LINE__);

                NODE_HANDLE hNode;
                checkForErrorAndExitWhenError(GenApiNodeMapGetNode(hStreamNodeMap, "PayloadSize", &hNode), __LINE__);
                if (GENAPIC_INVALID_HANDLE == hNode) {
                    std::cerr << "[opendlv-device-camera-pylon]: Could not determine size of payload." << std::endl;
                    PylonTerminate();
                    return 1;
                }
                int64_t i64sizeOfPayload{0};
                checkForErrorAndExitWhenError(GenApiIntegerGetValue(hNode, &i64sizeOfPayload), __LINE__);
                sizeOfPayload = static_cast<int32_t>(i64sizeOfPayload);

                checkForErrorAndExitWhenError(PylonStreamGrabberClose(hGrabber), __LINE__);
            }

            // Get memory to store frames.
            unsigned char *imageBuffer = reinterpret_cast<unsigned char*>(malloc(sizeOfPayload));
            if (nullptr == imageBuffer) {
                std::cerr << "[opendlv-device-camera-pylon]: Could not acquire buffer." << std::endl;
                PylonTerminate();
                std::exit(1);
            }

            // Determine timebase.
            {
                struct timeval epochTime{};
                {
                    gettimeofday(&epochTime, nullptr);
                    checkForErrorAndExitWhenError(PylonDeviceExecuteCommandFeature(handleForDevice, "GevTimestampControlLatch"), __LINE__);
                }

                int64_t cameraTimeStamp{0};
                checkForErrorAndExitWhenError(PylonDeviceGetIntegerFeature(handleForDevice, "GevTimestampValue", &cameraTimeStamp), __LINE__);

                if (VERBOSE) {
                    std::clog << "[opendlv-device-camera-pylon]: Camera time base in microseconds: " << cameraTimeStamp << "." << std::endl;
                }
            }

            // Accessing the low-level X11 data display.
            Display* display{nullptr};
            Visual* visual{nullptr};
            Window window{0};
            XImage* ximage{nullptr};
            if (VERBOSE) {
                display = XOpenDisplay(NULL);
                visual = DefaultVisual(display, 0);
                window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                sharedMemoryARGB->lock();
                {
                    ximage = XCreateImage(display, visual, 24, ZPixmap, 0, sharedMemoryARGB->data(), WIDTH, HEIGHT, 32, 0);
                }
                sharedMemoryARGB->unlock();
                XMapWindow(display, window);
            }

            // Frame grabbing loop.
            while (!cluon::TerminateHandler::instance().isTerminated.load()) {
                // Grab a single frame from channel 0 with 0.5s timeout.
                PylonGrabResult_t grabResult{};
                _Bool bufferReady{false};
                GENAPIC_RESULT res = PylonDeviceGrabSingleFrame(handleForDevice, 0, imageBuffer, sizeOfPayload, &grabResult, &bufferReady, 500);
                cluon::data::TimeStamp nowOnHost = cluon::time::now();
                if ( (GENAPI_E_OK == res) && !bufferReady ) {
                    std::cerr << "[opendlv-device-camera-pylon]: Timeout while grabbing frame." << std::endl;
                }
                checkForErrorAndExitWhenError(res, __LINE__);

                // TODO: Check grabResult.Status == Grabbed / !Failed (compile error?)
                int64_t timeStampInMicroseconds = (static_cast<int64_t>(grabResult.TimeStamp)/static_cast<int64_t>(1000));
                if (VERBOSE) {
                    std::cout << "[opendlv-device-camera-pylon]: Grabbed frame at " << timeStampInMicroseconds << " us: host is " << cluon::time::toMicroseconds(nowOnHost) << " us, delta: " << cluon::time::deltaInMicroseconds(nowOnHost, cluon::time::fromMicroseconds(timeStampInMicroseconds)) << "." << std::endl;
                }
                cluon::data::TimeStamp ts{cluon::time::fromMicroseconds(timeStampInMicroseconds)};

                sharedMemoryI420->lock();
                sharedMemoryI420->setTimeStamp(ts);
                {
                    libyuv::YUY2ToI420(imageBuffer, WIDTH * 2 /* 2*WIDTH for YUYV 422*/,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                       WIDTH, HEIGHT);
                }
                sharedMemoryI420->unlock();

                sharedMemoryARGB->lock();
                sharedMemoryARGB->setTimeStamp(ts);
                {
                    libyuv::I420ToARGB(reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                       reinterpret_cast<uint8_t*>(sharedMemoryARGB->data()), WIDTH * 4, WIDTH, HEIGHT);

                    if (VERBOSE) {
                        XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                    }
                }
                sharedMemoryARGB->unlock();

                // Wake up any pending processes.
                sharedMemoryI420->notifyAll();
                sharedMemoryARGB->notifyAll();
            }

            // Release any resources.
            checkForErrorAndExitWhenError(PylonDeviceClose(handleForDevice), __LINE__);
            checkForErrorAndExitWhenError(PylonDestroyDevice(handleForDevice), __LINE__);
            std::free(imageBuffer);
            PylonTerminate();
        }
        retCode = 0;
    }
    return retCode;
}

