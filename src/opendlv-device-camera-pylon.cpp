/*
 * Copyright (C) 2020 Christian Berger
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

#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>

#include <sys/time.h>
#include <libyuv.h>
#include <X11/Xlib.h>

#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <memory>

using namespace Pylon;

int32_t main(int32_t argc, char **argv) {
    // Automatic initialization and cleanup.
    Pylon::PylonAutoInitTerm autoInitTerm;

    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("camera")) ||
         (0 == commandlineArguments.count("width")) ||
         (0 == commandlineArguments.count("height")) ) {
        std::cerr << argv[0] << " interfaces with a Pylon camera (given by the numerical identifier, e.g., 0) and provides the captured image in two shared memory areas: one in I420 format and one in ARGB format." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --camera=<identifier> --width=<width> --height=<height> [--name.i420=<unique name for the shared memory in I420 format>] [--name.argb=<unique name for the shared memory in ARGB format>] --width=W --height=H [--offsetX=X] [--offsetY=Y] [--packetsize=1500] [--fps=17] [--verbose]" << std::endl;
        std::cerr << "         --camera:     serial number for Pylon-compatible camera to be used" << std::endl;
        std::cerr << "         --name.i420:  name of the shared memory for the I420 formatted image; when omitted, 'video0.i420' is chosen" << std::endl;
        std::cerr << "         --name.argb:  name of the shared memory for the I420 formatted image; when omitted, 'video0.argb' is chosen" << std::endl;
        std::cerr << "         --skip.argb:  don't decode frame into argb format; default: false" << std::endl;
        std::cerr << "         --width:      desired width of a frame" << std::endl;
        std::cerr << "         --height:     desired height of a frame" << std::endl;
        std::cerr << "         --offsetX:    X for desired ROI (default: 0)" << std::endl;
        std::cerr << "         --offsetY:    Y for desired ROI (default: 0)" << std::endl;
        std::cerr << "         --packetsize: if supported by the adapter (eg., jumbo frames), use this packetsize (default: 1500)" << std::endl;
        std::cerr << "         --autoexposuretimeabslowerlimit: default: 26" << std::endl;
        std::cerr << "         --autoexposuretimeabsupperlimit: default: 50000" << std::endl;
        std::cerr << "         --fps:        desired acquisition frame rate (depends on bandwidth)" << std::endl;
        std::cerr << "         --sync:       force all cameras to capture in sync (lowers frame rate)" << std::endl;
        std::cerr << "         --verbose:    display captured image" << std::endl;
        std::cerr << "         --info:       show grabbing information " << std::endl;
        std::cerr << "Example: " << argv[0] << " --camera=0 --width=640 --height=480 --verbose" << std::endl;
        retCode = 1;
    }
    else {
        const std::string CAMERA{commandlineArguments["camera"]};
        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const uint32_t OFFSET_X{static_cast<uint32_t>((commandlineArguments.count("offsetX") != 0) ?std::stoi(commandlineArguments["offsetX"]) : 0)};
        const uint32_t OFFSET_Y{static_cast<uint32_t>((commandlineArguments.count("offsetY") != 0) ?std::stoi(commandlineArguments["offsetY"]) : 0)};
        const uint32_t PACKET_SIZE{static_cast<uint32_t>((commandlineArguments.count("packetsize") != 0) ?std::stoi(commandlineArguments["packetsize"]) : 1500)};
        const uint32_t AUTOEXPOSURETIMEABSLOWERLIMIT{static_cast<uint32_t>((commandlineArguments.count("autoexposuretimeabslowerlimit") != 0) ? std::stoi(commandlineArguments["autoexposuretimeabslowerlimit"]) : 26)};
        const uint32_t AUTOEXPOSURETIMEABSUPPERLIMIT{static_cast<uint32_t>((commandlineArguments.count("autoexposuretimeabsupperlimit") != 0) ? std::stoi(commandlineArguments["autoexposuretimeabsupperlimit"]) : 50000)};
        const bool FPS_SET{commandlineArguments.count("fps") != 0};
        const float FPS{static_cast<float>((commandlineArguments.count("fps") != 0) ? std::stof(commandlineArguments["fps"]) : 17)};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};
        const bool SYNC{commandlineArguments.count("sync") != 0};
        const bool INFO{commandlineArguments.count("info") != 0};
        const bool SKIP_ARGB{commandlineArguments.count("skip.argb") != 0};

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

            try {
                IPylonDevice *pDevice{nullptr};
                CTlFactory& TlFactory = CTlFactory::GetInstance();
                DeviceInfoList_t lstDevices;
                TlFactory.EnumerateDevices(lstDevices);
                if (!lstDevices.empty()) {
                    uint8_t cameraCounter{0};
                    for(DeviceInfoList_t::const_iterator it = lstDevices.begin(); it != lstDevices.end(); it++, cameraCounter++) {
                        std::clog << "[opendlv-device-camera-pylon]: " << it->GetModelName() << " (" << it->GetSerialNumber() << ") at " << it->GetIpAddress() << std::endl;
                        std::stringstream sstr;
                        sstr << it->GetSerialNumber();
                        const std::string str{sstr.str()};
                        if (str.find(CAMERA) != std::string::npos) {
                            pDevice = TlFactory.CreateDevice(lstDevices[cameraCounter]);
                        }
                    }
                }

                if (pDevice == nullptr) {
                    std::cout << "[opendlv-device-camera-pylon] Failed to open camera." << std::endl;
                    return -1;
                }

								CBaslerUniversalInstantCamera camera(pDevice);
                std::clog << "[opendlv-device-camera-pylon]: Using " << camera.GetDeviceInfo().GetModelName() << " (" << camera.GetDeviceInfo().GetSerialNumber() << ") at " << camera.GetDeviceInfo().GetIpAddress() << std::endl;

								// The parameter MaxNumBuffer can be used to control the count of buffers
								// allocated for grabbing. The default value of this parameter is 10.
								camera.MaxNumBuffer = 10;

								// Start the grabbing of c_countOfImagesToGrab images.
								// The camera device is parameterized with a default configuration which
								// sets up free-running continuous acquisition.
								camera.StartGrabbing();

								// This smart pointer will receive the grab result data.
								CGrabResultPtr ptrGrabResult;

                // Frame grabbing loop.
                int cnt{0};
                const uint32_t timeoutInMS{500};
                while (!cluon::TerminateHandler::instance().isTerminated.load() &&
                       camera.IsGrabbing() && cnt++ < 10) {
                    // TODO: Check grabResult.Status == Grabbed / !Failed (compile error?)
                    //int64_t timeStampInMicroseconds = (static_cast<int64_t>(grabResult.TimeStamp)/static_cast<int64_t>(1000));
                    //if (INFO) {
                    //    std::cout << "[opendlv-device-camera-pylon]: Grabbed frame at " << timeStampInMicroseconds << " us (delta to host: " << cluon::time::deltaInMicroseconds(nowOnHost, cluon::time::fromMicroseconds(timeStampInMicroseconds)) << " us); sizeOfPayload: " << sizeOfPayload << std::endl;
                    //}
                    //cluon::data::TimeStamp ts{cluon::time::fromMicroseconds(timeStampInMicroseconds)};
                    cluon::data::TimeStamp ts{cluon::time::now()};

										// Wait for an image and then retrieve it. A timeout of 5000 ms is used.
										camera.RetrieveResult(timeoutInMS, ptrGrabResult, TimeoutHandling_ThrowException);

										// Image grabbed successfully?
										if (ptrGrabResult->GrabSucceeded()) {
												// Access the image data.
												std::cout << "SizeX: " << ptrGrabResult->GetWidth() << std::endl;
												std::cout << "SizeY: " << ptrGrabResult->GetHeight() << std::endl;
												const uint8_t *pImageBuffer = (uint8_t *) ptrGrabResult->GetBuffer();
												std::cout << "Gray value of first pixel: " << (uint32_t) pImageBuffer[0] << std::endl << std::endl;
										}
										else {
												std::cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << std::endl;
										}
#ifdef ABC
                    uint8_t *imageBuffer;
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

                    if (!SKIP_ARGB) {
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
                        sharedMemoryARGB->notifyAll();
                    }

                    // Wake up any pending processes.
                    sharedMemoryI420->notifyAll();
#endif
                }
            }
						catch (const GenericException &e) {
                std::cerr << "[opendlv-device-camera-pylon]: Exception: '" << e.GetDescription() << "'." << std::endl;
								return -1;
						}

            // Release any resources.
        }
        retCode = 0;
    }
    return retCode;
}

