# PixelView

An image viewer optimized for low-res pixel graphics.

**Features**:
- displays common JPEG, PNG and other common image formats
- directly imports and renders ANSi art in various formats
- uses an "antialiased blocky pixels"-type filter for upscaling
- ... or can be set to only allow integer scaling ratios
- special features for very tall or wide images or ANSIs:
  - single keypress to go to upper-left or lower-right corners
  - smooth automatic scrolling
- can save display settings (zoom level etc.) for each file
- support for images with non-square pixel aspect ratios
- fullscreen mode
- minimal UI
- smoothly animated zoom


## User's Manual

Just run the executable to open the (initially blank) application window. Drag & drop an image file there to view it.

Alternatively, run the executable with the full path of an image file as a command-line argument (e.g. by dragging and dropping an input file from the file manager onto the PixelView executable). In this case, PixelView will start up directly in full-screen mode and show the image.

PixelView has four basic view modes:
- **Fit** mode fits the whole image into the screen or window, leaving black bars around it if the aspect ratios don't match. This is the default mode when a new image is opened.
- **Fill** mode shows the center of the image so that it fills the entire screen, cropping away parts of the image if the aspect ratios don't match.
- **Free** mode can show any part of the image with any magnification. This mode is automatically entered if the user pans or zooms around in the image.
- **Panel** mode is only available for *very* wide or tall images. It splits the whole image into multiple strips that are laid out on the screen next to each other, giving a good overview of the whole image.

In addition, there's two scaling modes: normal and integer-only scaling. Normal mode allows any zoom level and will try to display the image with little to no aliasing at all times. In integer scaling mode, only integral scaling factors are allowed, with the following side effects:
- When zooming in, pixels are shown as squares with an integer size, eliminating any aliasing.
- The Fit and Fill modes will respect integer scaling too, possibly causing the image to _not_ fill the entire screen. The user can, however, allow some amount of cropping along the edges in order to enable a higher zoom level.
- Integer scaling is not available for images with non-square pixels or in panel mode, because those features just don't mix.

For very tall or wide images, PixelView supports an automatic smooth scrolling feature where the visible area of the image is moved by a constant number of pixels with every video frame.

The currently configured view mode, scaling mode, aspect ratio, zoom level and display position can be saved into a file, which will then be automatically loaded if the associated image is opened the next time. The files are put into the same directory as the images, with the same name, but an additional `.pxv` extension. They are human-readable (and -editable) text files.

The following keyboard or mouse bindings are available:

| Event | Action
|-------|-------|
| **F1** | Show or hide a help window.
| **F2** or **Tab** | Show or hide the configuration window, where view mode, scaling mode, aspect ratio etc. can be configured
| **F10**, or **Q**, or **Esc** twice | quit the program
| **F** or **Numpad Multiply** | Switch to Fit mode, or to Fill mode if already there.
| **Z** or **Numpad Divide** | Switch to a 1:1 zoom mode, or Fit mode if already there.
| **T** | Switch to 1:1 zoom, or Fill mode if already there. In addition, move the visible part to the upper-left corner of the image. This also switches the view mode to Free.
| **I** | Toggle integer scaling.
| **P** | Switch into panel mode, or return to Free mode from there. This does nothing if the image isn't extremely tall or wide.
| **Numpad Plus** / **Numpad Minus** or **Mouse Wheel** | Zoom into or out of the image. This also switches the view mode to Free.
| click and hold **Left** or **Middle Mouse Button** | Move the visible area ("panning"). This also switches the view mode to Free.
| **Cursor Keys** | Move the visible area by a few pixels in one of the four main directions. This also switches the view mode to Free.
| **Ctrl** + Cursor Keys | Same, but faster (more pixels per keypress).
| **Shift** + Cursor Keys | Same, but slower (less pixels per keypress).
| **Alt** + Cursor Keys | Start automatic scrolling in one of the four main directions.
| **S** | Stop automatic scrolling. If no scrolling is in progress, start scrolling in the direction with the highest distance to the edge. (For example, scroll upwards in a tall image if the currently viewed area is in the lower half of the image.)
| number keys **1** to **9** | Set the automatic scrolling speed to one of nine presets, from slow (1) to fast (9). If no scrolling is in progress, start scrolling in an automatic direction, just like with the S key.
| **Home** / **End** | Quickly move the visible area to the upper-left or lower-right corner of the image. This also switches the view mode to Free.
| **Ctrl** + **S**, or **F6** | Save the current view settings into a file.
| **Page Up** / **Page Down** | Load the previous or next image file from the same directory as the currently shown image. (The sort order is case-insensitive lexicographic without any fancy support for diacritics or numerical sorting.)
| **Ctrl** + **Home** / **Ctrl** + **End** | Load the first or last image file from the same directory as the currently shown image.


## Caveats / Known Issues

- **unsafe:** never run it on untrusted data!
- does **not** support any kind of animated images (e.g. GIFs)
- some aliasing may still be seen when downscaling, especially during animations
- some display configuration items are screen size dependent (e.g. zoom level)
- the display area may sometimes make a sudden jump at the end of an animation
- scrolling speed is frame-rate dependent (the unit is pixels *per frame*, not per second); this is a deliberate decision to guarantee 100% stable and smooth scrolling
- animations have been tuned for 60 fps displays and may be too fast or slow on anything alse
- screen is updated every frame, even if nothing moves
- requires OpenGL 3.3 acceleration


## Build Instructions

The repository **must** be cloned recursively, otherwise the third-party library dependencies will be missing. After that, a standard [CMake](https://cmake.org) flow is used for the main build process.

For example, on current Ubuntu and Debian systems, the following commands should install all prerequisites and build a release binary in `PixelView/build/pixelview`:

    sudo apt install build-essential libglfw3-dev cmake ninja
    git clone --recursive https://github.com/kajott/PixelView
    cd PixelView
    cmake -S. -B_build -GNinja -DCMAKE_BUILD_TYPE=Release
    cmake --build _build

On other Linux distributions, the process is exactly the same, except that the names and means of installation of the prerequisite packages will differ of course.

On Windows, installations of Visual Studio (version 2017 or later, any edition), CMake and Git are required. Then, a "x64 Native Tools Command Prompt" can be opened and almost the same commands can be used:

    git clone --recursive https://github.com/kajott/PixelView
    cd PixelView
    cmake -S. -B_build
    cmake --build _build --config Release

On both platforms, instead of typing all these commands, Visual Studio Code and its CMake extensions can also be used to do all the heavy lifting.


## Credits

PixelView is written by Martin J. Fiedler (<keyj@emphy.de>).

Used third-party software:

- [GLFW](https://www.glfw.org/)
  for window and OpenGL context creation and event handling
- [Dear ImGui](https://github.com/ocornut/imgui)
  for the user interface
- the OpenGL function loader has been generated with
  [GLAD](https://glad.dav1d.de/)
- Sean Barrett's [STB](https://github.com/nothings/stb) libs
  for image file I/O
- [libansilove](https://github.com/ansilove/libansilove)
  for rendering ANSi files
