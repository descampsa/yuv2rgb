# yuv2rgb
C99 library for fast image conversion from yuv420p to rgb24

This is a simple library for optimized image conversion from YUV420p to rgb24.
It was done mainly as an exercise to learn to use sse instrinsics, so there may still be room for optimization.

There is a standard c optimized function, and two sse function (with aligned and unaligned memory
The sse version requires only SSE2, which is available on any reasonnably recent CPU.
The library also supports the three different YUV (YCrCb to be correct) transformations that exist (see comments in code), and others can be added simply.

There is a simple test program, that convert a raw YUV file to rgb ppm format, and measure computation time.
It also compares the result and computation time with the ffmpeg implementation (that uses MMX), and, if enabled, with the IPP function.

To compile, simply do :

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make

To test the program, you will have to generate a raw YUV file first, you can use avconv (previously ffmpeg) for that:

    avconv -i example.jpg -c:v rawvideo -pix_fmt yuv420p example.yuv

Then use the test program like that:

    ./test_yuv_rgb image.yuv 4096 2160 image
  
The second and third parameters are image width and height (that are needed because not available in the raw YUV file), and fourth parameter is the output filename template (several output files will be generated, named for example output_sse.ppm, output_av.ppm, etc.)

On my computer, the test program on a 4K image gave the following:

    Time will be measured in each configuration for 100 iterations...
    Processing time (standard) : 2.053917 sec
    Processing time (sse2) : 0.703588 sec
    Processing time (ffmpeg) : 1.221852 sec
    Processing time (ipp) : 0.695975 sec
    Processing time (sse2 aligned) : 0.586154 sec
    Processing time (ffmpeg aligned) : 1.212139 sec
    Processing time (ipp aligned) : 0.641598 sec

configuration : gcc 4.9.2, swscale 3.0.0, IPP 9.0.1, intel i7-5500U

Unexpectedly, my code is as fast as IPP, even a little bit faster in the aligned configuration!
