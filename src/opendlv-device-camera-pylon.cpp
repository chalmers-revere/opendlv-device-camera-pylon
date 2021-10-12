/*
 * Copyright (C) 2020-2021 Christian Berger, Ola Benderius
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
#include "opendlv-standard-message-set.hpp"

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
using namespace GenApi;

int32_t main(int32_t argc, char **argv) {
  // Automatic initialization and cleanup.
  Pylon::PylonAutoInitTerm autoInitTerm;

  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ( (0 == commandlineArguments.count("cid")) ||
      (0 == commandlineArguments.count("camera")) ||
      (0 == commandlineArguments.count("width")) ||
      (0 == commandlineArguments.count("height")) ) {
    std::cerr << argv[0] << " interfaces with a Pylon camera (given by the numerical identifier, e.g., 0) and provides the captured image in two shared memory areas: one in I420 format and one in ARGB format." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --camera=<identifier> --width=<width> --height=<height> [--name.i420=<unique name for the shared memory in I420 format>] [--name.argb=<unique name for the shared memory in ARGB format>] --width=W --height=H [--offsetX=X] [--offsetY=Y] [--packetsize=1500] [--fps=17] [--verbose]" << std::endl;
    std::cerr << "         --cid:    CID of the OD4Session to send and receive messages" << std::endl;
    std::cerr << "         --id:     ID to use as senderStamp for sending" << std::endl;
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
    std::cerr << "         --exposuretime: default: 5000" << std::endl;
    std::cerr << "         --fps:        desired acquisition frame rate (depends on bandwidth)" << std::endl;
    std::cerr << "         --sync:       force all cameras to capture in sync (lowers frame rate)" << std::endl;
    std::cerr << "         --verbose:    display captured image" << std::endl;
    std::cerr << "         --info:       show grabbing information " << std::endl;
    std::cerr << "Example: " << argv[0] << " --camera=0 --width=640 --height=480 --verbose" << std::endl;
    retCode = 1;
  }
  else {
    const uint32_t ID{(commandlineArguments["id"].size() != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0};
    const std::string CAMERA{commandlineArguments["camera"]};
    
    const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
    const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
    const float FPS{static_cast<float>((commandlineArguments.count("fps") != 0) ? std::stof(commandlineArguments["fps"]) : 17)};
    
    const uint32_t OFFSET_X{static_cast<uint32_t>((commandlineArguments.count("offsetX") != 0) ? std::stoi(commandlineArguments["offsetX"]) : 0)};
    const uint32_t OFFSET_Y{static_cast<uint32_t>((commandlineArguments.count("offsetY") != 0) ? std::stoi(commandlineArguments["offsetY"]) : 0)};
    
    const uint32_t AUTOEXPOSURETIMEABSLOWERLIMIT{static_cast<uint32_t>((commandlineArguments.count("autoexposuretimeabslowerlimit") != 0) ? std::stoi(commandlineArguments["autoexposuretimeabslowerlimit"]) : 26)};
    const uint32_t AUTOEXPOSURETIMEABSUPPERLIMIT{static_cast<uint32_t>((commandlineArguments.count("autoexposuretimeabsupperlimit") != 0) ? std::stoi(commandlineArguments["autoexposuretimeabsupperlimit"]) : 50000)};
    const uint32_t EXPOSURETIME{static_cast<uint32_t>((commandlineArguments.count("exposuretime") != 0) ? std::stoi(commandlineArguments["exposuretime"]) : 5000)};
    
    const double AUTOGAINLOWERLIMIT{(commandlineArguments.count("autogainlowerlimit") != 0) ? std::stod(commandlineArguments["autogainlowerlimit"]) : 0.0};
    const double AUTOGAINUPPERLIMIT{(commandlineArguments.count("autogainupperlimit") != 0) ? std::stod(commandlineArguments["autogainupperlimit"]) : 7.0};
    const double GAIN{(commandlineArguments.count("gain") != 0) ? std::stod(commandlineArguments["gain"]) : 4.2};
    
    const uint32_t PACKET_SIZE{static_cast<uint32_t>((commandlineArguments.count("packetsize") != 0) ? std::stoi(commandlineArguments["packetsize"]) : 1500)};
    
    const bool VERBOSE{commandlineArguments.count("verbose") != 0};
    const bool SYNC{commandlineArguments.count("sync") != 0};
    const bool INFO{commandlineArguments.count("info") != 0};
    const bool SKIP_ARGB{commandlineArguments.count("skip.argb") != 0};

    bool autoExposureAndGain{(commandlineArguments.count("autoexposuretimeabslowerlimit") != 0) || (commandlineArguments.count("autoexposuretimeabsupperlimit") != 0) || (commandlineArguments.count("autogainlowerlimit") != 0) || (commandlineArguments.count("autogainupperlimit") != 0)};

    if (autoExposureAndGain && ((commandlineArguments.count("exposuretime") != 0) || commandlineArguments.count("gain") != 0)) {
      std::cerr << "WARNING: Auto functions (exposure time, gain) AND fixed values selected. Using fixed values." << std::endl;
      autoExposureAndGain = false;
    }

    cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

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

      PylonInitialize();

      try {
        IPylonDevice *pDevice{nullptr};
        {
          // Find specified camera.
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
        }

        if (pDevice == nullptr) {
          std::cerr << "[opendlv-device-camera-pylon] Failed to open camera." << std::endl;
          PylonTerminate();
          return -1;
        }


        CBaslerUniversalInstantCamera camera(pDevice);
        std::clog << "[opendlv-device-camera-pylon]: Using " << camera.GetDeviceInfo().GetModelName() << " (id: "  << camera.GetDeviceInfo().GetSerialNumber() << ") at " << camera.GetDeviceInfo().GetIpAddress() << std::endl;

        if (autoExposureAndGain) {
          std::clog << "[opendlv-device-camera-pylon]: Auto exposure time and gain (exposure time: " << AUTOEXPOSURETIMEABSLOWERLIMIT << " to " << AUTOEXPOSURETIMEABSUPPERLIMIT << ", gain: " << AUTOGAINLOWERLIMIT << " to " << AUTOGAINUPPERLIMIT << ")" << std::endl;
        } else {
          std::clog << "[opendlv-device-camera-pylon]: Fixed exposure time and gain (exposure time: " << EXPOSURETIME << ", gain: " << GAIN << ")" << std::endl;
        }

        // Open the camera for accessing the parameters.
        camera.Open();
        // Replace any existing configuration.
        camera.RegisterConfiguration( new CAcquireContinuousConfiguration, RegistrationMode_ReplaceAll, Cleanup_Delete);

        GenICam::gcstring family = camera.DeviceFamilyName.GetValue();

        // Enable PTP for the current camera.
        if (family == "ace 2") {
          if (!camera.PtpEnable.GetValue()) {
            camera.BslPtpPriority1 = 128;
            camera.BslPtpProfile = Basler_UniversalCameraParams::BslPtpProfile_DelayRequestResponseDefaultProfile;
            camera.BslPtpNetworkMode = Basler_UniversalCameraParams::BslPtpNetworkMode_Multicast;
            camera.BslPtpManagementEnable = false;
            camera.BslPtpTwoStep = false;
            camera.PtpEnable = true;
          }
        } else {
          camera.GevIEEE1588 = true;
        }

        bool isMono{false};
        if (camera.PixelFormat.CanSetValue(Basler_UniversalCameraParams::PixelFormat_YCbCr422_8)) {
          camera.PixelFormat = Basler_UniversalCameraParams::PixelFormat_YCbCr422_8;
        } else {
          camera.PixelFormat = Basler_UniversalCameraParams::PixelFormat_Mono8;
          isMono = true;
        }

        camera.ExposureMode = Basler_UniversalCameraParams::ExposureMode_Timed;

        if (family == "ace 2") {
          if (autoExposureAndGain) {
            camera.AutoFunctionProfile = Basler_UniversalCameraParams::AutoFunctionProfile_MinimizeGain;
            camera.AutoFunctionROISelector = Basler_UniversalCameraParams::AutoFunctionROISelector_ROI1;
            camera.AutoFunctionROIUseBrightness = true;
            camera.AutoFunctionROIUseWhiteBalance = true;
            camera.AutoFunctionROIWidth = WIDTH;
            camera.AutoFunctionROIHeight = HEIGHT;
            camera.AutoFunctionROIOffsetX = OFFSET_X;
            camera.AutoFunctionROIOffsetY = OFFSET_Y;
            
            camera.AutoTargetBrightness = 0.6;

            camera.GainAuto = Basler_UniversalCameraParams::GainAuto_Continuous;
            
            camera.ExposureAuto = Basler_UniversalCameraParams::ExposureAuto_Continuous;
            camera.AutoExposureTimeLowerLimit = AUTOEXPOSURETIMEABSLOWERLIMIT;
            camera.AutoExposureTimeUpperLimit = AUTOEXPOSURETIMEABSUPPERLIMIT;
          } else {
            camera.ExposureAuto = Basler_UniversalCameraParams::ExposureAuto_Off;
            camera.ExposureTime = EXPOSURETIME;
            camera.Gain = GAIN;
          }
        } else {
          if (autoExposureAndGain) {
            camera.AutoFunctionProfile = Basler_UniversalCameraParams::AutoFunctionProfile_GainMinimum;
            camera.AutoFunctionAOISelector = Basler_UniversalCameraParams::AutoFunctionAOISelector_AOI1;
            camera.AutoFunctionAOIUsageIntensity = true;
            camera.AutoFunctionAOIUsageWhiteBalance = true;
            camera.AutoFunctionAOIWidth = WIDTH;
            camera.AutoFunctionAOIHeight = HEIGHT;
            camera.AutoFunctionAOIOffsetX = OFFSET_X;
            camera.AutoFunctionAOIOffsetY = OFFSET_Y;

            camera.AutoTargetValue = 50;

            camera.GainAuto = Basler_UniversalCameraParams::GainAuto_Continuous;
            camera.GrayValueAdjustmentDampingAbs = 0.683594;
            camera.BalanceWhiteAdjustmentDampingAbs = 0.976562;

            camera.ExposureAuto = Basler_UniversalCameraParams::ExposureAuto_Continuous;
            camera.AutoExposureTimeAbsLowerLimit = AUTOEXPOSURETIMEABSLOWERLIMIT;
            camera.AutoExposureTimeAbsUpperLimit = AUTOEXPOSURETIMEABSUPPERLIMIT;
          } else {
            camera.ExposureAuto = Basler_UniversalCameraParams::ExposureAuto_Off;
            camera.ExposureTimeMode = Basler_UniversalCameraParams::ExposureTimeMode_Standard;
            camera.ExposureTime = EXPOSURETIME;
            camera.GainRaw = static_cast<uint64_t>(GAIN);
          }
        }

        // AcquisitionMode:
        camera.AcquisitionMode = Basler_UniversalCameraParams::AcquisitionMode_Continuous;

        // FPS
        camera.AcquisitionFrameRateEnable = true;
        if (family == "ace 2") {
          camera.AcquisitionFrameRate = FPS;
        } else {
          camera.AcquisitionFrameRateAbs = FPS;
        }

        if (family == "ace 2") {
          if (SYNC) {
            camera.BslPeriodicSignalPeriod = 1.0 / FPS * 1.0e6;
            camera.BslPeriodicSignalDelay = 0;
            camera.TriggerSelector = Basler_UniversalCameraParams::TriggerSelector_FrameStart;
            camera.TriggerMode = Basler_UniversalCameraParams::TriggerMode_On;
            camera.TriggerSource = Basler_UniversalCameraParams::TriggerSource_PeriodicSignal1;
          } else {
            camera.TriggerMode = Basler_UniversalCameraParams::TriggerMode_Off;
          }
        } else {
          if (SYNC) {
            camera.SyncFreeRunTimerTriggerRateAbs = FPS;
            camera.SyncFreeRunTimerStartTimeHigh = 0;
            camera.SyncFreeRunTimerStartTimeLow = 0;
            camera.SyncFreeRunTimerUpdate();
            camera.SyncFreeRunTimerEnable = true;
          }
          else {
            camera.SyncFreeRunTimerEnable = false;
          }
        }

        //camera.TriggerSelector = Basler_UniversalCameraParams::TriggerSelector_AcquisitionStart;
        //camera.TriggerSelector = Basler_UniversalCameraParams::TriggerSelector_FrameBurstStart;
        camera.TriggerSelector = Basler_UniversalCameraParams::TriggerSelector_FrameStart;
        camera.TriggerMode = Basler_UniversalCameraParams::TriggerMode_Off;

        camera.Width = WIDTH;
        camera.Height = HEIGHT;
        camera.OffsetX = OFFSET_X;
        camera.OffsetY = OFFSET_Y;

        // Packet size (should match MTU).
        camera.GevSCPSPacketSize = PACKET_SIZE;

        // Enable chunks in general to read meta data.
        if (camera.ChunkModeActive.TrySetValue(true)) {
          camera.ChunkSelector.SetValue(Basler_UniversalCameraParams::ChunkSelector_Timestamp);
          camera.ChunkEnable.SetValue(true);
          camera.ChunkSelector.SetValue(Basler_UniversalCameraParams::ChunkSelector_ExposureTime);
          camera.ChunkEnable.SetValue(true);
          camera.ChunkSelector.SetValue(Basler_UniversalCameraParams::ChunkSelector_Gain);
          camera.ChunkEnable.SetValue(true);
        }

        // The parameter MaxNumBuffer can be used to control the count of buffers
        // allocated for grabbing. The default value of this parameter is 10.
        camera.MaxNumBuffer = 10;

        // Start the grabbing of c_countOfImagesToGrab images.
        // The camera device is parameterized with a default configuration which
        // sets up free-running continuous acquisition.
        camera.StartGrabbing();

        // This smart pointer will receive the grab result data.
        //CGrabResultPtr ptrGrabResult;
        CBaslerUniversalGrabResultPtr ptrGrabResult;

        // Frame grabbing loop.
        const uint32_t timeoutInMS{10000};
        while (od4.isRunning() && camera.IsGrabbing()) {
          // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
          camera.RetrieveResult(timeoutInMS, ptrGrabResult, TimeoutHandling_ThrowException);

          // Image grabbed successfully?
          if (ptrGrabResult->GrabSucceeded()) {
            double exposureTime{0};
            double gain{0};
            cluon::data::TimeStamp nowOnHost = cluon::time::now();
            int64_t timeStampInMicroseconds = (static_cast<int64_t>(ptrGrabResult->GetTimeStamp())/static_cast<int64_t>(1000));
            double temperature = camera.DeviceTemperature.GetValue();
            if (INFO) {
              if (ptrGrabResult->ChunkTimestamp.IsReadable()) {
                timeStampInMicroseconds = (static_cast<int64_t>(ptrGrabResult->ChunkTimestamp.GetValue())/static_cast<int64_t>(1000));
              }

              if (ptrGrabResult->ChunkExposureTime.IsReadable()) {
                exposureTime = ptrGrabResult->ChunkExposureTime.GetValue();
              }
              if (ptrGrabResult->ChunkGain.IsReadable()) {
                gain = ptrGrabResult->ChunkGain.GetValue();
              }
              std::clog << "[opendlv-device-camera-pylon]: Grabbed frame at " << timeStampInMicroseconds << " us (delta to host: " << cluon::time::deltaInMicroseconds(nowOnHost, cluon::time::fromMicroseconds(timeStampInMicroseconds)) << " us); payload side: " << ptrGrabResult->GetPayloadSize() << ", exposure time: " << exposureTime << ", gain: " << gain << ", temperature: " << temperature << std::endl;
            }
            cluon::data::TimeStamp ts{cluon::time::fromMicroseconds(timeStampInMicroseconds)};

            {
              // Propagate meta data.
              opendlv::proxy::AboutImageReading air;
              air.exposureTime(static_cast<float>(exposureTime));
              od4.send(air, ts, ID);
              
              opendlv::proxy::TemperatureReading tr;
              tr.temperature(static_cast<float>(temperature));
              od4.send(tr, ts, ID);
            }

            const uint8_t *imageBuffer = (uint8_t *) ptrGrabResult->GetBuffer();

            sharedMemoryI420->lock();
            sharedMemoryI420->setTimeStamp(ts);
            {
              if (isMono) {
                ::memset(sharedMemoryI420->data(), 128, sharedMemoryI420->size());
                ::memcpy(sharedMemoryI420->data(), imageBuffer, WIDTH * HEIGHT);
              } else {
                libyuv::YUY2ToI420(imageBuffer, WIDTH * 2 /* 2*WIDTH for YUYV 422*/,
                    reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                    reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                    reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                    WIDTH, HEIGHT);
              }
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
          }
          else {
            std::cerr << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << std::endl;
          }
        }
        camera.StopGrabbing();
      }
      catch (const GenericException &e) {
        std::cerr << "[opendlv-device-camera-pylon]: Exception: '" << e.GetDescription() << "'." << std::endl;
        PylonTerminate();
        return -1;
      }
    }
    retCode = 0;
    PylonTerminate();
  }
  return retCode;
}
