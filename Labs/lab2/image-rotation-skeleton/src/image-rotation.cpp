//==============================================================
// EECE.4510/5510 Heterogeneous Computing - Lab 2
//
// Image Rotation Sample Code - To be completed by students.
//
// Author: Yan Luo
// Edited by Colin Tierney on 6/10/2023
//
// Copyright ©  2020-
//
// MIT License
//
#include <CL/sycl.hpp>
#include <array>
#include <iostream>
#include "dpc_common.hpp"
#if FPGA || FPGA_EMULATOR || FPGA_PROFILE
#include <sycl/ext/intel/fpga_extensions.hpp>
#endif

using namespace sycl;

// useful header files for image convolution
#include "utils.h"
#include "bmp-utils.h"
#include "gold.h"

// time profiling functions
using Duration = std::chrono::duration<double>;
class Timer {
 public:
  Timer() : start(std::chrono::steady_clock::now()) {}

  Duration elapsed() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<Duration>(now - start);
  }

 private:
  std::chrono::steady_clock::time_point start;
};

static const char* inputImagePath = "./Images/cat.bmp";

#define IMAGE_SIZE (720*1080)
constexpr size_t array_size = IMAGE_SIZE;
typedef std::array<float, array_size> FloatArray;

//************************************
// Image Rotation in DPC++ on device:
//************************************
void ImageRotation(queue &q, float *image_in, float *image_out,
    const size_t W, const size_t H, float sinTheta, float cosTheta)
{
    // We create buffers for the input and output data.
    buffer<float, 1> image_in_buf(image_in, range<1>(H*W));
    buffer<float, 1> image_out_buf(image_out, range<1>(H*W));

    //for(int i=0; i<ImageRows; i++) {
      //for(int j=0; j<ImageCols; j++)
        //std::cout << "image_out[" << i << "," << j << "]=" << image_out[i*ImageCols+j] << std::endl;
    //}

    // Create the range object for the pixel data.
    range<2> num_items{H, W};

    // Submit a command group to the queue by a lambda function that contains the
    // data access permission and device computation (kernel).
    q.submit([&](handler &h) {
      // Create an accessor to buffers with access permission: read, write or
      // read/write. The accessor is a way to access the memory in the buffer.
      accessor srcPtr(image_in_buf, h, read_only);

      // Another way to get access is to call get_access() member function
      auto dstPtr = image_out_buf.get_access<access::mode::write>(h);

      // Use parallel_for to run image convolution in parallel on device. This
      // executes the kernel.
      //    1st parameter is the number of work items.
      //    2nd parameter is the kernel, a lambda that specifies what to do per
      //    work item. The parameter of the lambda is the work item id.
      // DPC++ supports unnamed lambda kernel by default.
      h.parallel_for(num_items, [=](id<2> item)
      {
        // get row and col of the pixel assigned to this work item
        int ix = item[0];
        int iy = item[1];

        // calculate location of data to move int (ix, iy)
        // output decomposition as mentioned on Page 17 of the slides
        float xpos = ((float)ix) * cosTheta + ((float)iy) * sinTheta;
        float ypos = -((float)ix) * sinTheta + ((float)iy) * cosTheta;

        /* Bound checking to make sure xpos and ypos are in range */
        if(((int)xpos >= 0) && ((int)xpos < W) &&
           ((int)ypos >= 0) && ((int)ypos < H))
        {
           /* read (ix,iy) src data and store at (xpos,ypos) in dest data
            * in this case, because we rotate about the origin and
            * there is no translation, we know that (xpos, ypos) will be
            * unique for each input (ix, iy) and so each work-item can
            * write its results independently */
           dstPtr[(int)ypos * W + (int)xpos] = srcPtr[(iy * W) + ix];
        }
      }
    );
  });
}


int main() {
  // Create device selector for the device of your interest.
#if FPGA_EMULATOR
  // DPC++ extension: FPGA emulator selector on systems without FPGA card.
  ext::intel::fpga_emulator_selector d_selector;
#elif FPGA || FPGA_PROFILE
  // DPC++ extension: FPGA selector on systems with FPGA card.
  ext::intel::fpga_selector d_selector;
#else
  // The default device selector will select the most performant device.
  default_selector d_selector;
#endif

  float *hInputImage;
  float *hOutputImage;

  int imageRows;
  int imageCols;
  int i;

  float sinTheta, cosTheta;

  // assume Theta=45 degrees
  sinTheta = 0.70710678118;
  cosTheta = 0.70710678118;

#ifndef FPGA_PROFILE
  // Query about the platform
  unsigned number = 0;
  auto myPlatforms = platform::get_platforms();
  // loop through the platforms to poke into
  for (auto &onePlatform : myPlatforms) {
    std::cout << ++number << " found .." << std::endl << "Platform: "
    << onePlatform.get_info<info::platform::name>() <<std::endl;
    // loop through the devices
    auto myDevices = onePlatform.get_devices();
    for (auto &oneDevice : myDevices) {
      std::cout << "Device: "
      << oneDevice.get_info<info::device::name>() <<std::endl;
    }
  }
  std::cout<<std::endl;
#endif

  /* Read in the BMP image */
  hInputImage = readBmpFloat(inputImagePath, &imageRows, &imageCols);
  printf("imageRows=%d, imageCols=%d\n", imageRows, imageCols);
  /* Allocate space for the output image */
  hOutputImage = (float *)malloc( imageRows*imageCols * sizeof(float) );
  for(i=0; i<imageRows*imageCols; i++)
    hOutputImage[i] = 1234.0;

  Timer t;

  try {
    queue q(d_selector, dpc_common::exception_handler);

    // Print out the device information used for the kernel code.
    std::cout << "Running on device: "
              << q.get_device().get_info<info::device::name>() << "\n";

    // Image convolution in DPC++
    ImageRotation(q, hInputImage, hOutputImage, imageRows, imageCols, sinTheta, cosTheta);
  } catch (exception const &e) {
    std::cout << "An exception is caught for image convolution.\n";
    std::terminate();
  }

  std::cout << t.elapsed().count() << " seconds\n";

  /* Save the output bmp */
  printf("Output image saved as: cat-rotated.bmp\n");
  writeBmpFloat(hOutputImage, "cat-rotated.bmp", imageRows, imageCols,
          inputImagePath);

#if 0
//#ifndef FPGA_PROFILE
  /* Verify result */
  float *refOutput = rotationGoldFloat(hInputImage, imageRows, imageCols,
    sinTheta, cosTheta);

  writeBmpFloat(refOutput, "cat-rotated-ref.bmp", imageRows, imageCols,
          inputImagePath);

  bool passed = true;
  for (i = 0; i < imageRows*imageCols; i++) {
    if (fabsf(refOutput[i]-hOutputImage[i]) > 0.001f) {
        printf("%f %f\n", refOutput[i], hOutputImage[i]);
        passed = false;
    }
  }
  if (passed) {
    printf("Passed!\n");
    std::cout << "Image Rotation successfully completed on device.\n";
  }
  else {
    printf("Failed!\n");
  }
#endif

  return 0;
}
