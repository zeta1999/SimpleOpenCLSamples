/*
// Copyright (c) 2019-2020 Ben Ashbaugh
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
*/

#include <CL/cl2.hpp>
#include "bmp.hpp"

#include <chrono>
#include <ctime>

const char* filename = "julia.bmp";

size_t iterations = 16;
size_t gwx = 512;
size_t gwy = 512;
size_t lwx = 0;
size_t lwy = 0;

float cr = -0.123f;
float ci =  0.745f;

cl::CommandQueue commandQueue;
cl::Kernel kernel;
cl::Buffer deviceMemDst;

static const char kernelString[] = R"CLC(
kernel void Julia( global uchar4* dst, float cr, float ci )
{
    const float cMinX = -1.5f;
    const float cMaxX =  1.5f;
    const float cMinY = -1.5f;
    const float cMaxY =  1.5f;

    const int cWidth = get_global_size(0);
    const int cIterations = 16;

    int x = (int)get_global_id(0);
    int y = (int)get_global_id(1);

    float a = x * ( cMaxX - cMinX ) / cWidth + cMinX;
    float b = y * ( cMaxY - cMinY ) / cWidth + cMinY;

    float result = 0.0f;
    const float thresholdSquared = cIterations * cIterations / 64.0f;

    for( int i = 0; i < cIterations; i++ ) {
        float aa = a * a;
        float bb = b * b;

        float magnitudeSquared = aa + bb;
        if( magnitudeSquared >= thresholdSquared ) {
            break;
        }

        result += 1.0f / cIterations;
        b = 2 * a * b + ci;
        a = aa - bb + cr;
    }

    result = max( result, 0.0f );
    result = min( result, 1.0f );

    // BGRA
    float4 color = (float4)( 1.0f, sqrt(result) , result, 1.0f );

    dst[ y * cWidth + x ] = convert_uchar4(color * 255.0f);
}
)CLC";

static void init( void )
{
    // No initialization is needed for this sample.
}

static void go()
{
    kernel.setArg(0, deviceMemDst);
    kernel.setArg(1, cr);
    kernel.setArg(2, ci);

    cl::NDRange lws;    // NullRange by default.

    printf("Executing the kernel %d times\n", (int)iterations);
    printf("Global Work Size = ( %d, %d )\n", (int)gwx, (int)gwy);
    if( lwx > 0 && lwy > 0 )
    {
        printf("Local Work Size = ( %d, %d )\n", (int)lwx, (int)lwy);
        lws = cl::NDRange{lwx, lwy};
    }
    else
    {
        printf("Local work size = NULL\n");
    }

    // Ensure the queue is empty and no processing is happening
    // on the device before starting the timer.
    commandQueue.finish();

    auto start = std::chrono::system_clock::now();
    for( int i = 0; i < iterations; i++ )
    {
        commandQueue.enqueueNDRangeKernel(
            kernel,
            cl::NullRange,
            cl::NDRange{gwx, gwy},
            lws);
    }

    // Enqueue all processing is complete before stopping the timer.
    commandQueue.finish();

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<float> elapsed_seconds = end - start;
    printf("Finished in %f seconds\n", elapsed_seconds.count());
}

static void checkResults()
{
    auto buf = reinterpret_cast<const uint32_t*>(
        commandQueue.enqueueMapBuffer(
            deviceMemDst,
            CL_TRUE,
            CL_MAP_READ,
            0,
            gwx * gwy * sizeof(cl_uchar4) ) );

    BMP::save_image(buf, gwx, gwy, filename);
    printf("Wrote image file %s\n", filename);

    commandQueue.enqueueUnmapMemObject(
        deviceMemDst,
        (void*)buf ); // TODO: Why isn't this a const void* in the API?
}

int main(
    int argc,
    char** argv )
{
    bool printUsage = false;
    int platformIndex = 0;
    int deviceIndex = 0;

    if( argc < 1 )
    {
        printUsage = true;
    }
    else
    {
        for( size_t i = 1; i < argc; i++ )
        {
            if( !strcmp( argv[i], "-d" ) )
            {
                if( ++i < argc )
                {
                    deviceIndex = strtol(argv[i], NULL, 10);
                }
            }
            else if( !strcmp( argv[i], "-p" ) )
            {
                if( ++i < argc )
                {
                    platformIndex = strtol(argv[i], NULL, 10);
                }
            }
            else if( !strcmp( argv[i], "-p" ) )
            {
                if( ++i < argc )
                {
                    platformIndex = strtol(argv[i], NULL, 10);
                }
            }
            else if( !strcmp( argv[i], "-i" ) )
            {
                if( ++i < argc )
                {
                    iterations = strtol(argv[i], NULL, 10);
                }
            }
            else if( !strcmp( argv[i], "-gwx" ) )
            {
                if( ++i < argc )
                {
                    gwx = strtol(argv[i], NULL, 10);
                }
            }
            else if( !strcmp( argv[i], "-gwy" ) )
            {
                if( ++i < argc )
                {
                    gwy = strtol(argv[i], NULL, 10);
                }
            }
            else if( !strcmp( argv[i], "-lwx" ) )
            {
                if( ++i < argc )
                {
                    lwx = strtol(argv[i], NULL, 10);
                }
            }
            else if( !strcmp( argv[i], "-lwy" ) )
            {
                if( ++i < argc )
                {
                    lwy = strtol(argv[i], NULL, 10);
                }
            }
            else
            {
                printUsage = true;
            }
        }
    }
    if( printUsage )
    {
        fprintf(stderr,
            "Usage: julia   [options]\n"
            "Options:\n"
            "      -d: Device Index (default = 0)\n"
            "      -p: Platform Index (default = 0)\n"
            "      -i: Number of Iterations (default = 16)\n"
            "      -gwx: Global Work Size X AKA Image Width (default = 512)\n"
            "      -gwy: Global Work Size Y AKA Image Height (default = 512)\n"
            "      -lwx: Local Work Size X (default = 0 = NULL Local Work Size)\n"
            "      -lwy: Local Work Size Y (default = 0 = Null Local Work size)\n"
            );

        return -1;
    }

    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    printf("Running on platform: %s\n",
        platforms[platformIndex].getInfo<CL_PLATFORM_NAME>().c_str() );

    std::vector<cl::Device> devices;
    platforms[platformIndex].getDevices(CL_DEVICE_TYPE_ALL, &devices);

    printf("Running on device: %s\n",
        devices[deviceIndex].getInfo<CL_DEVICE_NAME>().c_str() );

    cl::Context context{devices[deviceIndex]};
    commandQueue = cl::CommandQueue{context, devices[deviceIndex]};

    cl::Program program{ context, kernelString };
    program.build();
#if 0
    for( auto& device : program.getInfo<CL_PROGRAM_DEVICES>() )
    {
        printf("Program build log for device %s:\n",
            device.getInfo<CL_DEVICE_NAME>().c_str() );
        printf("%s\n",
            program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device).c_str() );
    }
#endif
    kernel = cl::Kernel{ program, "Julia" };

    deviceMemDst = cl::Buffer{
        context,
        CL_MEM_ALLOC_HOST_PTR,
        gwx * gwy * sizeof( cl_uchar4 ) };

    init();
    go();
    checkResults();

    return 0;
}