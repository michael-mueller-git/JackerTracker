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
