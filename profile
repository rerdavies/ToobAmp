sudo rm /tmp/prof.out
sudo nice -n 2 build/src/ConvolutionReverbTest convolution_benchmark --profile /tmp/prof.out
google-pprof -gv build/src/ConvolutionReverbTest /tmp/prof.out
