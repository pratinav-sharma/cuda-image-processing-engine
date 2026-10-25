# Image fixtures

These tiny test images were generated locally with Windows System.Drawing and
are checked in so test execution needs no image-generation dependencies.

`rgb.png` and `rgb.bmp` contain a 3x2 image, in row order:

```text
red    green  blue
white  black  gray(128)
```

`rgb.jpg` encodes the same source; its exact colors are not asserted because JPEG
is lossy. The BMP is a 24-bit bottom-up file with padded rows, exercising layout
conversion to packed, top-to-bottom RGB.

`rgba.png` contains two pixels: red with alpha 64 and blue with alpha 128. Loading
deliberately discards alpha without compositing, so RGB remains red and blue.
