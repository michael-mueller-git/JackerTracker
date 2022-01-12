# Build Application

## Arch Linux

First Install the dependencies:

```bash
sudo pacman -Sy vtk cuda nvidia opencv-cuda cmake make gcc hdf5 pugixml glew openmpi fmt ffmpeg base-devel git unzip
git clone https://aur.archlinux.org/nvidia-sdk.git
# now download Video_Codec_SDK_11.1.5.zip from https://developer.nvidia.com/nvidia-video-codec-sdk/download (registration required) into nvidia-sdk
makepkg -si
```

Then build the application with:

```
mkdir build && cd build
cmake -DARCH_LINUX=ON ..
make -j 24
```

Now download the following Models to your build directory:

```
https://www.dropbox.com/s/rr1lk9355vzolqv/dasiamrpn_model.onnx?dl=0
https://www.dropbox.com/s/qvmtszx5h339a0w/dasiamrpn_kernel_cls1.onnx?dl=0
https://www.dropbox.com/s/999cqx5zrfi7w4p/dasiamrpn_kernel_r1.onnx?dl=0
```
