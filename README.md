# sprigcam
proof of concept of a camera mod for hack club's sprig videogame console
# build
you only need a ESP32-CAM module. connect the esp's TX (UOT) to the pico's GP1 and the esp's RX (UOR) to the pico's GP0, you can either solder to the pads or sandwich the cables between the pi and the sprig.
# power
i (in a very convoluted pick) use a UART reader i used to debug the power as a usb-c power source for the esp32cam, but i'm sure there's a way to power the esp32 using the sprig. maybe. there's a lot of implementing to be done in the power side
# where do the photos go
take photos with the L button! the esp creates its own wifi network where it locally hosts a simple website where you can download every photo you make. photos are stored on an sd card (the module has an sd card slot)
# code
the code is not very solid, and because of how serial works, the image gets refreshed very slowly on the sprig, at about 1fps. maybe there was a better way of doing it. i also took the design choice of storing the images at the same resolution they get rendered in the sprig's screen, in that way what you see is what you get.
# rock
n roll
